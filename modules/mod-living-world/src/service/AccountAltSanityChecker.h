#pragma once

#include "model/AccountAltRuntime.h"

namespace living_world
{
namespace service
{
// Conservative sanity check comparing source and clone snapshots.
// Returns which sync domains are safe to copy from clone back to source.
// Reputation, Quests, and Achievements are always safe (additive: clone can
// only accumulate, never lose these). Level/XP/money have per-session caps.
class AccountAltSanityChecker
{
public:
    model::AccountAltSanityCheckResult Check(
        model::CharacterProgressSnapshot const& sourceSnapshot,
        model::CharacterProgressSnapshot const& cloneSnapshot) const;

private:
    // Maximum level gain considered plausible in a single bot session.
    static constexpr std::uint8_t kMaxAllowedLevelDelta = 5;
    // Maximum money gain (copper) considered plausible in a single bot session.
    // 500 gold = 5,000,000 copper.
    static constexpr std::uint32_t kMaxAllowedMoneyGainCopper = 5'000'000;
    // Maximum net honor gain considered plausible in a single bot session.
    // 15,000 honor is a generous but realistic PvP session cap.
    static constexpr std::uint32_t kMaxAllowedHonorGain = 15'000;
    // Maximum honorable kills considered plausible in a single bot session.
    static constexpr std::uint32_t kMaxAllowedKillDelta = 500;
};
} // namespace service
} // namespace living_world
