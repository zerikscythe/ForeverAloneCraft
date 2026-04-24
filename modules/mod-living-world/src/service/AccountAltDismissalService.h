#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/CharacterBankSyncRepository.h"
#include "integration/CharacterEquipmentSyncRepository.h"
#include "integration/CharacterInventorySyncRepository.h"
#include "integration/CharacterItemSnapshotRepository.h"
#include "integration/CharacterNameLeaseRepository.h"
#include "integration/CharacterProgressSnapshotRepository.h"
#include "integration/CharacterProgressSyncRepository.h"
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
    bool equipmentSynced = false;
    bool inventorySynced = false;
    bool bankSynced = false;
    bool namesRestored = false;
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
    AccountAltRecoveryService const& _recoveryService;
    AccountAltItemRecoveryOptions _itemRecoveryOptions;
};
} // namespace service
} // namespace living_world
