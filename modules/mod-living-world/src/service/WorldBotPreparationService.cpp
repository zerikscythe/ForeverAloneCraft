#include "service/WorldBotPreparationService.h"

#include "Globals/ObjectMgr.h"
#include "Log.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "DataStores/DBCStores.h"
#include "integration/BotCombatDefaultProfileRepository.h"
#include "integration/BotTalentTemplateRepository.h"
#include "integration/BotVirtualLoadoutRepository.h"
#include "model/BotCombatProfile.h"
#include "model/BotSpecKey.h"
#include "model/BotTalentTemplate.h"
#include "service/WorldBotTalentAllocationRules.h"

#include <algorithm>
#include <string_view>

namespace living_world
{
namespace service
{
namespace
{
std::string_view ToDoctrineClassKey(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_WARRIOR:
            return "Warrior";
        case CLASS_PALADIN:
            return "Paladin";
        case CLASS_HUNTER:
            return "Hunter";
        case CLASS_ROGUE:
            return "Rogue";
        case CLASS_PRIEST:
            return "Priest";
        case CLASS_DEATH_KNIGHT:
            return "Death Knight";
        case CLASS_SHAMAN:
            return "Shaman";
        case CLASS_MAGE:
            return "Mage";
        case CLASS_WARLOCK:
            return "Warlock";
        case CLASS_DRUID:
            return "Druid";
        default:
            return "";
    }
}

bool IsSpellUsableForLevel(std::uint32_t spellId, std::uint8_t level)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    std::uint32_t const requiredLevel = std::max(spellInfo->SpellLevel, spellInfo->BaseLevel);
    return requiredLevel == 0 || requiredLevel <= level;
}

void AddKnownSpellForAction(
    std::unordered_set<std::uint32_t>& knownSpells,
    model::BotCombatActionDefinition const& action,
    std::uint8_t level)
{
    if (action.actionType != model::BotCombatActionType::Spell || action.spellBaseId == 0)
        return;

    switch (action.rankMode)
    {
        case model::BotCombatRankMode::ExactSpellId:
            if (IsSpellUsableForLevel(action.spellBaseId, level))
                knownSpells.insert(action.spellBaseId);
            return;

        case model::BotCombatRankMode::SpecificRank:
        {
            if (action.rankValue == 0)
                return;

            std::uint32_t candidate = sSpellMgr->GetFirstSpellInChain(action.spellBaseId);
            if (!candidate)
                candidate = action.spellBaseId;

            for (std::uint8_t rank = 1; candidate; ++rank)
            {
                if (rank == action.rankValue)
                {
                    if (IsSpellUsableForLevel(candidate, level))
                        knownSpells.insert(candidate);
                    return;
                }

                candidate = sSpellMgr->GetNextSpellInChain(candidate);
            }

            return;
        }

        case model::BotCombatRankMode::BestKnown:
        {
            std::uint32_t candidate = sSpellMgr->GetLastSpellInChain(action.spellBaseId);
            if (!candidate)
                candidate = action.spellBaseId;

            while (candidate)
            {
                if (IsSpellUsableForLevel(candidate, level))
                {
                    knownSpells.insert(candidate);
                    return;
                }

                candidate = sSpellMgr->GetPrevSpellInChain(candidate);
            }

            return;
        }
    }
}

void AddProfileSpells(
    std::unordered_set<std::uint32_t>& knownSpells,
    model::BotCombatDefaultProfileRecord const& defaultProfile,
    std::uint8_t level)
{
    auto addEntries =
        [&](std::vector<model::BotCombatEntryDefinition> const& entries)
        {
            for (model::BotCombatEntryDefinition const& entry : entries)
            {
                AddKnownSpellForAction(knownSpells, entry.primaryAction, level);
                if (entry.secondaryAction)
                    AddKnownSpellForAction(knownSpells, *entry.secondaryAction, level);
            }
        };

    addEntries(defaultProfile.interruptEntries);
    addEntries(defaultProfile.rotationEntries);
}

void AddPlayerCreateInfoSpells(
    std::unordered_set<std::uint32_t>& knownSpells,
    PlayerInfo const& playerInfo,
    std::uint8_t level)
{
    for (std::uint32_t spellId : playerInfo.customSpells)
    {
        if (spellId != 0 && IsSpellUsableForLevel(spellId, level))
            knownSpells.insert(spellId);
    }
}

void AddClassSkillLineSpells(
    std::unordered_set<std::uint32_t>& knownSpells,
    PlayerInfo const& playerInfo,
    std::uint8_t level)
{
    std::uint16_t const approximatedSkillValue = static_cast<std::uint16_t>(level) * 5u;

    for (PlayerCreateInfoSkill const& skill : playerInfo.skills)
    {
        auto const& abilities = GetSkillLineAbilitiesBySkillLine(skill.SkillId);
        for (SkillLineAbilityEntry const* ability : abilities)
        {
            if (!ability || ability->Spell == 0)
                continue;

            if (ability->AcquireMethod != SKILL_LINE_ABILITY_LEARNED_ON_SKILL_VALUE
                && ability->AcquireMethod != SKILL_LINE_ABILITY_LEARNED_ON_SKILL_LEARN)
            {
                continue;
            }

            if (ability->MinSkillLineRank > approximatedSkillValue)
                continue;

            if (IsSpellUsableForLevel(ability->Spell, level))
                knownSpells.insert(ability->Spell);
        }
    }
}

std::uint32_t GetPlayerClassMask(std::uint8_t classId)
{
    if (classId == 0)
        return 0;

    return 1u << (classId - 1);
}

WorldBotTalentDefinition BuildWorldBotTalentDefinition(TalentEntry const& talent)
{
    WorldBotTalentDefinition definition;
    definition.talentId = static_cast<std::uint16_t>(talent.TalentID);
    definition.talentTabId = talent.TalentTab;
    definition.row = static_cast<std::uint8_t>(talent.Row);
    definition.dependsOnTalentId = static_cast<std::uint16_t>(talent.DependsOn);
    definition.dependsOnRequiredRank = static_cast<std::uint8_t>(talent.DependsOnRank + 1u);
    definition.rankSpellIds = talent.RankID;

    if (TalentTabEntry const* talentTab = sTalentTabStore.LookupEntry(talent.TalentTab))
        definition.classMask = talentTab->ClassMask;

    return definition;
}

std::vector<model::WorldBotPreparedTalentEntry> AllocateTalents(
    std::uint8_t classId,
    model::BotTalentTemplateRecord const& talentTemplate,
    std::uint8_t availableTalentPoints,
    std::uint8_t& allocatedTalentPoints)
{
    std::vector<WorldBotTalentDefinition> talentDefinitions;
    talentDefinitions.reserve(talentTemplate.entries.size());

    for (model::BotTalentTemplateEntry const& entry : talentTemplate.entries)
    {
        TalentEntry const* talent = sTalentStore.LookupEntry(entry.talentId);
        if (!talent)
            continue;

        talentDefinitions.push_back(BuildWorldBotTalentDefinition(*talent));
    }

    return AllocateWorldBotTalents(
        talentTemplate.entries,
        talentDefinitions,
        GetPlayerClassMask(classId),
        availableTalentPoints,
        allocatedTalentPoints);
}

void AddAllocatedTalentSpells(
    std::unordered_set<std::uint32_t>& knownSpells,
    std::vector<model::WorldBotPreparedTalentEntry> const& allocatedTalents,
    std::uint8_t level)
{
    for (model::WorldBotPreparedTalentEntry const& entry : allocatedTalents)
    {
        if (entry.grantedSpellId == 0 || !IsSpellUsableForLevel(entry.grantedSpellId, level))
            continue;

        knownSpells.insert(entry.grantedSpellId);

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(entry.grantedSpellId);
        if (!spellInfo)
            continue;

        for (std::uint8_t effectIndex = 0; effectIndex < MAX_SPELL_EFFECTS; ++effectIndex)
        {
            if (spellInfo->Effects[effectIndex].Effect != SPELL_EFFECT_LEARN_SPELL)
                continue;

            std::uint32_t const triggerSpellId = spellInfo->Effects[effectIndex].TriggerSpell;
            if (triggerSpellId == 0 || !sSpellMgr->IsAdditionalTalentSpell(triggerSpellId))
                continue;

            if (IsSpellUsableForLevel(triggerSpellId, level))
                knownSpells.insert(triggerSpellId);
        }
    }
}
} // namespace

