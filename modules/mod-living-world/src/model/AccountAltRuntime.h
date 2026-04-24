#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace living_world
{
namespace model
{
// Durable lifecycle for a player-owned alt that is being represented by a
// bot-owned runtime clone. The runtime record is intentionally separate
// from roster ownership so crash recovery can reason about partially
// prepared clones without re-cloning stale source data over newer progress.
enum class AccountAltRuntimeState
{
    PreparingClone,
    Active,
    SyncingBack,
    Recovering,
    Failed,
    SyncingEquipment,
    SyncingInventory,
    SyncingBank
};

enum class AccountAltRuntimeDecisionKind
{
    PrepareNewClone,
    ReuseActiveClone,
    RecoverCloneBeforeUse,
    BlockedNoBotAccount,
    BlockedRuntimeFailed
};

enum class AccountAltSyncDomain
{
    Experience,
    Money,
    Inventory,
    Bank,
    Equipment,
    Reputation,
    Quests,
    Mail
};

enum class AccountAltRecoveryPlanKind
{
    NoAction,
    ReuseClone,
    SyncCloneToSource,
    ManualReview,
    Blocked
};

enum class AccountAltItemRecoveryPlanKind
{
    NoAction,
    SyncEquipmentToSource,
    SyncBagDomainsToSource,
    ManualReview,
    Blocked
};

struct CharacterProgressSnapshot
{
    std::uint8_t level = 1;
    std::uint32_t experience = 0;
    std::uint32_t money = 0;
};

struct BotAccountLease
{
    std::uint32_t accountId = 0;
    std::string accountName;
};

struct AccountAltRuntimeRecord
{
    std::uint64_t runtimeId = 0;
    std::uint32_t sourceAccountId = 0;
    std::uint64_t sourceCharacterGuid = 0;
    std::uint64_t ownerCharacterGuid = 0;
    std::uint32_t cloneAccountId = 0;
    std::uint64_t cloneCharacterGuid = 0;
    std::string sourceCharacterName;
    std::string reservedSourceCharacterName;
    std::string cloneCharacterName;
    AccountAltRuntimeState state = AccountAltRuntimeState::PreparingClone;
    CharacterProgressSnapshot sourceSnapshot;
    CharacterProgressSnapshot cloneSnapshot;
};

struct AccountAltSanityCheckResult
{
    bool passed = false;
    bool bagContainersChanged = false;
    std::vector<AccountAltSyncDomain> safeDomains;
    std::vector<std::string> failures;
};

struct AccountAltRuntimeRequest
{
    std::uint32_t sourceAccountId = 0;
    std::uint64_t sourceCharacterGuid = 0;
    std::uint64_t ownerCharacterGuid = 0;
    std::string sourceCharacterName;
    CharacterProgressSnapshot sourceSnapshot;
};

struct AccountAltRuntimeDecision
{
    AccountAltRuntimeDecisionKind kind =
        AccountAltRuntimeDecisionKind::BlockedNoBotAccount;
    std::optional<AccountAltRuntimeRecord> runtime;
    bool cloneProgressIsAuthoritative = false;
};

struct AccountAltRecoveryPlan
{
    AccountAltRecoveryPlanKind kind = AccountAltRecoveryPlanKind::NoAction;
    AccountAltRuntimeRecord runtime;
    bool cloneProgressIsAhead = false;
    bool requiresSanityCheck = false;
    std::vector<AccountAltSyncDomain> domainsToSync;
    std::string reason;
};

struct AccountAltItemRecoveryPlan
{
    AccountAltItemRecoveryPlanKind kind =
        AccountAltItemRecoveryPlanKind::NoAction;
    std::vector<AccountAltSyncDomain> domainsToSync;
    std::string reason;
};
} // namespace model
} // namespace living_world
