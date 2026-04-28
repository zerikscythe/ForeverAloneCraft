#include "ai/CompanionAI.h"

#include "Duration.h"
#include "EventProcessor.h"
#include "Log.h"
#include "MotionMaster.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
#include "Unit.h"

#include <array>
#include <chrono>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace living_world
{
namespace ai
{

// ---------------------------------------------------------------
// Per-bot command override state
// ---------------------------------------------------------------

struct BotOverride
{
    ObjectGuid forcedTarget;                                         // Empty = no forced target
    bool       disengaged    = false;
    std::chrono::steady_clock::time_point disengageExpiry = {};      // Zero = never expires
    std::chrono::steady_clock::time_point retreatExpiry   = {};      // Zero = not retreating
};

static std::mutex                                    s_overrideMutex;
static std::unordered_map<ObjectGuid, BotOverride>   s_overrides;

static BotOverride GetOverride(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    auto it = s_overrides.find(botGuid);
    return it != s_overrides.end() ? it->second : BotOverride{};
}

static void ModifyOverride(ObjectGuid botGuid, std::function<void(BotOverride&)> fn)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    fn(s_overrides[botGuid]);
    // Clean up empty entries
    auto it = s_overrides.find(botGuid);
    if (it != s_overrides.end() && !it->second.forcedTarget && !it->second.disengaged)
        s_overrides.erase(it);
}

void SetBotForcedTarget(ObjectGuid botGuid, ObjectGuid targetGuid)
{
    ModifyOverride(botGuid, [&](BotOverride& o) {
        o.forcedTarget = targetGuid;
        o.disengaged   = false; // attacking clears disengage
    });
}

void SetBotDisengaged(ObjectGuid botGuid, bool disengaged)
{
    ModifyOverride(botGuid, [&](BotOverride& o) {
        o.disengaged      = disengaged;
        o.forcedTarget    = ObjectGuid::Empty; // disengaging clears forced target
        // Auto-expire after 500ms so a subsequent r-click re-enables assist.
        o.disengageExpiry = disengaged
            ? std::chrono::steady_clock::now() + std::chrono::milliseconds(500)
            : std::chrono::steady_clock::time_point{};
    });
}

void ClearBotOverride(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    s_overrides.erase(botGuid);
}

bool SetBotRetreat(ObjectGuid botGuid, uint32_t durationMs)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    BotOverride& o = s_overrides[botGuid];
    auto const now = std::chrono::steady_clock::now();
    if (o.retreatExpiry > now)
    {
        // Already retreating — cancel.
        o.retreatExpiry = {};
        return false;
    }
    o.retreatExpiry = now + std::chrono::milliseconds(durationMs);
    // Disengage/forced target cleared while retreating.
    o.disengaged    = false;
    o.forcedTarget  = ObjectGuid::Empty;
    return true;
}

bool IsBotRetreating(ObjectGuid botGuid)
{
    std::lock_guard<std::mutex> lock(s_overrideMutex);
    auto it = s_overrides.find(botGuid);
    if (it == s_overrides.end())
        return false;
    return it->second.retreatExpiry > std::chrono::steady_clock::now();
}

