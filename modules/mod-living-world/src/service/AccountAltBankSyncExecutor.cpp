#include "service/AccountAltBankSyncExecutor.h"

namespace living_world
{
namespace service
{
AccountAltBankSyncExecutor::AccountAltBankSyncExecutor(
    integration::AccountAltRuntimeRepository& runtimeRepository,
    integration::CharacterBankSyncRepository& bankSyncRepository)
    : _runtimeRepository(runtimeRepository),
      _bankSyncRepository(bankSyncRepository)
{
}

bool AccountAltBankSyncExecutor::Execute(
    model::AccountAltRuntimeRecord const& runtime,
    model::CharacterItemSnapshot const& sourceSnapshot,
    model::CharacterItemSnapshot const& cloneSnapshot)
{
    model::AccountAltRuntimeRecord mutableRuntime = runtime;
    mutableRuntime.state = model::AccountAltRuntimeState::SyncingBank;
    _runtimeRepository.SaveRuntime(mutableRuntime);

    if (!_bankSyncRepository.SyncBankToCharacter(
            runtime.sourceCharacterGuid,
            sourceSnapshot,
            runtime.cloneCharacterGuid,
            cloneSnapshot))
    {
        return false;
    }

    mutableRuntime.state = model::AccountAltRuntimeState::Active;
    _runtimeRepository.SaveRuntime(mutableRuntime);
    return true;
}
} // namespace service
} // namespace living_world
