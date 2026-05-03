#include "ai/CompanionAI.h"

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
#include "integration/SqlAccountAltRuntimeRepository.h"
#include "integration/SqlBotCombatDefaultProfileRepository.h"
#include "integration/SqlBotCombatProfileRepository.h"
#include "integration/SqlBotCombatProfileSelectionRepository.h"
#include "model/BotCombatMode.h"
#include "model/BotCombatProfile.h"
#include "service/BotCombatDoctrineResolver.h"
#include "service/BotCombatProfilePreparationService.h"
#include "service/BotCombatRuntimeEvaluator.h"
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
constexpr float HealSelfCritical     = 40.0f;
constexpr float HealSelfModerate     = 65.0f;
constexpr float HybridHealThreshold  = 70.0f;

// --- Healer mana thresholds for hybrid offense ---
constexpr float HealerManaConserveBelow = 40.0f;  // Stop attacking below this
constexpr float HealerManaResumeAbove   = 60.0f;  // Resume attacking above this

// --- DK disease aura IDs ---
constexpr std::uint32_t AuraFrostFever  = 55095;
constexpr std::uint32_t AuraBloodPlague = 55078;

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
        resolver);
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
    std::uint32_t ownerAccountId = owner->GetSession()
        ? owner->GetSession()->GetAccountId()
        : 0;
    service::BotCombatDoctrineResolution resolution =
        GetDoctrineResolver().ResolveForBot(
            bot->GetGUID().GetCounter(),
            bot->getClass(),
            ownerAccountId);
    doctrine.settings = resolution.profile.settings;

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

    std::uint32_t ownerAccountId = owner->GetSession()
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

bool TryExecuteProfileRotation(Player* bot, Player* owner, Unit* primaryTarget)
{
    if (!bot || !owner)
        return false;

    service::BotCombatPreparedProfile preparedProfile =
        GetPreparedCombatProfile(bot, owner);
    if (preparedProfile.interruptEntries.empty()
        && preparedProfile.rotationEntries.empty())
        return false;

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

    return handleEvaluationResult(
        GetRuntimeEvaluator().EvaluateRotation(preparedProfile, context),
        "rotation");
}

bool IsOffenseSuppressed(
    model::BotCombatConservationMode mode,
    bool conserving)
{
    if (mode == model::BotCombatConservationMode::JitCasting)
        return true;

    return mode == model::BotCombatConservationMode::Conservative && conserving;
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
// Works correctly for both single-rank spells and multi-rank chains.
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

// Offensive spells available to pure healers when mana allows.
std::uint32_t GetHealerOffensiveSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
        {
            // Shadow Word: Pain — instant DoT, apply when missing
            std::uint32_t const swp = FindBestKnownSpellInChain(bot, 589);
            if (swp && !HasAuraFromChain(target, 589))
                return swp;
            // Mind Blast — direct shadow nuke
            std::uint32_t const mb = FindBestKnownSpellInChain(bot, 8092);
            if (mb && !bot->HasSpellCooldown(mb))
                return mb;
            // Smite — holy filler when Mind Blast is on cooldown
            return FindBestKnownSpellInChain(bot, 585);
        }
        default:
            return 0;
    }
}

// ---------------------------------------------------------------
// Offensive spells — melee roles
// ---------------------------------------------------------------

