#include "ai/CompanionAI.h"
#include "ai/BotHazardSensor.h"

#include "Chat.h"
#include "Duration.h"
#include "EventProcessor.h"
#include "Log.h"
#include "MotionMaster.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "integration/SqlAccountAltRuntimeRepository.h"
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
#include "service/BotCombatProfilePreparationService.h"
#include "service/BotCombatRuntimeEvaluator.h"
#include "service/BotGlobalConfigService.h"
#include "service/BotPlayerRegistry.h"
#include "service/SimpleBotCombatSpecRoleResolver.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <mutex>
#include <unordered_map>

namespace living_world
{
namespace ai
{

service::BotContextService& GetSharedContextService()
{
    static service::BotContextService service;
    return service;
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
constexpr float CombatFollowOverrideDistance = 20.0f;
constexpr float RepositionDistance    = 8.0f;
constexpr float RangedMinDistance     = 8.0f;    // Back away when closer than this
constexpr float RangedOptimalDistance = 25.0f;   // Target spacing for ranged bots
constexpr float RangedCastRange      = 30.0f;   // Approach target when farther than this
constexpr float RangedRetreatDistance = 5.0f;    // Short backstep when hurt in melee range
constexpr float RangedRetreatTrigger  = 80.0f;   // Retreat when HP drops below this %
constexpr float RangedRetreatReset    = 60.0f;   // Allow another retreat only after HP drops below this %

// --- Heal thresholds ---
constexpr float HealOwnerCritical    = 50.0f;
constexpr float HealOwnerModerate    = 85.0f;

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

BotCombatDoctrine LoadCombatDoctrine(Player* bot, Player* owner)
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

    doctrine.role = ResolveDoctrineRole(
        bot->getClass(),
        resolution.effectiveSpecKey,
        resolution.effectiveRoleKey);
    return doctrine;
}

BotCombatDoctrine GetCombatDoctrine(Player* bot, Player* owner)
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

service::BotCombatPreparedProfile GetPreparedCombatProfile(Player* bot, Player* owner)
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
    service::BotCombatPreparedProfile preparedProfile =
        GetProfilePreparationService().PrepareForBot(bot, ownerAccountId);

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

bool TryExecuteProfileRotation(Player* bot, Player* owner, Unit* primaryTarget)
{
    if (!bot)
        return false;

    PushThreatAddonMessage(bot, owner, primaryTarget);

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
                "[LivingWorldDebug] ProfileActionCast bot='{}' guid={} phase={} entryId={} actionId={} spellId={} targetKey='{}' targetGuid={} breaksCurrentCast={}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                phase,
                evaluatedAction.entryId,
                evaluatedAction.actionId,
                evaluatedAction.spellId,
                evaluatedAction.targetKey,
                evaluatedAction.target ? evaluatedAction.target->GetGUID().GetCounter() : 0,
                evaluatedAction.breaksCurrentCast);

            bot->CastSpell(evaluatedAction.target, evaluatedAction.spellId, false);
            return true;
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
        if (bot->HasSpell(candidate))
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
// Heal spells
// ---------------------------------------------------------------

// Fast direct heal for when a target drops critically low.
std::uint32_t GetDirectHealSpell(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:  return FindBestKnownSpellInChain(bot, 2061);  // Flash Heal
        case CLASS_DRUID:   return FindBestKnownSpellInChain(bot, 5185);  // Healing Touch
        case CLASS_PALADIN: return FindBestKnownSpellInChain(bot, 19750); // Flash of Light
        case CLASS_SHAMAN:  return FindBestKnownSpellInChain(bot, 8004);  // Lesser Healing Wave
        default:            return 0;
    }
}

// Sustained heal or HoT for topping off a moderately damaged target.
std::uint32_t GetSustainedHealSpell(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:  return FindBestKnownSpellInChain(bot, 139);  // Renew
        case CLASS_DRUID:   return FindBestKnownSpellInChain(bot, 774);  // Rejuvenation
        case CLASS_PALADIN: return FindBestKnownSpellInChain(bot, 635);  // Holy Light
        case CLASS_SHAMAN:  return FindBestKnownSpellInChain(bot, 331);  // Healing Wave
        default:            return 0;
    }
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
    if (bot->IsInCombat() || owner->IsInCombat())
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
    if (bot->GetDistance(target) >= RangedMinDistance)
        return;

