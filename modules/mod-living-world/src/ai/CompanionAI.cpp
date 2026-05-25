#include "ai/CompanionAI.h"
#include "ai/BotHazardSensor.h"
#include "ai/CompanionFollowFormation.h"

#include "Chat.h"
#include "CellImpl.h"
#include "Config.h"
#include "Duration.h"
#include "EventProcessor.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "MotionMaster.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "integration/BotActivityLog.h"
#include "integration/SqlAccountAltRuntimeRepository.h"
#include "integration/SqlBotShellRuntimeRepository.h"
#include "integration/SqlBotCombatDefaultProfileRepository.h"
#include "integration/SqlBotCombatProfileRepository.h"
#include "integration/SqlBotCombatProfileSelectionRepository.h"
#include "integration/SqlBotGlobalConfigRepository.h"
#include "model/BotCombatMode.h"
#include "model/BotCombatProfile.h"
#include "model/BotGlobalConfig.h"
#include "service/BotContextService.h"
#include "service/BotOocConfigService.h"
#include "integration/SqlBotOocConfigRepository.h"
#include "service/BotCombatDoctrineResolver.h"
#include "service/BotCombatActionExecution.h"
#include "service/BotCombatProfilePreparationService.h"
#include "service/BotCombatRuntimeEvaluator.h"
#include "service/BotGlobalConfigService.h"
#include "service/BotPlayerRegistry.h"
#include "service/SimpleBotCombatSpecRoleResolver.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace living_world
{
namespace ai
{

service::BotContextService& GetSharedContextService()
{
    static service::BotContextService service;
    return service;
}

bool IsFriendlySupportBotCandidate(Player* bot, Unit* candidate)
{
    if (!bot || !candidate || !candidate->IsAlive() || !candidate->IsInWorld())
        return false;

    if (!bot->IsFriendlyTo(candidate))
        return false;

    if (candidate == bot)
        return true;

    if (Player* player = candidate->ToPlayer())
    {
        model::BotRuntimeKind const kind =
            service::BotPlayerRegistry::Instance().GetBotRuntimeKind(player->GetGUID());
        return kind == model::BotRuntimeKind::LedgerShell
            || kind == model::BotRuntimeKind::Ambient
            || kind == model::BotRuntimeKind::Companion;
    }

    return false;
}

std::vector<Player*> CollectNearbyFriendlySupportPlayers(Player* bot, float radius, bool includeSelf)
{
    std::vector<Player*> players;
    if (!bot || radius <= 0.0f)
        return players;

    std::vector<Unit*> allies;
    if (includeSelf)
        allies.push_back(bot);

    Acore::AnyFriendlyUnitInObjectRangeCheck check(bot, bot, radius);
    Acore::UnitListSearcher<Acore::AnyFriendlyUnitInObjectRangeCheck> searcher(bot, allies, check);
    Cell::VisitObjects(bot, searcher, radius);

    std::sort(allies.begin(), allies.end());
    allies.erase(std::unique(allies.begin(), allies.end()), allies.end());

    for (Unit* ally : allies)
    {
        if (ally == bot && !includeSelf)
            continue;
        if (!IsFriendlySupportBotCandidate(bot, ally))
            continue;
        if (Player* player = ally->ToPlayer())
            players.push_back(player);
    }

    return players;
}

// ---------------------------------------------------------------
// Per-bot command override state
// ---------------------------------------------------------------

struct BotOverride
{
    ObjectGuid forcedTarget;                                         // Empty = no forced target
    bool       attackLocked  = false;                                // .lwbot attack latch until combat resolves/disengage
    bool       disengaged    = false;
    std::chrono::steady_clock::time_point disengageExpiry = {};      // Zero = never expires
    std::chrono::steady_clock::time_point retreatExpiry   = {};      // Zero = not retreating
};

static std::mutex                                    s_overrideMutex;
static std::unordered_map<ObjectGuid, BotOverride>   s_overrides;

struct FollowDiagnosticSnapshot
{
    std::uint32_t formation = 0;
    std::uint32_t slotCount = 0;
    std::uint32_t rosterSize = 0;
    std::uint32_t rosterIndex = 0;
    std::uint32_t slot = 0;
    bool usedRosterSlot = false;
    float angle = 0.0f;
    float distance = 0.0f;
};

static std::mutex s_followDiagnosticMutex;
static std::unordered_map<std::uint64_t, FollowDiagnosticSnapshot> s_lastFollowDiagnostics;
static std::unordered_map<std::uint64_t, std::uint32_t> s_lastCompanionPetSummonAttemptMs;

static BotOverride GetOverride(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    auto it = s_overrides.find(botGuid);
    return it != s_overrides.end() ? it->second : BotOverride{};
}

static void ModifyOverride(ObjectGuid botGuid, std::function<void(BotOverride&)> fn)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    fn(s_overrides[botGuid]);
    // Clean up empty entries
    auto const now = std::chrono::steady_clock::now();
    auto it = s_overrides.find(botGuid);
    if (it != s_overrides.end()
        && !it->second.forcedTarget
        && !it->second.attackLocked
        && !it->second.disengaged
        && it->second.retreatExpiry <= now)
        s_overrides.erase(it);
}

void SetBotForcedTarget(ObjectGuid botGuid, ObjectGuid targetGuid)
{
    ModifyOverride(botGuid, [&](BotOverride& o) {
        o.forcedTarget = targetGuid;
        o.disengaged   = false; // attacking clears disengage
        if (targetGuid)
            o.attackLocked = true;
    });
}

void SetBotDisengaged(ObjectGuid botGuid, bool disengaged)
{
    ModifyOverride(botGuid, [&](BotOverride& o) {
        o.disengaged      = disengaged;
        o.forcedTarget    = ObjectGuid::Empty; // disengaging clears forced target
        o.attackLocked    = false;
        // Auto-expire after 500ms so a subsequent r-click re-enables assist.
        o.disengageExpiry = disengaged
            ? std::chrono::steady_clock::now() + std::chrono::milliseconds(500)
            : std::chrono::steady_clock::time_point{};
    });
}

void ClearBotOverride(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    s_overrides.erase(botGuid);
    s_lastCompanionPetSummonAttemptMs.erase(botGuid.GetCounter());
    BotHazardSensor::ClearHazardState(botGuid);
    GetSharedContextService().Clear(botGuid.GetCounter());
}

bool SetBotRetreat(ObjectGuid botGuid, uint32_t durationMs)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    BotOverride& o = s_overrides[botGuid];
    auto const now = std::chrono::steady_clock::now();
    if (o.retreatExpiry > now)
    {
        // Already retreating — cancel.
        o.retreatExpiry = {};
        return false;
    }
    o.retreatExpiry = now + std::chrono::milliseconds(durationMs);
    // Disengage/forced target cleared while retreating.
    o.disengaged    = false;
    o.forcedTarget  = ObjectGuid::Empty;
    o.attackLocked  = false;
    return true;
}

bool IsBotAttackLocked(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    auto it = s_overrides.find(botGuid);
    if (it == s_overrides.end())
        return false;
    return it->second.attackLocked;
}

bool IsBotRetreating(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    auto it = s_overrides.find(botGuid);
    if (it == s_overrides.end())
        return false;
    return it->second.retreatExpiry > std::chrono::steady_clock::now();
}

namespace
{
// --- Follow / reposition constants ---
constexpr float FollowDistance        = 2.0f;
constexpr float FollowAngle           = 3.14159265358979323846f;
constexpr std::uint32_t SummonWaterElementalSpellId = 31687u;
constexpr std::uint32_t CompanionPetSummonRetryMs = 5000u;
// --- Heal thresholds ---

// --- Priest Weakened Soul debuff: prevents re-shielding for 15 seconds ---
constexpr std::uint32_t AuraWeakenedSoul = 6788;

// ---------------------------------------------------------------
// Role classification
// ---------------------------------------------------------------

enum class BotCombatRole
{
    Healer,
    HybridHealer,
    Ranged,
    Melee
};

BotCombatRole GetCombatRole(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_PRIEST:
            return BotCombatRole::Healer;
        case CLASS_DRUID:
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
            return BotCombatRole::HybridHealer;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_HUNTER:
            return BotCombatRole::Ranged;
        default:
            return BotCombatRole::Melee;
    }
}

struct BotCombatResolvedProfile
{
    model::BotCombatProfileSettings settings;
    std::vector<model::BotCombatEntryDefinition> interruptEntries;
    std::vector<model::BotCombatEntryDefinition> rotationEntries;
};

struct BotCombatDoctrine
{
    BotCombatRole role = BotCombatRole::Melee;
    std::string effectiveSpecKey;
    std::string effectiveRoleKey;
    BotCombatResolvedProfile profile;
    model::BotCombatProfileSettings settings;
};

struct CachedBotCombatDoctrine
{
    BotCombatDoctrine doctrine;
    std::chrono::steady_clock::time_point expiresAt;
};

struct CachedPreparedBotCombatProfile
{
    service::BotCombatPreparedProfile profile;
    std::chrono::steady_clock::time_point expiresAt;
};

std::mutex s_doctrineMutex;
std::unordered_map<std::uint64_t, CachedBotCombatDoctrine> s_doctrineByBotGuid;
std::mutex s_preparedProfileMutex;
std::unordered_map<std::uint64_t, CachedPreparedBotCombatProfile> s_preparedProfileByBotGuid;
constexpr auto DoctrineCacheTtl = std::chrono::seconds(5);

std::uint32_t FindBestKnownSpellInChain(Player* bot, std::uint32_t baseSpellId);

service::BotGlobalConfigService& GetGlobalConfigService()
{
    static integration::SqlBotGlobalConfigRepository repo;
    static service::BotGlobalConfigService            service(repo);
    return service;
}

service::BotOocConfigService& GetOocConfigService()
{
    static integration::SqlBotOocConfigRepository repo;
    static service::BotOocConfigService           service(repo);
    return service;
}

