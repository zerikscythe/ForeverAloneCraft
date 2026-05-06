#pragma once

#include "ObjectGuid.h"
#include <string>

class Player;
class Unit;

namespace living_world
{
namespace ai
{
void ScheduleCompanionAI(Player* botPlayer, Player* ownerPlayer);

// Force a bot to attack a specific target on the next AI tick.
// Passing ObjectGuid::Empty clears any forced target.
void SetBotForcedTarget(ObjectGuid botGuid, ObjectGuid targetGuid);

// Tell a bot to stop combat and return to follow. Cleared automatically
// when the bot's owner enters combat.
void SetBotDisengaged(ObjectGuid botGuid, bool disengaged);

// Remove all override state for a bot. Call this when the bot is dismissed
// so stale state does not carry over if the GUID is ever reused.
void ClearBotOverride(ObjectGuid botGuid);

// Enter retreat mode for durationMs milliseconds: bot stops combat and only
// follows / casts instant heals. Returns false if retreat is already active
// (caller can use this to cancel instead).
bool SetBotRetreat(ObjectGuid botGuid, uint32_t durationMs);

// Returns true while retreat mode is active for this bot.
bool IsBotRetreating(ObjectGuid botGuid);

// Force the bot to re-apply its class maintenance buffs immediately,
// bypassing the normal out-of-combat guard.
void ForceBotBuffRefresh(Player* bot, Player* owner);

// Clear cached doctrine/prepared-profile state for a live bot so profile edits
// or slot changes take effect on the next AI tick instead of waiting for cache
// TTL expiry.
void InvalidateBotCombatCaches(ObjectGuid botGuid);

// Set the active combat context for a bot. Contexts drive which default profile
// the bot uses: "PvE" (default), "PvP", and future values like "Arena", "BG".
// After changing context call InvalidateBotCombatCaches() if the new profile
// should take effect on the very next tick.
void SetBotContext(ObjectGuid botGuid, std::string const& contextKey);
std::string GetBotContext(ObjectGuid botGuid);
} // namespace ai
} // namespace living_world
