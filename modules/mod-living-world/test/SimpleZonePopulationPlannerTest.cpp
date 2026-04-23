#include "planner/PlannerTypes.h"
#include "planner/ProgressionGateResolver.h"
#include "planner/SimpleZonePopulationPlanner.h"
#include "planner/SpawnSelector.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace planner
{
namespace
{
class FakeProgressionGateResolver : public ProgressionGateResolver
{
public:
    bool IsBotAllowedInContext(
        model::BotProfile const& profile,
        model::BotSpawnContext const&,
        model::ProgressionPhaseState const&) const override
    {
        return profile.level <= 40;
    }
};

class RecordingSpawnSelector : public SpawnSelector
{
public:
    mutable std::size_t lastProfileCount = 0;
    mutable std::vector<std::uint64_t> lastProfileIds;

    std::vector<PlannedSpawn> SelectSpawns(
        std::vector<model::BotProfile> const& profiles,
        std::vector<model::BotAbstractState> const&,
        model::BotSpawnContext const& context,
        model::ProgressionPhaseState const&) const override
    {
        lastProfileCount = profiles.size();
        lastProfileIds.clear();

        std::vector<PlannedSpawn> result;
        for (model::BotProfile const& profile : profiles)
        {
            lastProfileIds.push_back(profile.botId);
            result.push_back({ profile.botId, context.playerZoneId, 100.0f - static_cast<float>(profile.level) });
        }

        return result;
    }
};
} // namespace

TEST(SimpleZonePopulationPlannerTest, BuildsIntentOnlyPlanFromEligibleProfiles)
{
    RecordingSpawnSelector spawnSelector;
    FakeProgressionGateResolver gateResolver;
    SimpleZonePopulationPlanner planner(spawnSelector, gateResolver);

    std::vector<model::BotProfile> profiles =
    {
        { 1, "Lowbie", 1, 1, model::BotFaction::Alliance, 20, "Explorers", model::BotPersonality::Indifferent, model::BotRole::Damage, { 12 } },
        { 2, "Veteran", 1, 1, model::BotFaction::Alliance, 60, "Explorers", model::BotPersonality::Cautious, model::BotRole::Tank, { 12 } },
        { 3, "Scout", 1, 1, model::BotFaction::Alliance, 35, "Explorers", model::BotPersonality::Aggressive, model::BotRole::Support, { 12 } }
    };

    std::vector<model::BotAbstractState> states =
    {
        { 1, 12, model::BotActivity::City, false, true, 0, 0, 0, 0.7f, 0.3f },
        { 2, 12, model::BotActivity::Outdoor, false, true, 0, 0, 0, 0.2f, 0.8f },
        { 3, 12, model::BotActivity::Traveling, false, true, 0, 0, 0, 0.5f, 0.5f }
    };

    model::BotSpawnContext context;
    context.playerZoneId = 12;
    context.localSpawnBudget = 1;
    context.visibleBotLimit = 2;

    model::ProgressionPhaseState phaseState;
    phaseState.currentPhase = model::ProgressionPhase::Classic;
    phaseState.maxPlayerLevel = 40;

    ZonePopulationPlan plan = planner.BuildPlan(profiles, states, context, phaseState);

    EXPECT_EQ(spawnSelector.lastProfileCount, 2u);
    ASSERT_EQ(plan.spawns.size(), 1u);
    EXPECT_EQ(plan.reservedBudget, 1u);
    EXPECT_EQ(plan.spawns.front().botId, 1u);
    EXPECT_EQ(plan.spawns.front().zoneId, 12u);
}

TEST(SimpleZonePopulationPlannerTest, SuppressesProfilesBlockedByStateActivityAndUnlockedZones)
{
    RecordingSpawnSelector spawnSelector;
    FakeProgressionGateResolver gateResolver;
    SimpleZonePopulationPlanner planner(spawnSelector, gateResolver);

    std::vector<model::BotProfile> profiles =
    {
        { 1, "EligibleCity", 1, 1, model::BotFaction::Alliance, 20, "Explorers", model::BotPersonality::Indifferent, model::BotRole::Damage, { 12 } },
        { 2, "Spawned", 1, 1, model::BotFaction::Alliance, 20, "Explorers", model::BotPersonality::Indifferent, model::BotRole::Damage, { 12 } },
        { 3, "Cooldown", 1, 1, model::BotFaction::Alliance, 20, "Explorers", model::BotPersonality::Indifferent, model::BotRole::Damage, { 12 } },
        { 4, "LockedZone", 1, 1, model::BotFaction::Alliance, 20, "Explorers", model::BotPersonality::Indifferent, model::BotRole::Damage, { 571 } },
        { 5, "OutdoorBlocked", 1, 1, model::BotFaction::Alliance, 20, "Explorers", model::BotPersonality::Indifferent, model::BotRole::Damage, { 12 } }
    };

    std::vector<model::BotAbstractState> states =
    {
        { 1, 12, model::BotActivity::City, false, true, 0, 0, 0, 0.9f, 0.1f },
        { 2, 12, model::BotActivity::City, true, true, 0, 0, 0, 0.9f, 0.1f },
        { 3, 12, model::BotActivity::City, false, true, 30, 0, 0, 0.9f, 0.1f },
        { 4, 571, model::BotActivity::Outdoor, false, true, 0, 0, 0, 0.1f, 0.9f },
        { 5, 12, model::BotActivity::Outdoor, false, true, 0, 0, 0, 0.1f, 0.9f }
    };

    model::BotSpawnContext context;
    context.playerZoneId = 12;
    context.localSpawnBudget = 5;
    context.visibleBotLimit = 5;
    context.allowCitySpawns = true;
    context.allowOutdoorSpawns = false;

    model::ProgressionPhaseState phaseState;
    phaseState.currentPhase = model::ProgressionPhase::Classic;
    phaseState.maxPlayerLevel = 40;
    phaseState.unlockedZoneIds.insert(12);

    ZonePopulationPlan plan = planner.BuildPlan(profiles, states, context, phaseState);

    ASSERT_EQ(spawnSelector.lastProfileIds.size(), 1u);
    EXPECT_EQ(spawnSelector.lastProfileIds.front(), 1u);
    ASSERT_EQ(plan.spawns.size(), 1u);
    EXPECT_EQ(plan.spawns.front().botId, 1u);
}

TEST(SimpleZonePopulationPlannerTest, ReordersSelectorOutputByPlannerScoreBeforeBudgeting)
{
    class ReverseOrderSpawnSelector : public SpawnSelector
    {
    public:
        std::vector<PlannedSpawn> SelectSpawns(
            std::vector<model::BotProfile> const& profiles,
            std::vector<model::BotAbstractState> const&,
            model::BotSpawnContext const& context,
            model::ProgressionPhaseState const&) const override
        {
            std::vector<PlannedSpawn> result;
            result.push_back({ profiles[1].botId, context.playerZoneId, 50.0f });
            result.push_back({ profiles[0].botId, context.playerZoneId, 50.0f });
            return result;
        }
    } spawnSelector;

    FakeProgressionGateResolver gateResolver;
    SimpleZonePopulationPlanner planner(spawnSelector, gateResolver);

    std::vector<model::BotProfile> profiles =
    {
        { 10, "ExactMatch", 1, 1, model::BotFaction::Alliance, 20, "Explorers", model::BotPersonality::Aggressive, model::BotRole::Damage, { 12 } },
        { 20, "NearbyOnly", 1, 1, model::BotFaction::Alliance, 20, "Explorers", model::BotPersonality::Indifferent, model::BotRole::Damage, { 13 } }
    };

    std::vector<model::BotAbstractState> states =
    {
        { 10, 12, model::BotActivity::Outdoor, false, true, 0, 0, 0, 0.2f, 1.0f },
        { 20, 13, model::BotActivity::Outdoor, false, true, 0, 0, 0, 0.2f, 0.4f }
    };

    model::BotSpawnContext context;
    context.playerZoneId = 12;
    context.nearbyZoneIds = { 13 };
    context.localSpawnBudget = 1;
    context.visibleBotLimit = 2;
    context.allowOutdoorSpawns = true;

    model::ProgressionPhaseState phaseState;
    phaseState.currentPhase = model::ProgressionPhase::Classic;
    phaseState.maxPlayerLevel = 40;
    phaseState.unlockedZoneIds.insert(12);
    phaseState.unlockedZoneIds.insert(13);

    ZonePopulationPlan plan = planner.BuildPlan(profiles, states, context, phaseState);

    ASSERT_EQ(plan.spawns.size(), 1u);
    EXPECT_EQ(plan.spawns.front().botId, 10u);
    EXPECT_GT(plan.spawns.front().score, 50.0f);
}
} // namespace planner
} // namespace living_world
