#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/CharacterAchievementSyncRepository.h"
#include "integration/CharacterBankSyncRepository.h"
#include "integration/CharacterEquipmentSyncRepository.h"
#include "integration/CharacterInventorySyncRepository.h"
#include "integration/CharacterItemSnapshotRepository.h"
#include "integration/CharacterNameLeaseRepository.h"
#include "integration/CharacterProgressSnapshotRepository.h"
#include "integration/CharacterProgressSyncRepository.h"
#include "integration/CharacterQuestSyncRepository.h"
#include "integration/CharacterReputationSyncRepository.h"
#include "service/AccountAltRecoveryService.h"
#include "service/AccountAltItemRecoveryService.h"

#include <cstdint>
#include <string>

namespace living_world
{
namespace service
{
struct AccountAltDismissalSummary
{
    bool runtimeFound = false;
    bool progressSynced = false;
    bool reputationSynced = false;
    bool questsSynced = false;
    bool achievementsSynced = false;
    bool equipmentSynced = false;
    bool inventorySynced = false;
    bool bankSynced = false;
    bool namesRestored = false;
    bool runtimeRetired = false;
    bool manualReviewRequired = false;
    bool blocked = false;
    std::string reason;
};

class AccountAltDismissalService
{
public:
    AccountAltDismissalService(
        integration::AccountAltRuntimeRepository& runtimeRepository,
        integration::CharacterItemSnapshotRepository const& itemSnapshotRepository,
        integration::CharacterInventorySyncRepository& inventorySyncRepository,
        integration::CharacterBankSyncRepository& bankSyncRepository,
        integration::CharacterEquipmentSyncRepository& equipmentSyncRepository,
        integration::CharacterNameLeaseRepository& nameLeaseRepository,
        integration::CharacterProgressSnapshotRepository const& snapshotRepository,
        integration::CharacterProgressSyncRepository& syncRepository,
        integration::CharacterReputationSyncRepository& reputationSyncRepository,
        integration::CharacterQuestSyncRepository& questSyncRepository,
        integration::CharacterAchievementSyncRepository& achievementSyncRepository,
        AccountAltRecoveryService const& recoveryService,
        AccountAltItemRecoveryOptions itemRecoveryOptions = {});

    AccountAltDismissalSummary DismissClone(
        std::uint64_t cloneCharacterGuid) const;

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::CharacterItemSnapshotRepository const& _itemSnapshotRepository;
    integration::CharacterInventorySyncRepository& _inventorySyncRepository;
    integration::CharacterBankSyncRepository& _bankSyncRepository;
    integration::CharacterEquipmentSyncRepository& _equipmentSyncRepository;
    integration::CharacterNameLeaseRepository& _nameLeaseRepository;
    integration::CharacterProgressSnapshotRepository const& _snapshotRepository;
    integration::CharacterProgressSyncRepository& _syncRepository;
    integration::CharacterReputationSyncRepository& _reputationSyncRepository;
    integration::CharacterQuestSyncRepository& _questSyncRepository;
    integration::CharacterAchievementSyncRepository& _achievementSyncRepository;
    AccountAltRecoveryService const& _recoveryService;
    AccountAltItemRecoveryOptions _itemRecoveryOptions;
};
} // namespace service
} // namespace living_world
