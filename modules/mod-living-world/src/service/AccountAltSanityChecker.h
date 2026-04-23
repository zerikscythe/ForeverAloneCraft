#pragma once

#include "model/AccountAltRuntime.h"

namespace living_world
{
namespace service
{
// Conservative sanity check comparing source and clone snapshots.
// Returns which sync domains are safe to copy from clone back to source.
// Only domains covered by the snapshot (level, XP, money) can be approved;
// inventory, equipment, reputation, quests, and mail are never safe here.
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
};
} // namespace service
} // namespace living_world
