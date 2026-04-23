#pragma once

#include "integration/AccountAltRuntimeRepository.h"
#include "integration/CharacterProgressSnapshotRepository.h"
#include "integration/CharacterProgressSyncRepository.h"
#include "service/AccountAltRecoveryService.h"

#include <cstdint>

namespace living_world
{
namespace service
{
struct AccountAltStartupRecoverySummary
{
    std::uint32_t scanned = 0;
    std::uint32_t recoveredSyncs = 0;
    std::uint32_t pendingRecovery = 0;
    std::uint32_t manualReviewRequired = 0;
    std::uint32_t blocked = 0;
};

// Performs a lightweight owner-login recovery pass over persisted account-alt
// runtimes. It only executes writes for runtimes already marked SyncingBack;
// all other clone-ahead cases are surfaced as pending recovery for the normal
// spawn path to handle later.
class AccountAltStartupRecoveryService
{
public:
    AccountAltStartupRecoveryService(
        integration::AccountAltRuntimeRepository& runtimeRepository,
        integration::CharacterProgressSnapshotRepository const& snapshotRepository,
        integration::CharacterProgressSyncRepository& syncRepository,
        AccountAltRecoveryService const& recoveryService);

    AccountAltStartupRecoverySummary RecoverForAccount(
        std::uint32_t sourceAccountId);

private:
    integration::AccountAltRuntimeRepository& _runtimeRepository;
    integration::CharacterProgressSnapshotRepository const& _snapshotRepository;
    integration::CharacterProgressSyncRepository& _syncRepository;
    AccountAltRecoveryService const& _recoveryService;
};
} // namespace service
} // namespace living_world
