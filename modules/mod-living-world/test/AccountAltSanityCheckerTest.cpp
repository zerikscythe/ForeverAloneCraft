#include "service/AccountAltSanityChecker.h"
#include "gtest/gtest.h"

#include <algorithm>

namespace living_world
{
namespace service
{
namespace
{
model::CharacterProgressSnapshot Snapshot(
    std::uint8_t level,
    std::uint32_t experience,
    std::uint32_t money)
{
    model::CharacterProgressSnapshot s;
    s.level = level;
    s.experience = experience;
    s.money = money;
    return s;
}

bool HasDomain(
    model::AccountAltSanityCheckResult const& result,
    model::AccountAltSyncDomain domain)
{
    return std::find(
               result.safeDomains.begin(),
               result.safeDomains.end(),
               domain) != result.safeDomains.end();
}
} // namespace

TEST(AccountAltSanityCheckerTest, PassesWhenCloneEqualsSource)
{
    AccountAltSanityChecker checker;
    auto result = checker.Check(Snapshot(10, 500, 1000), Snapshot(10, 500, 1000));

    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(result.failures.empty());
    EXPECT_TRUE(HasDomain(result, model::AccountAltSyncDomain::Experience));
    EXPECT_TRUE(HasDomain(result, model::AccountAltSyncDomain::Money));
}

TEST(AccountAltSanityCheckerTest, PassesOnSmallLevelGain)
{
    AccountAltSanityChecker checker;
    auto result = checker.Check(Snapshot(10, 500, 1000), Snapshot(13, 200, 1000));

    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(result.failures.empty());
}

TEST(AccountAltSanityCheckerTest, PassesAtExactLevelDeltaCap)
{
    AccountAltSanityChecker checker;
    auto result = checker.Check(Snapshot(10, 0, 0), Snapshot(15, 0, 0));

    EXPECT_TRUE(result.passed);
}

TEST(AccountAltSanityCheckerTest, FailsWhenLevelDeltaExceedsCap)
{
    AccountAltSanityChecker checker;
    auto result = checker.Check(Snapshot(10, 0, 0), Snapshot(16, 0, 0));

    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.failures.empty());
    EXPECT_TRUE(result.safeDomains.empty());
}

TEST(AccountAltSanityCheckerTest, PassesOnSmallMoneyGain)
{
    AccountAltSanityChecker checker;
    // 50g gain (500,000 copper) well within the 500g cap.
    auto result = checker.Check(Snapshot(20, 0, 100'000), Snapshot(20, 0, 600'000));

    EXPECT_TRUE(result.passed);
    EXPECT_TRUE(HasDomain(result, model::AccountAltSyncDomain::Money));
}

TEST(AccountAltSanityCheckerTest, FailsWhenMoneyGainExceedsCap)
{
    AccountAltSanityChecker checker;
    // 600g gain (6,000,000 copper) exceeds the 500g cap.
    auto result = checker.Check(Snapshot(20, 0, 0), Snapshot(20, 0, 6'000'000));

    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.failures.empty());
    EXPECT_TRUE(result.safeDomains.empty());
}

TEST(AccountAltSanityCheckerTest, FailsBothChecksIndependently)
{
    AccountAltSanityChecker checker;
    // Level delta = 10 (too high), money gain = 10,000,000 copper (too high).
    auto result = checker.Check(Snapshot(10, 0, 0), Snapshot(20, 0, 10'000'000));

    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.failures.size(), 2u);
    EXPECT_TRUE(result.safeDomains.empty());
}
} // namespace service
} // namespace living_world