service::BotCombatDoctrineResolver& GetDoctrineResolver()
{
    static integration::SqlAccountAltRuntimeRepository runtimeRepository;
    static integration::SqlBotCombatProfileRepository profileRepository;
    static integration::SqlBotCombatProfileSelectionRepository selectionRepository;
    static integration::SqlBotCombatDefaultProfileRepository defaultProfileRepository;
    static service::SimpleBotCombatSpecRoleResolver resolver;
    static service::BotCombatDoctrineResolver doctrineResolver(
        runtimeRepository,
        profileRepository,
        selectionRepository,
        defaultProfileRepository,
        resolver,
        GetSharedContextService());
    return doctrineResolver;
}

service::BotCombatProfilePreparationService& GetProfilePreparationService()
{
    static service::BotCombatProfilePreparationService preparationService(
        GetDoctrineResolver());
    return preparationService;
}

service::BotCombatRuntimeEvaluator& GetRuntimeEvaluator()
{
    static service::BotCombatRuntimeEvaluator runtimeEvaluator;
    return runtimeEvaluator;
}

BotCombatRole ResolveDoctrineRole(
    std::uint8_t classId,
    std::string const& specKey,
    std::string const& roleKey)
{
    if (roleKey == "HEAL")
    {
        switch (classId)
        {
            case CLASS_PRIEST:
                return BotCombatRole::Healer;
            case CLASS_DRUID:
            case CLASS_PALADIN:
            case CLASS_SHAMAN:
                return BotCombatRole::HybridHealer;
            default:
                return BotCombatRole::Healer;
        }
    }

    if (roleKey == "TANK")
        return BotCombatRole::Melee;

    if (roleKey == "DPS")
    {
        switch (classId)
        {
            case CLASS_HUNTER:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                return BotCombatRole::Ranged;
            case CLASS_PRIEST:
                return specKey == "Shadow"
                    ? BotCombatRole::Ranged
                    : BotCombatRole::Healer;
            case CLASS_DRUID:
                return specKey == "Balance"
                    ? BotCombatRole::Ranged
                    : BotCombatRole::Melee;
            case CLASS_SHAMAN:
                return specKey == "Elemental"
                    ? BotCombatRole::Ranged
                    : BotCombatRole::Melee;
            case CLASS_PALADIN:
                return specKey == "Holy"
                    ? BotCombatRole::HybridHealer
                    : BotCombatRole::Melee;
            default:
                return BotCombatRole::Melee;
        }
    }

    return GetCombatRole(classId);
}

BotCombatDoctrine LoadCombatDoctrine(Unit* bot, Player* owner)
{
    BotCombatDoctrine doctrine;
    std::uint32_t ownerAccountId = (owner && owner->GetSession())
        ? owner->GetSession()->GetAccountId()
        : 0;
    service::BotCombatDoctrineResolution resolution =
        GetDoctrineResolver().ResolveForBot(
            bot->GetGUID().GetCounter(),
            bot->getClass(),
            ownerAccountId);
    doctrine.settings    = resolution.profile.settings;
    doctrine.effectiveSpecKey = resolution.effectiveSpecKey;
    doctrine.effectiveRoleKey = resolution.effectiveRoleKey;

    doctrine.role = ResolveDoctrineRole(
        bot->getClass(),
        resolution.effectiveSpecKey,
        resolution.effectiveRoleKey);
    return doctrine;
}

BotCombatDoctrine GetCombatDoctrine(Unit* bot, Player* owner)
{
    std::uint64_t const botGuid = bot->GetGUID().GetCounter();
    auto const now = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(s_doctrineMutex);
        auto it = s_doctrineByBotGuid.find(botGuid);
        if (it != s_doctrineByBotGuid.end() && it->second.expiresAt > now)
            return it->second.doctrine;
    }

    BotCombatDoctrine doctrine = LoadCombatDoctrine(bot, owner);

    {
        std::lock_guard<std::mutex> lock(s_doctrineMutex);
        s_doctrineByBotGuid[botGuid] = { doctrine, now + DoctrineCacheTtl };
    }

    return doctrine;
}

service::BotCombatPreparedProfile GetPreparedCombatProfile(Unit* bot, Player* owner)
{
    std::uint64_t const botGuid = bot->GetGUID().GetCounter();
    auto const now = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(s_preparedProfileMutex);
        auto it = s_preparedProfileByBotGuid.find(botGuid);
        if (it != s_preparedProfileByBotGuid.end() && it->second.expiresAt > now)
            return it->second.profile;
    }

    std::uint32_t ownerAccountId = (owner && owner->GetSession())
        ? owner->GetSession()->GetAccountId()
        : 0;

    // Build the known-spell set. For Player session bots use GetSpellMap().
    // For creature bots this will be loaded from living_world_bot_spell_list
    // once WorldBotCreatureAI is implemented.
    std::unordered_set<std::uint32_t> knownSpells;
    if (Player* player = bot->ToPlayer())
    {
        for (auto const& [spellId, playerSpell] : player->GetSpellMap())
            if (playerSpell && playerSpell->State != PLAYERSPELL_REMOVED && playerSpell->Active)
                knownSpells.insert(spellId);
    }
    // Creature path: knownSpells populated by WorldBotCreatureAI before calling.

    service::BotCombatPreparedProfile preparedProfile =
        GetProfilePreparationService().PrepareForUnit(bot, knownSpells, ownerAccountId);

    {
        std::lock_guard<std::mutex> lock(s_preparedProfileMutex);
        s_preparedProfileByBotGuid[botGuid] =
            { preparedProfile, now + DoctrineCacheTtl };
    }

    return preparedProfile;
}

void PushThreatAddonMessage(Player* bot, Player* owner, Unit* primaryTarget)
{
    if (!bot || !owner || !owner->GetSession() || !primaryTarget)
        return;

    ThreatManager& mgr = primaryTarget->GetThreatMgr();
    float const myThreat = mgr.GetThreat(bot);
    float topThreat = 0.0f;
    for (ThreatReference const* ref : mgr.GetSortedThreatList())
    {
        if (ref->IsOnline())
        {
            topThreat = ref->GetThreat();
            break;
        }
    }
    int const threatPct = (topThreat > 0.0f)
        ? static_cast<int>(myThreat / topThreat * 100.0f) : 0;
    bool const holdsAggro = (mgr.GetCurrentVictim() == bot);

    std::string const msg = "LWBOT\tLWBT:THREAT:" + bot->GetName()
        + ":" + std::to_string(threatPct)
        + ":" + (holdsAggro ? "1" : "0");
    WorldPacket data;
    ChatHandler::BuildChatPacket(
        data, CHAT_MSG_WHISPER, LANG_ADDON, owner, owner, msg);
    owner->GetSession()->SendPacket(&data);
}

bool TryExecuteProfileRotation(Unit* bot, Player* owner, Unit* primaryTarget)
{
    if (!bot)
        return false;

    // Addon threat messages are only meaningful for Player session bots with an owner.
    if (Player* botPlayer = bot->ToPlayer())
        PushThreatAddonMessage(botPlayer, owner, primaryTarget);

    service::BotCombatPreparedProfile preparedProfile =
        GetPreparedCombatProfile(bot, owner);
    if (preparedProfile.interruptEntries.empty()
        && preparedProfile.rotationEntries.empty())
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] ProfileFallback bot='{}' guid={} reason=no_prepared_entries sourceGuid={} slot={} spec='{}' role='{}' sourceKind={}",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            preparedProfile.resolution.sourceCharacterGuid,
            static_cast<std::uint32_t>(preparedProfile.resolution.activeProfileSlot),
            preparedProfile.resolution.effectiveSpecKey,
            preparedProfile.resolution.effectiveRoleKey,
            static_cast<std::uint32_t>(preparedProfile.resolution.source));
        return false;
    }

    service::BotCombatRuntimeContext context;
    context.bot = bot;
    context.owner = owner;
    context.primaryTarget = primaryTarget;
    context.rotationWaitMs = preparedProfile.resolution.profile.settings.rotationWaitMs;
    context.defaultAoEMode = preparedProfile.resolution.profile.settings.defaultAoEMode;
    context.defaultAoEMinTargets = preparedProfile.resolution.profile.settings.defaultAoEMinTargets;
    context.defaultAoEScanRadius = preparedProfile.resolution.profile.settings.defaultAoEScanRadius;
    context.conservationMode = preparedProfile.resolution.profile.settings.conservationMode;
    context.enableDownRank = preparedProfile.resolution.profile.settings.enableDownRank;
    context.downRankFloor = preparedProfile.resolution.profile.settings.downRankFloor;
    context.availableSpells = preparedProfile.availableSpells;

    auto const handleEvaluationResult =
        [&](service::BotCombatEvaluationResult const& result, char const* phase) -> bool
        {
            if (result.disposition == service::BotCombatEvaluationDisposition::None)
                return false;

            if (result.disposition == service::BotCombatEvaluationDisposition::Wait)
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] ProfileActionWait bot='{}' guid={} phase={} waitMs={}",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    phase,
                    result.waitMs);
                return true;
            }

            if (!result.action)
                return false;

            service::BotCombatEvaluatedAction const& evaluatedAction = *result.action;

            if (evaluatedAction.breaksCurrentCast && bot->IsNonMeleeSpellCast(false))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] ProfileActionBreakCast bot='{}' guid={} phase={} entryId={} actionId={} spellId={}",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    phase,
                    evaluatedAction.entryId,
                    evaluatedAction.actionId,
                    evaluatedAction.spellId);
                bot->InterruptNonMeleeSpells(false);
            }

            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] ProfileActionCast bot='{}' guid={} phase={} entryId={} actionId={} actionType={} spellId={} itemId={} itemSelector='{}' simulatedItemUse={} targetKey='{}' targetGuid={} aoeMode={} useDestination={} dest=({:.2f},{:.2f},{:.2f}) breaksCurrentCast={}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                phase,
                evaluatedAction.entryId,
                evaluatedAction.actionId,
                static_cast<std::uint32_t>(evaluatedAction.actionType),
                evaluatedAction.spellId,
                evaluatedAction.itemId,
                evaluatedAction.itemSelector,
                evaluatedAction.simulatedItemUse,
                evaluatedAction.targetKey,
                evaluatedAction.target ? evaluatedAction.target->GetGUID().GetCounter() : 0,
                evaluatedAction.aoeMode ? static_cast<std::int32_t>(*evaluatedAction.aoeMode) : -1,
                evaluatedAction.useDestination,
                evaluatedAction.destinationX,
                evaluatedAction.destinationY,
                evaluatedAction.destinationZ,
                evaluatedAction.breaksCurrentCast);

            return service::CastEvaluatedAction(bot, evaluatedAction);
        };

    if (handleEvaluationResult(
            GetRuntimeEvaluator().EvaluateInterrupts(preparedProfile, context),
            "interrupt"))
    {
        return true;
    }

    if (handleEvaluationResult(
            GetRuntimeEvaluator().EvaluateRotation(preparedProfile, context),
            "rotation"))
    {
        return true;
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] ProfileFallback bot='{}' guid={} reason=no_runtime_action sourceGuid={} slot={} spec='{}' role='{}' interruptEntries={} rotationEntries={} targetGuid={}",
        bot->GetName(),
        bot->GetGUID().GetCounter(),
        preparedProfile.resolution.sourceCharacterGuid,
        static_cast<std::uint32_t>(preparedProfile.resolution.activeProfileSlot),
        preparedProfile.resolution.effectiveSpecKey,
        preparedProfile.resolution.effectiveRoleKey,
        preparedProfile.interruptEntries.size(),
        preparedProfile.rotationEntries.size(),
        primaryTarget ? primaryTarget->GetGUID().GetCounter() : 0);
    return false;
}

