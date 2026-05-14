#include "ai/WorldBotCreatureAI.h"
#include "Creature.h"
#include "DataStores/DBCStores.h"
#include "Globals/ObjectMgr.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "service/WorldBotCriticalStrikeBaseline.h"
#include "service/WorldBotDefensiveCombatBaseline.h"
#include "service/WorldBotSpellCriticalStrikeBaseline.h"

#include <algorithm>

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

    void OnBeforeRollMeleeOutcomeAgainst(
        Unit const* attacker,
        Unit const* victim,
        WeaponAttackType /*attType*/,
        int32& /*attackerMaxSkillValueForLevel*/,
        int32& /*victimMaxSkillValueForLevel*/,
        int32& /*attackerWeaponSkill*/,
        int32& /*victimDefenseSkill*/,
        int32& crit_chance,
        int32& /*miss_chance*/,
        int32& dodge_chance,
        int32& parry_chance,
        int32& block_chance) override
    {
        if (ai::WorldBotCreatureAI const* attackerWorldBotAI = GetWorldBotCreatureAI(attacker))
        {
            (void)attackerWorldBotAI;
            service::WorldBotCriticalStrikeBaseline const critBaseline =
                ResolveWorldBotCriticalStrikeBaseline(attacker);
            crit_chance = service::AdjustWorldBotCriticalStrikeChance(crit_chance, critBaseline.critChance);
        }

        ai::WorldBotCreatureAI const* worldBotAI = GetWorldBotCreatureAI(victim);
        if (!worldBotAI || !victim)
            return;

        service::WorldBotDefensiveCombatBaseline const baseline =
            service::BuildWorldBotDefensiveCombatBaseline(
                victim->getClass(),
                victim->GetStat(STAT_AGILITY),
                ResolveWorldBotDodgeRatio(victim->getClass(), victim->GetLevel()),
                worldBotAI->HasShieldBaseline());

        dodge_chance = service::AdjustWorldBotDefensiveChance(dodge_chance, baseline.dodgeChance);
        parry_chance = service::AdjustWorldBotDefensiveChance(parry_chance, baseline.parryChance);
        block_chance = service::AdjustWorldBotDefensiveChance(block_chance, baseline.blockChance);
    }
};
} // namespace script
} // namespace living_world

void AddSC_WorldBotDefensiveCombatUnitScript()
{
    new living_world::script::WorldBotDefensiveCombatUnitScript();
}