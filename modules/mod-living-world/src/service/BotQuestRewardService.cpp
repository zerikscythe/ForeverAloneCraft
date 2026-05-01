#include "service/BotQuestRewardService.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "QueryResult.h"
#include "SharedDefines.h"
#include "service/BotPlayerRegistry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace living_world
{
namespace service
{
namespace
{
struct RewardScore
{
    float score = -1000000.0f;
    bool strongSignal = false;
};

constexpr std::array<std::uint8_t, 2> RingSlots = { EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2 };
constexpr std::array<std::uint8_t, 2> TrinketSlots = { EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2 };

std::string SanitizeAddonField(std::string_view input)
{
    std::string out(input);
    std::replace(out.begin(), out.end(), ';', ',');
    return out;
}

std::string BuildItemLink(ItemTemplate const* itemTemplate)
{
    if (!itemTemplate)
    {
        return "<unknown item>";
    }

    std::stringstream stream;
    stream << "|c";
    stream << std::hex << ItemQualityColors[itemTemplate->Quality] << std::dec;
    stream << "|Hitem:";
    stream << itemTemplate->ItemId;
    stream << ":0:0:0:0:0:0:0:0:0|h[";
    stream << itemTemplate->Name1;
    stream << "]|h|r";
    return stream.str();
}

void BroadcastPartyRewardMessage(Player* owner, Player* bot, ItemTemplate const* itemTemplate)
{
    if (!owner || !bot || !itemTemplate)
    {
        return;
    }

    std::string const message = "Chose " + BuildItemLink(itemTemplate);
    WorldPacket data;
    ChatHandler::BuildChatPacket(
        data,
        CHAT_MSG_MONSTER_PARTY,
        LANG_UNIVERSAL,
        bot->GetGUID(),
        ObjectGuid::Empty,
        message,
        0,
        bot->GetName());

    if (Group* group = owner->GetGroup())
    {
        group->BroadcastPacket(&data, true);
        return;
    }

    owner->GetSession()->SendPacket(&data);
}

bool IsArmorLikeInventoryType(std::uint32_t inventoryType)
{
    switch (inventoryType)
    {
        case INVTYPE_HEAD:
        case INVTYPE_NECK:
        case INVTYPE_SHOULDERS:
        case INVTYPE_BODY:
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:
        case INVTYPE_WAIST:
        case INVTYPE_LEGS:
        case INVTYPE_FEET:
        case INVTYPE_WRISTS:
        case INVTYPE_HANDS:
        case INVTYPE_FINGER:
        case INVTYPE_TRINKET:
        case INVTYPE_CLOAK:
        case INVTYPE_HOLDABLE:
        case INVTYPE_SHIELD:
        case INVTYPE_TABARD:
            return true;
        default:
            return false;
    }
}

std::optional<std::uint32_t> GetPreferredArmorSubclass(Player const* bot)
{
    if (!bot)
    {
        return std::nullopt;
    }

    switch (bot->getClass())
    {
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
            return ITEM_SUBCLASS_ARMOR_CLOTH;
        case CLASS_ROGUE:
        case CLASS_DRUID:
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return ITEM_SUBCLASS_ARMOR_MAIL;
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return ITEM_SUBCLASS_ARMOR_PLATE;
        default:
            return std::nullopt;
    }
}

float GetStatWeight(Player const* bot, std::uint32_t statType)
{
    if (!bot)
    {
        return 0.0f;
    }

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
            switch (statType)
            {
                case ITEM_MOD_STRENGTH: return 5.0f;
                case ITEM_MOD_STAMINA: return 3.0f;
                case ITEM_MOD_ATTACK_POWER: return 3.0f;
                case ITEM_MOD_CRIT_RATING:
                case ITEM_MOD_HIT_RATING:
                case ITEM_MOD_EXPERTISE_RATING:
                case ITEM_MOD_HASTE_RATING:
                case ITEM_MOD_ARMOR_PENETRATION_RATING:
                    return 2.0f;
                case ITEM_MOD_DEFENSE_SKILL_RATING:
                case ITEM_MOD_DODGE_RATING:
                case ITEM_MOD_PARRY_RATING:
                case ITEM_MOD_BLOCK_RATING:
                case ITEM_MOD_BLOCK_VALUE:
                    return 1.5f;
                default:
                    return 0.0f;
            }

        case CLASS_PALADIN:
            switch (statType)
            {
                case ITEM_MOD_STRENGTH: return 4.5f;
                case ITEM_MOD_STAMINA: return 3.0f;
                case ITEM_MOD_INTELLECT: return 2.0f;
                case ITEM_MOD_SPELL_POWER: return 2.0f;
                case ITEM_MOD_ATTACK_POWER: return 2.0f;
                case ITEM_MOD_CRIT_RATING:
                case ITEM_MOD_HIT_RATING:
                case ITEM_MOD_HASTE_RATING:
                case ITEM_MOD_EXPERTISE_RATING:
                    return 1.8f;
                case ITEM_MOD_DEFENSE_SKILL_RATING:
                case ITEM_MOD_DODGE_RATING:
                case ITEM_MOD_PARRY_RATING:
                case ITEM_MOD_BLOCK_RATING:
                case ITEM_MOD_BLOCK_VALUE:
                    return 1.6f;
                default:
                    return 0.0f;
            }

        case CLASS_HUNTER:
            switch (statType)
            {
                case ITEM_MOD_AGILITY: return 5.0f;
                case ITEM_MOD_STAMINA: return 2.5f;
                case ITEM_MOD_ATTACK_POWER:
                case ITEM_MOD_RANGED_ATTACK_POWER:
                    return 3.0f;
                case ITEM_MOD_CRIT_RATING:
                case ITEM_MOD_HIT_RATING:
                case ITEM_MOD_HASTE_RATING:
                case ITEM_MOD_ARMOR_PENETRATION_RATING:
                    return 2.0f;
                case ITEM_MOD_INTELLECT:
                    return 0.6f;
                default:
                    return 0.0f;
            }

        case CLASS_ROGUE:
            switch (statType)
            {
                case ITEM_MOD_AGILITY: return 5.0f;
                case ITEM_MOD_STAMINA: return 2.5f;
                case ITEM_MOD_ATTACK_POWER: return 3.0f;
                case ITEM_MOD_CRIT_RATING:
                case ITEM_MOD_HIT_RATING:
                case ITEM_MOD_HASTE_RATING:
                case ITEM_MOD_EXPERTISE_RATING:
                case ITEM_MOD_ARMOR_PENETRATION_RATING:
                    return 2.0f;
                default:
                    return 0.0f;
            }

        case CLASS_PRIEST:
            switch (statType)
            {
                case ITEM_MOD_INTELLECT: return 4.0f;
                case ITEM_MOD_SPIRIT: return 3.0f;
                case ITEM_MOD_STAMINA: return 2.0f;
                case ITEM_MOD_SPELL_POWER: return 5.0f;
                case ITEM_MOD_CRIT_SPELL_RATING:
                case ITEM_MOD_HASTE_SPELL_RATING:
                case ITEM_MOD_HIT_SPELL_RATING:
                case ITEM_MOD_CRIT_RATING:
                case ITEM_MOD_HASTE_RATING:
                    return 2.0f;
                case ITEM_MOD_MANA_REGENERATION:
                    return 2.2f;
                default:
                    return 0.0f;
            }

        case CLASS_SHAMAN:
        case CLASS_DRUID:
            switch (statType)
            {
                case ITEM_MOD_INTELLECT: return 3.5f;
                case ITEM_MOD_SPIRIT: return 2.0f;
                case ITEM_MOD_STAMINA: return 2.5f;
                case ITEM_MOD_SPELL_POWER: return 4.5f;
                case ITEM_MOD_AGILITY: return 2.0f;
                case ITEM_MOD_STRENGTH: return 1.5f;
                case ITEM_MOD_ATTACK_POWER: return 1.5f;
                case ITEM_MOD_CRIT_RATING:
                case ITEM_MOD_HASTE_RATING:
                case ITEM_MOD_HIT_RATING:
                case ITEM_MOD_CRIT_SPELL_RATING:
                case ITEM_MOD_HASTE_SPELL_RATING:
                case ITEM_MOD_HIT_SPELL_RATING:
                case ITEM_MOD_MANA_REGENERATION:
                    return 1.8f;
                default:
                    return 0.0f;
            }

        case CLASS_MAGE:
        case CLASS_WARLOCK:
            switch (statType)
            {
                case ITEM_MOD_INTELLECT: return 4.0f;
                case ITEM_MOD_STAMINA: return 2.0f;
                case ITEM_MOD_SPIRIT: return 1.0f;
                case ITEM_MOD_SPELL_POWER: return 5.0f;
                case ITEM_MOD_CRIT_SPELL_RATING:
                case ITEM_MOD_HASTE_SPELL_RATING:
                case ITEM_MOD_HIT_SPELL_RATING:
                case ITEM_MOD_CRIT_RATING:
                case ITEM_MOD_HASTE_RATING:
                    return 2.0f;
                default:
                    return 0.0f;
            }

        default:
            return 0.0f;
    }
}

float ScoreTemplateForClass(Player const* bot, ItemTemplate const* itemTemplate)
{
    if (!bot || !itemTemplate)
    {
        return 0.0f;
    }

    float score = static_cast<float>(itemTemplate->ItemLevel) * 2.0f;

    for (std::uint32_t i = 0; i < itemTemplate->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
    {
        auto const& stat = itemTemplate->ItemStat[i];
        score += GetStatWeight(bot, stat.ItemStatType) *
            static_cast<float>(stat.ItemStatValue);
    }

    if (itemTemplate->Class == ITEM_CLASS_WEAPON)
    {
        float const dps = itemTemplate->getDPS();
        switch (bot->getClass())
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
            case CLASS_ROGUE:
                score += dps * 8.0f;
                break;
            case CLASS_HUNTER:
                if (itemTemplate->InventoryType == INVTYPE_RANGED ||
                    itemTemplate->InventoryType == INVTYPE_RANGEDRIGHT)
                {
                    score += dps * 9.0f;
                }
                else
                {
                    score += dps * 3.0f;
                }
                break;
            case CLASS_MAGE:
            case CLASS_PRIEST:
            case CLASS_WARLOCK:
                if (itemTemplate->SubClass == ITEM_SUBCLASS_WEAPON_WAND)
                {
                    score += dps * 3.0f;
                }
                break;
            default:
                score += dps * 2.0f;
                break;
        }
    }

    if (itemTemplate->Armor > 0)
    {
        score += static_cast<float>(itemTemplate->Armor) * 0.05f;
    }

    if (itemTemplate->InventoryType == INVTYPE_TRINKET)
    {
        score += 35.0f;
    }

    if (itemTemplate->InventoryType == INVTYPE_FINGER)
    {
        score += 25.0f;
    }

    return score;
}

std::vector<std::uint8_t> GetCandidateEquipSlots(ItemTemplate const* itemTemplate)
{
    if (!itemTemplate)
    {
        return {};
    }

    switch (itemTemplate->InventoryType)
    {
        case INVTYPE_HEAD: return { EQUIPMENT_SLOT_HEAD };
        case INVTYPE_NECK: return { EQUIPMENT_SLOT_NECK };
        case INVTYPE_SHOULDERS: return { EQUIPMENT_SLOT_SHOULDERS };
        case INVTYPE_BODY: return { EQUIPMENT_SLOT_BODY };
        case INVTYPE_CHEST:
        case INVTYPE_ROBE: return { EQUIPMENT_SLOT_CHEST };
        case INVTYPE_WAIST: return { EQUIPMENT_SLOT_WAIST };
        case INVTYPE_LEGS: return { EQUIPMENT_SLOT_LEGS };
        case INVTYPE_FEET: return { EQUIPMENT_SLOT_FEET };
        case INVTYPE_WRISTS: return { EQUIPMENT_SLOT_WRISTS };
        case INVTYPE_HANDS: return { EQUIPMENT_SLOT_HANDS };
        case INVTYPE_CLOAK: return { EQUIPMENT_SLOT_BACK };
        case INVTYPE_TABARD: return { EQUIPMENT_SLOT_TABARD };
        case INVTYPE_FINGER:
            return { EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2 };
        case INVTYPE_TRINKET:
            return { EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2 };
        case INVTYPE_WEAPON:
            return { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND };
        case INVTYPE_2HWEAPON:
        case INVTYPE_WEAPONMAINHAND:
            return { EQUIPMENT_SLOT_MAINHAND };
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:
        case INVTYPE_SHIELD:
            return { EQUIPMENT_SLOT_OFFHAND };
        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_THROWN:
        case INVTYPE_RELIC:
            return { EQUIPMENT_SLOT_RANGED };
        default:
            return {};
    }
}

RewardScore ScoreRewardChoice(Player* bot, Quest const* quest, std::uint32_t rewardIndex)
{
    RewardScore result;
    if (!bot || !quest)
    {
        return result;
    }

    if (!bot->CanRewardQuest(quest, rewardIndex, false))
    {
        return result;
    }

    std::uint32_t const itemId = quest->RewardChoiceItemId[rewardIndex];
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
    {
        return result;
    }

    result.score = static_cast<float>(itemTemplate->SellPrice) / 1000.0f;

    bool const isEquippable =
        itemTemplate->Class == ITEM_CLASS_ARMOR ||
        itemTemplate->Class == ITEM_CLASS_WEAPON ||
        itemTemplate->InventoryType == INVTYPE_TRINKET ||
        itemTemplate->InventoryType == INVTYPE_FINGER ||
        itemTemplate->InventoryType == INVTYPE_NECK ||
        itemTemplate->InventoryType == INVTYPE_CLOAK ||
        itemTemplate->InventoryType == INVTYPE_HOLDABLE ||
        itemTemplate->InventoryType == INVTYPE_SHIELD;

    if (!isEquippable)
    {
        result.score += static_cast<float>(itemTemplate->ItemLevel);
        return result;
    }

    if (bot->CanUseItem(itemTemplate) != EQUIP_ERR_OK)
    {
        result.score -= 250.0f;
        return result;
    }

    result.score += 250.0f;
    result.strongSignal = true;

    if (itemTemplate->Class == ITEM_CLASS_ARMOR &&
        itemTemplate->SubClass >= ITEM_SUBCLASS_ARMOR_CLOTH &&
        itemTemplate->SubClass <= ITEM_SUBCLASS_ARMOR_PLATE &&
        itemTemplate->InventoryType != INVTYPE_CLOAK)
    {
        if (std::optional<std::uint32_t> preferredArmor = GetPreferredArmorSubclass(bot))
        {
            if (itemTemplate->SubClass == *preferredArmor)
            {
                result.score += 180.0f;
            }
            else
            {
                result.score -= 60.0f;
            }
        }
    }

    float const candidateTemplateScore = ScoreTemplateForClass(bot, itemTemplate);
    result.score += candidateTemplateScore;

    std::vector<std::uint8_t> const candidateSlots = GetCandidateEquipSlots(itemTemplate);
    if (!candidateSlots.empty())
    {
        float currentBest = std::numeric_limits<float>::max();
        bool foundCurrent = false;

        for (std::uint8_t slot : candidateSlots)
        {
            Item* currentItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            float currentScore = 0.0f;
            if (currentItem)
            {
                currentScore = ScoreTemplateForClass(bot, currentItem->GetTemplate());
                foundCurrent = true;
            }

            currentBest = std::min(currentBest, currentScore);
        }

        if (!foundCurrent)
        {
            result.score += 120.0f;
        }
        else
        {
            result.score += (candidateTemplateScore - currentBest) * 2.0f;
        }
    }

    return result;
}
} // namespace

void BotQuestRewardService::EnsureSchema() const
{
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_quest_settings ("
        " owner_character_guid BIGINT UNSIGNED NOT NULL,"
        " reward_mode TINYINT UNSIGNED NOT NULL DEFAULT 1,"
        " updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP "
        "   ON UPDATE CURRENT_TIMESTAMP,"
        " PRIMARY KEY (owner_character_guid)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
}

BotQuestRewardMode BotQuestRewardService::GetRewardMode(
    std::uint64_t ownerCharacterGuid) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT reward_mode FROM living_world_bot_quest_settings "
        "WHERE owner_character_guid = {}",
        ownerCharacterGuid);
    if (!result)
    {
        return BotQuestRewardMode::Smart;
    }

    std::uint8_t const rawMode = result->Fetch()[0].Get<std::uint8_t>();
    if (rawMode == static_cast<std::uint8_t>(BotQuestRewardMode::Manual))
    {
        return BotQuestRewardMode::Manual;
    }

    return BotQuestRewardMode::Smart;
}

