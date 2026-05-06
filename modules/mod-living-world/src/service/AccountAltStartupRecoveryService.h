#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/CharacterAchievementSyncRepository.h"
#include "integration/CharacterBankSyncRepository.h"
#include "integration/CharacterEquipmentSyncRepository.h"
#include "integration/CharacterInventorySyncRepository.h"
#include "integration/CharacterItemSnapshotRepository.h"
#include "integration/CharacterProgressSnapshotRepository.h"
#include "integration/CharacterProgressSyncRepository.h"
#include "integration/CharacterQuestSyncRepository.h"
#include "integration/CharacterReputationSyncRepository.h"
#include "integration/CharacterSkillSyncRepository.h"
#include "integration/CharacterSpellSyncRepository.h"
#include "service/AccountAltRecoveryService.h"
#include "service/AccountAltItemRecoveryService.h"

#include <cstdint>

namespace living_world
{
namespace service
{
struct AccountAltStartupRecoverySummary
{
    std::uint32_t scanned = 0;
    std::uint32_t recoveredSyncs = 0;
    std::uint32_t recoveredProgressSyncs = 0;
    std::uint32_t recoveredEquipmentSyncs = 0;
    std::uint32_t recoveredInventorySyncs = 0;
    std::uint32_t recoveredBankSyncs = 0;
    std::uint32_t pendingRecovery = 0;
    std::uint32_t manualReviewRequired = 0;
    std::uint32_t blocked = 0;
};

// Performs a lightweight owner-login recovery pass over persisted account-alt
// runtimes. It only executes writes for runtimes already marked as an
// interrupted sync state; all other clone-ahead cases are surfaced as pending
// recovery for the normal spawn path to handle later.
class AccountAltStartupRecoveryService
{
public:
    AccountAltStartupRecoveryService(
        integration::AccountAltRuntimeRepository& runtimeRepository,
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

    AccountAltStartupRecoverySummary RecoverForAccount(
        std::uint32_t sourceAccountId);

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
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
};
} // namespace service
} // namespace living_world
