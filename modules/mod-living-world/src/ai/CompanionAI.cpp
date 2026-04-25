#include "ai/CompanionAI.h"

#include "Duration.h"
#include "EventProcessor.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "Unit.h"

#include <array>
#include <cmath>

namespace living_world
{
namespace ai
{
namespace
{
// --- Follow / reposition constants ---
constexpr float FollowDistance        = 2.0f;
constexpr float FollowAngle           = 3.14159265358979323846f;
constexpr float RepositionDistance    = 8.0f;
constexpr float RangedMinDistance     = 8.0f;    // Back away when closer than this
constexpr float RangedOptimalDistance = 25.0f;   // Target spacing for ranged bots

// --- Heal thresholds ---
constexpr float HealOwnerCritical    = 50.0f;
constexpr float HealOwnerModerate    = 85.0f;
constexpr float HealSelfCritical     = 40.0f;
constexpr float HealSelfModerate     = 65.0f;
constexpr float HybridHealThreshold  = 70.0f;

// --- DK disease aura IDs ---
constexpr std::uint32_t AuraFrostFever  = 55095;
constexpr std::uint32_t AuraBloodPlague = 55078;

// --- Priest Weakened Soul debuff: prevents re-shielding for 15 seconds ---
constexpr std::uint32_t AuraWeakenedSoul = 6788;

// ---------------------------------------------------------------
// Role classification
// ---------------------------------------------------------------

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

// ---------------------------------------------------------------
// Spell utilities
// ---------------------------------------------------------------

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

// Returns true if the target has an aura from any rank of the given spell chain.
// Works correctly for both single-rank spells and multi-rank chains.
bool HasAuraFromChain(Unit const* target, std::uint32_t baseSpellId)
{
    std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(baseSpellId);
    while (candidate)
    {
        if (target->HasAura(candidate))
            return true;
        candidate = sSpellMgr->GetPrevSpellInChain(candidate);
    }
    return false;
}

// Returns true when this bot can still fire mana-based spells. Non-mana
// users (Warriors, Rogues, DKs) always return true. A caster at zero mana
// should switch to melee autoattack rather than spamming failed cast attempts.
bool BotHasManaToFight(Player const* bot)
{
    if (bot->GetMaxPower(POWER_MANA) == 0)
        return true;
    return bot->GetPower(POWER_MANA) > 0;
}

// ---------------------------------------------------------------
// Heal spells
// ---------------------------------------------------------------

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

// ---------------------------------------------------------------
// Offensive spells — melee roles
// ---------------------------------------------------------------

// Returns the best melee-range offensive ability for the bot's class and state.
std::uint32_t GetMeleeOffensiveSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        {
            // Execute: highest-priority finisher at low target health
            std::uint32_t const execute = FindBestKnownSpellInChain(bot, 5308);
            if (execute && target->GetHealthPct() < 20.0f)
                return execute;

            // Mortal Strike (Arms)
            std::uint32_t spell = FindBestKnownSpellInChain(bot, 12294);
            if (spell)
                return spell;

            // Bloodthirst (Fury)
            spell = FindBestKnownSpellInChain(bot, 23881);
            if (spell)
                return spell;

            // Rend: apply the bleed DoT when not present on target
            {
                std::uint32_t const rend = FindBestKnownSpellInChain(bot, 772);
                if (rend && !target->HasAura(rend))
                    return rend;
            }

            // Heroic Strike: basic melee filler
            return FindBestKnownSpellInChain(bot, 78);
        }

        case CLASS_ROGUE:
        {
            std::uint32_t const snd   = FindBestKnownSpellInChain(bot, 5171); // Slice and Dice
            std::uint32_t const evisc = FindBestKnownSpellInChain(bot, 2098); // Eviscerate
            std::uint32_t const ss    = FindBestKnownSpellInChain(bot, 1752); // Sinister Strike

            std::uint8_t const cp = bot->GetComboPoints();

            // At 2+ combo points, apply Slice and Dice when the haste buff is missing
            if (cp >= 2 && snd && !bot->HasAura(snd))
                return snd;

            // At 4+ combo points spend with Eviscerate
            if (cp >= 4 && evisc)
                return evisc;

            return ss;
        }

