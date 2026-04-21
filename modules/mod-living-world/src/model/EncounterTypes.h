#pragma once

#include <cstdint>

namespace living_world
{
namespace model
{
enum class BotFaction : std::uint8_t
{
    Alliance,
    Horde,
    Neutral
};

enum class BotPersonality : std::uint8_t
{
    Indifferent,
    Cautious,
    Aggressive
};

enum class BotRole : std::uint8_t
{
    Unknown,
    Tank,
    Healer,
    Damage,
    Support
};

enum class BotActivity : std::uint8_t
{
    Abstract,
    City,
    Traveling,
    Outdoor,
    PartySupport,
    RivalPatrol,
    Recovering
};

enum class GroupAlertState : std::uint8_t
{
    Idle,
    Alert,
    Engaged
};

enum class EncounterDisposition : std::uint8_t
{
    PassBy,
    Observe,
    Stalk,
    Skirmish,
    Clash
};

enum class ProgressionPhase : std::uint8_t
{
    Classic,
    BurningCrusade,
    Wrath
};

enum class RosterEntrySource : std::uint8_t
{
    GenericBot,
    AccountAlt
};

enum class PossessionMode : std::uint8_t
{
    FollowOnly,
    Assisted,
    DirectControl
};
} // namespace model
} // namespace living_world