bool IsOffenseSuppressed(
    model::BotCombatConservationMode mode,
    bool conserving)
{
    if (mode == model::BotCombatConservationMode::JitCasting)
        return true;

    // Conservative: suppress offense during full hysteresis window.
    // Reserve: suppress offense only while below the resource floor.
    return conserving &&
        (mode == model::BotCombatConservationMode::Conservative ||
         mode == model::BotCombatConservationMode::Reserve);
}

// ---------------------------------------------------------------
// Spell utilities
// ---------------------------------------------------------------

// Walks the spell rank chain from the highest rank downward and returns the
// first spell ID the bot has learned. Returns 0 if none are known.
std::uint32_t FindBestKnownSpellInChain(Player* bot, std::uint32_t baseSpellId)
{
    std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(baseSpellId);
    while (candidate)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(candidate);
        std::uint32_t const requiredLevel = spellInfo
            ? std::max<std::uint32_t>(spellInfo->BaseLevel, spellInfo->SpellLevel)
            : 0u;
        if (bot->HasSpell(candidate)
            && (requiredLevel == 0u || requiredLevel <= bot->GetLevel()))
            return candidate;
        candidate = sSpellMgr->GetPrevSpellInChain(candidate);
    }
    return 0;
}

// Returns true if the target has an aura from any rank of the given spell chain.
bool HasAuraFromChain(Unit const* target, std::uint32_t baseSpellId)
{
    std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(baseSpellId);
    while (candidate)
    {
        if (target->HasAura(candidate))
            return true;
        candidate = sSpellMgr->GetPrevSpellInChain(candidate);
    }
    return false;
}

// Returns the remaining duration in milliseconds of the highest-rank aura from
// the spell chain on target. Returns 0 if the aura is not present.
int32 AuraRemainingMsFromChain(Unit const* target, std::uint32_t baseSpellId)
{
    std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(baseSpellId);
    while (candidate)
    {
        if (Aura const* aura = target->GetAura(candidate))
        {
            int32 dur = aura->GetDuration();
            return dur > 0 ? dur : 0;
        }
        candidate = sSpellMgr->GetPrevSpellInChain(candidate);
    }
    return 0;
}

// Returns true if the aura from baseSpellId on target is missing or has fewer
// than thresholdSecs seconds of duration remaining.
bool AuraNeedsRefresh(Unit const* target, std::uint32_t baseSpellId,
                      std::uint16_t thresholdSecs)
{
    int32 remainingMs = AuraRemainingMsFromChain(target, baseSpellId);
    if (remainingMs == 0)
        return true;
    return remainingMs < static_cast<int32>(thresholdSecs) * 1000;
}

bool TryMaintainBasicCompanionPet(Player* bot, Player* owner)
{
    if (!bot || !owner || !bot->IsAlive())
        return false;

    Guardian* guardianPet = bot->GetGuardianPet();
    if (guardianPet && guardianPet->IsAlive())
    {
        guardianPet->SetReactState(REACT_DEFENSIVE);
        return false;
    }

    if (bot->getClass() != CLASS_MAGE || !bot->HasSpell(SummonWaterElementalSpellId))
        return false;

    if (bot->IsInCombat() || bot->GetVictim() || bot->IsNonMeleeSpellCast(false))
        return false;

    std::uint32_t const nowMs = getMSTime();
    std::uint32_t& lastAttemptMs = s_lastCompanionPetSummonAttemptMs[bot->GetGUID().GetCounter()];
    if (lastAttemptMs != 0 && getMSTimeDiff(lastAttemptMs, nowMs) < CompanionPetSummonRetryMs)
        return false;

    if (bot->HasSpellCooldown(SummonWaterElementalSpellId))
        return false;

    lastAttemptMs = nowMs;
    SpellCastResult const result = bot->CastSpell(bot, SummonWaterElementalSpellId, false);
    if (result != SPELL_CAST_OK)
        return false;

    return true;
}

// Returns true when this bot can still fire mana-based spells. Non-mana
// users (Warriors, Rogues, DKs) always return true. A caster at zero mana
// should switch to melee autoattack rather than spamming failed cast attempts.
bool BotHasManaToFight(Player const* bot)
{
    if (bot->GetMaxPower(POWER_MANA) == 0)
        return true;
    return bot->GetPower(POWER_MANA) > 0;
}

BotCombatResolvedProfile LoadResolvedCombatProfile(Player* bot, Player* owner)
{
    BotCombatResolvedProfile resolvedProfile;
    service::BotCombatPreparedProfile preparedProfile =
        GetPreparedCombatProfile(bot, owner);

    resolvedProfile.settings = preparedProfile.resolution.profile.settings;
    resolvedProfile.interruptEntries = std::move(preparedProfile.interruptEntries);
    resolvedProfile.rotationEntries = std::move(preparedProfile.rotationEntries);

    char const* sourceKind = "none";
    switch (preparedProfile.resolution.source)
    {
        case service::BotCombatDoctrineSource::DefaultProfile:
            sourceKind = "default";
            break;
        case service::BotCombatDoctrineSource::CustomProfile:
            sourceKind = "custom";
            break;
        case service::BotCombatDoctrineSource::CustomProfileWithDefaultFallback:
            sourceKind = "custom_with_default_fallback";
            break;
        case service::BotCombatDoctrineSource::None:
        default:
            sourceKind = "none";
            break;
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] ProfileLoad bot='{}' guid={} sourceGuid={} ownerAccountId={} slot={} sourceKind={} customProfileId={} defaultProfileId={} spec='{}' role='{}' interruptEntries={} rotationEntries={}",
        bot->GetName(),
        bot->GetGUID().GetCounter(),
        preparedProfile.resolution.sourceCharacterGuid,
        preparedProfile.resolution.ownerAccountId,
        static_cast<std::uint32_t>(preparedProfile.resolution.activeProfileSlot),
        sourceKind,
        preparedProfile.resolution.customProfileId.value_or(0),
        preparedProfile.resolution.defaultProfileId.value_or(0),
        preparedProfile.resolution.effectiveSpecKey,
        preparedProfile.resolution.effectiveRoleKey,
        resolvedProfile.interruptEntries.size(),
        resolvedProfile.rotationEntries.size());

    return resolvedProfile;
}

// ---------------------------------------------------------------
// Offensive spells — Paladin seal helpers
// ---------------------------------------------------------------

// Returns the Paladin's best-known seal to apply, preferring the highest-DPS option.
std::uint32_t GetPreferredSeal(Player* bot)
{
    // Seal of Vengeance (Alliance) / Seal of Corruption (Horde): best sustained DPS seal
    if (std::uint32_t s = FindBestKnownSpellInChain(bot, 31801)) return s;
    if (std::uint32_t s = FindBestKnownSpellInChain(bot, 53736)) return s;
    if (std::uint32_t s = FindBestKnownSpellInChain(bot, 20375)) return s; // Seal of Command
    if (std::uint32_t s = FindBestKnownSpellInChain(bot, 20154)) return s; // Seal of Righteousness
    return 0;
}

// Returns true when the Paladin bot has any seal aura active.
bool HasSealActive(Player const* bot)
{
    static constexpr std::array<std::uint32_t, 7> SealBases = {
        20154, // Seal of Righteousness
        20375, // Seal of Command
        31801, // Seal of Vengeance
        53736, // Seal of Corruption
        19854, // Seal of Wisdom
        20165, // Seal of Light
        20164, // Seal of Justice
    };
    for (std::uint32_t base : SealBases)
    {
        if (HasAuraFromChain(bot, base))
            return true;
    }
    return false;
}

constexpr std::uint32_t SpellCategoryFood  = 11;
constexpr std::uint32_t SpellCategoryDrink = 59;

bool InventoryHasConjuredFamilyItem(
    Player* bot,
    std::initializer_list<std::uint32_t> itemIds)
{
    if (!bot)
        return false;

    for (std::uint32_t itemId : itemIds)
    {
        if (itemId != 0 && bot->HasItemCount(itemId, 1))
            return true;
    }

    return false;
}