// Returns the best melee-range offensive ability for the bot's class and state.
std::uint32_t GetMeleeOffensiveSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        {
            // Execute: highest-priority finisher at low target health
            std::uint32_t const execute = FindBestKnownSpellInChain(bot, 5308);
            if (execute && target->GetHealthPct() < 20.0f)
                return execute;

            // Mortal Strike (Arms)
            std::uint32_t spell = FindBestKnownSpellInChain(bot, 12294);
            if (spell)
                return spell;

            // Bloodthirst (Fury)
            spell = FindBestKnownSpellInChain(bot, 23881);
            if (spell)
                return spell;

            // Rend: apply the bleed DoT when not present on target
            {
                std::uint32_t const rend = FindBestKnownSpellInChain(bot, 772);
                if (rend && !target->HasAura(rend))
                    return rend;
            }

            // Heroic Strike: basic melee filler
            return FindBestKnownSpellInChain(bot, 78);
        }

        case CLASS_ROGUE:
        {
            std::uint32_t const snd   = FindBestKnownSpellInChain(bot, 5171); // Slice and Dice
            std::uint32_t const evisc = FindBestKnownSpellInChain(bot, 2098); // Eviscerate
            std::uint32_t const ss    = FindBestKnownSpellInChain(bot, 1752); // Sinister Strike

            std::uint8_t const cp = bot->GetComboPoints();

            // At 2+ combo points, apply Slice and Dice when the haste buff is missing
            if (cp >= 2 && snd && !bot->HasAura(snd))
                return snd;

            // At 4+ combo points spend with Eviscerate
            if (cp >= 4 && evisc)
                return evisc;

            return ss;
        }

        case CLASS_DEATH_KNIGHT:
        {
            bool const hasFrostFever  = target->HasAura(AuraFrostFever);
            bool const hasBloodPlague = target->HasAura(AuraBloodPlague);

            // Apply diseases before committing to strike abilities
            if (!hasBloodPlague && bot->HasSpell(45462))
                return 45462; // Plague Strike — applies Blood Plague
            if (!hasFrostFever && bot->HasSpell(45477))
                return 45477; // Icy Touch — applies Frost Fever

            // Diseases up: Death Strike when off cooldown (damage + self-heal)
            if (bot->HasSpell(49998) && !bot->HasSpellCooldown(49998))
                return 49998;

            // Death Strike is on cooldown — fill with rune strikes
            {
                std::uint32_t const heartStrike = FindBestKnownSpellInChain(bot, 55050);
                if (heartStrike && !bot->HasSpellCooldown(heartStrike))
                    return heartStrike; // Heart Strike
            }
            if (bot->HasSpell(45902) && !bot->HasSpellCooldown(45902))
                return 45902; // Blood Strike

            // Fallback: let the engine handle the cooldown; autoattack continues
            if (bot->HasSpell(49998))
                return 49998;
            return 0;
        }

        default:
            return 0;
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
// Offensive spells — hybrid healers acting offensively
// ---------------------------------------------------------------

// Returns the best short-range DPS ability for hybrid healers acting offensively.
std::uint32_t GetHybridDamageSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_PALADIN:
        {
            // Hammer of Wrath: execute-range burst, requires target below 20% HP
            std::uint32_t const how = FindBestKnownSpellInChain(bot, 24275);
            if (how && target->GetHealthPct() < 20.0f
                && !bot->HasSpellCooldown(how))
                return how;

            // Judgement of Light: holy damage + party heal proc on hit
            std::uint32_t const jol = FindBestKnownSpellInChain(bot, 20271);
            if (jol && !bot->HasSpellCooldown(jol))
                return jol;

            // Consecration: sustained AoE holy damage field
            std::uint32_t const cons = FindBestKnownSpellInChain(bot, 20116);
            if (cons && !bot->HasSpellCooldown(cons))
                return cons;

            // Crusader Strike: primary single-target melee ability
            if (bot->HasSpell(35395) && !bot->HasSpellCooldown(35395))
                return 35395;

            return 0;
        }

        case CLASS_SHAMAN:
        {
            std::uint32_t const flameShock = FindBestKnownSpellInChain(bot, 8050);
            if (flameShock && !target->HasAura(flameShock))
                return flameShock; // Apply Flame Shock DoT first
            return FindBestKnownSpellInChain(bot, 8042); // Earth Shock filler
        }

        case CLASS_DRUID:
        {
            std::uint32_t const moonfire = FindBestKnownSpellInChain(bot, 8921);
            if (moonfire && !target->HasAura(moonfire))
                return moonfire; // Apply Moonfire DoT first
            return FindBestKnownSpellInChain(bot, 5176); // Wrath filler
        }

        default:
            return 0;
    }
}

// ---------------------------------------------------------------
// Offensive spells — ranged roles
// ---------------------------------------------------------------

