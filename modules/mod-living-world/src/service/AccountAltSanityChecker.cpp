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

    // Reputation, quests, and achievements are additive: the clone starts as an
    // exact copy of the source and can only accumulate these, never lose them.
    // Always safe to sync regardless of level/money sanity results.
    if (cloneSnapshot.totalReputationStanding >= sourceSnapshot.totalReputationStanding)
        result.safeDomains.push_back(model::AccountAltSyncDomain::Reputation);
    if (cloneSnapshot.completedQuestCount >= sourceSnapshot.completedQuestCount)
        result.safeDomains.push_back(model::AccountAltSyncDomain::Quests);
    if (cloneSnapshot.achievementCount >= sourceSnapshot.achievementCount)
        result.safeDomains.push_back(model::AccountAltSyncDomain::Achievements);

    // Honor: only sync positive net gains within the per-session cap.
    // totalKills is strictly additive; honor points can be spent by the bot so
    // we take the net gain (clone - snapshot) and cap it.
    bool const honorSafe =
        cloneSnapshot.totalHonorPoints <= sourceSnapshot.totalHonorPoints + kMaxAllowedHonorGain &&
        cloneSnapshot.totalKills >= sourceSnapshot.totalKills &&
        (cloneSnapshot.totalKills - sourceSnapshot.totalKills) <= kMaxAllowedKillDelta;
    if (honorSafe)
        result.safeDomains.push_back(model::AccountAltSyncDomain::Honor);
    else
        result.failures.push_back("honor or kill delta exceeds the safe per-session threshold");

    // Skills and spells are strictly additive (INSERT IGNORE / GREATEST).
    // The clone can only accumulate them, never lose them. Always safe.
    result.safeDomains.push_back(model::AccountAltSyncDomain::Skills);
    result.safeDomains.push_back(model::AccountAltSyncDomain::Spells);

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