bool InventoryHasConsumableCategory(Player* bot, std::uint32_t spellCategory)
{
    if (!bot)
        return false;

    auto matches = [&](Item* item) -> bool
    {
        if (!item)
            return false;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || proto->Class != ITEM_CLASS_CONSUMABLE)
            return false;

        for (std::uint8_t i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (static_cast<std::uint32_t>(proto->Spells[i].SpellCategory) == spellCategory)
                return true;
        }

        return false;
    };

    for (std::uint8_t slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot); matches(item))
            return true;
    }

    for (std::uint8_t bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* pBag = bot->GetBagByPos(bag))
        {
            for (std::uint32_t slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                if (Item* item = pBag->GetItemByPos(slot); matches(item))
                    return true;
            }
        }
    }

    return false;
}

bool IsLedgerShellRuntime(Player* bot)
{
    if (!bot)
        return false;

    return service::BotPlayerRegistry::Instance().GetBotRuntimeKind(bot->GetGUID())
        == model::BotRuntimeKind::LedgerShell;
}

std::optional<std::uint32_t> ResolveLedgerShellIdentityId(Player* bot)
{
    if (!bot || !bot->GetSession())
        return std::nullopt;

    living_world::integration::SqlBotShellRuntimeRepository repository;
    std::optional<living_world::model::BotShellRuntimeRecord> shellRuntime =
        repository.FindByShell(
            bot->GetSession()->GetAccountId(),
            bot->GetGUID().GetCounter());
    if (!shellRuntime)
        return std::nullopt;

    return shellRuntime->identityId;
}

Unit* TryAcquireHostileDebugTarget(Player* bot)
{
    if (!bot || !bot->IsAlive())
        return nullptr;

    std::uint32_t const forcedEntry =
        sConfigMgr->GetOption<std::uint32_t>("LivingWorld.DebugForceCombatTargetEntry", 0);
    float const forcedRadius =
        sConfigMgr->GetOption<float>("LivingWorld.DebugForceCombatTargetSearchRadius", 40.0f);

    if (forcedEntry != 0)
    {
        if (Creature* creature = bot->FindNearestCreature(forcedEntry, forcedRadius, true))
        {
            if (bot->IsValidAttackTarget(creature))
                return creature;
        }
    }

    float const hostileAcquireRadius =
        sConfigMgr->GetOption<float>("LivingWorld.DebugHostileAcquireRadius", 0.0f);
    if (hostileAcquireRadius <= 0.0f || !bot->GetMap())
        return nullptr;

    Player* bestTarget = nullptr;
    float bestDistance = hostileAcquireRadius;
    bot->GetMap()->DoForAllPlayers([&](Player* candidate)
    {
        if (!candidate || candidate == bot || !candidate->IsAlive())
            return;

        if (!bot->IsWithinDistInMap(candidate, hostileAcquireRadius))
            return;

        if (!bot->IsValidAttackTarget(candidate))
            return;

        float const distance = bot->GetDistance(candidate);
        if (!bestTarget || distance < bestDistance)
        {
            bestTarget = candidate;
            bestDistance = distance;
        }
    });

    return bestTarget;
}

bool TryCastLedgerShellStartupSpell(
    Player* bot,
    std::uint32_t baseSpellId,
    char const* label)
{
    if (!bot)
        return false;

    std::uint32_t const spellId = FindBestKnownSpellInChain(bot, baseSpellId);
    if (!spellId || bot->HasSpellCooldown(spellId))
        return false;

    SpellCastResult const result = bot->CastSpell(bot, spellId, false);
    if (result != SPELL_CAST_OK)
        return false;

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] LedgerShellStartup bot='{}' guid={} step={} spellId={}",
        bot->GetName(),
        bot->GetGUID().GetCounter(),
        label,
        spellId);
    return true;
}

void ApplyBotBuff(Player* bot, Player* owner, model::BotOocBehavior const& ooc);

bool TryRunLedgerShellStartupPrep(Player* bot, model::BotOocBehavior const& ooc)
{
    if (!bot || !IsLedgerShellRuntime(bot) || bot->IsInCombat() || !bot->IsAlive())
        return false;

    if (bot->IsNonMeleeSpellCast(false))
        return true;

    if (bot->getClass() == CLASS_MAGE)
    {
        if (!InventoryHasConjuredFamilyItem(bot, { 5514u, 5513u, 8007u, 8008u, 22044u, 33312u })
            && TryCastLedgerShellStartupSpell(bot, 759u, "conjure_mana_gem"))
            return true;

        if (!InventoryHasConsumableCategory(bot, SpellCategoryFood)
            && TryCastLedgerShellStartupSpell(bot, 587u, "conjure_food"))
            return true;

        if (!InventoryHasConsumableCategory(bot, SpellCategoryDrink)
            && TryCastLedgerShellStartupSpell(bot, 5504u, "conjure_water"))
            return true;
    }

    if (ooc.buffScope != model::BotBuffScope::Off)
    {
        bool const wasCasting = bot->IsNonMeleeSpellCast(false);
        ApplyBotBuff(bot, nullptr, ooc);
        if (!wasCasting && bot->IsNonMeleeSpellCast(false))
            return true;
    }

    return false;
}

// ---------------------------------------------------------------
// Out-of-combat maintenance
// ---------------------------------------------------------------

// Core buff application — no combat guard. Called by both the idle tick and
// the on-spawn path. Respects buffScope (self/party) and buffReapplySecs.
void ApplyBotBuff(Player* bot, Player* owner, model::BotOocBehavior const& ooc)
{
    if (bot->IsNonMeleeSpellCast(false))
        return;
    if (ooc.buffScope == model::BotBuffScope::Off)
        return;

    uint16_t const thresh = ooc.buffReapplySecs;

    // Helper: resolve the target list based on scope.
    // Self scope: {bot}. Party scope: all connected party members + bot.
    auto buildTargets = [&]() -> std::vector<Player*>
    {
        std::vector<Player*> targets;
        if (ooc.buffScope == model::BotBuffScope::Self)
        {
            targets.push_back(bot);
            return targets;
        }
        if (Group const* group = bot->GetGroup())
        {
            for (Group::MemberSlot const& slot : group->GetMemberSlots())
            {
                Player* t = ObjectAccessor::FindConnectedPlayer(slot.guid);
                if (t && t->IsAlive() && t->IsInWorld())
                    targets.push_back(t);
            }
        }
        else
        {
            targets = CollectNearbyFriendlySupportPlayers(
                bot,
                40.0f,
                true);
        }
        if (targets.empty())
            targets.push_back(bot);
        return targets;
    };

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        {
            std::uint32_t const shout = FindBestKnownSpellInChain(bot, 6673);
            if (shout && AuraNeedsRefresh(bot, 6673, thresh))
                bot->CastSpell(bot, shout, false);
            break;
        }

        case CLASS_DEATH_KNIGHT:
        {
            if (bot->HasSpell(57330) && AuraNeedsRefresh(bot, 57330, thresh))
                bot->CastSpell(bot, 57330U, false);
            break;
        }

        case CLASS_PALADIN:
        {
            if (!HasSealActive(bot))
            {
                std::uint32_t const seal = GetPreferredSeal(bot);
                if (seal) { bot->CastSpell(bot, seal, false); break; }
            }
            std::uint32_t const bok = FindBestKnownSpellInChain(bot, 20217);
            std::uint32_t const bom = FindBestKnownSpellInChain(bot, 19740);
            std::uint32_t const chosen = bok ? bok : bom;
            std::uint32_t const base   = bok ? 20217 : 19740;
            if (!chosen) break;
            for (Player* t : buildTargets())
                if (AuraNeedsRefresh(t, base, thresh))
                { bot->CastSpell(t, chosen, false); return; }
            break;
        }

        case CLASS_PRIEST:
        {
            std::uint32_t const pwf = FindBestKnownSpellInChain(bot, 1243);
            if (!pwf) break;
            for (Player* t : buildTargets())
                if (AuraNeedsRefresh(t, 1243, thresh))
                { bot->CastSpell(t, pwf, false); return; }
            break;
        }

        case CLASS_DRUID:
        {
            std::uint32_t const motw = FindBestKnownSpellInChain(bot, 1126);
            if (!motw) break;
            for (Player* t : buildTargets())
                if (AuraNeedsRefresh(t, 1126, thresh))
                { bot->CastSpell(t, motw, false); return; }
            break;
        }

        case CLASS_MAGE:
        {
            bool const usePartyBuff =
                ooc.buffScope == model::BotBuffScope::Party
                && buildTargets().size() > 1;
            std::uint32_t const brilliance = usePartyBuff
                ? FindBestKnownSpellInChain(bot, 23028)
                : 0;
            if (brilliance)
            {
                for (Player* t : buildTargets())
                {
                    if (AuraNeedsRefresh(t, 1459, thresh))
                    {
                        bot->CastSpell(bot, brilliance, false);
                        return;
                    }
                }
            }

            std::uint32_t const ai = FindBestKnownSpellInChain(bot, 1459);
            if (!ai) break;
            for (Player* t : buildTargets())
                if (AuraNeedsRefresh(t, 1459, thresh))
                { bot->CastSpell(t, ai, false); return; }
            break;
        }

        case CLASS_WARLOCK:
        {
            std::uint32_t armor = FindBestKnownSpellInChain(bot, 28176);
            if (!armor) armor   = FindBestKnownSpellInChain(bot, 706);
            if (!armor) armor   = FindBestKnownSpellInChain(bot, 696);
            if (armor && AuraNeedsRefresh(bot, armor, thresh))
                bot->CastSpell(bot, armor, false);
            break;
        }

        default:
            break;
    }
}

void TryApplyOutOfCombatBuff(Player* bot, Player* owner,
                              model::BotOocBehavior const& ooc)
{
    if (bot->IsInCombat() || (owner && owner->IsInCombat()))
        return;
    ApplyBotBuff(bot, owner, ooc);
}

// ---------------------------------------------------------------
// Motion helpers
// ---------------------------------------------------------------