WorldBotPreparationService::WorldBotPreparationService(
    integration::BotCombatDefaultProfileRepository const& defaultProfileRepository,
    integration::BotTalentTemplateRepository const& talentTemplateRepository,
    integration::BotVirtualLoadoutRepository const& virtualLoadoutRepository)
    : _defaultProfileRepository(defaultProfileRepository)
    , _talentTemplateRepository(talentTemplateRepository)
    , _virtualLoadoutRepository(virtualLoadoutRepository)
{
}

model::WorldBotPreparedBuild WorldBotPreparationService::Prepare(
    integration::BotIdentityRecord const& identity,
    std::string const& contextKey) const
{
    model::WorldBotPreparedBuild prepared;
    prepared.identityId = identity.id;
    prepared.raceId = identity.raceId;
    prepared.classId = identity.classId;
    prepared.level = identity.level;
    prepared.personalityKey = identity.personalityKey.empty() ? "uninterested" : identity.personalityKey;
    prepared.requestedLoadoutKey = identity.loadoutKey;
    prepared.contextKey = contextKey;
    prepared.canonicalSpecKey = model::CanonicalizeBotSpecKey(identity.specKey);
    prepared.resolvedRoleKey = ResolveRoleKey(identity.classId, prepared.canonicalSpecKey);
    prepared.availableTalentPoints = ComputeAvailableTalentPoints(identity.level);

    LOG_INFO("server.worldserver",
        "[LivingWorld] WorldBotPreparation identity={} personality='{}' spec_canonical='{}' role='{}' loadout='{}' class={} level={}",
        prepared.identityId,
        prepared.personalityKey,
        prepared.canonicalSpecKey,
        prepared.resolvedRoleKey,
        prepared.requestedLoadoutKey,
        static_cast<std::uint32_t>(prepared.classId),
        static_cast<std::uint32_t>(prepared.level));

    auto defaultProfile = _defaultProfileRepository.FindDefaultProfile(
        prepared.canonicalSpecKey,
        prepared.resolvedRoleKey,
        std::string(ToDoctrineClassKey(identity.classId)),
        contextKey,
        prepared.requestedLoadoutKey);

    if (!defaultProfile && contextKey != "PvE")
    {
        defaultProfile = _defaultProfileRepository.FindDefaultProfile(
            prepared.canonicalSpecKey,
            prepared.resolvedRoleKey,
            std::string(ToDoctrineClassKey(identity.classId)),
            "PvE",
            prepared.requestedLoadoutKey);
        if (defaultProfile)
            prepared.contextKey = "PvE";
    }

    if (!defaultProfile)
    {
        prepared.status = model::WorldBotPreparationStatus::MissingDefaultCombatProfile;
        prepared.failureReason = "missing_default_combat_profile";
        LOG_WARN("server.worldserver",
            "[LivingWorld] WorldBotPreparation identity={} failed reason={} spec='{}' role='{}' class='{}' context='{}'",
            prepared.identityId,
            prepared.failureReason,
            prepared.canonicalSpecKey,
            prepared.resolvedRoleKey,
            ToDoctrineClassKey(identity.classId),
            contextKey);
        return prepared;
    }

    prepared.defaultCombatProfileId = defaultProfile->defaultProfileId;
    prepared.defaultCombatProfileName = defaultProfile->displayName;
    prepared.defaultCombatProfileVariantKey = defaultProfile->variantKey;
    prepared.defaultCombatProfileDescription = defaultProfile->description;
    prepared.resolvedRoleKey = defaultProfile->roleKey;
    LOG_INFO("server.worldserver",
        "[LivingWorld] WorldBotPreparation identity={} default_profile_id={} profile='{}' variant='{}' context='{}'",
        prepared.identityId,
        prepared.defaultCombatProfileId,
        prepared.defaultCombatProfileName,
        prepared.defaultCombatProfileVariantKey,
        prepared.contextKey);

    auto talentTemplate = _talentTemplateRepository.FindTemplateForSpec(
        prepared.canonicalSpecKey,
        identity.classId,
        prepared.requestedLoadoutKey);
    if (!talentTemplate)
    {
        prepared.status = model::WorldBotPreparationStatus::MissingTalentTemplate;
        prepared.failureReason = "missing_talent_template";
        LOG_WARN("server.worldserver",
            "[LivingWorld] WorldBotPreparation identity={} failed reason={} spec='{}' class={} default_profile_id={}",
            prepared.identityId,
            prepared.failureReason,
            prepared.canonicalSpecKey,
            static_cast<std::uint32_t>(identity.classId),
            prepared.defaultCombatProfileId);
        return prepared;
    }

    prepared.talentTemplateId = talentTemplate->templateId;
    prepared.talentTemplateName = talentTemplate->displayName;
    prepared.talentTemplateVariantKey = talentTemplate->variantKey;
    prepared.talentTemplateDescription = talentTemplate->description;
    LOG_INFO("server.worldserver",
        "[LivingWorld] WorldBotPreparation identity={} talent_template_id={} template='{}' variant='{}'",
        prepared.identityId,
        prepared.talentTemplateId,
        prepared.talentTemplateName,
        prepared.talentTemplateVariantKey);

    prepared.allocatedTalents = AllocateTalents(
        identity.classId,
        *talentTemplate,
        prepared.availableTalentPoints,
        prepared.allocatedTalentPoints);
    LOG_INFO("server.worldserver",
        "[LivingWorld] WorldBotPreparation identity={} talent_points={} allocated_points={} allocated_talents={}",
        prepared.identityId,
        static_cast<std::uint32_t>(prepared.availableTalentPoints),
        static_cast<std::uint32_t>(prepared.allocatedTalentPoints),
        prepared.allocatedTalents.size());

    prepared.virtualLoadout = _virtualLoadoutRepository.FindLoadout(
        identity.classId,
        prepared.canonicalSpecKey,
        prepared.requestedLoadoutKey,
        std::max<std::uint8_t>(identity.gearTier, 1u));

    if (!prepared.virtualLoadout)
    {
        LOG_WARN("server.worldserver",
            "[LivingWorld] WorldBotPreparation identity={} missing_virtual_loadout class={} spec='{}' loadout='{}' gear_tier={}",
            prepared.identityId,
            static_cast<std::uint32_t>(identity.classId),
            prepared.canonicalSpecKey,
            prepared.requestedLoadoutKey,
            static_cast<std::uint32_t>(std::max<std::uint8_t>(identity.gearTier, 1u)));
    }

    PlayerInfo const* playerInfo = sObjectMgr->GetPlayerInfo(identity.raceId, identity.classId);
    if (!playerInfo)
    {
        prepared.status = model::WorldBotPreparationStatus::MissingPlayerInfo;
        prepared.failureReason = "missing_player_info";
        LOG_WARN("server.worldserver",
            "[LivingWorld] WorldBotPreparation identity={} failed reason={} race={} class={}",
            prepared.identityId,
            prepared.failureReason,
            static_cast<std::uint32_t>(identity.raceId),
            static_cast<std::uint32_t>(identity.classId));
        return prepared;
    }

    AddPlayerCreateInfoSpells(prepared.knownSpellIds, *playerInfo, identity.level);
    AddClassSkillLineSpells(prepared.knownSpellIds, *playerInfo, identity.level);
    AddAllocatedTalentSpells(prepared.knownSpellIds, prepared.allocatedTalents, identity.level);
    AddProfileSpells(prepared.knownSpellIds, *defaultProfile, identity.level);

    prepared.status = model::WorldBotPreparationStatus::Ready;
    LOG_INFO("server.worldserver",
        "[LivingWorld] WorldBotPreparation identity={} known_spells={} virtual_loadout={} gear_tier={} ready=1",
        prepared.identityId,
        prepared.knownSpellIds.size(),
        prepared.virtualLoadout ? prepared.virtualLoadout->displayName : "none",
        static_cast<std::uint32_t>(std::max<std::uint8_t>(identity.gearTier, 1u)));
    return prepared;
}

