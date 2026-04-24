#include "service/AccountAltRuntimeService.h"

#include <algorithm>

namespace living_world
{
namespace service
{
namespace
{
bool CloneProgressIsAhead(
    model::CharacterProgressSnapshot const& source,
    model::CharacterProgressSnapshot const& clone)
{
    if (clone.level != source.level)
    {
        return clone.level > source.level;
    }

    return clone.experience > source.experience;
}

std::string BuildReservedSourceName(std::uint64_t sourceCharacterGuid)
{
    // This is the parked hidden name used to move the offline source alt out
    // of the way while the live clone leases the real visible character name.
    // Mix position * 7 (prime stride) into each letter choice so that when
    // sourceCharacterGuid encodes to 0 in the high digits the trailing
    // characters still vary — preventing long runs of the same letter, which
    // WoW's CheckPlayerName rejects.
    std::string name = "Lw";
    std::uint64_t value = sourceCharacterGuid;
    std::uint64_t pos = 0;
    while (name.size() < 12)
    {
        name.push_back(static_cast<char>('a' + (value + pos * 7) % 26));
        value /= 26;
        ++pos;
    }
    return name;
}

model::AccountAltRuntimeDecision BuildExistingRuntimeDecision(
    model::AccountAltRuntimeRecord runtime,
    model::CharacterProgressSnapshot const& currentSourceSnapshot)
{
    model::AccountAltRuntimeDecision decision;
    decision.runtime = runtime;
    decision.cloneProgressIsAuthoritative =
        CloneProgressIsAhead(currentSourceSnapshot, runtime.cloneSnapshot);

    switch (runtime.state)
    {
        case model::AccountAltRuntimeState::Active:
            decision.kind =
                model::AccountAltRuntimeDecisionKind::ReuseActiveClone;
            break;
        case model::AccountAltRuntimeState::Failed:
            decision.kind =
                model::AccountAltRuntimeDecisionKind::BlockedRuntimeFailed;
            break;
        case model::AccountAltRuntimeState::PreparingClone:
        case model::AccountAltRuntimeState::Recovering:
        case model::AccountAltRuntimeState::SyncingBack:
        case model::AccountAltRuntimeState::SyncingEquipment:
        case model::AccountAltRuntimeState::SyncingInventory:
        case model::AccountAltRuntimeState::SyncingBank:
            decision.kind =
                model::AccountAltRuntimeDecisionKind::RecoverCloneBeforeUse;
            break;
    }

    return decision;
}
} // namespace

AccountAltRuntimeService::AccountAltRuntimeService(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::BotAccountPoolRepository& botAccountPool)
    : _runtimeRepository(runtimeRepository),
      _botAccountPool(botAccountPool)
{
}

model::AccountAltRuntimeDecision AccountAltRuntimeService::PrepareRuntime(
    model::AccountAltRuntimeRequest const& request) const
{
    std::optional<model::AccountAltRuntimeRecord> existing =
        _runtimeRepository.FindBySourceCharacter(
            request.sourceAccountId, request.sourceCharacterGuid);
    if (existing)
    {
        return BuildExistingRuntimeDecision(
            *existing, request.sourceSnapshot);
    }

    std::optional<model::BotAccountLease> lease =
        _botAccountPool.ReserveAccountForSource(
            request.sourceAccountId, request.sourceCharacterGuid);
    if (!lease)
    {
        model::AccountAltRuntimeDecision blocked;
        blocked.kind =
            model::AccountAltRuntimeDecisionKind::BlockedNoBotAccount;
        return blocked;
    }

    model::AccountAltRuntimeRecord runtime;
    runtime.sourceAccountId = request.sourceAccountId;
    runtime.sourceCharacterGuid = request.sourceCharacterGuid;
    runtime.ownerCharacterGuid = request.ownerCharacterGuid;
    runtime.cloneAccountId = lease->accountId;
    runtime.sourceCharacterName = request.sourceCharacterName;
    runtime.reservedSourceCharacterName =
        BuildReservedSourceName(request.sourceCharacterGuid);
    runtime.cloneCharacterName = request.sourceCharacterName;
    runtime.state = model::AccountAltRuntimeState::PreparingClone;
    runtime.sourceSnapshot = request.sourceSnapshot;
    runtime.cloneSnapshot = request.sourceSnapshot;

    _runtimeRepository.SaveRuntime(runtime);

    model::AccountAltRuntimeDecision decision;
    decision.kind = model::AccountAltRuntimeDecisionKind::PrepareNewClone;
    decision.runtime = runtime;
    return decision;
}
} // namespace service
} // namespace living_world