// Socketless bot Players have no client-driven movement, so an Attack() call by
// itself just plants the bot at follow distance swinging at air. The melee /
// hybrid combat paths must explicitly drive the motion master into chase mode
// for the current victim. MotionMaster::MoveChase short-circuits when the
// active generator is already chasing the same target, so this is cheap to
// re-issue every tick.
void EnsureChasingVictim(Player* bot, Unit* target)
{
    if (!target)
        return;
    bot->GetMotionMaster()->MoveChase(target);
}

// Backs a ranged bot away from a target that has closed to melee range. The
// bot moves to a point RangedOptimalDistance yards from the target, projected
// through the current bot position. Only fires when within RangedMinDistance so
// it does not interrupt normal ranged combat positioning.
// Short backstep (RangedRetreatDistance yards) away from the target when the
// bot has taken significant damage in melee range. A small fixed step avoids
// wall/cliff traps that a full 25y retreat would cause.
void EnsureRangedPosition(Player* bot, Unit* target)
{
    model::BotGlobalConfig const cfg = GetGlobalConfigService().Get();

    if (bot->GetDistance(target) >= cfg.rangedMinDistance)
        return;

    // Angle pointing from target toward the bot — step further that way
    float const angle = target->GetAngle(bot);
    float const x     = bot->GetPositionX() + cfg.rangedRetreatDistance * std::cos(angle);
    float const y     = bot->GetPositionY() + cfg.rangedRetreatDistance * std::sin(angle);
    float const z     = bot->GetPositionZ();
    bot->GetMotionMaster()->MovePoint(0, x, y, z);
}

// Closes a ranged bot to RangedOptimalDistance when it is too far to cast.
// MoveChase with an explicit stop distance lets the engine handle pathing and
// stops the bot at the right spot without overshooting into melee range.
void EnsureRangedApproach(Player* bot, Unit* target)
{
    model::BotGlobalConfig const cfg = GetGlobalConfigService().Get();
    bot->GetMotionMaster()->MoveChase(target, cfg.rangedOptimalDistance);
}

void IssueFormationFollow(Player* bot, Player* owner, BotCombatRole role)
{
    if (!bot || !owner)
        return;

    model::BotGlobalConfig const cfg = GetGlobalConfigService().Get();

    // Pick follow distance by combat role so bots naturally pre-position.
    // Melee/Tank: close in and ready to engage.
    // Healer/HybridHealer: mid-range, out of the melee pile but in cast range.
    // Ranged: further back, pre-positioned for spells.
    float baseDistance;
    switch (role)
    {
        case BotCombatRole::Healer:
        case BotCombatRole::HybridHealer:
            baseDistance = cfg.followDistanceHealer;
            break;
        case BotCombatRole::Ranged:
            baseDistance = cfg.followDistanceRanged;
            break;
        case BotCombatRole::Melee:
        default:
            baseDistance = cfg.followDistanceMelee;
            break;
    }

    std::vector<std::uint64_t> ownerBotGuids;
    for (Player* ownerBot : service::BotPlayerRegistry::Instance().FindBotsForOwner(owner->GetGUID()))
    {
        if (ownerBot)
            ownerBotGuids.push_back(ownerBot->GetGUID().GetCounter());
    }

    CompanionFollowFormationResult const formation = ResolveCompanionFollowFormation(
        { cfg.followFormation, baseDistance, cfg.followSlotCount, bot->GetGUID().GetCounter(), std::move(ownerBotGuids) });

    FollowDiagnosticSnapshot currentSnapshot;
    currentSnapshot.formation = static_cast<std::uint32_t>(cfg.followFormation);
    currentSnapshot.slotCount = std::max(1u, cfg.followSlotCount);
    currentSnapshot.rosterSize = formation.rosterSize;
    currentSnapshot.rosterIndex = formation.rosterIndex;
    currentSnapshot.slot = formation.slot;
    currentSnapshot.usedRosterSlot = formation.usedRosterSlot;
    currentSnapshot.angle = formation.angle;
    currentSnapshot.distance = formation.distance;

    bool shouldLogFollowDiagnostic = false;
    {
        std::lock_guard<std::mutex> lock(s_followDiagnosticMutex);
        std::uint64_t const botGuidLow = bot->GetGUID().GetCounter();
        auto const it = s_lastFollowDiagnostics.find(botGuidLow);
        if (it == s_lastFollowDiagnostics.end()
            || it->second.formation != currentSnapshot.formation
            || it->second.slotCount != currentSnapshot.slotCount
            || it->second.rosterSize != currentSnapshot.rosterSize
            || it->second.rosterIndex != currentSnapshot.rosterIndex
            || it->second.slot != currentSnapshot.slot
            || it->second.usedRosterSlot != currentSnapshot.usedRosterSlot
            || std::fabs(it->second.angle - currentSnapshot.angle) > 0.01f
            || std::fabs(it->second.distance - currentSnapshot.distance) > 0.01f)
        {
            s_lastFollowDiagnostics[botGuidLow] = currentSnapshot;
            shouldLogFollowDiagnostic = true;
        }
    }

    if (shouldLogFollowDiagnostic)
    {
        char const* formationName = "Cluster";
        switch (cfg.followFormation)
        {
            case model::FollowFormation::Ring:
                formationName = "Ring";
                break;
            case model::FollowFormation::V:
                formationName = "V";
                break;
            case model::FollowFormation::Line:
                formationName = "Line";
                break;
            case model::FollowFormation::Cluster:
            default:
                formationName = "Cluster";
                break;
        }

        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] FollowFormation bot='{}' guid={} owner='{}' ownerGuid={} formation={} slotCount={} rosterSize={} rosterIndex={} slot={} source={} angle={:.3f} distance={:.2f}",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            owner->GetName(),
            owner->GetGUID().GetCounter(),
            formationName,
            currentSnapshot.slotCount,
            currentSnapshot.rosterSize,
            currentSnapshot.rosterIndex,
            currentSnapshot.slot,
            currentSnapshot.usedRosterSlot ? "owner_roster" : "guid_fallback",
            currentSnapshot.angle,
            currentSnapshot.distance);
    }

    bot->GetMotionMaster()->MoveFollow(owner, formation.distance, formation.angle);
}

bool ShouldCombatFollowOverride(Player* bot, Player* owner, BotCombatRole role)
{
    if (!bot || !owner)
        return false;

    if (role != BotCombatRole::Healer
        && role != BotCombatRole::HybridHealer
        && role != BotCombatRole::Ranged)
        return false;

    return bot->GetDistance(owner) > GetGlobalConfigService().Get().combatFollowOverrideDistance;
}

void BreakFollowForAttack(Player* bot)
{
    if (!bot)
        return;

    if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        return;

    // If a commanded attack starts while the follow generator is still active,
    // the owner's movement can continue dragging ranged casters along and break
    // casts. Clear the stale follow generator once so the combat branch fully
    // owns movement from here.
    bot->StopMoving();
    bot->GetMotionMaster()->Clear(false);
}

// ---------------------------------------------------------------
// Per-role combat ticks
// ---------------------------------------------------------------

void UpdateConservationState(
    model::BotCombatProfileSettings const& settings,
    Player const* bot,
    bool& conserving)
{
    if (settings.conservationMode == model::BotCombatConservationMode::FullForce ||
        settings.conservationMode == model::BotCombatConservationMode::JitCasting)
    {
        conserving = false;
        return;
    }
    if (bot->GetMaxPower(POWER_MANA) == 0)
        return;
    float const manaPct = 100.0f * static_cast<float>(bot->GetPower(POWER_MANA))
                                 / static_cast<float>(bot->GetMaxPower(POWER_MANA));
    if (settings.conservationMode == model::BotCombatConservationMode::Reserve)
    {
        // Reserve: simple floor — suppress while below low water, resume immediately once above.
        conserving = manaPct < static_cast<float>(settings.resourceLowWater);
    }
    else // Conservative: full hysteresis band
    {
        if (conserving)
        {
            if (manaPct >= static_cast<float>(settings.resourceHighWater))
                conserving = false;
        }
        else if (manaPct < static_cast<float>(settings.resourceLowWater))
        {
            conserving = true;
        }
    }
}

void TickRanged(Player* bot, Player* owner, Unit* target)
{
    TryExecuteProfileRotation(bot, owner, target);
}

void TickMelee(Player* bot, Player* owner, Unit* target)
{
    TryExecuteProfileRotation(bot, owner, target);
}

// ---------------------------------------------------------------
// Assist target resolution
// ---------------------------------------------------------------

// Returns true when this unit is something a bot should engage on the owner's
// behalf: alive, on the same map, hostile to the owner, and currently flagged
// as a legal attack target.
bool IsValidAssistTarget(
    Player const* bot,
    Player const* owner,
    Unit const* candidate,
    model::BotGlobalConfig const& cfg)
{
    if (!bot || !owner || !candidate || !candidate->IsInWorld() || !candidate->IsAlive())
        return false;
    if (candidate == owner)
        return false;
    if (candidate == bot)
        return false;
    if (candidate->GetMap() != bot->GetMap())
        return false;
    if (owner->IsFriendlyTo(candidate) || bot->IsFriendlyTo(candidate))
        return false;
    if (cfg.assistRequireTargetableForAttack
        && !candidate->isTargetableForAttack(true, bot))
        return false;
    return true;
}

// Commanded attack targets need slightly looser validation than normal assist.
// While a player-issued attack command is latched, we still want casters to keep
// their target and approach even if line-of-sight / targetable checks flicker
// during pull movement or while the mob has not fully engaged yet.
bool IsViableCommandTarget(
    Player const* bot,
    Player const* owner,
    Unit const* candidate,
    model::BotGlobalConfig const& cfg)
{
    if (!bot || !owner || !candidate || !candidate->IsInWorld() || !candidate->IsAlive())
        return false;
    if (candidate == owner || candidate == bot)
        return false;
    if (candidate->GetMap() != bot->GetMap())
        return false;
    if (owner->IsFriendlyTo(candidate) || bot->IsFriendlyTo(candidate))
        return false;
    if (cfg.commandRequireTargetableForAttack
        && !candidate->isTargetableForAttack(true, bot))
        return false;
    return true;
}

