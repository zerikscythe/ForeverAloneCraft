#include "ai/WorldBotCreatureAI.h"
#include "Creature.h"
#include "DataStores/DBCStores.h"
#include "Globals/ObjectMgr.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "service/WorldBotCriticalStrikeBaseline.h"
#include "service/WorldBotDefensiveCombatBaseline.h"
#include "service/WorldBotExpertiseBaseline.h"
#include "service/WorldBotMeleeHitBaseline.h"
#include "service/WorldBotSpellCriticalStrikeBaseline.h"
#include "service/WorldBotSpellHitBaseline.h"
#include "service/WorldBotSpellPowerBaseline.h"

#include <algorithm>
#include <cmath>

namespace living_world
{
namespace script
{
namespace
{
std::uint32_t GetWorldBotScriptId()
{
    static std::uint32_t const scriptId = sObjectMgr->GetScriptId("worldbot_ai");
    return scriptId;
}

ai::WorldBotCreatureAI const* GetWorldBotCreatureAI(Unit const* victim)
{
    Creature const* creature = victim ? victim->ToCreature() : nullptr;
    if (!creature || creature->GetScriptId() != GetWorldBotScriptId() || !creature->AI())
        return nullptr;

    return static_cast<ai::WorldBotCreatureAI const*>(creature->AI());
}

float ResolveWorldBotDodgeRatio(std::uint8_t classId, std::uint8_t level)
{
    if (classId == 0 || classId > MAX_CLASSES)
        return 0.0f;

    std::uint8_t const clampedLevel = std::clamp<std::uint8_t>(level, 1, GT_MAX_LEVEL);
    GtChanceToMeleeCritEntry const* dodgeRatio =
        sGtChanceToMeleeCritStore.LookupEntry((classId - 1) * GT_MAX_LEVEL + clampedLevel - 1);
    return dodgeRatio ? dodgeRatio->ratio : 0.0f;
}

float ResolveWorldBotCombatRatingBonus(Unit const* unit, CombatRating rating, std::int32_t ratingValue)
{
    if (!unit || ratingValue == 0)
        return 0.0f;

    std::uint8_t const classId = unit->getClass();
    if (classId == 0 || classId > MAX_CLASSES)
        return 0.0f;

    std::uint8_t const clampedLevel = std::clamp<std::uint8_t>(unit->GetLevel(), 1, GT_MAX_LEVEL);
    GtCombatRatingsEntry const* ratingEntry =
        sGtCombatRatingsStore.LookupEntry(rating * GT_MAX_LEVEL + clampedLevel - 1);
    GtOCTClassCombatRatingScalarEntry const* classRating =
        sGtOCTClassCombatRatingScalarStore.LookupEntry((classId - 1) * GT_MAX_RATING + rating + 1);
    if (!ratingEntry || !classRating)
        return static_cast<float>(ratingValue);

    return static_cast<float>(ratingValue) * classRating->ratio / ratingEntry->ratio;
}

std::int32_t ChancePctToBasisPoints(float chancePct)
{
    return static_cast<std::int32_t>(std::lround(chancePct * 100.0f));
}

service::WorldBotCriticalStrikeBaseline ResolveWorldBotCriticalStrikeBaseline(Unit const* attacker)
{
    if (!attacker)
        return {};

    std::uint8_t const classId = attacker->getClass();
    if (classId == 0 || classId > MAX_CLASSES)
        return {};

    std::uint8_t const clampedLevel = std::clamp<std::uint8_t>(attacker->GetLevel(), 1, GT_MAX_LEVEL);
    GtChanceToMeleeCritBaseEntry const* critBase = sGtChanceToMeleeCritBaseStore.LookupEntry(classId - 1);
    GtChanceToMeleeCritEntry const* critRatio =
        sGtChanceToMeleeCritStore.LookupEntry((classId - 1) * GT_MAX_LEVEL + clampedLevel - 1);
    if (!critBase || !critRatio)
        return {};

    return service::BuildWorldBotCriticalStrikeBaseline(
        attacker->GetStat(STAT_AGILITY),
        critBase->base,
        critRatio->ratio);
}

service::WorldBotSpellCriticalStrikeBaseline ResolveWorldBotSpellCriticalStrikeBaseline(Unit const* attacker)
{
    if (!attacker)
        return {};

    std::uint8_t const classId = attacker->getClass();
    if (classId == 0 || classId > MAX_CLASSES)
        return {};

    std::uint8_t const clampedLevel = std::clamp<std::uint8_t>(attacker->GetLevel(), 1, GT_MAX_LEVEL);
    GtChanceToSpellCritBaseEntry const* critBase = sGtChanceToSpellCritBaseStore.LookupEntry(classId - 1);
    GtChanceToSpellCritEntry const* critRatio =
        sGtChanceToSpellCritStore.LookupEntry((classId - 1) * GT_MAX_LEVEL + clampedLevel - 1);
    if (!critBase || !critRatio)
        return {};

    return service::BuildWorldBotSpellCriticalStrikeBaseline(
        attacker->GetStat(STAT_INTELLECT),
        critBase->base,
        critRatio->ratio);
}
} // namespace

class WorldBotDefensiveCombatUnitScript final : public UnitScript
{
public:
    WorldBotDefensiveCombatUnitScript()
        : UnitScript("WorldBotDefensiveCombatUnitScript")
    {
    }