        case CLASS_DEATH_KNIGHT:
        {
            bool const hasFrostFever  = target->HasAura(AuraFrostFever);
            bool const hasBloodPlague = target->HasAura(AuraBloodPlague);

            // Apply diseases before committing to strike abilities
            if (!hasBloodPlague && bot->HasSpell(45462))
                return 45462; // Plague Strike — applies Blood Plague
            if (!hasFrostFever && bot->HasSpell(45477))
                return 45477; // Icy Touch — applies Frost Fever

            // Diseases up: Death Strike when off cooldown (damage + self-heal)
            if (bot->HasSpell(49998) && !bot->GetSpellHistory()->HasCooldown(49998))
                return 49998;

            // Death Strike is on cooldown — fill with rune strikes
            {
                std::uint32_t const heartStrike = FindBestKnownSpellInChain(bot, 55050);
                if (heartStrike && !bot->GetSpellHistory()->HasCooldown(heartStrike))
                    return heartStrike; // Heart Strike
            }
            if (bot->HasSpell(45902) && !bot->GetSpellHistory()->HasCooldown(45902))
                return 45902; // Blood Strike

            // Fallback: let the engine handle the cooldown; autoattack continues
            if (bot->HasSpell(49998))
                return 49998;
            return 0;
        }

        default:
            return 0;
    }
}

// ---------------------------------------------------------------
// Offensive spells — Paladin seal helpers
// ---------------------------------------------------------------

// Returns the Paladin's best-known seal to apply, preferring the highest-DPS option.
std::uint32_t GetPreferredSeal(Player* bot)
{
    // Seal of Vengeance (Alliance) / Seal of Corruption (Horde): best sustained DPS seal
    if (std::uint32_t s = FindBestKnownSpellInChain(bot, 31801)) return s;
    if (std::uint32_t s = FindBestKnownSpellInChain(bot, 53736)) return s;
    if (std::uint32_t s = FindBestKnownSpellInChain(bot, 20375)) return s; // Seal of Command
    if (std::uint32_t s = FindBestKnownSpellInChain(bot, 20154)) return s; // Seal of Righteousness
    return 0;
}

// Returns true when the Paladin bot has any seal aura active.
bool HasSealActive(Player const* bot)
{
    static constexpr std::array<std::uint32_t, 7> SealBases = {
        20154, // Seal of Righteousness
        20375, // Seal of Command
        31801, // Seal of Vengeance
        53736, // Seal of Corruption
        19854, // Seal of Wisdom
        20165, // Seal of Light
        20164, // Seal of Justice
    };
    for (std::uint32_t base : SealBases)
    {
        if (HasAuraFromChain(bot, base))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------
// Offensive spells — hybrid healers acting offensively
// ---------------------------------------------------------------

// Returns the best short-range DPS ability for hybrid healers acting offensively.
std::uint32_t GetHybridDamageSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_PALADIN:
        {
            // Hammer of Wrath: execute-range burst, requires target below 20% HP
            std::uint32_t const how = FindBestKnownSpellInChain(bot, 24275);
            if (how && target->GetHealthPct() < 20.0f
                && !bot->GetSpellHistory()->HasCooldown(how))
                return how;

            // Judgement of Light: holy damage + party heal proc on hit
            std::uint32_t const jol = FindBestKnownSpellInChain(bot, 20271);
            if (jol && !bot->GetSpellHistory()->HasCooldown(jol))
                return jol;

            // Consecration: sustained AoE holy damage field
            std::uint32_t const cons = FindBestKnownSpellInChain(bot, 20116);
            if (cons && !bot->GetSpellHistory()->HasCooldown(cons))
                return cons;

            // Crusader Strike: primary single-target melee ability
            if (bot->HasSpell(35395) && !bot->GetSpellHistory()->HasCooldown(35395))
                return 35395;

            return 0;
        }

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

// ---------------------------------------------------------------
// Offensive spells — ranged roles
// ---------------------------------------------------------------

// Returns the best ranged damage spell, preferring DoTs when not yet applied.
std::uint32_t GetDamageSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
            return FindBestKnownSpellInChain(bot, 116); // Frostbolt

        case CLASS_WARLOCK:
        {
            // Curse of Agony: highest-DPS curse, apply first
            std::uint32_t const coa = FindBestKnownSpellInChain(bot, 980);
            if (coa && !target->HasAura(coa))
                return coa;

            // Immolate: fire DoT, apply when missing
            std::uint32_t const immolate = FindBestKnownSpellInChain(bot, 348);
            if (immolate && !target->HasAura(immolate))
                return immolate;

            // Corruption: instant shadow DoT
            std::uint32_t const corruption = FindBestKnownSpellInChain(bot, 172);
            if (corruption && !target->HasAura(corruption))
                return corruption;

            // Shadow Bolt: primary filler when all DoTs are rolling
            return FindBestKnownSpellInChain(bot, 686);
        }

        case CLASS_HUNTER:
        {
            // Serpent Sting: nature DoT, apply when missing
            std::uint32_t const serpent = FindBestKnownSpellInChain(bot, 1978);
            if (serpent && !target->HasAura(serpent))
                return serpent;

            // Multi-Shot: strong filler when off cooldown
            std::uint32_t const multiShot = FindBestKnownSpellInChain(bot, 2643);
            if (multiShot && !bot->GetSpellHistory()->HasCooldown(multiShot))
                return multiShot;

            // Steady Shot: primary ranged filler
            if (bot->HasSpell(34120))
                return 34120;

            // Arcane Shot: fallback if Steady Shot is not yet learned
            return FindBestKnownSpellInChain(bot, 3044);
        }

        default:
            return 0;
    }
}

// ---------------------------------------------------------------
// Out-of-combat maintenance
// ---------------------------------------------------------------

// Applies class-specific maintenance buffs when neither the bot nor the owner
// is in combat. Called from the idle tick path so buffs are refreshed naturally
// between pulls without any dedicated buff-loop timers.
void TryApplyOutOfCombatBuff(Player* bot, Player* owner)
{
    if (bot->IsNonMeleeSpellCast(false) || bot->IsInCombat() || owner->IsInCombat())
        return;

    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        {
            // Battle Shout: party-wide AP buff; cast on self, hits all nearby members
            std::uint32_t const shout = FindBestKnownSpellInChain(bot, 6673);
            if (shout && !HasAuraFromChain(bot, 6673) && !HasAuraFromChain(owner, 6673))
                bot->CastSpell(bot, shout, false);
            break;
        }

        case CLASS_DEATH_KNIGHT:
        {
            // Horn of Winter: party-wide Strength/Agility buff
            if (bot->HasSpell(57330)
                && !HasAuraFromChain(bot, 57330)
                && !HasAuraFromChain(owner, 57330))
                bot->CastSpell(bot, 57330U, false);
            break;
        }

        case CLASS_PALADIN:
        {
            // Re-apply seal if it dropped between fights
            if (!HasSealActive(bot))
            {
                std::uint32_t const seal = GetPreferredSeal(bot);
                if (seal)
                    bot->CastSpell(bot, seal, false);
            }
            break;
        }

        case CLASS_PRIEST:
        {
            // Power Word: Fortitude: Stamina buff; prioritise the owner
            std::uint32_t const pwf = FindBestKnownSpellInChain(bot, 1243);
            if (pwf)
            {
                if (!HasAuraFromChain(owner, 1243))
                    bot->CastSpell(owner, pwf, false);
                else if (!HasAuraFromChain(bot, 1243))
                    bot->CastSpell(bot, pwf, false);
            }
            break;
        }

        case CLASS_DRUID:
        {
            // Mark of the Wild: multi-stat buff; prioritise the owner
            std::uint32_t const motw = FindBestKnownSpellInChain(bot, 1126);
            if (motw)
            {
                if (!HasAuraFromChain(owner, 1126))
                    bot->CastSpell(owner, motw, false);
                else if (!HasAuraFromChain(bot, 1126))
                    bot->CastSpell(bot, motw, false);
            }
            break;
        }

        case CLASS_MAGE:
        {
            // Arcane Intellect: Intellect buff; prioritise the owner
            std::uint32_t const ai = FindBestKnownSpellInChain(bot, 1459);
            if (ai)
            {
                if (!HasAuraFromChain(owner, 1459))
                    bot->CastSpell(owner, ai, false);
                else if (!HasAuraFromChain(bot, 1459))
                    bot->CastSpell(bot, ai, false);
            }
            break;
        }

        default:
            break;
    }
}