void BotQuestRewardService::SetRewardMode(
    std::uint64_t ownerCharacterGuid,
    BotQuestRewardMode mode) const
{
    CharacterDatabase.DirectExecute(
        "INSERT INTO living_world_bot_quest_settings "
        "(owner_character_guid, reward_mode) VALUES ({}, {}) "
        "ON DUPLICATE KEY UPDATE reward_mode = VALUES(reward_mode)",
        ownerCharacterGuid,
        static_cast<std::uint8_t>(mode));
}

std::vector<PendingQuestReward> BotQuestRewardService::BuildPendingRewards(
    Player* owner) const
{
    std::vector<PendingQuestReward> pending;
    if (!owner)
    {
        return pending;
    }

    for (Player* bot : BotPlayerRegistry::Instance().FindBotsForOwner(owner->GetGUID()))
    {
        if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
        {
            continue;
        }

        for (auto const& [questId, questStatus] : bot->getQuestStatusMap())
        {
            if (questStatus.Status != QUEST_STATUS_COMPLETE ||
                bot->GetQuestRewardStatus(questId))
            {
                continue;
            }

            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest || quest->GetRewChoiceItemsCount() == 0 ||
                !bot->CanRewardQuest(quest, false))
            {
                continue;
            }

            PendingQuestReward entry;
            entry.botGuid = bot->GetGUID().GetCounter();
            entry.botName = bot->GetName();
            entry.questId = questId;
            entry.questTitle = SanitizeAddonField(quest->GetTitle());

            for (std::uint32_t i = 0; i < quest->GetRewChoiceItemsCount(); ++i)
            {
                std::uint32_t const itemId = quest->RewardChoiceItemId[i];
                if (!itemId || !bot->CanRewardQuest(quest, i, false))
                {
                    continue;
                }

                QuestRewardChoice choice;
                choice.choiceNumber = static_cast<std::uint8_t>(i + 1);
                choice.itemId = itemId;
                choice.count = quest->RewardChoiceItemCount[i];
                entry.choices.push_back(choice);
            }

            if (!entry.choices.empty())
            {
                pending.push_back(std::move(entry));
            }
        }
    }

    std::sort(
        pending.begin(),
        pending.end(),
        [](PendingQuestReward const& left, PendingQuestReward const& right)
        {
            if (left.botName != right.botName)
            {
                return left.botName < right.botName;
            }

            if (left.questId != right.questId)
            {
                return left.questId < right.questId;
            }

            return left.questTitle < right.questTitle;
        });

    return pending;
}