// Returns the best ranged damage spell, preferring DoTs when not yet applied.
std::uint32_t GetDamageSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
        {
            // Walk the Frostbolt chain; if chain lookup fails (e.g. rank data
            // missing), fall back to direct spell ID checks for common ranks.
            std::uint32_t fb = FindBestKnownSpellInChain(bot, 116);
            if (fb)
                return fb;
            // Direct fallback: Frostbolt ranks 1-14 in reverse order
            static constexpr std::uint32_t FrostboltRanks[] = {
                42842, 42841, 38697, 27072, 25304, 10161, 10160, 10159,
                8406,  8405,  8404,  837,   228,   116
            };
            for (std::uint32_t id : FrostboltRanks)
                if (bot->HasSpell(id))
                    return id;
            // No Frostbolt — try Fireball as alternate
            fb = FindBestKnownSpellInChain(bot, 133);
            if (fb)
                return fb;
            // Frostfire Bolt (dual-school, learned via talent)
            if (bot->HasSpell(44614))
                return 44614;
            return 0;
        }

        case CLASS_WARLOCK:
        {
            // Curse of Agony: highest-DPS curse, apply first
            std::uint32_t const coa = FindBestKnownSpellInChain(bot, 980);
            if (coa && !target->HasAura(coa))
                return coa;

            // Immolate: fire DoT, apply when missing
            std::uint32_t const immolate = FindBestKnownSpellInChain(bot, 348);
            if (immolate && !target->HasAura(immolate))
                return immolate;

            // Corruption: instant shadow DoT
            std::uint32_t const corruption = FindBestKnownSpellInChain(bot, 172);
            if (corruption && !target->HasAura(corruption))
                return corruption;

            // Shadow Bolt: primary filler when all DoTs are rolling
            return FindBestKnownSpellInChain(bot, 686);
        }

        case CLASS_HUNTER:
        {
            // Serpent Sting: nature DoT, apply when missing
            std::uint32_t const serpent = FindBestKnownSpellInChain(bot, 1978);
            if (serpent && !target->HasAura(serpent))
                return serpent;

            // Multi-Shot: strong filler when off cooldown
            std::uint32_t const multiShot = FindBestKnownSpellInChain(bot, 2643);
            if (multiShot && !bot->HasSpellCooldown(multiShot))
                return multiShot;

            // Steady Shot: primary ranged filler
            if (bot->HasSpell(34120))
                return 34120;

            // Arcane Shot: fallback if Steady Shot is not yet learned
            return FindBestKnownSpellInChain(bot, 3044);
        }

        default:
            return 0;
    }
}

// ---------------------------------------------------------------
// Out-of-combat maintenance
// ---------------------------------------------------------------