namespace
{
// --- Follow / reposition constants ---
constexpr float FollowDistance        = 2.0f;
constexpr float FollowAngle           = 3.14159265358979323846f;
constexpr float RepositionDistance    = 8.0f;
constexpr float RangedMinDistance     = 8.0f;    // Back away when closer than this
constexpr float RangedOptimalDistance = 25.0f;   // Target spacing for ranged bots
constexpr float RangedCastRange      = 30.0f;   // Approach target when farther than this
constexpr float RangedRetreatDistance = 5.0f;    // Short backstep when hurt in melee range
constexpr float RangedRetreatTrigger  = 80.0f;   // Retreat when HP drops below this %
constexpr float RangedRetreatReset    = 60.0f;   // Allow another retreat only after HP drops below this %

// --- Heal thresholds ---
constexpr float HealOwnerCritical    = 50.0f;
constexpr float HealOwnerModerate    = 85.0f;
constexpr float HealSelfCritical     = 40.0f;
constexpr float HealSelfModerate     = 65.0f;
constexpr float HybridHealThreshold  = 70.0f;

// --- Healer mana thresholds for hybrid offense ---
constexpr float HealerManaConserveBelow = 40.0f;  // Stop attacking below this
constexpr float HealerManaResumeAbove   = 60.0f;  // Resume attacking above this

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

// Offensive spells available to pure healers when mana allows.
std::uint32_t GetHealerOffensiveSpell(Player* bot, Unit* target)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
        {
            // Shadow Word: Pain — instant DoT, apply when missing
            std::uint32_t const swp = FindBestKnownSpellInChain(bot, 589);
            if (swp && !HasAuraFromChain(target, 589))
                return swp;
            // Mind Blast — direct shadow nuke
            std::uint32_t const mb = FindBestKnownSpellInChain(bot, 8092);
            if (mb && !bot->HasSpellCooldown(mb))
                return mb;
            // Smite — holy filler when Mind Blast is on cooldown
            return FindBestKnownSpellInChain(bot, 585);
        }
        default:
            return 0;
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
            if (bot->HasSpell(49998) && !bot->HasSpellCooldown(49998))
                return 49998;

            // Death Strike is on cooldown — fill with rune strikes
            {
                std::uint32_t const heartStrike = FindBestKnownSpellInChain(bot, 55050);
                if (heartStrike && !bot->HasSpellCooldown(heartStrike))
                    return heartStrike; // Heart Strike
            }
            if (bot->HasSpell(45902) && !bot->HasSpellCooldown(45902))
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
                && !bot->HasSpellCooldown(how))
                return how;

            // Judgement of Light: holy damage + party heal proc on hit
            std::uint32_t const jol = FindBestKnownSpellInChain(bot, 20271);
            if (jol && !bot->HasSpellCooldown(jol))
                return jol;

            // Consecration: sustained AoE holy damage field
            std::uint32_t const cons = FindBestKnownSpellInChain(bot, 20116);
            if (cons && !bot->HasSpellCooldown(cons))
                return cons;

            // Crusader Strike: primary single-target melee ability
            if (bot->HasSpell(35395) && !bot->HasSpellCooldown(35395))
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
        {
            // Walk the Frostbolt chain; if chain lookup fails (e.g. rank data
            // missing), fall back to direct spell ID checks for common ranks.
            std::uint32_t fb = FindBestKnownSpellInChain(bot, 116);
            if (fb)
                return fb;
            // Direct fallback: Frostbolt ranks 1-14 in reverse order
            static constexpr std::uint32_t FrostboltRanks[] = {
                42842, 42841, 38697, 27072, 25304, 10161, 10160, 10159,
                8406,  8405,  8404,  837,   228,   116
            };
            for (std::uint32_t id : FrostboltRanks)
                if (bot->HasSpell(id))
                    return id;
            // No Frostbolt — try Fireball as alternate
            fb = FindBestKnownSpellInChain(bot, 133);
            if (fb)
                return fb;
            // Frostfire Bolt (dual-school, learned via talent)
            if (bot->HasSpell(44614))
                return 44614;
            return 0;
        }

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
            if (multiShot && !bot->HasSpellCooldown(multiShot))
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