std::string WorldBotPreparationService::ResolveRoleKey(
    std::uint8_t classId,
    std::string const& specKey)
{
    if (classId == CLASS_PRIEST)
    {
        if (specKey == "Holy" || specKey == "Discipline")
            return "HEAL";
        return "DPS";
    }

    if (classId == CLASS_PALADIN)
    {
        if (specKey == "Holy")
            return "HEAL";
        if (specKey == "Protection")
            return "TANK";
        return "DPS";
    }

    if (classId == CLASS_DRUID)
    {
        if (specKey == "Restoration")
            return "HEAL";
        if (specKey == "Feral")
            return "TANK";
        return "DPS";
    }

    if (classId == CLASS_SHAMAN)
    {
        if (specKey == "Restoration")
            return "HEAL";
        return "DPS";
    }

    if (classId == CLASS_WARRIOR)
        return specKey == "Protection" ? "TANK" : "DPS";

    if (classId == CLASS_DEATH_KNIGHT)
        return specKey == "Blood" ? "TANK" : "DPS";

    return "DPS";
}

std::uint8_t WorldBotPreparationService::ComputeAvailableTalentPoints(std::uint8_t level)
{
    if (level <= 9)
        return 0;

    return static_cast<std::uint8_t>(level - 9);
}
} // namespace service
} // namespace living_world