// Core buff application — no combat guard. Called by both the idle tick and
// the explicit party buff command.
void ApplyBotBuff(Player* bot, Player* owner)
{
    if (bot->IsNonMeleeSpellCast(false))
        return;

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        {
            // Battle Shout: party-wide AP buff; cast on self, hits all nearby members
            std::uint32_t const shout = FindBestKnownSpellInChain(bot, 6673);
            if (shout && !HasAuraFromChain(bot, 6673) && !HasAuraFromChain(owner, 6673))
                bot->CastSpell(bot, shout, false);
            break;
        }

        case CLASS_DEATH_KNIGHT:
        {
            // Horn of Winter: party-wide Strength/Agility buff
            if (bot->HasSpell(57330)
                && !HasAuraFromChain(bot, 57330)
                && !HasAuraFromChain(owner, 57330))
                bot->CastSpell(bot, 57330U, false);
            break;
        }

        case CLASS_PALADIN:
        {
            // Re-apply seal if it dropped between fights
            if (!HasSealActive(bot))
            {
                std::uint32_t const seal = GetPreferredSeal(bot);
                if (seal)
                { bot->CastSpell(bot, seal, false); break; }
            }
            // Blessing of Kings: prioritise owner, then self
            {
                std::uint32_t const bok = FindBestKnownSpellInChain(bot, 20217);
                if (bok)
                {
                    if (!HasAuraFromChain(owner, 20217))
                    { bot->CastSpell(owner, bok, false); break; }
                    if (!HasAuraFromChain(bot, 20217))
                    { bot->CastSpell(bot, bok, false); break; }
                }
            }
            // Blessing of Might: fallback if Kings not known
            {
                std::uint32_t const bom = FindBestKnownSpellInChain(bot, 19740);
                if (bom)
                {
                    if (!HasAuraFromChain(owner, 19740))
                    { bot->CastSpell(owner, bom, false); break; }
                    if (!HasAuraFromChain(bot, 19740))
                    { bot->CastSpell(bot, bom, false); break; }
                }
            }
            break;
        }

        case CLASS_PRIEST:
        {
            // Power Word: Fortitude: Stamina buff for all group members
            std::uint32_t const pwf = FindBestKnownSpellInChain(bot, 1243);
            if (!pwf) break;
            if (Group const* group = bot->GetGroup())
            {
                for (Group::MemberSlot const& slot : group->GetMemberSlots())
                {
                    Player* target = ObjectAccessor::FindConnectedPlayer(slot.guid);
                    if (!target || !target->IsAlive() || !target->IsInWorld()) continue;
                    if (!HasAuraFromChain(target, 1243))
                    { bot->CastSpell(target, pwf, false); return; }
                }
            }
            if (!HasAuraFromChain(bot, 1243))
                bot->CastSpell(bot, pwf, false);
            break;
        }

        case CLASS_DRUID:
        {
            // Mark of the Wild: multi-stat buff for all group members
            std::uint32_t const motw = FindBestKnownSpellInChain(bot, 1126);
            if (!motw) break;
            if (Group const* group = bot->GetGroup())
            {
                for (Group::MemberSlot const& slot : group->GetMemberSlots())
                {
                    Player* target = ObjectAccessor::FindConnectedPlayer(slot.guid);
                    if (!target || !target->IsAlive() || !target->IsInWorld()) continue;
                    if (!HasAuraFromChain(target, 1126))
                    { bot->CastSpell(target, motw, false); return; }
                }
            }
            if (!HasAuraFromChain(bot, 1126))
                bot->CastSpell(bot, motw, false);
            break;
        }

        case CLASS_MAGE:
        {
            // Arcane Intellect: Intellect buff for all group members
            std::uint32_t const ai = FindBestKnownSpellInChain(bot, 1459);
            if (!ai) break;
            if (Group const* group = bot->GetGroup())
            {
                for (Group::MemberSlot const& slot : group->GetMemberSlots())
                {
                    Player* target = ObjectAccessor::FindConnectedPlayer(slot.guid);
                    if (!target || !target->IsAlive() || !target->IsInWorld()) continue;
                    if (!HasAuraFromChain(target, 1459))
                    { bot->CastSpell(target, ai, false); return; }
                }
            }
            if (!HasAuraFromChain(bot, 1459))
                bot->CastSpell(bot, ai, false);
            break;
        }

        case CLASS_WARLOCK:
        {
            // Fel Armor preferred; fall back to Demon Armor / Demon Skin — self-only
            std::uint32_t armor = FindBestKnownSpellInChain(bot, 28176); // Fel Armor
            if (!armor) armor   = FindBestKnownSpellInChain(bot, 706);   // Demon Armor
            if (!armor) armor   = FindBestKnownSpellInChain(bot, 696);   // Demon Skin
            if (armor && !bot->HasAura(armor))
                bot->CastSpell(bot, armor, false);
            break;
        }

        default:
            break;
    }
}

