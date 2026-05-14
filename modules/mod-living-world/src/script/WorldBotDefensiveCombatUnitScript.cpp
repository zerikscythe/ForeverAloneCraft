#include "ai/WorldBotCreatureAI.h"
#include "Creature.h"
#include "DataStores/DBCStores.h"
#include "Globals/ObjectMgr.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "World.h"
#include "service/WorldBotArmorPenetrationBaseline.h"
#include "service/WorldBotBlockValueBaseline.h"
#include "service/WorldBotCombatRatingBaseline.h"
#include "service/WorldBotCriticalStrikeBaseline.h"
#include "service/WorldBotDefensiveCombatBaseline.h"
#include "service/WorldBotExpertiseBaseline.h"
#include "service/WorldBotManaRegenBaseline.h"
#include "service/WorldBotMeleeHitBaseline.h"
#include "service/WorldBotResilienceBaseline.h"
#include "service/WorldBotSpellCriticalStrikeBaseline.h"
#include "service/WorldBotSpellHitBaseline.h"
#include "service/WorldBotSpellPenetrationBaseline.h"
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

bool IsWorldBotArmorPenetrationAuraApplicable(AuraEffect const* auraEffect, SpellInfo const* spellInfo)
{
    if (!auraEffect || !auraEffect->GetSpellInfo())
        return false;

    SpellInfo const* auraSpellInfo = auraEffect->GetSpellInfo();
    if (auraSpellInfo->EquippedItemClass != -1)
        return false;

    if (!spellInfo || auraEffect->IsAffectedOnSpell(spellInfo) || auraEffect->GetMiscValue() & spellInfo->GetSchoolMask())
        return true;

    return !auraEffect->GetMiscValue() && !auraEffect->HasSpellClassMask();
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
        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(attacker);
        if (!worldBotAI || !spellProto)
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
        critChance = service::AddWorldBotSpellCriticalStrikeRatingBonus(
            critChance,
            service::ResolveWorldBotCombatRatingBonus(
                attacker,
                CR_CRIT_SPELL,
                worldBotAI->GetAssignedGearSummary().bonusSpellCritRating));
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
            service::ResolveWorldBotCombatRatingBonus(attacker, CR_HIT_SPELL, gearSummary.bonusSpellHitRating));
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

    void OnCalculatePowerRegen(Unit* unit, Powers power, float& addValue) override
    {
        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(unit);
        if (!worldBotAI || !unit || power != POWER_MANA)
            return;

        if (unit->HasAuraTypeWithMiscvalue(SPELL_AURA_PREVENT_REGENERATE_POWER, POWER_MANA + 1))
        {
            addValue = 0.0f;
            return;
        }

        std::uint8_t const classId = unit->getClass();
        if (classId == 0 || classId > MAX_CLASSES)
            return;

        std::uint8_t const clampedLevel = std::clamp<std::uint8_t>(unit->GetLevel(), 1, GT_MAX_LEVEL);
        GtRegenMPPerSptEntry const* regenRatio =
            sGtRegenMPPerSptStore.LookupEntry((classId - 1) * GT_MAX_LEVEL + clampedLevel - 1);
        if (!regenRatio)
            return;

        float const spiritRegen = service::BuildWorldBotSpiritManaRegenPerSecond(
            unit->GetStat(STAT_INTELLECT),
            unit->GetStat(STAT_SPIRIT),
            regenRatio->ratio,
            unit->GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA));

        float mp5Regen = static_cast<float>(
            unit->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA)
                + worldBotAI->GetAssignedGearSummary().bonusManaRegen) / 5.0f;

        Unit::AuraEffectList const& regenAuras = unit->GetAuraEffectsByType(SPELL_AURA_MOD_MANA_REGEN_FROM_STAT);
        for (AuraEffect const* auraEffect : regenAuras)
        {
            if (!auraEffect)
                continue;

            mp5Regen += unit->GetStat(Stats(auraEffect->GetMiscValue()))
                * static_cast<float>(auraEffect->GetAmount())
                / 500.0f;
        }

        addValue = service::BuildWorldBotManaRegenPerTick(
            spiritRegen,
            mp5Regen,
            static_cast<float>(unit->GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT)),
            unit->IsUnderLastManaUseEffect(),
            sWorld->getRate(RATE_POWER_MANA),
            CREATURE_REGEN_INTERVAL);
    }

    void OnCalculateShieldBlockValue(Unit const* unit, uint32& blockValue) override
    {
        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(unit);
        if (!worldBotAI || !unit)
            return;

        model::WorldBotAssignedGearSummary const& gearSummary = worldBotAI->GetAssignedGearSummary();
        float const flatBlockValue =
            static_cast<float>(gearSummary.bonusBlockValue)
                + static_cast<float>(unit->GetTotalAuraModifier(SPELL_AURA_MOD_SHIELD_BLOCKVALUE));

        blockValue = service::BuildWorldBotShieldBlockValue(
            worldBotAI->HasShieldBaseline(),
            unit->GetStat(STAT_STRENGTH),
            flatBlockValue,
            unit->GetTotalAuraMultiplier(SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT));
    }

    void OnCalculateArmorForDamageReduction(
        Unit const* attacker,
        Unit const* victim,
        SpellInfo const* spellInfo,
        float& armor) override
    {
        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(attacker);
        if (!worldBotAI || !attacker || !victim)
            return;

        float bonusPct = service::ResolveWorldBotCombatRatingBonus(
            attacker,
            CR_ARMOR_PENETRATION,
            worldBotAI->GetAssignedGearSummary().bonusArmorPenetrationRating);

        bonusPct += attacker->GetTotalAuraModifier(
            SPELL_AURA_MOD_ARMOR_PENETRATION_PCT,
            [spellInfo](AuraEffect const* auraEffect)
            {
                return IsWorldBotArmorPenetrationAuraApplicable(auraEffect, spellInfo);
            });

        armor = service::ApplyWorldBotArmorPenetration(armor, victim->GetLevel(), bonusPct);
    }

    void OnCalculateEffectiveResistance(
        Unit const* owner,
        SpellSchoolMask schoolMask,
        Unit const* /*victim*/,
        SpellInfo const* /*spellInfo*/,
        float& victimResistance) override
    {
        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(owner);
        if (!worldBotAI)
            return;

        victimResistance = service::ApplyWorldBotSpellPenetration(
            schoolMask,
            victimResistance,
            worldBotAI->GetAssignedGearSummary().bonusSpellPenetration);
    }

    void OnApplyResilience(
        Unit const* victim,
        float* crit,
        int32* damage,
        bool isCrit,
        CombatRating type) override
    {
        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(victim);
        if (!worldBotAI || !victim)
            return;

        if (type != CR_CRIT_TAKEN_MELEE && type != CR_CRIT_TAKEN_RANGED && type != CR_CRIT_TAKEN_SPELL)
            return;

        model::WorldBotAssignedGearSummary const& gearSummary = worldBotAI->GetAssignedGearSummary();
        float const resiliencePct = service::ResolveWorldBotCombatRatingBonus(
            victim,
            type,
            gearSummary.bonusResilienceRating + gearSummary.bonusCritTakenRating);

        service::ApplyWorldBotResilience(resiliencePct, crit, damage, isCrit);
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
            CombatRating const critRating = attType == RANGED_ATTACK
                ? CR_CRIT_RANGED
                : CR_CRIT_MELEE;
            std::int32_t const assignedCritRating = attType == RANGED_ATTACK
                ? gearSummary.bonusRangedCritRating
                : gearSummary.bonusMeleeCritRating;
            crit_chance = service::AddWorldBotCriticalStrikeRatingBonus(
                crit_chance,
                service::ResolveWorldBotCombatRatingBonus(attacker, critRating, assignedCritRating));

            CombatRating const hitRating = attType == RANGED_ATTACK
                ? CR_HIT_RANGED
                : CR_HIT_MELEE;
            std::int32_t const assignedHitRating = attType == RANGED_ATTACK
                ? gearSummary.bonusRangedHitRating
                : gearSummary.bonusMeleeHitRating;
            miss_chance = service::AdjustWorldBotMeleeMissChance(
                miss_chance,
                service::ResolveWorldBotCombatRatingBonus(attacker, hitRating, assignedHitRating));

            if (attType != RANGED_ATTACK)
            {
                float const expertiseBonus = service::ResolveWorldBotCombatRatingBonus(
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
            service::ResolveWorldBotCombatRatingBonus(victim, CR_DEFENSE_SKILL, gearSummary.bonusDefenseSkillRating);
        float const dodgeRatingBonus =
            service::ResolveWorldBotCombatRatingBonus(victim, CR_DODGE, gearSummary.bonusDodgeRating);
        float const parryRatingBonus =
            service::ResolveWorldBotCombatRatingBonus(victim, CR_PARRY, gearSummary.bonusParryRating);
        float const blockRatingBonus =
            service::ResolveWorldBotCombatRatingBonus(victim, CR_BLOCK, gearSummary.bonusBlockRating);

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