// Core buff application — no combat guard. Called by both the idle tick and
// the explicit party buff command.
void ApplyBotBuff(Player* bot, Player* owner)
{
    if (bot->IsNonMeleeSpellCast(false))
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
                { bot->CastSpell(bot, seal, false); break; }
            }
            // Blessing of Kings: prioritise owner, then self
            {
                std::uint32_t const bok = FindBestKnownSpellInChain(bot, 20217);
                if (bok)
                {
                    if (!HasAuraFromChain(owner, 20217))
                    { bot->CastSpell(owner, bok, false); break; }
                    if (!HasAuraFromChain(bot, 20217))
                    { bot->CastSpell(bot, bok, false); break; }
                }
            }
            // Blessing of Might: fallback if Kings not known
            {
                std::uint32_t const bom = FindBestKnownSpellInChain(bot, 19740);
                if (bom)
                {
                    if (!HasAuraFromChain(owner, 19740))
                    { bot->CastSpell(owner, bom, false); break; }
                    if (!HasAuraFromChain(bot, 19740))
                    { bot->CastSpell(bot, bom, false); break; }
                }
            }
            break;
        }

        case CLASS_PRIEST:
        {
            // Power Word: Fortitude: Stamina buff for all group members
            std::uint32_t const pwf = FindBestKnownSpellInChain(bot, 1243);
            if (!pwf) break;
            if (Group const* group = bot->GetGroup())
            {
                for (Group::MemberSlot const& slot : group->GetMemberSlots())
                {
                    Player* target = ObjectAccessor::FindConnectedPlayer(slot.guid);
                    if (!target || !target->IsAlive() || !target->IsInWorld()) continue;
                    if (!HasAuraFromChain(target, 1243))
                    { bot->CastSpell(target, pwf, false); return; }
                }
            }
            if (!HasAuraFromChain(bot, 1243))
                bot->CastSpell(bot, pwf, false);
            break;
        }

        case CLASS_DRUID:
        {
            // Mark of the Wild: multi-stat buff for all group members
            std::uint32_t const motw = FindBestKnownSpellInChain(bot, 1126);
            if (!motw) break;
            if (Group const* group = bot->GetGroup())
            {
                for (Group::MemberSlot const& slot : group->GetMemberSlots())
                {
                    Player* target = ObjectAccessor::FindConnectedPlayer(slot.guid);
                    if (!target || !target->IsAlive() || !target->IsInWorld()) continue;
                    if (!HasAuraFromChain(target, 1126))
                    { bot->CastSpell(target, motw, false); return; }
                }
            }
            if (!HasAuraFromChain(bot, 1126))
                bot->CastSpell(bot, motw, false);
            break;
        }

        case CLASS_MAGE:
        {
            // Arcane Intellect: Intellect buff for all group members
            std::uint32_t const ai = FindBestKnownSpellInChain(bot, 1459);
            if (!ai) break;
            if (Group const* group = bot->GetGroup())
            {
                for (Group::MemberSlot const& slot : group->GetMemberSlots())
                {
                    Player* target = ObjectAccessor::FindConnectedPlayer(slot.guid);
                    if (!target || !target->IsAlive() || !target->IsInWorld()) continue;
                    if (!HasAuraFromChain(target, 1459))
                    { bot->CastSpell(target, ai, false); return; }
                }
            }
            if (!HasAuraFromChain(bot, 1459))
                bot->CastSpell(bot, ai, false);
            break;
        }

        case CLASS_WARLOCK:
        {
            // Fel Armor preferred; fall back to Demon Armor / Demon Skin — self-only
            std::uint32_t armor = FindBestKnownSpellInChain(bot, 28176); // Fel Armor
            if (!armor) armor   = FindBestKnownSpellInChain(bot, 706);   // Demon Armor
            if (!armor) armor   = FindBestKnownSpellInChain(bot, 696);   // Demon Skin
            if (armor && !bot->HasAura(armor))
                bot->CastSpell(bot, armor, false);
            break;
        }

        default:
            break;
    }
}

