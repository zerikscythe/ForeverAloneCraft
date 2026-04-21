#include "planner/SimplePartyRosterPlanner.h"
#include "gtest/gtest.h"

namespace living_world
{
namespace planner
{
TEST(SimplePartyRosterPlannerTest, ApprovesOwnedAccountAltAndReturnsIntentOnlyPlan)
{
    SimplePartyRosterPlanner planner;

    model::RosterEntry accountAlt;
    accountAlt.rosterEntryId = 44;
    accountAlt.source = model::RosterEntrySource::AccountAlt;
    accountAlt.ownerAccountId = 77;
    accountAlt.characterGuid = 9001;
    accountAlt.controllableProfile.profile.botId = 501;
    accountAlt.controllableProfile.profile.name = "AltMage";
    accountAlt.controllableProfile.canBePlayerControlled = true;
    accountAlt.controllableProfile.canEarnProgression = true;

    std::vector<model::RosterEntry> rosterEntries = { accountAlt };

    model::PlayerRosterRequest request;
    request.requesterCharacterGuid = 1000;
    request.requesterAccountId = 77;
    request.requestedRosterEntryId = 44;
    request.possessionMode = model::PossessionMode::DirectControl;
    request.spawnAtPlayerLocation = true;
    request.addToPlayerParty = true;

    model::PlayerRosterContext context;
    context.playerZoneId = 1519;
    context.partyMemberCount = 2;
    context.maxPartyMembers = 5;
    context.playerIsInWorld = true;
    context.playerCanControlCompanions = true;

    PartyRosterPlan plan = planner.BuildPlan(rosterEntries, request, context);

    EXPECT_TRUE(plan.isApproved);
    EXPECT_EQ(plan.failureReason, PartyRosterFailureReason::None);
    EXPECT_TRUE(plan.shouldSpawnAtPlayer);
    EXPECT_TRUE(plan.shouldJoinPlayerParty);
    EXPECT_TRUE(plan.progressionShouldAccrueToControlledCharacter);
    EXPECT_EQ(plan.plannedSpawn.rosterEntryId, 44u);
    EXPECT_EQ(plan.plannedSpawn.characterGuid, 9001u);
    EXPECT_EQ(plan.plannedSpawn.zoneId, 1519u);
    EXPECT_EQ(plan.plannedSpawn.source, model::RosterEntrySource::AccountAlt);
    EXPECT_EQ(plan.plannedSpawn.possessionMode, model::PossessionMode::DirectControl);
}

TEST(SimplePartyRosterPlannerTest, RejectsAccountAltOwnedByAnotherAccount)
{
    SimplePartyRosterPlanner planner;

    model::RosterEntry accountAlt;
    accountAlt.rosterEntryId = 88;
    accountAlt.source = model::RosterEntrySource::AccountAlt;
    accountAlt.ownerAccountId = 13;
    accountAlt.characterGuid = 1234;
    accountAlt.controllableProfile.profile.botId = 777;

    std::vector<model::RosterEntry> rosterEntries = { accountAlt };

    model::PlayerRosterRequest request;
    request.requesterAccountId = 77;
    request.requestedRosterEntryId = 88;
    request.possessionMode = model::PossessionMode::Assisted;

    model::PlayerRosterContext context;
    context.playerZoneId = 12;
    context.partyMemberCount = 1;
    context.maxPartyMembers = 5;

    PartyRosterPlan plan = planner.BuildPlan(rosterEntries, request, context);

    EXPECT_FALSE(plan.isApproved);
    EXPECT_EQ(plan.failureReason, PartyRosterFailureReason::OwnershipMismatch);
    EXPECT_EQ(plan.plannedSpawn.rosterEntryId, 0u);
}
} // namespace planner
} // namespace living_world