void TryApplyOutOfCombatBuff(Player* bot, Player* owner)
{
    if (bot->IsInCombat() || owner->IsInCombat())
        return;
    ApplyBotBuff(bot, owner);
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

void IssueFormationFollow(Player* bot, Player* owner)
{
    if (!bot || !owner)
        return;

    // Deterministic per-bot slotting around the owner to avoid stack/bunching.
    // Keep a minimum ~1y angular separation while maintaining the nominal
    // follow ring distance.
    std::uint64_t const seed = bot->GetGUID().GetCounter();
    std::uint32_t const slot = static_cast<std::uint32_t>(seed % 7ULL); // 0..6
    float const angleStep = 2.0f * 3.14159265358979323846f / 7.0f;
    float const slotAngle = FollowAngle + (static_cast<float>(slot) * angleStep);

    bot->GetMotionMaster()->MoveFollow(owner, FollowDistance, slotAngle);
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

void TickHealer(Player* bot, Player* owner)
{
    if (bot->IsNonMeleeSpellCast(false))
        return;

    std::uint32_t const directSpell    = GetDirectHealSpell(bot);
    std::uint32_t const sustainedSpell = GetSustainedHealSpell(bot);

    if (owner->GetHealthPct() < HealOwnerModerate)
    {
        std::uint32_t const supportSpell = owner->GetHealthPct() < HealOwnerCritical
            ? directSpell
            : sustainedSpell;
        if (supportSpell)
            EnsureSupportRange(bot, owner, supportSpell);
    }

    // Power Word: Shield (Priest only): proactive absorb while the owner is in
    // combat. Applied as soon as the fight starts so damage is partially absorbed
    // before reactive heals are needed. Weakened Soul prevents re-shielding for
    // 15 seconds after the absorb is consumed.
    if (bot->getClass() == CLASS_PRIEST && owner->IsInCombat())
    {
        std::uint32_t const pws = FindBestKnownSpellInChain(bot, 17);
        if (pws && !owner->HasAura(pws) && !owner->HasAura(AuraWeakenedSoul))
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] HealerAI cast attempt: bot='{}' guid={} action=shield_owner spell={} targetGuid={} ownerHp={:.1f} botHp={:.1f}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                pws,
                owner->GetGUID().GetCounter(),
                owner->GetHealthPct(),
                bot->GetHealthPct());
            bot->CastSpell(owner, pws, false);
            return;
        }
    }

    if (owner->GetHealthPct() < HealOwnerCritical)
    {
        if (directSpell)
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] HealerAI cast attempt: bot='{}' guid={} action=direct_heal_owner spell={} targetGuid={} ownerHp={:.1f} botHp={:.1f}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                directSpell,
                owner->GetGUID().GetCounter(),
                owner->GetHealthPct(),
                bot->GetHealthPct());
            bot->CastSpell(owner, directSpell, false);
        }
        return;
    }

    if (owner->GetHealthPct() < HealOwnerModerate)
    {
        if (sustainedSpell && !owner->HasAura(sustainedSpell))
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] HealerAI cast attempt: bot='{}' guid={} action=sustain_owner spell={} targetGuid={} ownerHp={:.1f} botHp={:.1f}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                sustainedSpell,
                owner->GetGUID().GetCounter(),
                owner->GetHealthPct(),
                bot->GetHealthPct());
            bot->CastSpell(owner, sustainedSpell, false);
        }
        return;
    }

    if (bot->GetHealthPct() < HealSelfCritical)
    {
        if (directSpell)
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] HealerAI cast attempt: bot='{}' guid={} action=direct_heal_self spell={} targetGuid={} ownerHp={:.1f} botHp={:.1f}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                directSpell,
                bot->GetGUID().GetCounter(),
                owner->GetHealthPct(),
                bot->GetHealthPct());
            bot->CastSpell(bot, directSpell, false);
        }
        return;
    }

    if (bot->GetHealthPct() < HealSelfModerate)
    {
        if (sustainedSpell && !bot->HasAura(sustainedSpell))
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldDebug] HealerAI cast attempt: bot='{}' guid={} action=sustain_self spell={} targetGuid={} ownerHp={:.1f} botHp={:.1f}",
                bot->GetName(),
                bot->GetGUID().GetCounter(),
                sustainedSpell,
                bot->GetGUID().GetCounter(),
                owner->GetHealthPct(),
                bot->GetHealthPct());
            bot->CastSpell(bot, sustainedSpell, false);
        }
    }
}

void TickRanged(Player* bot, Player* owner, Unit* target)
{
    if (TryExecuteProfileRotation(bot, owner, target))
        return;

    if (bot->IsNonMeleeSpellCast(false))
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] RangedAI cast blocked: bot='{}' guid={} targetGuid={} reason=already_casting",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            target ? target->GetGUID().GetCounter() : 0);
        return;
    }

    std::uint32_t const spell = GetDamageSpell(bot, target);
    if (!spell)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] RangedAI cast blocked: bot='{}' guid={} class={} targetGuid={} distance={:.2f} mana={}/{} reason=no_spell_selected",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            static_cast<std::uint32_t>(bot->getClass()),
            target ? target->GetGUID().GetCounter() : 0,
            target ? bot->GetDistance(target) : 0.0f,
            static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
            static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)));
        return;
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] RangedAI cast attempt: bot='{}' guid={} class={} spell={} targetGuid={} distance={:.2f} mana={}/{} victimGuid={}",
        bot->GetName(),
        bot->GetGUID().GetCounter(),
        static_cast<std::uint32_t>(bot->getClass()),
        spell,
        target ? target->GetGUID().GetCounter() : 0,
        target ? bot->GetDistance(target) : 0.0f,
        static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
        static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)),
        bot->GetVictim() ? bot->GetVictim()->GetGUID().GetCounter() : 0);

    bot->CastSpell(target, spell, false);
}

void TickMelee(Player* bot, Player* owner, Unit* target)
{
    if (TryExecuteProfileRotation(bot, owner, target))
        return;

    if (bot->IsNonMeleeSpellCast(false))
        return;

    std::uint32_t const spell = GetMeleeOffensiveSpell(bot, target);
    if (spell)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] MeleeAI cast attempt: bot='{}' guid={} class={} spell={} targetGuid={} distance={:.2f} victimGuid={}",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            static_cast<std::uint32_t>(bot->getClass()),
            spell,
            target ? target->GetGUID().GetCounter() : 0,
            target ? bot->GetDistance(target) : 0.0f,
            bot->GetVictim() ? bot->GetVictim()->GetGUID().GetCounter() : 0);
        bot->CastSpell(target, spell, false);
    }
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

