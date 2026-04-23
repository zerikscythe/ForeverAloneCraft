#include "service/AccountAltSanityChecker.h"

namespace living_world
{
namespace service
{
model::AccountAltSanityCheckResult AccountAltSanityChecker::Check(
    model::CharacterProgressSnapshot const& sourceSnapshot,
    model::CharacterProgressSnapshot const& cloneSnapshot) const
{
    model::AccountAltSanityCheckResult result;

    // Level must be >= source and the delta must not exceed the per-session cap.
    bool const levelSafe =
        cloneSnapshot.level >= sourceSnapshot.level &&
        (cloneSnapshot.level - sourceSnapshot.level) <= kMaxAllowedLevelDelta;

    if (!levelSafe)
    {
        result.failures.push_back(
            "level delta exceeds the safe per-session threshold");
    }

    // Money: clone may have earned gold; an implausibly large gain is flagged.
    bool const moneySafe =
        cloneSnapshot.money <= sourceSnapshot.money + kMaxAllowedMoneyGainCopper;

    if (!moneySafe)
    {
        result.failures.push_back(
            "money gain exceeds the safe per-session threshold");
    }

    if (result.failures.empty())
    {
        result.passed = true;
        result.safeDomains.push_back(model::AccountAltSyncDomain::Experience);
        result.safeDomains.push_back(model::AccountAltSyncDomain::Money);
    }

    return result;
}
} // namespace service
} // namespace living_world
