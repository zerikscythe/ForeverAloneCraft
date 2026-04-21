#include "service/PartyBotService.h"

#include "model/PlayerRosterContext.h"
#include "model/RosterEntry.h"

namespace living_world
{
namespace service
{
namespace
{
// The planner operates on PlayerRosterContext; the facade produces a richer
// PlayerWorldContext. This translation keeps the planner signature stable
// and prevents facade-specific fields from leaking into planner logic.
model::PlayerRosterContext BuildPlannerContext(
    integration::PlayerWorldContext const& worldContext)
{
    model::PlayerRosterContext plannerContext;
    plannerContext.playerZoneId = worldContext.position.zoneId;
    plannerContext.partyMemberCount =
        static_cast<std::uint32_t>(worldContext.party.members.size());
    plannerContext.maxPartyMembers = worldContext.party.maxPartyMembers;
    plannerContext.playerIsInWorld = worldContext.isInWorld;
    plannerContext.playerCanControlCompanions =
        worldContext.canControlCompanions &&
        !worldContext.isDead;
    return plannerContext;
}

PartyBotDispatchResult BuildRejection(
    planner::PartyRosterFailureReason reason)
{
    PartyBotDispatchResult result;
    result.isApproved = false;
    result.failureReason = reason;
    result.plan.failureReason = reason;
    return result;
}

// Convert an approved planner plan into the explicit action records a
// future commit layer will execute. New action kinds (profession swap,
// follow-mode change, etc.) should be emitted here rather than taught to
// the planner.
std::vector<integration::WorldCommitAction> BuildCommitActions(
    planner::PartyRosterPlan const& plan,
    integration::PlayerWorldContext const& worldContext)
{
    std::vector<integration::WorldCommitAction> actions;
    if (!plan.isApproved)
    {
        return actions;
    }

    integration::SpawnRosterBodyAction spawn;
    spawn.rosterEntryId = plan.plannedSpawn.rosterEntryId;
    spawn.botId = plan.plannedSpawn.botId;
    spawn.characterGuid = plan.plannedSpawn.characterGuid;
    spawn.source = plan.plannedSpawn.source;
    spawn.possessionMode = plan.plannedSpawn.possessionMode;
    spawn.position = plan.shouldSpawnAtPlayer
        ? worldContext.position
        : integration::WorldPosition{};
    actions.emplace_back(spawn);

    if (plan.shouldJoinPlayerParty)
    {
        integration::AttachToPartyAction attach;
        attach.rosterEntryId = plan.plannedSpawn.rosterEntryId;
        attach.leaderCharacterGuid = worldContext.identity.characterGuid;
        actions.emplace_back(attach);
    }

    integration::UpdateAbstractStateAction state;
    state.botId = plan.plannedSpawn.botId;
    state.newActivity = model::BotActivity::PartySupport;
    state.newBoundZoneId = plan.plannedSpawn.zoneId;
    actions.emplace_back(state);

    return actions;
}
} // namespace

PartyBotService::PartyBotService(
    integration::AzerothWorldFacade const& facade,
    integration::RosterRepository const& rosterRepository,
    planner::PartyRosterPlanner const& plannerImpl)
    : _facade(facade),
      _rosterRepository(rosterRepository),
      _planner(plannerImpl)
{
}

PartyBotDispatchResult PartyBotService::DispatchRosterRequest(
    model::PlayerRosterRequest const& request) const
{
    std::optional<integration::PlayerWorldContext> worldContext =
        _facade.GetPlayerContext(request.requesterCharacterGuid);

    if (!worldContext)
    {
        return BuildRejection(
            planner::PartyRosterFailureReason::RequesterUnavailable);
    }

    std::optional<model::RosterEntry> entry =
        _rosterRepository.FindRosterEntryForAccount(
            request.requesterAccountId, request.requestedRosterEntryId);

    if (!entry)
    {
        return BuildRejection(
            planner::PartyRosterFailureReason::RosterEntryNotFound);
    }

    // Enforce the "alt already online" rule before the planner runs. The
    // planner is intentionally ignorant of live online state so it stays
    // testable with pure data inputs.
    if (entry->source == model::RosterEntrySource::AccountAlt &&
        _facade.IsCharacterOnline(entry->characterGuid))
    {
        return BuildRejection(
            planner::PartyRosterFailureReason::RosterEntryAlreadySummoned);
    }

    model::PlayerRosterContext plannerContext =
        BuildPlannerContext(*worldContext);

    std::vector<model::RosterEntry> plannerInput { *entry };
    planner::PartyRosterPlan plan =
        _planner.BuildPlan(plannerInput, request, plannerContext);

    PartyBotDispatchResult result;
    result.plan = plan;
    result.isApproved = plan.isApproved;
    result.failureReason = plan.failureReason;
    if (plan.isApproved)
    {
        result.commitActions = BuildCommitActions(plan, *worldContext);
    }
    return result;
}
} // namespace service
} // namespace living_world