void Tick(Player* bot, Player* owner, float& retreatHpPct, bool& healerConserving)
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
        TryApplyOutOfCombatBuff(bot, owner);
        if (!bot->IsNonMeleeSpellCast(false)
            && !bot->IsWithinDistInMap(owner, RepositionDistance))
            bot->GetMotionMaster()->MoveFollow(owner, FollowDistance, FollowAngle);
        return;
    }

    BotCombatDoctrine const doctrine = GetCombatDoctrine(bot, owner);
    BotCombatRole const role = doctrine.role;

    // Pure healers: heal first, then optionally attack based on mana.
    if (role == BotCombatRole::Healer)
    {
        TickHealer(bot, owner);

        if (doctrine.settings.conservationMode != model::BotCombatConservationMode::Conservative)
        {
            healerConserving = false;
        }
        else if (bot->GetMaxPower(POWER_MANA) > 0)
        {
            float const manaPct = 100.0f * static_cast<float>(bot->GetPower(POWER_MANA))
                                         / static_cast<float>(bot->GetMaxPower(POWER_MANA));
            if (healerConserving)
            {
                if (manaPct >= doctrine.settings.manaHighWater)
                    healerConserving = false;
            }
            else if (manaPct < doctrine.settings.manaLowWater)
            {
                healerConserving = true;
            }
        }

        // Keep a valid assist target snapshot so follow logic does not yank
        // healers back to 2y during active combat.
        Unit* attackTarget = ResolveAssistTarget(bot, owner);
        if (attackTarget && bot->GetVictim() != attackTarget)
        {
            // Match ranged behavior: lock victim/combat state without forcing
            // chase movement that can interrupt casts.
            bot->Attack(attackTarget, false);
        }

        if (attackTarget)
            BreakFollowForAttack(bot);

        if (attackTarget)
        {
            float const distance = bot->GetDistance(attackTarget);
            if (!bot->IsNonMeleeSpellCast(false) && distance > RangedCastRange)
            {
                // Priests/healers need explicit approach just like ranged DPS,
                // otherwise they can stay latched to owner follow spacing when
                // the pull starts far away.
                EnsureRangedApproach(bot, attackTarget);
                return;
            }
        }

        // Attempt an offensive spell if not already casting and mana is healthy.
        if (attackTarget
            && !IsOffenseSuppressed(doctrine.settings.conservationMode, healerConserving))
        {
            if (TryExecuteProfileRotation(bot, owner, attackTarget))
                return;

            std::uint32_t const spell = GetHealerOffensiveSpell(bot, attackTarget);
            if (spell && !bot->IsNonMeleeSpellCast(false))
                bot->CastSpell(attackTarget, spell, false);
        }

        TryApplyOutOfCombatBuff(bot, owner);

        if (ShouldCombatFollowOverride(bot, owner, role))
        {
            if (!bot->IsNonMeleeSpellCast(false))
            {
                bot->GetMotionMaster()->Clear(false);
                IssueFormationFollow(bot, owner);
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
            IssueFormationFollow(bot, owner);
        }
        return;
    }

    Unit* const assistTarget = (mode == model::BotCombatMode::Guard)
        ? ResolveGuardTarget(bot, owner)
        : ResolveAssistTarget(bot, owner);

        if (assistTarget)
        {
            if (ShouldCombatFollowOverride(bot, owner, role))
            {
                if (!bot->IsNonMeleeSpellCast(false))
                {
                    bot->GetMotionMaster()->Clear(false);
                    IssueFormationFollow(bot, owner);
                }
                return;
            }

            BreakFollowForAttack(bot);

        if (role == BotCombatRole::HybridHealer)
        {
            // Hybrid casters still triage owner health first. Only commit to
            // damage when the owner is healthy enough to take a few seconds
            // of attention shift.
            if (owner->GetHealthPct() < HybridHealThreshold)
            {
                std::uint32_t const emergencySpell = owner->GetHealthPct() < HealOwnerCritical
                    ? GetDirectHealSpell(bot)
                    : GetSustainedHealSpell(bot);
                if (emergencySpell)
                    EnsureSupportRange(bot, owner, emergencySpell);
                TickHealer(bot, owner);
                return;
            }

            if (bot->GetVictim() != assistTarget)
                bot->Attack(assistTarget, true);

            // Ensure seal is active before striking
            if (bot->getClass() == CLASS_PALADIN && !HasSealActive(bot)
                && !bot->IsNonMeleeSpellCast(false))
            {
                std::uint32_t const seal = GetPreferredSeal(bot);
                if (seal)
                {
                    bot->CastSpell(bot, seal, false);
                    EnsureChasingVictim(bot, assistTarget);
                    return;
                }
            }

            EnsureChasingVictim(bot, assistTarget);
            if (TryExecuteProfileRotation(bot, owner, assistTarget))
                return;

            std::uint32_t const spell = GetHybridDamageSpell(bot, assistTarget);
            if (spell && !bot->IsNonMeleeSpellCast(false))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] HybridAI cast attempt: bot='{}' guid={} class={} spell={} targetGuid={} distance={:.2f} ownerHp={:.1f} botHp={:.1f}",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    static_cast<std::uint32_t>(bot->getClass()),
                    spell,
                    assistTarget->GetGUID().GetCounter(),
                    bot->GetDistance(assistTarget),
                    owner->GetHealthPct(),
                    bot->GetHealthPct());
                bot->CastSpell(assistTarget, spell, false);
            }
            return;
        }

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
            IssueFormationFollow(bot, owner);
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
            IssueFormationFollow(bot, owner);
        }
        return;
    }

    TryApplyOutOfCombatBuff(bot, owner);

    if (!bot->IsNonMeleeSpellCast(false)
        && !IsBotAttackLocked(bot->GetGUID())
        && bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
    {
        bot->GetMotionMaster()->Clear(false);
        IssueFormationFollow(bot, owner);
    }
}