void TryApplyOutOfCombatBuff(Player* bot, Player* owner)
{
    if (bot->IsInCombat() || owner->IsInCombat())
        return;
    ApplyBotBuff(bot, owner);
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
// Short backstep (RangedRetreatDistance yards) away from the target when the
// bot has taken significant damage in melee range. A small fixed step avoids
// wall/cliff traps that a full 25y retreat would cause.
void EnsureRangedPosition(Player* bot, Unit* target)
{
    if (bot->GetDistance(target) >= RangedMinDistance)
        return;

    // Angle pointing from target toward the bot — step further that way
    float const angle = target->GetAngle(bot);
    float const x     = bot->GetPositionX() + RangedRetreatDistance * std::cos(angle);
    float const y     = bot->GetPositionY() + RangedRetreatDistance * std::sin(angle);
    float const z     = bot->GetPositionZ();
    bot->GetMotionMaster()->MovePoint(0, x, y, z);
}

// Closes a ranged bot to RangedOptimalDistance when it is too far to cast.
// MoveChase with an explicit stop distance lets the engine handle pathing and
// stops the bot at the right spot without overshooting into melee range.
void EnsureRangedApproach(Player* bot, Unit* target)
{
    bot->GetMotionMaster()->MoveChase(target, RangedOptimalDistance);
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
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] RangedAI cast blocked: bot='{}' guid={} targetGuid={} reason=already_casting",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            target ? target->GetGUID().GetCounter() : 0);
        return;
    }

    std::uint32_t const spell = GetDamageSpell(bot, target);
    if (!spell)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] RangedAI cast blocked: bot='{}' guid={} class={} targetGuid={} distance={:.2f} mana={}/{} reason=no_spell_selected",
            bot->GetName(),
            bot->GetGUID().GetCounter(),
            static_cast<std::uint32_t>(bot->getClass()),
            target ? target->GetGUID().GetCounter() : 0,
            target ? bot->GetDistance(target) : 0.0f,
            static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
            static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)));
        return;
    }

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] RangedAI cast attempt: bot='{}' guid={} class={} spell={} targetGuid={} distance={:.2f} mana={}/{} victimGuid={}",
        bot->GetName(),
        bot->GetGUID().GetCounter(),
        static_cast<std::uint32_t>(bot->getClass()),
        spell,
        target ? target->GetGUID().GetCounter() : 0,
        target ? bot->GetDistance(target) : 0.0f,
        static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
        static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)),
        bot->GetVictim() ? bot->GetVictim()->GetGUID().GetCounter() : 0);

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

// Resolve the unit a bot should be fighting right now, honouring any
// explicit player override before falling back to normal assist logic.
Unit* ResolveAssistTarget(Player* bot, Player* owner)
{
    BotOverride const ovr = GetOverride(bot->GetGUID());

    auto const now = std::chrono::steady_clock::now();

    // Retreat mode: bot follows and heals only — no combat at all.
    if (ovr.retreatExpiry > now)
        return nullptr;

    // If the player ordered disengage, hold — but auto-expire after 500ms so
    // a subsequent r-click (owner attacks → mob agros back → bot assists) works.
    if (ovr.disengaged)
    {
        if (now < ovr.disengageExpiry)
            return nullptr;
        // Expired — clear the flag and fall through to normal assist.
        ClearBotOverride(bot->GetGUID());
    }

    // If the player ordered a specific target, use it while it's valid.
    if (ovr.forcedTarget)
    {
        Unit* forced = ObjectAccessor::GetUnit(*bot, ovr.forcedTarget);
        if (forced && IsValidAssistTarget(owner, forced))
            return forced;
        // Target gone — clear the override and fall through.
        SetBotForcedTarget(bot->GetGUID(), ObjectGuid::Empty);
    }

    // Normal assist logic:
    // 1. Keep fighting the current victim while it's alive.
    if (Unit* current = bot->GetVictim())
    {
        if (IsValidAssistTarget(owner, current))
            return current;
    }

    // 2. Pick up owner's active victim only when that mob is fighting back —
    //    i.e. the mob's current victim is the owner. This prevents the bot from
    //    chasing a mob the owner merely auto-attacked once but that hasn't
    //    aggroed yet or that the owner accidentally clicked.
    if (Unit* ownerVictim = owner->GetVictim())
    {
        if (IsValidAssistTarget(owner, ownerVictim)
            && ownerVictim->GetVictim() == owner)
            return ownerVictim;
    }

    return nullptr;
}

// ---------------------------------------------------------------
// Main tick
// ---------------------------------------------------------------

