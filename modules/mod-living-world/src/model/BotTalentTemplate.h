#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace living_world
{
namespace model
{
struct BotTalentTemplateEntry
{
    std::uint64_t entryId    = 0;
    std::uint16_t talentId   = 0;   // DBC TalentEntry::TalentID
    std::uint16_t priority   = 0;   // fill order: lower = fill first
    std::uint8_t  desiredRank = 0;  // total ranks to invest (1-5)
};

struct BotTalentTemplateRecord
{
    std::uint64_t templateId = 0;
    std::string   specKey;
    std::uint8_t  classId    = 0;
    std::string   displayName;
    std::string   variantKey;
    std::string   description;
    std::vector<BotTalentTemplateEntry> entries; // sorted by priority ASC
};

struct BotTalentPreference
{
    std::uint64_t sourceCharacterGuid = 0;
    std::uint64_t templateId          = 0;  // 0 = auto-detect from spec+class
    bool          autoApplyOnLevel    = true;
};
} // namespace model
} // namespace living_world
