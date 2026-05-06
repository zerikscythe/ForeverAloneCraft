#include "service/BotTalentApplicator.h"
#include "service/SimpleBotCombatSpecRoleResolver.h"

#include "DataStores/DBCStores.h"
#include "DataStores/DBCStructure.h"
#include "Player.h"

#include <algorithm>

namespace living_world
{
namespace service
{
BotTalentApplicator::BotTalentApplicator(
    integration::BotTalentTemplateRepository const& templateRepo,
    integration::BotTalentPreferenceRepository& preferenceRepo,
    integration::AccountAltRuntimeRepository const& altRuntimeRepo)
    : _templateRepo(templateRepo),
      _preferenceRepo(preferenceRepo),
      _altRuntimeRepo(altRuntimeRepo)
{
}

bool BotTalentApplicator::IsAccountAlt(Player* bot) const
{
    return _altRuntimeRepo.FindByCloneCharacter(
        bot->GetGUID().GetCounter()).has_value();
}

int BotTalentApplicator::ApplyTemplate(
    Player* bot,
    model::BotTalentTemplateRecord const& tmpl) const
{
    int totalSpent = 0;
    for (model::BotTalentTemplateEntry const& entry : tmpl.entries)
    {
        if (bot->GetFreeTalentPoints() == 0)
            break;

        TalentEntry const* talent = sTalentStore.LookupEntry(entry.talentId);
        if (!talent)
            continue;

        // Count ranks the bot already has in this talent.
        std::uint8_t maxRank = 0;
        for (std::uint32_t spellId : talent->RankID)
            if (spellId) ++maxRank;

        std::uint8_t currentRank = 0;
        for (std::uint8_t r = 0; r < maxRank; ++r)
            if (bot->HasSpell(talent->RankID[r]))
                currentRank = r + 1;

        std::uint8_t desiredRank =
            std::min(entry.desiredRank, maxRank);
        if (currentRank >= desiredRank)
            continue;

        std::uint8_t ranksNeeded =
            static_cast<std::uint8_t>(desiredRank - currentRank);
        std::uint8_t ranksToAdd = static_cast<std::uint8_t>(
            std::min<std::uint32_t>(ranksNeeded, bot->GetFreeTalentPoints()));

        for (std::uint8_t r = 0; r < ranksToAdd; ++r)
            bot->LearnTalent(entry.talentId, currentRank + r);

        totalSpent += ranksToAdd;
    }
    return totalSpent;
}

TalentResetResult BotTalentApplicator::ReapplyTemplate(
    Player* bot,
    model::BotTalentTemplateRecord const& tmpl) const
{
    if (IsAccountAlt(bot))
    {
        std::uint32_t cost = bot->resetTalentsCost();
        if (bot->GetMoney() < cost)
            return TalentResetResult::InsufficientGold;
        bot->resetTalents(false);
    }
    else
    {
        bot->resetTalents(true);
    }

    ApplyTemplate(bot, tmpl);
    return TalentResetResult::Ok;
}

bool BotTalentApplicator::ApplyPreferredTemplate(
    Player* bot,
    std::uint64_t sourceCharGuid) const
{
    std::optional<model::BotTalentPreference> pref =
        _preferenceRepo.GetPreference(sourceCharGuid);

    std::uint64_t templateId = pref ? pref->templateId : 0;

    std::optional<model::BotTalentTemplateRecord> tmpl;
    if (templateId != 0)
    {
        tmpl = _templateRepo.FindTemplate(templateId);
    }
    else
    {
        SimpleBotCombatSpecRoleResolver resolver;
        BotCombatSpecRoleResolutionRequest req;
        req.classId            = bot->getClass();
        req.sourceCharacterGuid = sourceCharGuid;
        BotCombatSpecRoleResolutionResult res = resolver.Resolve(req);
        if (!res.effectiveSpecKey.empty())
            tmpl = _templateRepo.FindTemplateForSpec(
                res.effectiveSpecKey, bot->getClass());
    }

    if (!tmpl)
        return false;

    ApplyTemplate(bot, *tmpl);
    return true;
}
} // namespace service
} // namespace living_world