void Tick(Player* bot, Player* owner, float& retreatHpPct, bool& healerConserving)
{
    BotCombatRole const role = GetCombatRole(bot->getClass());

    // Pure healers: heal first, then optionally attack based on mana.
    if (role == BotCombatRole::Healer)
    {
        TickHealer(bot, owner);

        // Hysteresis: stop attacking below 40% mana, resume above 60%.
        if (bot->GetMaxPower(POWER_MANA) > 0)
        {
            float const manaPct = 100.0f * static_cast<float>(bot->GetPower(POWER_MANA))
                                         / static_cast<float>(bot->GetMaxPower(POWER_MANA));
            if (healerConserving)
            {
                if (manaPct >= HealerManaResumeAbove)
                    healerConserving = false;
            }
            else if (manaPct < HealerManaConserveBelow)
            {
                healerConserving = true;
            }
        }

        // Attempt an offensive spell if not already casting and mana is healthy.
        if (!healerConserving && !bot->IsNonMeleeSpellCast(false))
        {
            Unit* const attackTarget = ResolveAssistTarget(bot, owner);
            if (attackTarget)
            {
                std::uint32_t const spell = GetHealerOffensiveSpell(bot, attackTarget);
                if (spell)
                    bot->CastSpell(attackTarget, spell, false);
            }
        }

        TryApplyOutOfCombatBuff(bot, owner);

        if (!bot->IsNonMeleeSpellCast(false)
            && bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        {
            bot->GetMotionMaster()->Clear(false);
            bot->GetMotionMaster()->MoveFollow(owner, FollowDistance, FollowAngle);
        }
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

            // Ensure seal is active before striking
            if (bot->getClass() == CLASS_PALADIN && !HasSealActive(bot)
                && !bot->IsNonMeleeSpellCast(false))
            {
                std::uint32_t const seal = GetPreferredSeal(bot);
                if (seal)
                {
                    bot->CastSpell(bot, seal, false);
                    EnsureChasingVictim(bot, assistTarget);
                    return;
                }
            }

            EnsureChasingVictim(bot, assistTarget);
            std::uint32_t const spell = GetHybridDamageSpell(bot, assistTarget);
            if (spell && !bot->IsNonMeleeSpellCast(false))
                bot->CastSpell(assistTarget, spell, false);
            return;
        }

        if (role == BotCombatRole::Ranged)
        {
            // Attack(false) sets the victim and enters combat without issuing
            // MoveChase, which would interrupt an in-progress cast every tick.
            // We drive positioning ourselves via EnsureRangedApproach/Position.
            if (bot->GetVictim() != assistTarget)
                bot->Attack(assistTarget, false);

            float const distance = bot->GetDistance(assistTarget);

            if (!BotHasManaToFight(bot))
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] RangedAI decision: bot='{}' guid={} targetGuid={} distance={:.2f} mana={}/{} action=oom_chase",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    assistTarget->GetGUID().GetCounter(),
                    distance,
                    static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
                    static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)));

                // OOM: close to melee and autoattack until mana returns rather
                // than wasting every tick on failed cast attempts.
                EnsureChasingVictim(bot, assistTarget);
            }
            else if (distance < RangedMinDistance
                && bot->GetHealthPct() < retreatHpPct)
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] RangedAI decision: bot='{}' guid={} targetGuid={} distance={:.2f} mana={}/{} action=retreat hp={:.1f}",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    assistTarget->GetGUID().GetCounter(),
                    distance,
                    static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
                    static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)),
                    bot->GetHealthPct());

                // Step back 5y away from the target. Set the next retreat
                // threshold to 60% so another retreat can only fire once the
                // bot has taken more sustained damage.
                EnsureRangedPosition(bot, assistTarget);
                retreatHpPct = RangedRetreatReset;
            }
            else if (distance > RangedCastRange)
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] RangedAI decision: bot='{}' guid={} targetGuid={} distance={:.2f} mana={}/{} action=approach",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    assistTarget->GetGUID().GetCounter(),
                    distance,
                    static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
                    static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)));

                // Target is beyond spell range: close to optimal distance.
                EnsureRangedApproach(bot, assistTarget);
            }
            else
            {
                LOG_INFO(
                    "server.worldserver",
                    "[LivingWorldDebug] RangedAI decision: bot='{}' guid={} targetGuid={} distance={:.2f} mana={}/{} action=cast_window",
                    bot->GetName(),
                    bot->GetGUID().GetCounter(),
                    assistTarget->GetGUID().GetCounter(),
                    distance,
                    static_cast<std::uint32_t>(bot->GetPower(POWER_MANA)),
                    static_cast<std::uint32_t>(bot->GetMaxPower(POWER_MANA)));

                TickRanged(bot, assistTarget);
            }
        }
        else // Melee
        {
            if (bot->GetVictim() != assistTarget)
                bot->Attack(assistTarget, true);

            EnsureChasingVictim(bot, assistTarget);
            TickMelee(bot, assistTarget);
        }

        return;
    }

    if (bot->GetVictim())
    {
        bot->AttackStop();
        bot->GetMotionMaster()->Clear(false);
        bot->GetMotionMaster()->MoveFollow(owner, FollowDistance, FollowAngle);
        return;
    }

    // No combat target — apply out-of-combat maintenance buffs, then resume
    // following the owner. Only (re-)issue MoveFollow when not already in follow
    // mode, to avoid killing the active follow generator every 500ms tick which
    // causes the bot to appear frozen or to stutter.
    TryApplyOutOfCombatBuff(bot, owner);

    if (!bot->IsNonMeleeSpellCast(false)
        && bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
    {
        bot->GetMotionMaster()->Clear(false);
        bot->GetMotionMaster()->MoveFollow(owner, FollowDistance, FollowAngle);
    }
}

