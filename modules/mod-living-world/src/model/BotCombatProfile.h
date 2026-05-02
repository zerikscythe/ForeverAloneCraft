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

enum class BotCombatConservationMode : std::uint8_t
{
    FullForce = 0,
    Conservative = 1,
    JitCasting = 2
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

struct BotCombatProfileSettings
{
    BotCombatConservationMode conservationMode =
        BotCombatConservationMode::Conservative;
    std::uint8_t manaLowWater = 55;
    std::uint8_t manaHighWater = 75;
    bool enableDownRank = true;
    std::uint8_t downRankFloor = 2;
    BotCombatAoEMode defaultAoEMode = BotCombatAoEMode::Centroid;
    std::uint8_t defaultAoEMinTargets = 2;
    float defaultAoEScanRadius = 10.0f;
};

struct BotCombatActionDefinition
{
    std::uint64_t actionId = 0;
    std::uint8_t slot = 0; // 0 = primary, 1 = secondary
    BotCombatActionType actionType = BotCombatActionType::Spell;
    std::uint32_t spellBaseId = 0;
    std::uint32_t itemId = 0;
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
    std::string displayName;
    BotCombatProfileSettings settings;
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
