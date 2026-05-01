#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Player;
class Quest;

namespace living_world
{
namespace service
{
enum class BotQuestRewardMode : std::uint8_t
{
    Smart = 1,
    Manual = 2
};

struct QuestRewardChoice
{
    std::uint8_t choiceNumber = 0; // 1-based for UI / chat commands.
    std::uint32_t itemId = 0;
    std::uint32_t count = 0;
};

struct PendingQuestReward
{
    std::uint64_t botGuid = 0;
    std::string botName;
    std::uint32_t questId = 0;
    std::string questTitle;
    std::vector<QuestRewardChoice> choices;
};

class BotQuestRewardService
{
public:
    void EnsureSchema() const;

    BotQuestRewardMode GetRewardMode(std::uint64_t ownerCharacterGuid) const;
    void SetRewardMode(
        std::uint64_t ownerCharacterGuid,
        BotQuestRewardMode mode) const;

    std::vector<PendingQuestReward> BuildPendingRewards(Player* owner) const;

    void ApplyOwnerQuestRewardToBots(Player* owner, Quest const* quest) const;

    bool RewardBotQuestById(
        Player* owner,
        Player* bot,
        std::uint32_t questId,
        std::uint8_t choiceNumber,
        std::string& error) const;

private:
    std::optional<std::uint32_t> ChooseSmartReward(
        Player* bot,
        Quest const* quest) const;

    bool RewardBotQuest(
        Player* owner,
        Player* bot,
        Quest const* quest,
        std::uint32_t rewardIndex,
        std::string& error) const;
};
} // namespace service
} // namespace living_world