// ---------------------------------------------------------------
// Event class
// ---------------------------------------------------------------

class CompanionAIEvent final : public BasicEvent
{
public:
    CompanionAIEvent(ObjectGuid botGuid, ObjectGuid ownerGuid,
                     std::uint8_t notInWorldRetries = 0,
                     float retreatHpPct = RangedRetreatTrigger,
                     bool healerConserving = false)
        : _botGuid(botGuid), _ownerGuid(ownerGuid)
        , _notInWorldRetries(notInWorldRetries), _retreatHpPct(retreatHpPct)
        , _healerConserving(healerConserving)
    {
    }

    bool Execute(uint64, uint32) override
    {
        Player* bot   = ObjectAccessor::FindPlayer(_botGuid);
        Player* owner = ObjectAccessor::FindConnectedPlayer(_ownerGuid);
        if (!bot || !owner)
            return true;

        if (!bot->IsInWorld() || !owner->IsInWorld())
        {
            if (_notInWorldRetries >= MaxNotInWorldRetries)
                return true;

            // Backoff: 500ms, 1s, 2s, 4s, 4s, 4s, ...
            Milliseconds const delay = 500ms * (1u << std::min(_notInWorldRetries, std::uint8_t{3}));
            bot->m_Events.AddEventAtOffset(
                new CompanionAIEvent(_botGuid, _ownerGuid, _notInWorldRetries + 1, _retreatHpPct, _healerConserving),
                delay);
            return true;
        }

        // Reset retreat threshold if the bot has healed back above the trigger level.
        if (_retreatHpPct < RangedRetreatTrigger && bot && bot->GetHealthPct() >= RangedRetreatTrigger)
            _retreatHpPct = RangedRetreatTrigger;

        Tick(bot, owner, _retreatHpPct, _healerConserving);
        bot->m_Events.AddEventAtOffset(
            new CompanionAIEvent(_botGuid, _ownerGuid, 0, _retreatHpPct, _healerConserving),
            500ms);
        return true;
    }

private:
    static constexpr std::uint8_t MaxNotInWorldRetries = 20;

    ObjectGuid   _botGuid;
    ObjectGuid   _ownerGuid;
    std::uint8_t _notInWorldRetries;
    float        _retreatHpPct;
    bool         _healerConserving;
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

void ForceBotBuffRefresh(Player* bot, Player* owner)
{
    if (!bot || !owner)
        return;
    ApplyBotBuff(bot, owner);
}
} // namespace ai
} // namespace living_world
