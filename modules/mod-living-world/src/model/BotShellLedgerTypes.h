#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace living_world
{
namespace model
{

struct BotDisplayLoadoutRecord
{
    std::uint32_t identityId = 0;
    std::string   displayLoadoutKey;
    std::uint32_t helmItemId = 0;
    std::uint32_t shoulderItemId = 0;
    std::uint32_t shirtItemId = 0;
    std::uint32_t chestItemId = 0;
    std::uint32_t waistItemId = 0;
    std::uint32_t legsItemId = 0;
    std::uint32_t feetItemId = 0;
    std::uint32_t wristItemId = 0;
    std::uint32_t handsItemId = 0;
    std::uint32_t backItemId = 0;
    std::uint32_t tabardItemId = 0;
    std::uint32_t mainHandItemId = 0;
    std::uint32_t offHandItemId = 0;
    std::uint32_t rangedItemId = 0;
    bool          hideHelm = false;
    bool          hideCloak = false;
};

struct BotRuntimeSnapshotRecord
{
    std::uint32_t identityId = 0;
    std::uint16_t mapId = 0;
    std::uint32_t zoneId = 0;
    float         x = 0.0f;
    float         y = 0.0f;
    float         z = 0.0f;
    float         o = 0.0f;
    std::string   runtimeState;
    std::string   runtimeDetail;
    std::string   lastTaskFamily;
    std::string   lastTaskActivityKey;
    std::uint32_t lastTaskTargetZoneId = 0;
    std::string   homeBindPointKey;
    std::uint8_t  genericPotionCharges = 0;
};

struct BotShellRuntimeRecord
{
    std::uint32_t identityId = 0;
    std::uint32_t shellAccountId = 0;
    std::uint64_t shellCharacterGuid = 0;
    bool          isMaterialized = false;
    std::uint32_t shellStateVersion = 0;
    std::string   leasedAt;
    std::string   lastSyncAt;
    std::string   lastDismissedAt;
};

struct BotRebuildLogEntry
{
    std::uint64_t entryId = 0;
    std::uint32_t identityId = 0;
    std::optional<std::uint32_t> shellAccountId;
    std::optional<std::uint64_t> shellCharacterGuid;
    std::string   rebuildReason;
    std::uint8_t  level = 1;
    std::uint8_t  gearTier = 1;
    std::string   specKey;
    std::string   loadoutKey;
    std::string   displayLoadoutKey;
    std::string   doctrineProfileKey;
    std::uint32_t shellStateVersion = 0;
    std::string   notes;
    std::string   rebuiltAt;
};

} // namespace model
} // namespace living_world
