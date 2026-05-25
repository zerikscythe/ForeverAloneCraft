#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace living_world
{
namespace model
{
// Groundwork data model for server-authoritative bot combat profiles.
// This is intentionally aggregate-oriented and independent from any specific
// SQL layout so service/repository work can evolve without changing callers.
// See `modules/mod-living-world/docs/BotCombatProfiles.md` for the full design
// direction and table concepts that this model is expected to support.

// Controls how a bot manages its primary combat resource (mana for casters,
// rage/energy for physical classes) during a fight.
//
// FullForce   — spend everything; no reservation (Warriors, Rogues, DKs,
//               pure nukers with no utility)
// Reserve     — stay above a floor (resourceLowWater) for utility spells
//               (Cleanse, Remove Curse, interrupt, emergency heal); otherwise
//               full offense. No high-water resume band. (Ret Paladin, Enhance
//               Shaman, Balance Druid)
// Conservative — full hysteresis: stop all offense below resourceLowWater,
//               resume above resourceHighWater. (Pure healers, hybrid healers)
// JitCasting  — offense entries always suppressed; cast only on external command
enum class BotCombatConservationMode : std::uint8_t
{
    FullForce    = 0,
    Reserve      = 1,
    Conservative = 2,
    JitCasting   = 3
};

enum class BotCombatActionType : std::uint8_t
{
    Spell = 0,
    Item = 1
};

enum class BotCombatRankMode : std::uint8_t
{
    BestKnown = 0,
    ExactSpellId = 1,
    SpecificRank = 2
};

enum class BotCombatConditionLogic : std::uint8_t
{
    All = 0,
    Any = 1
};

enum class BotCombatAoEMode : std::uint8_t
{
    Centroid = 0,
    Feet = 1
};

enum class BotCombatTargetingMode : std::uint8_t
{
    Standard = 0,
    Assist = 1,
    Skirmish = 2,
};

// These operators intentionally stay generic. Subject/stat/target keys remain
// strings for now so the profile system can stay data-driven while the rule
// vocabulary is still being finalized.
enum class BotCombatConditionOperator : std::uint8_t
{
    Equal = 0,
    NotEqual = 1,
    LessThan = 2,
    LessThanOrEqual = 3,
    GreaterThan = 4,
    GreaterThanOrEqual = 5,
    Has = 6,
    NotHas = 7,
    Exists = 8
};

enum class BotBuffScope : std::uint8_t
{
    Off    = 0,  // never cast maintenance buffs
    Self   = 1,  // buff only this bot
    Party  = 2,  // buff all connected party members
};

// Bitmask for item categories that are always looted regardless of lootQualityMin.
// Gold is always collected implicitly and has no flag here.
// Bit positions mirror LOOT_CATEGORY_DEFS in constants.py.
enum BotLootCategoryFlags : uint32_t
{
    LootCatNone         = 0x00,
    LootCatClothLeather = 0x01,  // Trade Goods class 7, subclass 5 & 6
    LootCatOreMetal     = 0x02,  // Trade Goods class 7, subclass 7
    LootCatHerbs        = 0x04,  // Trade Goods class 7, subclass 1
    LootCatEnchanting   = 0x08,  // Trade Goods class 7, subclass 10
    LootCatRecipes      = 0x10,  // Item class 9 (any subclass)
    LootCatGems         = 0x20,  // Item class 3 (any subclass)
};

enum class BotGatherNodes : uint8_t
{
    Off = 0,
    On  = 1,   // auto-detect profession; gather herbs & ore while travelling
};

enum class BotGatherSkin : uint8_t
{
    Off = 0,
    On  = 1,   // skin corpses after the kill if the bot has skinning
};

// Out-of-combat maintenance behaviour attached to a per-bot profile slot.
// NULL/absent optional values fall back to the global living_world_bot_global_config.
struct BotOocBehavior
{
    // ── Buff maintenance ────────────────────────────────────────────────────
    BotBuffScope    buffScope        = BotBuffScope::Party;
    std::uint16_t   buffReapplySecs  = 30;   // re-cast when aura has < N seconds left
    bool            buffOnSpawn      = true; // cast immediately when bot logs in

    // ── Movement override ───────────────────────────────────────────────────
    // std::nullopt  → use global role-based distance
    std::optional<float> followDistOverride;

    // ── Looting ─────────────────────────────────────────────────────────────
    // std::nullopt  → use global auto_loot setting
    std::optional<bool>  autoLootOverride;
    std::uint8_t    lootQualityMin      = 0;    // 0=all, 1=white+, 2=green+, 3=blue+, 4=purple+
    uint32_t        lootCategoryFlags   = 0;    // BotLootCategoryFlags — always loot these even if below quality min

    // ── Gathering ───────────────────────────────────────────────────────────
    BotGatherNodes  gatherNodes      = BotGatherNodes::Off;
    BotGatherSkin   gatherSkin       = BotGatherSkin::Off;
    std::uint8_t    skinLootQualityMax = 0;  // skin if all loot <= this quality (0=gray only)

    // ── Future: chat reactions ───────────────────────────────────────────────
    // Placeholder — implementation deferred.
    // bot_chat_reactions table (trigger_type, message_template, emote_id) will
    // reference profile_id once the loot/gather events are hooked up.
};

struct BotCombatProfileSettings
{
    struct TargetingSettings
    {
        BotCombatTargetingMode mode = BotCombatTargetingMode::Standard;
        float currentTargetBias = 80.0f;
        float assistTargetBias = 140.0f;
        float focusFireBias = 55.0f;
        float protectAllyBias = 170.0f;
        float preferHealerBias = 220.0f;
        float preferDpsBias = 140.0f;
        float avoidTankBias = 120.0f;
    };

    BotCombatConservationMode conservationMode =
        BotCombatConservationMode::Conservative;
    std::uint32_t rotationWaitMs = 500;
    // resourceLowWater: enter conservation (Conservative) or floor for utility
    // reserve (Reserve). Ignored for FullForce and JitCasting.
    std::uint8_t resourceLowWater = 55;
    // resourceHighWater: exit conservation once resource recovers above this
    // threshold. Only used by Conservative mode; ignored by Reserve.
    std::uint8_t resourceHighWater = 75;
    bool enableDownRank = true;
    std::uint8_t downRankFloor = 2;
    BotCombatAoEMode defaultAoEMode = BotCombatAoEMode::Centroid;
    std::uint8_t defaultAoEMinTargets = 2;
    float defaultAoEScanRadius = 10.0f;
    TargetingSettings targeting;
};

struct BotCombatActionDefinition
{
    std::uint64_t actionId = 0;
    std::uint8_t slot = 0; // 0 = primary, 1 = secondary
    BotCombatActionType actionType = BotCombatActionType::Spell;
    std::uint32_t spellBaseId = 0;
    std::uint32_t itemId = 0;
    std::string itemSelector;
    BotCombatRankMode rankMode = BotCombatRankMode::BestKnown;
    std::uint8_t rankValue = 0;
    std::string targetKey = "enemy";
    std::optional<BotCombatAoEMode> aoeMode;
    std::optional<std::uint8_t> aoeMinTargets;
    std::optional<float> aoeRadius;
};

struct BotCombatConditionDefinition
{
    std::uint64_t conditionId = 0;
    std::uint8_t sequence = 0;
    std::string subjectKey;
    std::string statKey;
    BotCombatConditionOperator comparison =
        BotCombatConditionOperator::GreaterThanOrEqual;
    float numericValue = 0.0f;
    std::string stringValue;
};

struct BotCombatEntryDefinition
{
    std::uint64_t entryId = 0;
    std::uint8_t priority = 0;
    std::string label;
    bool isInterrupt = false;
    bool breaksCurrentCast = false;
    bool enabled = true;
    BotCombatConditionLogic conditionLogic = BotCombatConditionLogic::All;
    BotCombatActionDefinition primaryAction;
    std::optional<BotCombatActionDefinition> secondaryAction;
    std::vector<BotCombatConditionDefinition> conditions;
};

struct BotCombatProfileRecord
{
    std::uint64_t profileId = 0;
    std::uint64_t sourceCharacterGuid = 0;
    std::uint32_t ownerAccountId = 0;
    std::uint8_t slot = 0;
    std::string profileName;

    std::string guessedSpecKey;
    std::string guessedRoleKey;
    std::optional<std::string> specOverrideKey;
    std::optional<std::string> roleOverrideKey;

    BotCombatProfileSettings settings;
    BotOocBehavior oocBehavior;
    std::vector<BotCombatEntryDefinition> interruptEntries;
    std::vector<BotCombatEntryDefinition> rotationEntries;

    [[nodiscard]] std::string EffectiveSpecKey() const
    {
        if (specOverrideKey && !specOverrideKey->empty())
            return *specOverrideKey;
        return guessedSpecKey;
    }

    [[nodiscard]] std::string EffectiveRoleKey() const
    {
        if (roleOverrideKey && !roleOverrideKey->empty())
            return *roleOverrideKey;
        return guessedRoleKey;
    }

    [[nodiscard]] bool HasEntryOverrides() const
    {
        return !interruptEntries.empty() || !rotationEntries.empty();
    }
};

struct BotCombatDefaultProfileRecord
{
    std::uint64_t defaultProfileId = 0;
    std::string specKey;
    std::string roleKey;
    std::string classKey;
    std::string contextKey = "PvE";  // "PvE" | "PvP" (future: "Arena", "BG")
    std::string displayName;
    std::string variantKey;
    std::string description;
    BotCombatProfileSettings settings;
    BotOocBehavior oocBehavior;
    std::vector<BotCombatEntryDefinition> interruptEntries;
    std::vector<BotCombatEntryDefinition> rotationEntries;
};

struct BotCombatRuntimeSelection
{
    std::uint64_t sourceCharacterGuid = 0;
    std::uint8_t activeProfileSlot = 0;
};
} // namespace model
} // namespace living_world
