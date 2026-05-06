#pragma once

#include "integration/BotTalentPreferenceRepository.h"
#include "integration/BotTalentTemplateRepository.h"
#include "integration/AccountAltRuntimeRepository.h"
#include "model/BotTalentTemplate.h"

#include <cstdint>

class Player;

namespace living_world
{
namespace service
{
enum class TalentResetResult : std::uint8_t
{
    Ok = 0,
    InsufficientGold,
};

class BotTalentApplicator
{
public:
    BotTalentApplicator(
        integration::BotTalentTemplateRepository const& templateRepo,
        integration::BotTalentPreferenceRepository& preferenceRepo,
        integration::AccountAltRuntimeRepository const& altRuntimeRepo);

    // Spend all available free talent points on bot according to tmpl,
    // in priority order, stopping when points run out. Never resets.
    // Returns the number of points spent. Safe for all bot types.
    int ApplyTemplate(
        Player* bot,
        model::BotTalentTemplateRecord const& tmpl) const;

    // Full retall + apply. Generic bots get a free reset; account-alt bots
    // must afford the in-game gold cost — returns InsufficientGold and does
    // nothing if they cannot.
    TalentResetResult ReapplyTemplate(
        Player* bot,
        model::BotTalentTemplateRecord const& tmpl) const;

    // Look up the preferred template for sourceCharGuid (auto-detect if
    // templateId == 0), then call ApplyTemplate. Returns false if no
    // template is found.
    bool ApplyPreferredTemplate(
        Player* bot,
        std::uint64_t sourceCharGuid) const;

private:
    bool IsAccountAlt(Player* bot) const;

    integration::BotTalentTemplateRepository const& _templateRepo;
    integration::BotTalentPreferenceRepository& _preferenceRepo;
    integration::AccountAltRuntimeRepository const& _altRuntimeRepo;
};
} // namespace service
} // namespace living_world