// ---------------------------------------------------------------
// Motion helpers
// ---------------------------------------------------------------

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

// Backs a ranged bot away from a target that has closed to melee range. The
// bot moves to a point RangedOptimalDistance yards from the target, projected
// through the current bot position. Only fires when within RangedMinDistance so
// it does not interrupt normal ranged combat positioning.
void EnsureRangedPosition(Player* bot, Unit* target)
{
    if (bot->GetDistance(target) >= RangedMinDistance)
        return;

    // Angle pointing from target toward the bot — bot retreats further that way
    float const angle = target->GetAngle(bot);
    float const x     = target->GetPositionX() + RangedOptimalDistance * std::cos(angle);
    float const y     = target->GetPositionY() + RangedOptimalDistance * std::sin(angle);
    float const z     = target->GetPositionZ();
    bot->GetMotionMaster()->MovePoint(0, x, y, z);
}

// ---------------------------------------------------------------
// Per-role combat ticks
// ---------------------------------------------------------------

void TickHealer(Player* bot, Player* owner)
{
    if (bot->IsNonMeleeSpellCast(false))
        return;

    // Power Word: Shield (Priest only): proactive absorb while the owner is in
    // combat. Applied as soon as the fight starts so damage is partially absorbed
    // before reactive heals are needed. Weakened Soul prevents re-shielding for
    // 15 seconds after the absorb is consumed.
    if (bot->getClass() == CLASS_PRIEST && owner->IsInCombat())
    {
        std::uint32_t const pws = FindBestKnownSpellInChain(bot, 17);
        if (pws && !owner->HasAura(pws) && !owner->HasAura(AuraWeakenedSoul))
        {
            bot->CastSpell(owner, pws, false);
            return;
        }
    }

    std::uint32_t const directSpell    = GetDirectHealSpell(bot);
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

// ---------------------------------------------------------------
// Assist target resolution
// ---------------------------------------------------------------

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

// ---------------------------------------------------------------
// Main tick
// ---------------------------------------------------------------

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
            if (!BotHasManaToFight(bot))
            {
                // OOM: close to melee and autoattack until mana returns rather
                // than wasting every tick on failed cast attempts.
                EnsureChasingVictim(bot, assistTarget);
            }
            else if (bot->GetDistance(assistTarget) < RangedMinDistance)
            {
                // Target closed to melee range: retreat before resuming casts.
                EnsureRangedPosition(bot, assistTarget);
            }
            else
            {
                TickRanged(bot, assistTarget);
            }
        }
        else // Melee
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

    // No combat target — apply out-of-combat maintenance buffs, then follow
    // if the bot has drifted. Guard MoveFollow against interrupting an active
    // cast started by TryApplyOutOfCombatBuff.
    TryApplyOutOfCombatBuff(bot, owner);

    if (!bot->IsNonMeleeSpellCast(false)
        && !bot->IsWithinDistInMap(owner, RepositionDistance))
        bot->GetMotionMaster()->MoveFollow(owner, FollowDistance, FollowAngle);
}

// ---------------------------------------------------------------
// Event class
// ---------------------------------------------------------------

class CompanionAIEvent final : public BasicEvent
{
public:
    CompanionAIEvent(ObjectGuid botGuid, ObjectGuid ownerGuid)
        : _botGuid(botGuid), _ownerGuid(ownerGuid)
    {
    }

    bool Execute(uint64, uint32) override
    {
        Player* bot   = ObjectAccessor::FindPlayer(_botGuid);
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
