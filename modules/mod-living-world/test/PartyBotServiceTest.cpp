#include "integration/AzerothWorldFacade.h"
#include "service/PartyBotService.h"
#include "planner/SimplePartyRosterPlanner.h"
#include "gtest/gtest.h"

#include <unordered_map>
#include <unordered_set>

namespace living_world
{
namespace service
{
namespace
{
// Test-only facade. Lets each case stage the world state the service
// should observe without touching real AzerothCore singletons.
class FakeAzerothWorldFacade : public integration::AzerothWorldFacade
{
public:
    void SetPlayerContext(
        std::uint64_t characterGuid,
        integration::PlayerWorldContext context)
    {
        _players[characterGuid] = std::move(context);
    }

    void SetCharacterOnline(std::uint64_t characterGuid, bool online)
    {
        if (online)
        {
            _onlineCharacters.insert(characterGuid);
        }
        else
        {
            _onlineCharacters.erase(characterGuid);
        }
    }

    std::optional<integration::PlayerWorldContext> GetPlayerContext(
        std::uint64_t characterGuid) const override
    {
        auto it = _players.find(characterGuid);
        if (it == _players.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::vector<integration::SpawnAnchor> GetSpawnAnchorsInZone(
        std::uint32_t /*zoneId*/) const override
    {
        return {};
    }

    bool IsCharacterOnline(std::uint64_t characterGuid) const override
    {
        return _onlineCharacters.count(characterGuid) > 0;
    }

private:
    std::unordered_map<std::uint64_t, integration::PlayerWorldContext> _players;
    std::unordered_set<std::uint64_t> _onlineCharacters;
};

integration::PlayerWorldContext BuildPlayerContext(
    std::uint64_t characterGuid, std::uint32_t accountId, std::uint32_t zoneId)
{
    integration::PlayerWorldContext ctx;
    ctx.identity.characterGuid = characterGuid;
    ctx.identity.accountId = accountId;
    ctx.position.zoneId = zoneId;
    ctx.isInWorld = true;
    ctx.canControlCompanions = true;
    ctx.party.maxPartyMembers = 5;
    return ctx;
}

model::RosterEntry BuildAccountAlt(
    std::uint64_t rosterEntryId,
    std::uint32_t ownerAccountId,
    std::uint64_t characterGuid)
{
    model::RosterEntry entry;
    entry.rosterEntryId = rosterEntryId;
    entry.source = model::RosterEntrySource::AccountAlt;
    entry.ownerAccountId = ownerAccountId;
    entry.characterGuid = characterGuid;
    entry.controllableProfile.profile.botId = 1234;
    entry.controllableProfile.canBePlayerControlled = true;
    entry.controllableProfile.canEarnProgression = true;
    return entry;
}
} // namespace

TEST(PartyBotServiceTest, RejectsWhenFacadeHasNoPlayerContext)
{
    FakeAzerothWorldFacade facade;
    planner::SimplePartyRosterPlanner plannerImpl;
    PartyBotService service(facade, plannerImpl);

    model::PlayerRosterRequest request;
    request.requesterCharacterGuid = 42;
    request.requesterAccountId = 7;
    request.requestedRosterEntryId = 1;

    PartyBotDispatchResult result = service.DispatchRosterRequest({}, request);

    EXPECT_FALSE(result.isApproved);
    EXPECT_EQ(result.failureReason,
              planner::PartyRosterFailureReason::RequesterUnavailable);
    EXPECT_TRUE(result.commitActions.empty());
}

TEST(PartyBotServiceTest, ApprovedRequestEmitsSpawnAttachAndStateActions)
{
    FakeAzerothWorldFacade facade;
    planner::SimplePartyRosterPlanner plannerImpl;
    PartyBotService service(facade, plannerImpl);

    auto ctx = BuildPlayerContext(42, 7, 1519);
    ctx.position.mapId = 0;
    ctx.position.x = 100.0f;
    ctx.position.y = 200.0f;
    ctx.position.z = 50.0f;
    facade.SetPlayerContext(42, ctx);

    model::RosterEntry alt = BuildAccountAlt(55, 7, 9001);

    model::PlayerRosterRequest request;
    request.requesterCharacterGuid = 42;
    request.requesterAccountId = 7;
    request.requestedRosterEntryId = 55;
    request.possessionMode = model::PossessionMode::DirectControl;
    request.spawnAtPlayerLocation = true;
    request.addToPlayerParty = true;

    PartyBotDispatchResult result =
        service.DispatchRosterRequest({ alt }, request);

    ASSERT_TRUE(result.isApproved);
    ASSERT_EQ(result.commitActions.size(), 3u);

    auto const* spawn = std::get_if<integration::SpawnRosterBodyAction>(
        &result.commitActions[0]);
    ASSERT_NE(spawn, nullptr);
    EXPECT_EQ(spawn->rosterEntryId, 55u);
    EXPECT_EQ(spawn->characterGuid, 9001u);
    EXPECT_EQ(spawn->position.zoneId, 1519u);
    EXPECT_FLOAT_EQ(spawn->position.x, 100.0f);
    EXPECT_EQ(spawn->possessionMode, model::PossessionMode::DirectControl);

    auto const* attach = std::get_if<integration::AttachToPartyAction>(
        &result.commitActions[1]);
    ASSERT_NE(attach, nullptr);
    EXPECT_EQ(attach->rosterEntryId, 55u);
    EXPECT_EQ(attach->leaderCharacterGuid, 42u);

    auto const* stateUpdate =
        std::get_if<integration::UpdateAbstractStateAction>(
            &result.commitActions[2]);
    ASSERT_NE(stateUpdate, nullptr);
    EXPECT_EQ(stateUpdate->newActivity, model::BotActivity::PartySupport);
    EXPECT_EQ(stateUpdate->newBoundZoneId, 1519u);
}

TEST(PartyBotServiceTest, RejectsAccountAltAlreadyOnline)
{
    FakeAzerothWorldFacade facade;
    planner::SimplePartyRosterPlanner plannerImpl;
    PartyBotService service(facade, plannerImpl);

    facade.SetPlayerContext(42, BuildPlayerContext(42, 7, 12));
    facade.SetCharacterOnline(9001, true);

    model::RosterEntry alt = BuildAccountAlt(55, 7, 9001);

    model::PlayerRosterRequest request;
    request.requesterCharacterGuid = 42;
    request.requesterAccountId = 7;
    request.requestedRosterEntryId = 55;

    PartyBotDispatchResult result =
        service.DispatchRosterRequest({ alt }, request);

    EXPECT_FALSE(result.isApproved);
    EXPECT_EQ(result.failureReason,
              planner::PartyRosterFailureReason::RosterEntryAlreadySummoned);
    EXPECT_TRUE(result.commitActions.empty());
}

TEST(PartyBotServiceTest, DeadPlayerCannotControlCompanions)
{
    FakeAzerothWorldFacade facade;
    planner::SimplePartyRosterPlanner plannerImpl;
    PartyBotService service(facade, plannerImpl);

    auto ctx = BuildPlayerContext(42, 7, 12);
    ctx.isDead = true;
    facade.SetPlayerContext(42, ctx);

    model::RosterEntry alt = BuildAccountAlt(55, 7, 9001);

    model::PlayerRosterRequest request;
    request.requesterCharacterGuid = 42;
    request.requesterAccountId = 7;
    request.requestedRosterEntryId = 55;

    PartyBotDispatchResult result =
        service.DispatchRosterRequest({ alt }, request);

    EXPECT_FALSE(result.isApproved);
    EXPECT_EQ(result.failureReason,
              planner::PartyRosterFailureReason::RequesterUnavailable);
}
} // namespace service
} // namespace living_world