void BotQuestRewardService::ApplyOwnerQuestRewardToBots(
    Player* owner,
    Quest const* quest) const
{
    if (!owner || !quest)
    {
        return;
    }

    BotQuestRewardMode const mode =
        GetRewardMode(owner->GetGUID().GetCounter());

    for (Player* bot : BotPlayerRegistry::Instance().FindBotsForOwner(owner->GetGUID()))
    {
        if (!bot || !bot->GetSession() || !bot->GetSession()->IsBotSession())
        {
            continue;
        }

        std::uint32_t const questId = quest->GetQuestId();
        if (bot->GetQuestRewardStatus(questId) ||
            bot->GetQuestStatus(questId) != QUEST_STATUS_COMPLETE ||
            !bot->CanRewardQuest(quest, false))
        {
            continue;
        }

        if (quest->GetRewChoiceItemsCount() == 0)
        {
            std::string unusedError;
            RewardBotQuest(owner, bot, quest, 0, unusedError);
            continue;
        }

        if (mode != BotQuestRewardMode::Smart)
        {
            continue;
        }

        std::optional<std::uint32_t> choice = ChooseSmartReward(bot, quest);
        if (!choice)
        {
            continue;
        }

        std::string unusedError;
        RewardBotQuest(owner, bot, quest, *choice, unusedError);
    }
}

