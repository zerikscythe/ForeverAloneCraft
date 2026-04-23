#include "ai/CompanionAI.h"

#include "Duration.h"
#include "EventProcessor.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Unit.h"

namespace living_world
{
namespace ai
{
namespace
{
constexpr float FollowDistance = 2.0f;
constexpr float FollowAngle = 3.14159265358979323846f;
constexpr float RepositionDistance = 8.0f;

class CompanionAIEvent final : public BasicEvent
{
public:
    CompanionAIEvent(ObjectGuid botGuid, ObjectGuid ownerGuid)
        : _botGuid(botGuid), _ownerGuid(ownerGuid)
    {
    }

    bool Execute(uint64, uint32) override
    {
        Player* bot = ObjectAccessor::FindPlayer(_botGuid);
        Player* owner = ObjectAccessor::FindPlayer(_ownerGuid);
        if (!bot || !owner || !bot->IsInWorld() || !owner->IsInWorld())
        {
            return true;
        }

        Tick(bot, owner);
        bot->m_Events.AddEventAtOffset(
            new CompanionAIEvent(_botGuid, _ownerGuid),
            500ms);
        return true;
    }

private:
    static void Tick(Player* bot, Player* owner)
    {
        if (owner->IsInCombat())
        {
            if (!bot->GetVictim())
            {
                if (Unit* target = owner->GetVictim())
                {
                    bot->Attack(target, true);
                }
            }
            return;
        }

        if (bot->GetVictim())
        {
            bot->AttackStop();
            bot->GetMotionMaster()->MoveFollow(owner, FollowDistance, FollowAngle);
            return;
        }

        if (!bot->IsWithinDistInMap(owner, RepositionDistance))
        {
            bot->GetMotionMaster()->MoveFollow(owner, FollowDistance, FollowAngle);
        }
    }

    ObjectGuid _botGuid;
    ObjectGuid _ownerGuid;
};
} // namespace

void ScheduleCompanionAI(Player* botPlayer, Player* ownerPlayer)
{
    if (!botPlayer || !ownerPlayer)
    {
        return;
    }

    botPlayer->m_Events.AddEventAtOffset(
        new CompanionAIEvent(botPlayer->GetGUID(), ownerPlayer->GetGUID()),
        500ms);
}
} // namespace ai
} // namespace living_world