// ---------------------------------------------------------------
// Event class
// ---------------------------------------------------------------

class CompanionAIEvent final : public BasicEvent
{
public:
    CompanionAIEvent(ObjectGuid botGuid, ObjectGuid ownerGuid,
                     std::uint8_t notInWorldRetries = 0,
                     float retreatHpPct = RangedRetreatTrigger,
                     bool healerConserving = false)
        : _botGuid(botGuid), _ownerGuid(ownerGuid)
        , _notInWorldRetries(notInWorldRetries), _retreatHpPct(retreatHpPct)
        , _healerConserving(healerConserving)
    {
    }

    bool Execute(uint64, uint32) override
    {
        Player* bot   = ObjectAccessor::FindPlayer(_botGuid);
        Player* owner = ObjectAccessor::FindConnectedPlayer(_ownerGuid);
        if (!bot || !owner)
            return true;

        if (!bot->IsInWorld() || !owner->IsInWorld())
        {
            if (_notInWorldRetries >= MaxNotInWorldRetries)
                return true;

            // Backoff: 500ms, 1s, 2s, 4s, 4s, 4s, ...
            Milliseconds const delay = 500ms * (1u << std::min(_notInWorldRetries, std::uint8_t{3}));
            bot->m_Events.AddEventAtOffset(
                new CompanionAIEvent(_botGuid, _ownerGuid, _notInWorldRetries + 1, _retreatHpPct, _healerConserving),
                delay);
            return true;
        }

        // Reset retreat threshold if the bot has healed back above the trigger level.
        if (_retreatHpPct < RangedRetreatTrigger && bot && bot->GetHealthPct() >= RangedRetreatTrigger)
            _retreatHpPct = RangedRetreatTrigger;

        Tick(bot, owner, _retreatHpPct, _healerConserving);
        bot->m_Events.AddEventAtOffset(
            new CompanionAIEvent(_botGuid, _ownerGuid, 0, _retreatHpPct, _healerConserving),
            500ms);
        return true;
    }

private:
    static constexpr std::uint8_t MaxNotInWorldRetries = 20;

    ObjectGuid   _botGuid;
    ObjectGuid   _ownerGuid;
    std::uint8_t _notInWorldRetries;
    float        _retreatHpPct;
    bool         _healerConserving;
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

void ForceBotBuffRefresh(Player* bot, Player* owner)
{
    if (!bot || !owner)
        return;
    ApplyBotBuff(bot, owner);
}
} // namespace ai
} // namespace living_world
