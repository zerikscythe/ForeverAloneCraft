#pragma once

#include "integration/AccountAltRuntimeRepository.h"

namespace living_world
{
namespace integration
{
class SqlAccountAltRuntimeRepository final
    : public AccountAltRuntimeRepository
{
public:
    std::optional<model::AccountAltRuntimeRecord> FindBySourceCharacter(
        std::uint32_t sourceAccountId,
        std::uint64_t sourceCharacterGuid) const override;

    std::optional<model::AccountAltRuntimeRecord> FindByCloneCharacter(
        std::uint64_t cloneCharacterGuid) const override;

    std::vector<model::AccountAltRuntimeRecord> ListRecoverableForAccount(
        std::uint32_t sourceAccountId) const override;

    void SaveRuntime(
        model::AccountAltRuntimeRecord const& runtime) override;

    void DeleteRuntime(std::uint64_t runtimeId) override;
};
} // namespace integration
} // namespace living_world