    bool OnCalculateSpellDoneCritChance(
        Unit const* attacker,
        Unit const* /*victim*/,
        SpellInfo const* spellProto,
        SpellSchoolMask schoolMask,
        WeaponAttackType /*attackType*/,
        bool /*skipEffectCheck*/,
        float& critChance) override
    {
        if (!GetWorldBotCreatureAI(attacker) || !spellProto)
            return false;

        if (spellProto->DmgClass != SPELL_DAMAGE_CLASS_MAGIC)
            return false;

        if (schoolMask & SPELL_SCHOOL_MASK_NORMAL)
        {
            critChance = 0.0f;
            return true;
        }

        service::WorldBotSpellCriticalStrikeBaseline const critBaseline =
            ResolveWorldBotSpellCriticalStrikeBaseline(attacker);
        critChance = service::AdjustWorldBotSpellCriticalStrikeChance(
            static_cast<float>(attacker->m_baseSpellCritChance),
            critBaseline.critChance);
        critChance += static_cast<float>(attacker->GetTotalAuraModifierByMiscMask(
            SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL,
            schoolMask));
        return true;
    }

    void OnCalculateMagicSpellHitChance(
        Unit const* attacker,
        Unit const* /*victim*/,
        SpellInfo const* spellInfo,
        int32& hitChance) override
    {
        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(attacker);
        if (!worldBotAI || !spellInfo)
            return;

        model::WorldBotAssignedGearSummary const& gearSummary = worldBotAI->GetAssignedGearSummary();
        hitChance = service::AdjustWorldBotMagicSpellHitChance(
            hitChance,
            ResolveWorldBotCombatRatingBonus(attacker, CR_HIT_SPELL, gearSummary.bonusSpellHitRating));
    }

    void OnCalculateSpellBaseDamageBonusDone(
        Unit const* attacker,
        SpellSchoolMask schoolMask,
        int32& doneAdvertisedBenefit) override
    {
        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(attacker);
        if (!worldBotAI)
            return;

        service::ApplyWorldBotSpellPowerBonus(
            schoolMask,
            worldBotAI->GetAssignedGearSummary().bonusSpellPower,
            doneAdvertisedBenefit);
    }

    void OnCalculateSpellBaseHealingBonusDone(
        Unit const* attacker,
        SpellSchoolMask /*schoolMask*/,
        int32& advertisedBenefit) override
    {
        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(attacker);
        if (!worldBotAI)
            return;

        service::ApplyWorldBotHealingPowerBonus(
            worldBotAI->GetAssignedGearSummary().bonusHealingPower,
            advertisedBenefit);
    }