bool BotQuestRewardService::RewardBotQuestById(
    Player* owner,
    Player* bot,
    std::uint32_t questId,
    std::uint8_t choiceNumber,
    std::string& error) const
{
    if (!owner || !bot)
    {
        error = "owner or bot missing";
        return false;
    }

    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
    {
        error = "quest template not found";
        return false;
    }

    if (choiceNumber == 0 || choiceNumber > quest->GetRewChoiceItemsCount())
    {
        error = "reward choice number out of range";
        return false;
    }

    return RewardBotQuest(
        owner,
        bot,
        quest,
        static_cast<std::uint32_t>(choiceNumber - 1),
        error);
}

std::optional<std::uint32_t> BotQuestRewardService::ChooseSmartReward(
    Player* bot,
    Quest const* quest) const
{
    if (!bot || !quest)
    {
        return std::nullopt;
    }

    struct RankedChoice
    {
        std::uint32_t rewardIndex = 0;
        RewardScore score;
    };

    std::vector<RankedChoice> candidates;
    for (std::uint32_t i = 0; i < quest->GetRewChoiceItemsCount(); ++i)
    {
        if (!quest->RewardChoiceItemId[i])
        {
            continue;
        }

        RewardScore const score = ScoreRewardChoice(bot, quest, i);
        if (score.score <= -999999.0f)
        {
            continue;
        }

        candidates.push_back({ i, score });
    }

    if (candidates.empty())
    {
        return std::nullopt;
    }

    if (candidates.size() == 1)
    {
        return candidates.front().rewardIndex;
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](RankedChoice const& left, RankedChoice const& right)
        {
            return left.score.score > right.score.score;
        });

    RankedChoice const& best = candidates.front();
    RankedChoice const& second = candidates[1];

    if (!best.score.strongSignal &&
        !second.score.strongSignal)
    {
        return std::nullopt;
    }

    if (std::fabs(best.score.score - second.score.score) < 25.0f)
    {
        return std::nullopt;
    }

    return best.rewardIndex;
}

bool BotQuestRewardService::RewardBotQuest(
    Player* owner,
    Player* bot,
    Quest const* quest,
    std::uint32_t rewardIndex,
    std::string& error) const
{
    if (!owner || !bot || !quest)
    {
        error = "owner, bot, or quest missing";
        return false;
    }

    if (!bot->CanRewardQuest(quest, rewardIndex, false))
    {
        error = "bot cannot take that reward right now";
        return false;
    }

    bot->RewardQuest(quest, rewardIndex, bot, false);

    if (quest->GetRewChoiceItemsCount() > 0)
    {
        std::uint32_t const itemId = quest->RewardChoiceItemId[rewardIndex];
        if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId))
        {
            BroadcastPartyRewardMessage(owner, bot, itemTemplate);
        }
    }

    error.clear();
    return true;
}
} // namespace service
} // namespace living_world
