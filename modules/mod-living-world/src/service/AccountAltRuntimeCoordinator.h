#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/BotAccountPoolRepository.h"
#include "integration/CharacterProgressSnapshotRepository.h"
#include "model/AccountAltRuntime.h"
#include "service/AccountAltRecoveryService.h"
#include "service/AccountAltRuntimeService.h"

#include <optional>
#include <string>

namespace living_world
{
namespace service
{
enum class AccountAltSpawnDecisionKind
{
    SpawnUsingReservedAccount,
    SpawnUsingPersistentClone,
    RecoveryRequired,
    ManualReviewRequired,
    Blocked
};

struct AccountAltSpawnDecision
{
    AccountAltSpawnDecisionKind kind = AccountAltSpawnDecisionKind::Blocked;
    std::optional<model::AccountAltRuntimeRecord> runtime;
    std::uint64_t spawnCharacterGuid = 0;
    std::uint32_t botAccountId = 0;
    std::string reason;
};

// Coordinates durable runtime lookup with conservative recovery gating before
// any account-alt bot login is queued.
class AccountAltRuntimeCoordinator
{
public:
    AccountAltRuntimeCoordinator(
        integration::AccountAltRuntimeRepository& runtimeRepository,
        integration::BotAccountPoolRepository& botAccountPoolRepository,
        integration::CharacterProgressSnapshotRepository const& snapshotRepository,
        AccountAltRecoveryService const& recoveryService);

    AccountAltSpawnDecision PlanSpawn(
        std::uint32_t sourceAccountId,
        std::uint64_t sourceCharacterGuid,
        std::uint64_t ownerCharacterGuid,
        std::string const& sourceCharacterName) const;

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::CharacterProgressSnapshotRepository const& _snapshotRepository;
    AccountAltRecoveryService const& _recoveryService;
    AccountAltRuntimeService _runtimeService;
};
} // namespace service
} // namespace living_world