// Resolve the unit a bot should be fighting right now, honouring any
// explicit player override before falling back to normal assist logic.
Unit* ResolveAssistTarget(Player* bot, Player* owner)
{
    BotOverride const ovr = GetOverride(bot->GetGUID());
    model::BotGlobalConfig const cfg = GetGlobalConfigService().Get();

    auto const now = std::chrono::steady_clock::now();

    // Retreat mode: bot follows and heals only — no combat at all.
    if (ovr.retreatExpiry > now)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=retreat result=none",
            bot->GetName(),
            bot->GetGUID().GetCounter());
        return nullptr;
    }

    // If the player ordered disengage, hold — but auto-expire after 500ms so
    // a subsequent r-click (owner attacks → mob agros back → bot assists) works.
    if (ovr.disengaged)
    {
        if (now < ovr.disengageExpiry)
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=disengaged result=none",
                bot->GetName(),
                bot->GetGUID().GetCounter());
            return nullptr;
        }
        // Expired — clear the flag and fall through to normal assist.
        ClearBotOverride(bot->GetGUID());
    }

    // If the player ordered a specific target, use it while it's valid.
    if (ovr.forcedTarget)
    {
        Unit* forced = ObjectAccessor::GetUnit(*bot, ovr.forcedTarget);
        if (forced && IsViableCommandTarget(bot, owner, forced, cfg))
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=forced targetGuid={}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                forced->GetGUID().GetCounter());
            return forced;
        }
        // Target gone — clear the override and fall through.
        SetBotForcedTarget(bot->GetGUID(), ObjectGuid::Empty);
    }

    // Attack-lock mode (.lwbot attack): keep suppressing follow and maintain
    // aggressive assist behavior until combat naturally ends or the player
    // explicitly disengages/follows.
    if (ovr.attackLocked)
    {
        if (Unit* current = bot->GetVictim())
        {
            if (IsViableCommandTarget(bot, owner, current, cfg))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=attack_locked source=current_victim targetGuid={}",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    current->GetGUID().GetCounter());
                return current;
            }
        }

        if (cfg.attackLockUseOwnerVictim)
        {
            if (Unit* ownerVictim = owner->GetVictim())
            {
                if (IsViableCommandTarget(bot, owner, ownerVictim, cfg))
                {
                    LOG_INFO(
                        "server.worldserver",
                        "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=attack_locked source=owner_victim targetGuid={}",
                        bot->GetName(),
                        bot->GetGUID().GetCounter(),
                        ownerVictim->GetGUID().GetCounter());
                    return ownerVictim;
                }
            }
        }

        ObjectGuid const ownerSelection = cfg.attackLockUseOwnerSelection
            ? owner->GetTarget()
            : ObjectGuid::Empty;
        if (ownerSelection)
        {
            if (Unit* selected = ObjectAccessor::GetUnit(*bot, ownerSelection))
            {
                if (IsViableCommandTarget(bot, owner, selected, cfg))
                {
                    LOG_INFO(
                        "server.worldserver",
                        "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=attack_locked source=owner_selection targetGuid={}",
                        bot->GetName(),
                        bot->GetGUID().GetCounter(),
                        selected->GetGUID().GetCounter());
                    return selected;
                }
            }
        }

        // Only release the command lock once the commanded target context is
        // truly gone. Do not key this off transient in-combat flags, because
        // those can flicker during pull/setup and cause casters to snap back
        // into follow mid-cast.
        ModifyOverride(bot->GetGUID(), [](BotOverride& o) {
            o.attackLocked = false;
        });

        return nullptr;
    }

    // Normal assist logic:
    // 1. Keep fighting the current victim while it's alive.
    if (cfg.assistUseCurrentVictim)
    {
        if (Unit* current = bot->GetVictim())
        {
            if (IsValidAssistTarget(bot, owner, current, cfg))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=assist source=current_victim targetGuid={}",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    current->GetGUID().GetCounter());
                return current;
            }
        }
    }

    // 2. Pick up owner's active victim only when that mob is fighting back —
    //    i.e. the mob's current victim is the owner. This prevents the bot from
    //    chasing a mob the owner merely auto-attacked once but that hasn't
    //    aggroed yet or that the owner accidentally clicked.
    if (cfg.assistUseOwnerVictim)
    {
        if (Unit* ownerVictim = owner->GetVictim())
        {
            bool const ownerVictimAllowed =
                !cfg.assistOwnerVictimMustTargetOwner || ownerVictim->GetVictim() == owner;
            if (IsValidAssistTarget(bot, owner, ownerVictim, cfg)
                && ownerVictimAllowed)
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=assist source=owner_victim targetGuid={}",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    ownerVictim->GetGUID().GetCounter());
                return ownerVictim;
            }
        }
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=assist result=none",
        bot->GetName(),
        bot->GetGUID().GetCounter());
    return nullptr;
}

// Picks up an attacker that is currently hitting the owner, preferring the
// bot's current victim if it qualifies. Used by Guard mode.
Unit* ResolveGuardTarget(Player* bot, Player* owner)
{
    model::BotGlobalConfig const cfg = GetGlobalConfigService().Get();

    if (cfg.guardUseCurrentVictim)
    {
        if (Unit* current = bot->GetVictim())
        {
            if (IsValidAssistTarget(bot, owner, current, cfg))
                return current;
        }
    }

    if (!cfg.guardUseOwnerAttackers)
        return nullptr;

    for (Unit* attacker : owner->getAttackers())
    {
        if (IsValidAssistTarget(bot, owner, attacker, cfg))
            return attacker;
    }

    return nullptr;
}

bool IsLikelyHealerClass(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_PALADIN:
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            return true;
        default:
            return false;
    }
}

bool IsLikelyTankClass(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_DRUID:
            return true;
        default:
            return false;
    }
}

enum class SmartAssistTargetRole : std::uint8_t
{
    Unknown,
    Healer,
    Tank,
    Damage
};

SmartAssistTargetRole GuessSmartAssistTargetRole(Unit* candidate)
{
    Player* candidatePlayer = candidate ? candidate->ToPlayer() : nullptr;
    if (!candidatePlayer)
        return SmartAssistTargetRole::Damage;

    if (std::optional<ObjectGuid> ownerGuid =
            service::BotPlayerRegistry::Instance().FindOwnerForBot(candidatePlayer->GetGUID()))
    {
        Player* candidateOwner = ObjectAccessor::FindConnectedPlayer(*ownerGuid);
        BotCombatDoctrine const doctrine = GetCombatDoctrine(candidatePlayer, candidateOwner);
        if (doctrine.effectiveRoleKey == "HEAL")
            return SmartAssistTargetRole::Healer;
        if (doctrine.effectiveRoleKey == "TANK")
            return SmartAssistTargetRole::Tank;
        if (doctrine.effectiveRoleKey == "DPS")
            return SmartAssistTargetRole::Damage;
    }

    if (IsLikelyHealerClass(candidatePlayer->getClass()))
        return SmartAssistTargetRole::Healer;
    if (IsLikelyTankClass(candidatePlayer->getClass()))
        return SmartAssistTargetRole::Tank;
    return SmartAssistTargetRole::Damage;
}

Unit* ResolveSmartAssistTarget(Player* bot, Player* owner, BotCombatDoctrine const& doctrine)
{
    BotOverride const ovr = GetOverride(bot->GetGUID());
    model::BotGlobalConfig const cfg = GetGlobalConfigService().Get();

    auto const now = std::chrono::steady_clock::now();

    if (ovr.retreatExpiry > now)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=retreat result=none",
            bot->GetName(),
            bot->GetGUID().GetCounter());
        return nullptr;
    }

    if (ovr.disengaged)
    {
        if (now < ovr.disengageExpiry)
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=disengaged result=none",
                bot->GetName(),
                bot->GetGUID().GetCounter());
            return nullptr;
        }

        ClearBotOverride(bot->GetGUID());
    }

    if (ovr.forcedTarget)
    {
        Unit* forced = ObjectAccessor::GetUnit(*bot, ovr.forcedTarget);
        if (forced && IsViableCommandTarget(bot, owner, forced, cfg))
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=forced targetGuid={}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                forced->GetGUID().GetCounter());
            return forced;
        }

        SetBotForcedTarget(bot->GetGUID(), ObjectGuid::Empty);
    }

    if (ovr.attackLocked)
        return ResolveAssistTarget(bot, owner);

    std::vector<Player*> friendlyBots =
        service::BotPlayerRegistry::Instance().FindBotsForOwner(owner->GetGUID());

    std::unordered_map<std::uint64_t, std::uint32_t> allyFocusCounts;
    std::unordered_set<std::uint64_t> protectedAllyGuids;
    protectedAllyGuids.insert(owner->GetGUID().GetCounter());

    for (Player* friendlyBot : friendlyBots)
    {
        if (!friendlyBot)
            continue;

        protectedAllyGuids.insert(friendlyBot->GetGUID().GetCounter());

        if (Unit* victim = friendlyBot->GetVictim())
            ++allyFocusCounts[victim->GetGUID().GetCounter()];
    }

    if (Unit* ownerVictim = owner->GetVictim())
        ++allyFocusCounts[ownerVictim->GetGUID().GetCounter()];

    std::vector<Unit*> candidates;
    std::unordered_set<std::uint64_t> candidateGuids;
    auto addCandidate = [&](Unit* candidate)
    {
        if (!candidate || !IsValidAssistTarget(bot, owner, candidate, cfg))
            return;
        if (!candidateGuids.insert(candidate->GetGUID().GetCounter()).second)
            return;
        candidates.push_back(candidate);
    };

    addCandidate(bot->GetVictim());
    addCandidate(owner->GetVictim());

    if (ObjectGuid const ownerSelection = owner->GetTarget())
        addCandidate(ObjectAccessor::GetUnit(*owner, ownerSelection));

    for (Unit* attacker : owner->getAttackers())
        addCandidate(attacker);

    for (Player* friendlyBot : friendlyBots)
    {
        if (!friendlyBot)
            continue;

        addCandidate(friendlyBot->GetVictim());
        for (Unit* attacker : friendlyBot->getAttackers())
            addCandidate(attacker);
    }

    if (candidates.empty())
        return ResolveAssistTarget(bot, owner);

    model::BotCombatProfileSettings::TargetingSettings const& targeting =
        doctrine.settings.targeting;

    Unit* bestCandidate = nullptr;
    float bestScore = std::numeric_limits<float>::lowest();

    for (Unit* candidate : candidates)
    {
        float score = 0.0f;
        float const distance = bot->GetDistance(candidate);
        score -= distance * 2.0f;

        if (candidate == bot->GetVictim())
            score += static_cast<float>(targeting.currentTargetBias);

        if (candidate == owner->GetVictim())
            score += static_cast<float>(targeting.assistTargetBias);

        if (ObjectGuid const ownerSelection = owner->GetTarget();
            ownerSelection && ownerSelection == candidate->GetGUID())
        {
            score += static_cast<float>(targeting.assistTargetBias);
        }

        auto focusIt = allyFocusCounts.find(candidate->GetGUID().GetCounter());
        if (focusIt != allyFocusCounts.end() && focusIt->second > 0)
            score += static_cast<float>(targeting.focusFireBias) *
                static_cast<float>(focusIt->second);

        if (Unit* victim = candidate->GetVictim())
        {
            if (protectedAllyGuids.find(victim->GetGUID().GetCounter()) !=
                protectedAllyGuids.end())
                score += static_cast<float>(targeting.protectAllyBias);
        }

        switch (GuessSmartAssistTargetRole(candidate))
        {
            case SmartAssistTargetRole::Healer:
                score += static_cast<float>(targeting.preferHealerBias);
                break;
            case SmartAssistTargetRole::Damage:
                score += static_cast<float>(targeting.preferDpsBias);
                break;
            case SmartAssistTargetRole::Tank:
                score -= static_cast<float>(targeting.avoidTankBias);
                break;
            default:
                break;
        }

        if (candidate->GetHealthPct() < 35.0f)
            score += 35.0f;

        if (score > bestScore)
        {
            bestScore = score;
            bestCandidate = candidate;
        }
    }

    if (bestCandidate)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] AssistTarget bot='{}' guid={} mode=smart targetGuid={} score={:.1f}",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            bestCandidate->GetGUID().GetCounter(),
            bestScore);
        return bestCandidate;
    }

    return ResolveAssistTarget(bot, owner);
}

