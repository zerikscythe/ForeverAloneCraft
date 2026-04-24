#include "ai/CompanionAI.h"

#include "Duration.h"
#include "EventProcessor.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
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
constexpr float HealOwnerThreshold = 75.0f;
constexpr float HealSelfThreshold = 60.0f;
constexpr float HybridHealThreshold = 50.0f;

enum class BotCombatRole
{
    Healer,
    HybridHealer,
    Ranged,
    Melee
};

BotCombatRole GetCombatRole(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_PRIEST:
            return BotCombatRole::Healer;
        case CLASS_DRUID:
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
            return BotCombatRole::HybridHealer;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_HUNTER:
            return BotCombatRole::Ranged;
        default:
            return BotCombatRole::Melee;
    }
}

// Walks the spell rank chain from the highest rank downward and returns the
// first spell ID the bot has learned. Returns 0 if none are known.
std::uint32_t FindBestKnownSpellInChain(Player* bot, std::uint32_t baseSpellId)
{
    std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(baseSpellId);
    while (candidate)
    {
        if (bot->HasSpell(candidate))
            return candidate;
        candidate = sSpellMgr->GetPrevSpellInChain(candidate);
    }
    return 0;
}

// Base spell IDs (rank 1) for the signature heal of each healer class.
std::uint32_t GetHealSpell(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:  return FindBestKnownSpellInChain(bot, 2061); // Flash Heal
        case CLASS_DRUID:   return FindBestKnownSpellInChain(bot, 774);  // Rejuvenation
        case CLASS_PALADIN: return FindBestKnownSpellInChain(bot, 635);  // Holy Light
        case CLASS_SHAMAN:  return FindBestKnownSpellInChain(bot, 331);  // Healing Wave
        default:            return 0;
    }
}

// Base spell IDs (rank 1) for the signature ranged damage spell of each class.
std::uint32_t GetDamageSpell(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:    return FindBestKnownSpellInChain(bot, 116);  // Frostbolt
        case CLASS_WARLOCK: return FindBestKnownSpellInChain(bot, 686);  // Shadow Bolt
        case CLASS_HUNTER:  return FindBestKnownSpellInChain(bot, 3044); // Arcane Shot
        default:            return 0;
    }
}

void TickHealer(Player* bot, Player* owner)
{
    if (bot->IsNonMeleeSpellCast(false))
        return;

    std::uint32_t const spell = GetHealSpell(bot);
    if (!spell)
        return;

    if (owner->GetHealthPct() < HealOwnerThreshold && !owner->HasAura(spell))
    {
        bot->CastSpell(owner, spell, false);
        return;
    }

    if (bot->GetHealthPct() < HealSelfThreshold && !bot->HasAura(spell))
        bot->CastSpell(bot, spell, false);
}

void TickRanged(Player* bot, Unit* target)
{
    if (bot->IsNonMeleeSpellCast(false))
        return;

    std::uint32_t const spell = GetDamageSpell(bot);
    if (spell)
        bot->CastSpell(target, spell, false);
}

void Tick(Player* bot, Player* owner)
{
    BotCombatRole const role = GetCombatRole(bot->getClass());

    // Pure healers always monitor owner HP regardless of combat state.
    if (role == BotCombatRole::Healer)
    {
        TickHealer(bot, owner);
        return;
    }

    if (owner->IsInCombat())
    {
        // Hybrids prioritise healing when the owner drops low, otherwise fight.
        if (role == BotCombatRole::HybridHealer &&
            owner->GetHealthPct() < HybridHealThreshold)
        {
            TickHealer(bot, owner);
            return;
        }

        if (!bot->GetVictim())
        {
            if (Unit* target = owner->GetVictim())
                bot->Attack(target, true);
        }

        if (role == BotCombatRole::Ranged && bot->GetVictim())
            TickRanged(bot, bot->GetVictim());

        return;
    }

    if (bot->GetVictim())
    {
        bot->AttackStop();
        bot->GetMotionMaster()->MoveFollow(owner, FollowDistance, FollowAngle);
        return;
    }

    if (!bot->IsWithinDistInMap(owner, RepositionDistance))
        bot->GetMotionMaster()->MoveFollow(owner, FollowDistance, FollowAngle);
}

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
            return true;

        Tick(bot, owner);
        bot->m_Events.AddEventAtOffset(
            new CompanionAIEvent(_botGuid, _ownerGuid),
            500ms);
        return true;
    }

private:
    ObjectGuid _botGuid;
    ObjectGuid _ownerGuid;
};
} // namespace

void ScheduleCompanionAI(Player* botPlayer, Player* ownerPlayer)
{
    if (!botPlayer || !ownerPlayer)
        return;

    botPlayer->m_Events.AddEventAtOffset(
        new CompanionAIEvent(botPlayer->GetGUID(), ownerPlayer->GetGUID()),
        500ms);
}
} // namespace ai
} // namespace living_world