    void OnBeforeRollMeleeOutcomeAgainst(
        Unit const* attacker,
        Unit const* victim,
        WeaponAttackType attType,
        int32& /*attackerMaxSkillValueForLevel*/,
        int32& victimMaxSkillValueForLevel,
        int32& /*attackerWeaponSkill*/,
        int32& victimDefenseSkill,
        int32& crit_chance,
        int32& miss_chance,
        int32& dodge_chance,
        int32& parry_chance,
        int32& block_chance) override
    {
        if (ai::WorldBotCreatureAI const* attackerWorldBotAI = GetWorldBotCreatureAI(attacker))
        {
            service::WorldBotCriticalStrikeBaseline const critBaseline =
                ResolveWorldBotCriticalStrikeBaseline(attacker);
            crit_chance = service::AdjustWorldBotCriticalStrikeChance(crit_chance, critBaseline.critChance);

            model::WorldBotAssignedGearSummary const& gearSummary =
                attackerWorldBotAI->GetAssignedGearSummary();
            CombatRating const hitRating = attType == RANGED_ATTACK
                ? CR_HIT_RANGED
                : CR_HIT_MELEE;
            std::int32_t const assignedHitRating = attType == RANGED_ATTACK
                ? gearSummary.bonusRangedHitRating
                : gearSummary.bonusMeleeHitRating;
            miss_chance = service::AdjustWorldBotMeleeMissChance(
                miss_chance,
                ResolveWorldBotCombatRatingBonus(attacker, hitRating, assignedHitRating));

            if (attType != RANGED_ATTACK)
            {
                float const expertiseBonus = ResolveWorldBotCombatRatingBonus(
                    attacker,
                    CR_EXPERTISE,
                    gearSummary.bonusExpertiseRating);
                dodge_chance = service::AdjustWorldBotDodgeOrParryChanceForExpertise(
                    dodge_chance,
                    expertiseBonus);
                parry_chance = service::AdjustWorldBotDodgeOrParryChanceForExpertise(
                    parry_chance,
                    expertiseBonus);
            }
        }

        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(victim);
        if (!worldBotAI || !victim)
            return;

        model::WorldBotAssignedGearSummary const& gearSummary = worldBotAI->GetAssignedGearSummary();
        float const defenseRatingBonus =
            ResolveWorldBotCombatRatingBonus(victim, CR_DEFENSE_SKILL, gearSummary.bonusDefenseSkillRating);
        float const dodgeRatingBonus =
            ResolveWorldBotCombatRatingBonus(victim, CR_DODGE, gearSummary.bonusDodgeRating);
        float const parryRatingBonus =
            ResolveWorldBotCombatRatingBonus(victim, CR_PARRY, gearSummary.bonusParryRating);
        float const blockRatingBonus =
            ResolveWorldBotCombatRatingBonus(victim, CR_BLOCK, gearSummary.bonusBlockRating);

        service::WorldBotDefensiveCombatBaseline const baseline =
            service::BuildWorldBotDefensiveCombatBaseline(
                victim->getClass(),
                victim->GetStat(STAT_AGILITY),
                ResolveWorldBotDodgeRatio(victim->getClass(), victim->GetLevel()),
                worldBotAI->HasShieldBaseline(),
                defenseRatingBonus,
                dodgeRatingBonus,
                parryRatingBonus,
                blockRatingBonus);

        dodge_chance = service::AdjustWorldBotDefensiveChance(dodge_chance, baseline.dodgeChance);
        parry_chance = service::AdjustWorldBotDefensiveChance(parry_chance, baseline.parryChance);
        block_chance = service::AdjustWorldBotDefensiveChance(block_chance, baseline.blockChance);

        std::int32_t const defenseSkillBonus = static_cast<std::int32_t>(defenseRatingBonus);
        victimDefenseSkill = victimMaxSkillValueForLevel + defenseSkillBonus;

        miss_chance += ChancePctToBasisPoints(
            service::BuildWorldBotDefenseMissChanceBonusPct(victim->getClass(), defenseRatingBonus));
        crit_chance = std::max(
            0,
            crit_chance - ChancePctToBasisPoints(
                service::BuildWorldBotDefenseCritSuppressionPct(defenseRatingBonus)));
    }
};
} // namespace script
} // namespace living_world

void AddSC_WorldBotDefensiveCombatUnitScript()
{
    new living_world::script::WorldBotDefensiveCombatUnitScript();
}