// ---------------------------------------------------------------
// Main tick
// ---------------------------------------------------------------

void Tick(Player* bot, Player* owner, float& retreatHpPct, bool& conserving)
{
    TryMaintainBasicCompanionPet(bot, owner);

    model::BotCombatMode const mode =
        service::BotPlayerRegistry::Instance().GetBotMode(owner->GetGUID());
    model::BotCombatControlMode const controlMode =
        service::BotPlayerRegistry::Instance().GetBotControlMode(owner->GetGUID());

    // Hold: stop all combat and stand still.
    if (mode == model::BotCombatMode::Hold)
    {
        if (bot->GetVictim())
            bot->AttackStop();
        bot->GetMotionMaster()->Clear(false);
        return;
    }

    // Passive: follow and buff, never engage.
    if (mode == model::BotCombatMode::Passive)
    {
        if (bot->GetVictim())
            bot->AttackStop();
        // Passive mode runs before doctrine resolution — load OOC directly.
        TryApplyOutOfCombatBuff(bot, owner,
            GetOocConfigService().Get(bot->GetGUID().GetCounter()));
        if (!bot->IsNonMeleeSpellCast(false)
            && !bot->IsWithinDistInMap(owner, GetGlobalConfigService().Get().repositionDistance))
            bot->GetMotionMaster()->MoveFollow(owner,
                GetGlobalConfigService().Get().followDistanceFallback,
                FollowAngle);
        return;
    }

    // Hazard escape: fires before doctrine resolution so ground effects
    // are handled regardless of combat role, and skips the DB-cached
    // doctrine lookup entirely while the bot is actively fleeing.
    if (BotHazardSensor::ProcessHazardTick(bot, owner))
        return;

    BotCombatDoctrine const doctrine = GetCombatDoctrine(bot, owner);
    BotCombatRole const role = doctrine.role;

    // OOC config is per-character, resolved once per tick.
    model::BotOocBehavior const oocBehavior =
        GetOocConfigService().Get(bot->GetGUID().GetCounter());

    // Pure healers: profile rotation owns all healing and offense decisions.
    if (role == BotCombatRole::Healer)
    {
        UpdateConservationState(doctrine.settings, bot, conserving);

        Unit* attackTarget = ResolveAssistTarget(bot, owner);
        if (attackTarget && bot->GetVictim() != attackTarget)
            bot->Attack(attackTarget, false);

        if (attackTarget)
            BreakFollowForAttack(bot);

        // Profile rotation handles both healing (lowest_hp_party entries) and
        // offense (enemy_primary entries) in a single priority-ordered pass.
        // When mana-conserving, pass nullptr as the primary target so offense
        // entries are naturally skipped while healing entries still resolve via
        // lowest_hp_party independently of the combat target.
        bool const offenseSuppressed =
            IsOffenseSuppressed(doctrine.settings.conservationMode, conserving);
        TryExecuteProfileRotation(
            bot, owner, offenseSuppressed ? nullptr : attackTarget);

        if (attackTarget)
        {
            float const distance = bot->GetDistance(attackTarget);
            if (!bot->IsNonMeleeSpellCast(false) && distance > GetGlobalConfigService().Get().rangedCastRange)
            {
                EnsureRangedApproach(bot, attackTarget);
                return;
            }
        }

        TryApplyOutOfCombatBuff(bot, owner, oocBehavior);

        if (ShouldCombatFollowOverride(bot, owner, role))
        {
            if (!bot->IsNonMeleeSpellCast(false))
            {
                bot->GetMotionMaster()->Clear(false);
                IssueFormationFollow(bot, owner, role);
            }
            return;
        }

        if (!attackTarget
            && !bot->GetVictim()
            && !bot->IsNonMeleeSpellCast(false)
            && !bot->IsInCombat()
            && !owner->IsInCombat()
            && !IsBotAttackLocked(bot->GetGUID())
            && bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        {
            bot->GetMotionMaster()->Clear(false);
            IssueFormationFollow(bot, owner, role);
        }
        return;
    }

    // DPS roles track mana conservation every tick so out-of-combat mana regen
    // properly clears the conserving state before the next pull.
    if (role == BotCombatRole::Ranged || role == BotCombatRole::Melee)
        UpdateConservationState(doctrine.settings, bot, conserving);

    Unit* const assistTarget = (mode == model::BotCombatMode::Guard)
        ? ResolveGuardTarget(bot, owner)
        : (controlMode == model::BotCombatControlMode::Smart
            ? ResolveSmartAssistTarget(bot, owner, doctrine)
            : ResolveAssistTarget(bot, owner));

    if (role == BotCombatRole::HybridHealer)
    {
        UpdateConservationState(doctrine.settings, bot, conserving);

        if (assistTarget && bot->GetVictim() != assistTarget)
            bot->Attack(assistTarget, true);

        if (assistTarget)
            BreakFollowForAttack(bot);

        bool const offenseSuppressed =
            IsOffenseSuppressed(doctrine.settings.conservationMode, conserving);

        // Profile rotation owns all heal/offense arbitration for hybrid healers.
        // When mana-conserving, offense entries are suppressed via nullptr target.
        TryExecuteProfileRotation(
            bot, owner, offenseSuppressed ? nullptr : assistTarget);

        if (assistTarget && !offenseSuppressed && !bot->IsNonMeleeSpellCast(false))
            EnsureChasingVictim(bot, assistTarget);

        TryApplyOutOfCombatBuff(bot, owner, oocBehavior);

        if (ShouldCombatFollowOverride(bot, owner, role))
        {
            if (!bot->IsNonMeleeSpellCast(false))
            {
                bot->GetMotionMaster()->Clear(false);
                IssueFormationFollow(bot, owner, role);
            }
            return;
        }

        if (!assistTarget
            && !bot->GetVictim()
            && !bot->IsNonMeleeSpellCast(false)
            && !bot->IsInCombat()
            && !owner->IsInCombat()
            && !IsBotAttackLocked(bot->GetGUID())
            && bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        {
            bot->GetMotionMaster()->Clear(false);
            IssueFormationFollow(bot, owner, role);
        }
        return;
    }

    if (assistTarget)
    {
        if (ShouldCombatFollowOverride(bot, owner, role))
        {
            if (!bot->IsNonMeleeSpellCast(false))
            {
                bot->GetMotionMaster()->Clear(false);
                IssueFormationFollow(bot, owner, role);
            }
            return;
        }

        BreakFollowForAttack(bot);

        if (role == BotCombatRole::Ranged)
        {
            // Attack(false) sets the victim and enters combat without issuing
            // MoveChase, which would interrupt an in-progress cast every tick.
            // We drive positioning ourselves via EnsureRangedApproach/Position.
            if (bot->GetVictim() != assistTarget)
                bot->Attack(assistTarget, false);

            float const distance = bot->GetDistance(assistTarget);

            if (!BotHasManaToFight(bot))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] RangedAI decision: bot='{}' guid={} targetGuid={} distance={:.2f} mana={}/{} action=oom_chase",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    assistTarget->GetGUID().GetCounter(),
                    distance,
                    static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
                    static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)));

                // OOM: close to melee and autoattack until mana returns rather
                // than wasting every tick on failed cast attempts.
                EnsureChasingVictim(bot, assistTarget);
            }
            else if (distance < GetGlobalConfigService().Get().rangedMinDistance
                && bot->GetHealthPct() < retreatHpPct)
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] RangedAI decision: bot='{}' guid={} targetGuid={} distance={:.2f} mana={}/{} action=retreat hp={:.1f}",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    assistTarget->GetGUID().GetCounter(),
                    distance,
                    static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
                    static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)),
                    bot->GetHealthPct());

                // Step back 5y away from the target. Set the next retreat
                // threshold to 60% so another retreat can only fire once the
                // bot has taken more sustained damage.
                EnsureRangedPosition(bot, assistTarget);
                retreatHpPct = GetGlobalConfigService().Get().rangedRetreatResetPct;
            }
            else if (distance > GetGlobalConfigService().Get().rangedCastRange)
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] RangedAI decision: bot='{}' guid={} targetGuid={} distance={:.2f} mana={}/{} action=approach",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    assistTarget->GetGUID().GetCounter(),
                    distance,
                    static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
                    static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)));

                // Target is beyond spell range: close to optimal distance.
                EnsureRangedApproach(bot, assistTarget);
            }
            else if (IsOffenseSuppressed(doctrine.settings.conservationMode, conserving))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] RangedAI decision: bot='{}' guid={} targetGuid={} distance={:.2f} mana={}/{} action=conserving",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    assistTarget->GetGUID().GetCounter(),
                    distance,
                    static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
                    static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)));
            }
            else
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] RangedAI decision: bot='{}' guid={} targetGuid={} distance={:.2f} mana={}/{} action=cast_window",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    assistTarget->GetGUID().GetCounter(),
                    distance,
                    static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
                    static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)));

                TickRanged(bot, owner, assistTarget);
            }
        }
        else // Melee
        {
            if (bot->GetVictim() != assistTarget)
                bot->Attack(assistTarget, true);

            EnsureChasingVictim(bot, assistTarget);
            if (!IsOffenseSuppressed(doctrine.settings.conservationMode, conserving))
                TickMelee(bot, owner, assistTarget);
        }

        return;
    }

    if (bot->GetVictim())
    {
        if (IsBotAttackLocked(bot->GetGUID()))
        {
            // Keep victim/combat state stable while attack-lock is active.
            // This prevents transient target resolution misses from forcing
            // AttackStop() and interrupting caster channels/casts.
            return;
        }

        bot->AttackStop();
        if (!bot->IsInCombat() && !owner->IsInCombat() && !IsBotAttackLocked(bot->GetGUID()))
        {
            bot->GetMotionMaster()->Clear(false);
            IssueFormationFollow(bot, owner, role);
        }
        return;
    }

    // No combat target — apply out-of-combat maintenance buffs, then resume
    // following the owner. Only (re-)issue MoveFollow when not already in follow
    // mode, to avoid killing the active follow generator every 500ms tick which
    // causes the bot to appear frozen or to stutter.
    if (bot->IsInCombat() || owner->IsInCombat())
    {
        if (ShouldCombatFollowOverride(bot, owner, role)
            && !bot->IsNonMeleeSpellCast(false)
            && bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        {
            bot->GetMotionMaster()->Clear(false);
            IssueFormationFollow(bot, owner, role);
        }
        return;
    }

    TryApplyOutOfCombatBuff(bot, owner, oocBehavior);

    if (!bot->IsNonMeleeSpellCast(false)
        && !IsBotAttackLocked(bot->GetGUID())
        && bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
    {
        bot->GetMotionMaster()->Clear(false);
        IssueFormationFollow(bot, owner, role);
    }
}

// ---------------------------------------------------------------
// Event class
// ---------------------------------------------------------------

// Called for hostile (ownerless) bots each tick.
// No follow, no OOC buffs, no owner reference.
// Resolves target from GetVictim() / attacker list, then runs the full
// doctrine rotation (owner=nullptr is safe after null-hardening above).
void TickHostile(Player* bot, float& retreatHpPct, bool& conserving)
{
    BotCombatDoctrine const doctrine = GetCombatDoctrine(bot, nullptr);
    UpdateConservationState(doctrine.settings, bot, conserving);
    model::BotOocBehavior const oocBehavior =
        GetOocConfigService().Get(bot->GetGUID().GetCounter());

    Unit* target = bot->GetVictim();
    if (!target)
    {
        for (Unit* attacker : bot->getAttackers())
        {
            if (attacker && attacker->IsAlive()
                && bot->IsValidAttackTarget(attacker))
            {
                target = attacker;
                break;
            }
        }
    }

    if (!target)
    {
        if (Unit* acquired = TryAcquireHostileDebugTarget(bot))
        {
            target = acquired;
            if (bot->GetVictim() != target)
                bot->Attack(target, true);

            if (std::optional<std::uint32_t> identityId = ResolveLedgerShellIdentityId(bot))
            {
                living_world::integration::BotActivityLog::Record(
                    bot,
                    bot->GetName(),
                    *identityId,
                    "combat_enter",
                    std::string("acquired_target=") + target->GetName());
            }
        }
    }

    if (!target)
    {
        if (TryRunLedgerShellStartupPrep(bot, oocBehavior))
            return;
        return; // No combat context — stand idle.
    }

    if (bot->GetVictim() != target)
        bot->Attack(target, true);

    TryExecuteProfileRotation(bot, nullptr, target);
}

class CompanionAIEvent final : public BasicEvent
{
public:
    CompanionAIEvent(ObjectGuid botGuid, ObjectGuid ownerGuid,
                     std::uint8_t notInWorldRetries = 0,
                     float retreatHpPct = 80.0f,
                     bool conserving = false)
        : _botGuid(botGuid), _ownerGuid(ownerGuid)
        , _notInWorldRetries(notInWorldRetries), _retreatHpPct(retreatHpPct)
        , _conserving(conserving)
    {
    }

    bool Execute(uint64, uint32) override
    {
        Player* bot   = ObjectAccessor::FindPlayer(_botGuid);
        Player* owner = ObjectAccessor::FindConnectedPlayer(_ownerGuid);

        // Ownerless hostile bot — tick independently, no owner required.
        if (_ownerGuid.IsEmpty())
        {
            if (!bot)
                return true; // Bot gone; stop event.
            if (!bot->IsInWorld())
            {
                if (_notInWorldRetries >= MaxNotInWorldRetries)
                    return true;
                Milliseconds const delay =
                    500ms * (1u << std::min(_notInWorldRetries, std::uint8_t{3}));
                bot->m_Events.AddEventAtOffset(
                    new CompanionAIEvent(
                        _botGuid, ObjectGuid::Empty,
                        _notInWorldRetries + 1, _retreatHpPct, _conserving),
                    delay);
                return true;
            }
            TickHostile(bot, _retreatHpPct, _conserving);
            bot->m_Events.AddEventAtOffset(
                new CompanionAIEvent(
                    _botGuid, ObjectGuid::Empty,
                    0, _retreatHpPct, _conserving),
                500ms);
            return true;
        }

        if (!bot || !owner)
            return true;

        if (!bot->IsInWorld() || !owner->IsInWorld())
        {
            if (_notInWorldRetries >= MaxNotInWorldRetries)
                return true;

            // Backoff: 500ms, 1s, 2s, 4s, 4s, 4s, ...
            Milliseconds const delay = 500ms * (1u << std::min(_notInWorldRetries, std::uint8_t{3}));
            bot->m_Events.AddEventAtOffset(
                new CompanionAIEvent(_botGuid, _ownerGuid, _notInWorldRetries + 1, _retreatHpPct, _conserving),
                delay);
            return true;
        }

        // Reset retreat threshold if the bot has healed back above the trigger level.
        float const retreatTriggerPct = GetGlobalConfigService().Get().rangedRetreatTriggerPct;
        if (_retreatHpPct < retreatTriggerPct && bot && bot->GetHealthPct() >= retreatTriggerPct)
            _retreatHpPct = retreatTriggerPct;

        Tick(bot, owner, _retreatHpPct, _conserving);
        bot->m_Events.AddEventAtOffset(
            new CompanionAIEvent(_botGuid, _ownerGuid, 0, _retreatHpPct, _conserving),
            500ms);
        return true;
    }

private:
    static constexpr std::uint8_t MaxNotInWorldRetries = 20;

    ObjectGuid   _botGuid;
    ObjectGuid   _ownerGuid;
    std::uint8_t _notInWorldRetries;
    float        _retreatHpPct;
    bool         _conserving;
};
} // namespace