    // Angle pointing from target toward the bot — step further that way
    float const angle = target->GetAngle(bot);
    float const x     = bot->GetPositionX() + RangedRetreatDistance * std::cos(angle);
    float const y     = bot->GetPositionY() + RangedRetreatDistance * std::sin(angle);
    float const z     = bot->GetPositionZ();
    bot->GetMotionMaster()->MovePoint(0, x, y, z);
}

// Closes a ranged bot to RangedOptimalDistance when it is too far to cast.
// MoveChase with an explicit stop distance lets the engine handle pathing and
// stops the bot at the right spot without overshooting into melee range.
void EnsureRangedApproach(Player* bot, Unit* target)
{
    bot->GetMotionMaster()->MoveChase(target, RangedOptimalDistance);
}

void EnsureSupportRange(Player* bot, Player* owner, std::uint32_t spellId)
{
    if (!bot || !owner || !spellId)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    float const maxRange = bot->GetSpellMaxRangeForTarget(owner, spellInfo);
    if (maxRange <= 0.0f)
        return;

    if (!bot->IsWithinCombatRange(owner, maxRange))
        bot->GetMotionMaster()->MoveChase(owner, std::max(1.0f, maxRange - 2.0f));
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

    std::uint64_t const seed = bot->GetGUID().GetCounter();
    uint32_t const slots = std::max(1u, cfg.followSlotCount);
    std::uint32_t const slot = static_cast<std::uint32_t>(seed % slots);

    float slotAngle = FollowAngle;
    switch (cfg.followFormation)
    {
        case model::FollowFormation::Ring:
        {
            // Evenly distributed around the full circle.
            float const angleStep = 2.0f * 3.14159265358979323846f / static_cast<float>(slots);
            slotAngle = FollowAngle + (static_cast<float>(slot) * angleStep);
            break;
        }
        case model::FollowFormation::V:
        {
            // Spread behind owner in a V: centre bot at π, others fan outward.
            // Odd slots go right (+), even slots go left (-).
            float const spread = 3.14159265358979323846f / 6.0f; // 30° per step
            int const side  = (slot == 0) ? 0 : ((slot % 2 == 1) ? 1 : -1);
            int const depth = static_cast<int>((slot + 1) / 2);
            slotAngle = FollowAngle + static_cast<float>(side * depth) * spread;
            break;
        }
        case model::FollowFormation::Line:
        {
            // Single file: all directly behind, staggered by distance.
            // slotAngle stays at FollowAngle; caller uses default distance.
            slotAngle = FollowAngle;
            break;
        }
        case model::FollowFormation::Cluster:
        default:
            // All bots at same angle — bunched behind.
            slotAngle = FollowAngle;
            break;
    }

    float const dist = (cfg.followFormation == model::FollowFormation::Line)
        ? baseDistance + static_cast<float>(slot) * 1.5f
        : baseDistance;

    bot->GetMotionMaster()->MoveFollow(owner, dist, slotAngle);
}

bool ShouldCombatFollowOverride(Player* bot, Player* owner, BotCombatRole role)
{
    if (!bot || !owner)
        return false;

    if (role != BotCombatRole::Healer
        && role != BotCombatRole::HybridHealer
        && role != BotCombatRole::Ranged)
        return false;

    return bot->GetDistance(owner) > CombatFollowOverrideDistance;
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
bool IsValidAssistTarget(Player const* bot, Player const* owner, Unit const* candidate)
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
    if (!candidate->isTargetableForAttack(true, bot))
        return false;
    return true;
}

