#include "planner/SimplePartyRosterPlanner.h"

namespace living_world
{
namespace planner
{
namespace
{
PartyRosterPlan BuildRejectedPlan(PartyRosterFailureReason reason)
{
    PartyRosterPlan plan;
    plan.failureReason = reason;
    return plan;
}
} // namespace

PartyRosterPlan SimplePartyRosterPlanner::BuildPlan(
    std::vector<model::RosterEntry> const& rosterEntries,
    model::PlayerRosterRequest const& request,
    model::PlayerRosterContext const& context) const
{
    if (!context.playerIsInWorld || !context.playerCanControlCompanions)
    {
        return BuildRejectedPlan(PartyRosterFailureReason::RequesterUnavailable);
    }

    model::RosterEntry const* selectedEntry = nullptr;
    for (model::RosterEntry const& entry : rosterEntries)
    {
        if (entry.rosterEntryId == request.requestedRosterEntryId)
        {
            selectedEntry = &entry;
            break;
        }
    }

    if (!selectedEntry)
    {
        return BuildRejectedPlan(PartyRosterFailureReason::RosterEntryNotFound);
    }

    if (!selectedEntry->isEnabled)
    {
        return BuildRejectedPlan(PartyRosterFailureReason::RosterEntryDisabled);
    }

    if (selectedEntry->isAlreadySummoned)
    {
        return BuildRejectedPlan(PartyRosterFailureReason::RosterEntryAlreadySummoned);
    }

    if (request.addToPlayerParty && context.partyMemberCount >= context.maxPartyMembers)
    {
        return BuildRejectedPlan(PartyRosterFailureReason::PartyFull);
    }

    if (selectedEntry->source == model::RosterEntrySource::AccountAlt &&
        selectedEntry->ownerAccountId != request.requesterAccountId)
    {
        return BuildRejectedPlan(PartyRosterFailureReason::OwnershipMismatch);
    }

    if (request.possessionMode == model::PossessionMode::DirectControl &&
        !selectedEntry->controllableProfile.canBePlayerControlled)
    {
        return BuildRejectedPlan(PartyRosterFailureReason::DirectControlNotSupported);
    }

    PartyRosterPlan plan;
    plan.isApproved = true;
    plan.shouldSpawnAtPlayer = request.spawnAtPlayerLocation;
    plan.shouldJoinPlayerParty = request.addToPlayerParty;
    plan.progressionShouldAccrueToControlledCharacter = selectedEntry->controllableProfile.canEarnProgression;
    plan.plannedSpawn.rosterEntryId = selectedEntry->rosterEntryId;
    plan.plannedSpawn.botId = selectedEntry->controllableProfile.profile.botId;
    plan.plannedSpawn.characterGuid = selectedEntry->characterGuid;
    plan.plannedSpawn.zoneId = context.playerZoneId;
    plan.plannedSpawn.source = selectedEntry->source;
    plan.plannedSpawn.possessionMode = request.possessionMode;
    return plan;
}
} // namespace planner
} // namespace living_world

