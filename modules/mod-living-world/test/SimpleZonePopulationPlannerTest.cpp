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

    std::vector<PlannedSpawn> SelectSpawns(
        std::vector<model::BotProfile> const& profiles,
        std::vector<model::BotAbstractState> const&,
        model::BotSpawnContext const& context,
        model::ProgressionPhaseState const&) const override
    {
        lastProfileCount = profiles.size();

        std::vector<PlannedSpawn> result;
        for (model::BotProfile const& profile : profiles)
        {
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
} // namespace planner
} // namespace living_world