// Commanded attack targets need slightly looser validation than normal assist.
// While a player-issued attack command is latched, we still want casters to keep
// their target and approach even if line-of-sight / targetable checks flicker
// during pull movement or while the mob has not fully engaged yet.
bool IsViableCommandTarget(Player const* bot, Player const* owner, Unit const* candidate)
{
    if (!bot || !owner || !candidate || !candidate->IsInWorld() || !candidate->IsAlive())
        return false;
    if (candidate == owner || candidate == bot)
        return false;
    if (candidate->GetMap() != bot->GetMap())
        return false;
    if (owner->IsFriendlyTo(candidate) || bot->IsFriendlyTo(candidate))
        return false;
    return true;
}

// Resolve the unit a bot should be fighting right now, honouring any
// explicit player override before falling back to normal assist logic.
Unit* ResolveAssistTarget(Player* bot, Player* owner)
{
    BotOverride const ovr = GetOverride(bot->GetGUID());

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
        if (forced && IsViableCommandTarget(bot, owner, forced))
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
            if (IsViableCommandTarget(bot, owner, current))
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

        if (Unit* ownerVictim = owner->GetVictim())
        {
            if (IsViableCommandTarget(bot, owner, ownerVictim))
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

        ObjectGuid const ownerSelection = owner->GetTarget();
        if (ownerSelection)
        {
            if (Unit* selected = ObjectAccessor::GetUnit(*bot, ownerSelection))
            {
                if (IsViableCommandTarget(bot, owner, selected))
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
    if (Unit* current = bot->GetVictim())
    {
        if (IsValidAssistTarget(bot, owner, current))
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

    // 2. Pick up owner's active victim only when that mob is fighting back —
    //    i.e. the mob's current victim is the owner. This prevents the bot from
    //    chasing a mob the owner merely auto-attacked once but that hasn't
    //    aggroed yet or that the owner accidentally clicked.
    if (Unit* ownerVictim = owner->GetVictim())
    {
        if (IsValidAssistTarget(bot, owner, ownerVictim)
            && ownerVictim->GetVictim() == owner)
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
    if (Unit* current = bot->GetVictim())
    {
        if (IsValidAssistTarget(bot, owner, current))
            return current;
    }

    for (Unit* attacker : owner->getAttackers())
    {
        if (IsValidAssistTarget(bot, owner, attacker))
            return attacker;
    }

    return nullptr;
}

// ---------------------------------------------------------------
// Main tick
// ---------------------------------------------------------------

void Tick(Player* bot, Player* owner, float& retreatHpPct, bool& conserving)
{
    model::BotCombatMode const mode =
        service::BotPlayerRegistry::Instance().GetBotMode(owner->GetGUID());

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
            && !bot->IsWithinDistInMap(owner, RepositionDistance))
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
            if (!bot->IsNonMeleeSpellCast(false) && distance > RangedCastRange)
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
        : ResolveAssistTarget(bot, owner);

    if (role == BotCombatRole::HybridHealer)
    {
        UpdateConservationState(doctrine.settings, bot, conserving);

        std::uint32_t const directSpell = GetDirectHealSpell(bot);
        std::uint32_t const sustainedSpell = GetSustainedHealSpell(bot);

        if (owner->GetHealthPct() < HealOwnerModerate)
        {
            std::uint32_t const supportSpell = owner->GetHealthPct() < HealOwnerCritical
                ? directSpell
                : sustainedSpell;
            if (supportSpell)
                EnsureSupportRange(bot, owner, supportSpell);
        }

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
            else if (distance < RangedMinDistance
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
                retreatHpPct = RangedRetreatReset;
            }
            else if (distance > RangedCastRange)
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
        return; // No combat context — stand idle.

    if (bot->GetVictim() != target)
        bot->Attack(target, true);

    TryExecuteProfileRotation(bot, nullptr, target);
}

class CompanionAIEvent final : public BasicEvent
{
public:
    CompanionAIEvent(ObjectGuid botGuid, ObjectGuid ownerGuid,
                     std::uint8_t notInWorldRetries = 0,
                     float retreatHpPct = RangedRetreatTrigger,
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
        if (_retreatHpPct < RangedRetreatTrigger && bot && bot->GetHealthPct() >= RangedRetreatTrigger)
            _retreatHpPct = RangedRetreatTrigger;

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