void ScheduleCompanionAI(Player* botPlayer, Player* ownerPlayer)
{
    if (!botPlayer || !ownerPlayer)
        return;

    botPlayer->m_Events.AddEventAtOffset(
        new CompanionAIEvent(botPlayer->GetGUID(), ownerPlayer->GetGUID()),
        500ms);
}

void ScheduleHostileCompanionAI(Player* botPlayer)
{
    if (!botPlayer)
        return;

    // ObjectGuid::Empty as ownerGuid signals the hostile (ownerless) tick path.
    botPlayer->m_Events.AddEventAtOffset(
        new CompanionAIEvent(botPlayer->GetGUID(), ObjectGuid::Empty),
        500ms);
}

void ForceBotBuffRefresh(Player* bot, Player* owner)
{
    if (!bot || !owner)
        return;
    ApplyBotBuff(bot, owner,
        GetOocConfigService().Get(bot->GetGUID().GetCounter()));
}

void InvalidateBotCombatCaches(ObjectGuid botGuid)
{
    if (!botGuid.IsPlayer())
        return;

    std::uint64_t const botGuidLow = botGuid.GetCounter();

    {
        std::lock_guard<std::mutex> lock(s_doctrineMutex);
        s_doctrineByBotGuid.erase(botGuidLow);
    }

    {
        std::lock_guard<std::mutex> lock(s_preparedProfileMutex);
        s_preparedProfileByBotGuid.erase(botGuidLow);
    }
}

void SetBotContext(ObjectGuid botGuid, std::string const& contextKey)
{
    GetSharedContextService().Set(botGuid.GetCounter(), contextKey);
}

std::string GetBotContext(ObjectGuid botGuid)
{
    return GetSharedContextService().Get(botGuid.GetCounter());
}
} // namespace ai
} // namespace living_world
