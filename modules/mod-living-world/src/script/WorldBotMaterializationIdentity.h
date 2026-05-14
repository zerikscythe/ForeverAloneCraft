#pragma once

#include "integration/SqlBotIdentityRepository.h"

#include <optional>

namespace living_world
{
namespace script
{

inline integration::BotIdentityRecord SelectWorldBotMaterializationIdentity(
    integration::BotIdentityRecord const& cachedIdentity,
    std::optional<integration::BotIdentityRecord> const& refreshedIdentity)
{
    if (!refreshedIdentity)
        return cachedIdentity;

    if (refreshedIdentity->id != cachedIdentity.id)
        return cachedIdentity;

    if (refreshedIdentity->isRetired)
        return cachedIdentity;

    return *refreshedIdentity;
}

} // namespace script
} // namespace living_world