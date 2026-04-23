#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/BotAccountPoolRepository.h"
#include "integration/CharacterCloneMaterializer.h"
#include "integration/CharacterEquipmentSyncRepository.h"
#include "integration/CharacterItemSnapshotRepository.h"
#include "integration/CharacterProgressSnapshotRepository.h"
#include "integration/CharacterProgressSyncRepository.h"
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
        integration::CharacterCloneMaterializer& cloneMaterializer,
        integration::CharacterItemSnapshotRepository const& itemSnapshotRepository,
        integration::CharacterEquipmentSyncRepository& equipmentSyncRepository,
        integration::CharacterProgressSnapshotRepository const& snapshotRepository,
        integration::CharacterProgressSyncRepository& syncRepository,
        AccountAltRecoveryService const& recoveryService);

    AccountAltSpawnDecision PlanSpawn(
        std::uint32_t sourceAccountId,
        std::uint64_t sourceCharacterGuid,
        std::uint64_t ownerCharacterGuid,
        std::string const& sourceCharacterName) const;

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::CharacterCloneMaterializer& _cloneMaterializer;
    integration::CharacterItemSnapshotRepository const& _itemSnapshotRepository;
    integration::CharacterEquipmentSyncRepository& _equipmentSyncRepository;
    integration::CharacterProgressSnapshotRepository const& _snapshotRepository;
    integration::CharacterProgressSyncRepository& _syncRepository;
    AccountAltRecoveryService const& _recoveryService;
    AccountAltRuntimeService _runtimeService;
};
} // namespace service
} // namespace living_world
