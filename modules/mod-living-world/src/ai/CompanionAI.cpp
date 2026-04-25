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
constexpr float HealOwnerCritical = 50.0f;
constexpr float HealOwnerModerate = 85.0f;
constexpr float HealSelfCritical = 40.0f;
constexpr float HealSelfModerate = 65.0f;
constexpr float HybridHealThreshold = 70.0f;

// Debuff aura IDs applied to the target by DK rune strikes.
constexpr std::uint32_t AuraFrostFever = 55095;
constexpr std::uint32_t AuraBloodPlague = 55078;

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

// Fast direct heal for when a target drops critically low.
std::uint32_t GetDirectHealSpell(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:  return FindBestKnownSpellInChain(bot, 2061);  // Flash Heal
        case CLASS_DRUID:   return FindBestKnownSpellInChain(bot, 5185);  // Healing Touch
        case CLASS_PALADIN: return FindBestKnownSpellInChain(bot, 19750); // Flash of Light
        case CLASS_SHAMAN:  return FindBestKnownSpellInChain(bot, 8004);  // Lesser Healing Wave
        default:            return 0;
    }
}

// Sustained heal or HoT for topping off a moderately damaged target.
std::uint32_t GetSustainedHealSpell(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:  return FindBestKnownSpellInChain(bot, 139);  // Renew
        case CLASS_DRUID:   return FindBestKnownSpellInChain(bot, 774);  // Rejuvenation
        case CLASS_PALADIN: return FindBestKnownSpellInChain(bot, 635);  // Holy Light
        case CLASS_SHAMAN:  return FindBestKnownSpellInChain(bot, 331);  // Healing Wave
        default:            return 0;
    }
}

// Returns the best melee-range offensive ability for the bot's class and state.
std::uint32_t GetMeleeOffensiveSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        {
            std::uint32_t spell = FindBestKnownSpellInChain(bot, 12294); // Mortal Strike
            if (spell)
                return spell;
            spell = FindBestKnownSpellInChain(bot, 23881); // Bloodthirst
            if (spell)
                return spell;
            return FindBestKnownSpellInChain(bot, 78); // Heroic Strike
        }
        case CLASS_ROGUE:
        {
            if (bot->GetComboPoints() >= 4)
                return FindBestKnownSpellInChain(bot, 2098); // Eviscerate
            return FindBestKnownSpellInChain(bot, 1752); // Sinister Strike
        }
        case CLASS_DEATH_KNIGHT:
        {
            bool const hasFrostFever = target->HasAura(AuraFrostFever);
            bool const hasBloodPlague = target->HasAura(AuraBloodPlague);
            if (hasFrostFever && hasBloodPlague && bot->HasSpell(49998))
                return 49998; // Death Strike — consumes both diseases
            if (!hasBloodPlague && bot->HasSpell(45462))
                return 45462; // Plague Strike — applies Blood Plague
            if (!hasFrostFever && bot->HasSpell(45477))
                return 45477; // Icy Touch — applies Frost Fever
            if (bot->HasSpell(49998))
                return 49998; // Death Strike fallback when diseases are up
            return 0;
        }
        default:
            return 0;
    }
}

// Returns the best short-range DPS ability for hybrid healers acting offensively.
std::uint32_t GetHybridDamageSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_PALADIN:
            if (bot->HasSpell(35395)) // Crusader Strike (single rank in WotLK)
                return 35395;
            return 0;
        case CLASS_SHAMAN:
        {
            std::uint32_t const flameShock = FindBestKnownSpellInChain(bot, 8050);
            if (flameShock && !target->HasAura(flameShock))
                return flameShock; // Apply Flame Shock DoT first
            return FindBestKnownSpellInChain(bot, 8042); // Earth Shock filler
        }
        case CLASS_DRUID:
        {
            std::uint32_t const moonfire = FindBestKnownSpellInChain(bot, 8921);
            if (moonfire && !target->HasAura(moonfire))
                return moonfire; // Apply Moonfire DoT first
            return FindBestKnownSpellInChain(bot, 5176); // Wrath filler
        }
        default:
            return 0;
    }
}

// Returns the best ranged damage spell, preferring DoTs when not yet applied.
std::uint32_t GetDamageSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
            return FindBestKnownSpellInChain(bot, 116); // Frostbolt
        case CLASS_WARLOCK:
        {
            std::uint32_t const corruption = FindBestKnownSpellInChain(bot, 172);
            if (corruption && !target->HasAura(corruption))
                return corruption; // Apply Corruption DoT first
            return FindBestKnownSpellInChain(bot, 686); // Shadow Bolt filler
        }
        case CLASS_HUNTER:
            if (bot->HasSpell(34120)) // Steady Shot (single rank in WotLK)
                return 34120;
            return FindBestKnownSpellInChain(bot, 3044); // Arcane Shot fallback
        default:
            return 0;
    }
}

