#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/BotAccountPoolRepository.h"
#include "integration/CharacterCloneStateGateway.h"
#include "integration/CharacterAchievementSyncRepository.h"
#include "integration/CharacterBankSyncRepository.h"
#include "integration/CharacterCloneMaterializer.h"
#include "integration/CharacterEquipmentSyncRepository.h"
#include "integration/CharacterInventorySyncRepository.h"
#include "integration/CharacterItemSnapshotRepository.h"
#include "integration/CharacterProgressSnapshotRepository.h"
#include "integration/CharacterProgressSyncRepository.h"
#include "integration/CharacterQuestSyncRepository.h"
#include "integration/CharacterReputationSyncRepository.h"
#include "integration/CharacterSkillSyncRepository.h"
#include "integration/CharacterSpellSyncRepository.h"
#include "model/AccountAltRuntime.h"
#include "service/AccountAltItemRecoveryService.h"
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
        integration::CharacterCloneStateGateway const& cloneStateGateway,
        integration::CharacterItemSnapshotRepository const& itemSnapshotRepository,
        integration::CharacterInventorySyncRepository& inventorySyncRepository,
        integration::CharacterBankSyncRepository& bankSyncRepository,
        integration::CharacterEquipmentSyncRepository& equipmentSyncRepository,
        integration::CharacterProgressSnapshotRepository const& snapshotRepository,
        integration::CharacterProgressSyncRepository& syncRepository,
        integration::CharacterReputationSyncRepository& reputationSyncRepository,
        integration::CharacterQuestSyncRepository& questSyncRepository,
        integration::CharacterAchievementSyncRepository& achievementSyncRepository,
        integration::CharacterSkillSyncRepository& skillSyncRepository,
        integration::CharacterSpellSyncRepository& spellSyncRepository,
        AccountAltRecoveryService const& recoveryService,
        AccountAltItemRecoveryOptions itemRecoveryOptions = {});

    AccountAltSpawnDecision PlanSpawn(
        std::uint32_t sourceAccountId,
        std::uint64_t sourceCharacterGuid,
        std::uint64_t ownerCharacterGuid,
        std::string const& sourceCharacterName) const;

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::CharacterCloneMaterializer& _cloneMaterializer;
    integration::CharacterCloneStateGateway const& _cloneStateGateway;
    integration::CharacterItemSnapshotRepository const& _itemSnapshotRepository;
    integration::CharacterInventorySyncRepository& _inventorySyncRepository;
    integration::CharacterBankSyncRepository& _bankSyncRepository;
    integration::CharacterEquipmentSyncRepository& _equipmentSyncRepository;
    integration::CharacterProgressSnapshotRepository const& _snapshotRepository;
    integration::CharacterProgressSyncRepository& _syncRepository;
    integration::CharacterReputationSyncRepository& _reputationSyncRepository;
    integration::CharacterQuestSyncRepository& _questSyncRepository;
    integration::CharacterAchievementSyncRepository& _achievementSyncRepository;
    integration::CharacterSkillSyncRepository& _skillSyncRepository;
    integration::CharacterSpellSyncRepository& _spellSyncRepository;
    AccountAltRecoveryService const& _recoveryService;
    AccountAltItemRecoveryOptions _itemRecoveryOptions;
    AccountAltRuntimeService _runtimeService;
};
} // namespace service
} // namespace living_world