void TickHealer(Player* bot, Player* owner)
{
    if (bot->IsNonMeleeSpellCast(false))
        return;

    std::uint32_t const directSpell = GetDirectHealSpell(bot);
    std::uint32_t const sustainedSpell = GetSustainedHealSpell(bot);

    if (owner->GetHealthPct() < HealOwnerCritical)
    {
        if (directSpell)
            bot->CastSpell(owner, directSpell, false);
        return;
    }

    if (owner->GetHealthPct() < HealOwnerModerate)
    {
        if (sustainedSpell && !owner->HasAura(sustainedSpell))
            bot->CastSpell(owner, sustainedSpell, false);
        return;
    }

    if (bot->GetHealthPct() < HealSelfCritical)
    {
        if (directSpell)
            bot->CastSpell(bot, directSpell, false);
        return;
    }

    if (bot->GetHealthPct() < HealSelfModerate)
    {
        if (sustainedSpell && !bot->HasAura(sustainedSpell))
            bot->CastSpell(bot, sustainedSpell, false);
    }
}

void TickRanged(Player* bot, Unit* target)
{
    if (bot->IsNonMeleeSpellCast(false))
        return;

    std::uint32_t const spell = GetDamageSpell(bot, target);
    if (spell)
        bot->CastSpell(target, spell, false);
}

void TickMelee(Player* bot, Unit* target)
{
    if (bot->IsNonMeleeSpellCast(false))
        return;

    std::uint32_t const spell = GetMeleeOffensiveSpell(bot, target);
    if (spell)
        bot->CastSpell(target, spell, false);
}

// Socketless bot Players have no client-driven movement, so an Attack() call by
// itself just plants the bot at follow distance swinging at air. The melee /
// hybrid combat paths must explicitly drive the motion master into chase mode
// for the current victim. MotionMaster::MoveChase short-circuits when the
// active generator is already chasing the same target, so this is cheap to
// re-issue every tick.
void EnsureChasingVictim(Player* bot, Unit* target)
{
    if (!target)
        return;

    bot->GetMotionMaster()->MoveChase(target);
}

// Returns true when this unit is something a bot should engage on the owner's
// behalf: alive, on the same map, hostile to the owner, and currently flagged
// as a legal attack target.
bool IsValidAssistTarget(Player const* owner, Unit const* candidate)
{
    if (!candidate || !candidate->IsInWorld() || !candidate->IsAlive())
        return false;
    if (candidate == owner)
        return false;
    if (candidate->GetMap() != owner->GetMap())
        return false;
    if (owner->IsFriendlyTo(candidate))
        return false;
    if (!candidate->isTargetableForAttack(true, owner))
        return false;
    return true;
}

// Resolve the unit a bot should be fighting right now.
//   1. Stick to the bot's current victim if it is still valid (don't drop a
//      mob mid-fight just because the owner re-clicked something else).
//   2. Otherwise prefer whatever the owner is actively fighting.
//   3. Otherwise pick up the owner's selection (right-click target). This is
//      the hunter-pet "Attack" semantic: owner targets a mob, bots engage.
Unit* ResolveAssistTarget(Player* bot, Player* owner)
{
    if (Unit* current = bot->GetVictim())
    {
        if (IsValidAssistTarget(owner, current))
            return current;
    }

    if (Unit* ownerVictim = owner->GetVictim())
    {
        if (IsValidAssistTarget(owner, ownerVictim))
            return ownerVictim;
    }

    ObjectGuid const selectionGuid = owner->GetTarget();
    if (!selectionGuid)
        return nullptr;

    Unit* selection = ObjectAccessor::GetUnit(*owner, selectionGuid);
    if (selection && IsValidAssistTarget(owner, selection))
        return selection;

    return nullptr;
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

    Unit* const assistTarget = ResolveAssistTarget(bot, owner);

    if (assistTarget)
    {
        if (role == BotCombatRole::HybridHealer)
        {
            // Hybrid casters still triage owner health first. Only commit to
            // damage when the owner is healthy enough to take a few seconds
            // of attention shift.
            if (owner->GetHealthPct() < HybridHealThreshold)
            {
                TickHealer(bot, owner);
                return;
            }

            if (bot->GetVictim() != assistTarget)
                bot->Attack(assistTarget, true);

            EnsureChasingVictim(bot, assistTarget);
            std::uint32_t const spell = GetHybridDamageSpell(bot, assistTarget);
            if (spell && !bot->IsNonMeleeSpellCast(false))
                bot->CastSpell(assistTarget, spell, false);
            return;
        }

        if (bot->GetVictim() != assistTarget)
            bot->Attack(assistTarget, true);

        if (role == BotCombatRole::Ranged)
        {
            TickRanged(bot, assistTarget);
        }
        else if (role == BotCombatRole::Melee)
        {
            EnsureChasingVictim(bot, assistTarget);
            TickMelee(bot, assistTarget);
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
