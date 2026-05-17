#include "integration/SqlBotCombatProfileRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

#include <unordered_map>

namespace living_world
{
namespace integration
{
namespace
{
model::BotCombatConservationMode FromDbConservationMode(std::uint8_t value)
{
    switch (value)
    {
        case 0:
            return model::BotCombatConservationMode::FullForce;
        case 1:
            return model::BotCombatConservationMode::Reserve;
        case 3:
            return model::BotCombatConservationMode::JitCasting;
        case 2:
        default:
            return model::BotCombatConservationMode::Conservative;
    }
}

model::BotCombatAoEMode FromDbAoEMode(std::uint8_t value)
{
    switch (value)
    {
        case 1:
            return model::BotCombatAoEMode::Feet;
        case 0:
        default:
            return model::BotCombatAoEMode::Centroid;
    }
}

model::BotCombatTargetingMode FromDbTargetingMode(std::uint8_t value)
{
    switch (value)
    {
        case 1:
            return model::BotCombatTargetingMode::Assist;
        case 2:
            return model::BotCombatTargetingMode::Skirmish;
        case 0:
        default:
            return model::BotCombatTargetingMode::Standard;
    }
}

model::BotCombatActionType FromDbActionType(std::uint8_t value)
{
    switch (value)
    {
        case 1:
            return model::BotCombatActionType::Item;
        case 0:
        default:
            return model::BotCombatActionType::Spell;
    }
}

model::BotCombatRankMode FromDbRankMode(std::uint8_t value)
{
    switch (value)
    {
        case 1:
            return model::BotCombatRankMode::ExactSpellId;
        case 2:
            return model::BotCombatRankMode::SpecificRank;
        case 0:
        default:
            return model::BotCombatRankMode::BestKnown;
    }
}

model::BotCombatConditionLogic FromDbConditionLogic(std::uint8_t value)
{
    switch (value)
    {
        case 1:
            return model::BotCombatConditionLogic::Any;
        case 0:
        default:
            return model::BotCombatConditionLogic::All;
    }
}

model::BotCombatConditionOperator FromDbConditionOperator(std::uint8_t value)
{
    switch (value)
    {
        case 0:
            return model::BotCombatConditionOperator::Equal;
        case 1:
            return model::BotCombatConditionOperator::NotEqual;
        case 2:
            return model::BotCombatConditionOperator::LessThan;
        case 3:
            return model::BotCombatConditionOperator::LessThanOrEqual;
        case 5:
            return model::BotCombatConditionOperator::GreaterThanOrEqual;
        case 6:
            return model::BotCombatConditionOperator::Has;
        case 7:
            return model::BotCombatConditionOperator::NotHas;
        case 8:
            return model::BotCombatConditionOperator::Exists;
        case 4:
        default:
            return model::BotCombatConditionOperator::GreaterThan;
    }
}

std::string QuoteSql(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return "'" + value + "'";
}

std::string QuoteOptionalString(std::optional<std::string> const& value)
{
    if (!value || value->empty())
        return "NULL";

    std::string escaped = *value;
    CharacterDatabase.EscapeString(escaped);
    return "'" + escaped + "'";
}

model::BotCombatProfileSettings BuildSettings(Field const* fields, std::size_t offset)
{
    model::BotCombatProfileSettings settings;
    settings.conservationMode = FromDbConservationMode(fields[offset + 0].Get<std::uint8_t>());
    settings.resourceLowWater = fields[offset + 1].Get<std::uint8_t>();
    settings.resourceHighWater = fields[offset + 2].Get<std::uint8_t>();
    settings.enableDownRank = fields[offset + 3].Get<bool>();
    settings.downRankFloor = fields[offset + 4].Get<std::uint8_t>();
    settings.defaultAoEMode = FromDbAoEMode(fields[offset + 5].Get<std::uint8_t>());
    settings.defaultAoEMinTargets = fields[offset + 6].Get<std::uint8_t>();
    settings.defaultAoEScanRadius = fields[offset + 7].Get<float>();
    settings.targeting.mode = FromDbTargetingMode(fields[offset + 8].Get<std::uint8_t>());
    settings.targeting.currentTargetBias = fields[offset + 9].Get<float>();
    settings.targeting.assistTargetBias = fields[offset + 10].Get<float>();
    settings.targeting.focusFireBias = fields[offset + 11].Get<float>();
    settings.targeting.protectAllyBias = fields[offset + 12].Get<float>();
    settings.targeting.preferHealerBias = fields[offset + 13].Get<float>();
    settings.targeting.preferDpsBias = fields[offset + 14].Get<float>();
    settings.targeting.avoidTankBias = fields[offset + 15].Get<float>();
    return settings;
}

model::BotCombatActionDefinition BuildAction(Field const* fields)
{
    model::BotCombatActionDefinition action;
    action.actionId = fields[0].Get<std::uint64_t>();
    action.slot = fields[1].Get<std::uint8_t>();
    action.actionType = FromDbActionType(fields[2].Get<std::uint8_t>());
    action.spellBaseId = fields[3].Get<std::uint32_t>();
    action.itemId = fields[4].Get<std::uint32_t>();
    action.rankMode = FromDbRankMode(fields[5].Get<std::uint8_t>());
    action.rankValue = fields[6].Get<std::uint8_t>();
    action.targetKey = fields[7].Get<std::string>();
    if (!fields[8].IsNull())
        action.aoeMode = FromDbAoEMode(fields[8].Get<std::uint8_t>());
    if (!fields[9].IsNull())
        action.aoeMinTargets = fields[9].Get<std::uint8_t>();
    if (!fields[10].IsNull())
        action.aoeRadius = fields[10].Get<float>();
    return action;
}

model::BotCombatConditionDefinition BuildCondition(Field const* fields)
{
    model::BotCombatConditionDefinition condition;
    condition.conditionId = fields[0].Get<std::uint64_t>();
    condition.sequence = fields[1].Get<std::uint8_t>();
    condition.subjectKey = fields[2].Get<std::string>();
    condition.statKey = fields[3].Get<std::string>();
    condition.comparison = FromDbConditionOperator(fields[4].Get<std::uint8_t>());
    condition.numericValue = fields[5].Get<float>();
    condition.stringValue = fields[6].Get<std::string>();
    return condition;
}

model::BotCombatProfileRecord BuildProfile(Field const* fields)
{
    model::BotCombatProfileRecord profile;
    profile.profileId = fields[0].Get<std::uint64_t>();
    profile.sourceCharacterGuid = fields[1].Get<std::uint64_t>();
    profile.ownerAccountId = fields[2].Get<std::uint32_t>();
    profile.slot = fields[3].Get<std::uint8_t>();
    profile.profileName = fields[4].Get<std::string>();
    profile.guessedSpecKey = fields[5].Get<std::string>();
    profile.guessedRoleKey = fields[6].Get<std::string>();
    if (!fields[7].IsNull())
        profile.specOverrideKey = fields[7].Get<std::string>();
    if (!fields[8].IsNull())
        profile.roleOverrideKey = fields[8].Get<std::string>();
    profile.settings = BuildSettings(fields, 9);
    // OOC behavior — fields 24..32 after targeting settings columns
    // Graceful defaults if columns are absent (NULL fallback via COALESCE in SELECT).
    auto& ooc = profile.oocBehavior;
    ooc.buffScope         = static_cast<model::BotBuffScope>(fields[24].Get<std::uint8_t>());
    ooc.buffReapplySecs   = fields[25].Get<std::uint16_t>();
    ooc.buffOnSpawn       = fields[26].Get<bool>();
    if (!fields[27].IsNull())
        ooc.followDistOverride = fields[27].Get<float>();
    if (!fields[28].IsNull())
        ooc.autoLootOverride = fields[28].Get<std::uint8_t>() != 0;
    ooc.lootQualityMin    = fields[29].Get<std::uint8_t>();
    ooc.gatherNodes       = static_cast<model::BotGatherNodes>(fields[30].Get<std::uint8_t>());
    ooc.gatherSkin        = static_cast<model::BotGatherSkin>(fields[31].Get<std::uint8_t>());
    ooc.skinLootQualityMax= fields[32].Get<std::uint8_t>();
    ooc.lootCategoryFlags = fields[33].Get<std::uint32_t>();
    return profile;
}

void LoadProfileChildren(model::BotCombatProfileRecord& profile)
{
    std::unordered_map<std::uint64_t, std::size_t> interruptIndexByEntryId;
    std::unordered_map<std::uint64_t, std::size_t> rotationIndexByEntryId;

    QueryResult entryResult = CharacterDatabase.Query(
        "SELECT entry_id, priority, label, is_interrupt, breaks_current_cast, "
        "enabled, condition_logic "
        "FROM living_world_bot_combat_profile_entry "
        "WHERE profile_id = {} "
        "ORDER BY is_interrupt DESC, priority ASC, entry_id ASC",
        profile.profileId);
    if (entryResult)
    {
        do
        {
            Field const* fields = entryResult->Fetch();
            model::BotCombatEntryDefinition entry;
            entry.entryId = fields[0].Get<std::uint64_t>();
            entry.priority = fields[1].Get<std::uint8_t>();
            entry.label = fields[2].Get<std::string>();
            entry.isInterrupt = fields[3].Get<bool>();
            entry.breaksCurrentCast = fields[4].Get<bool>();
            entry.enabled = fields[5].Get<bool>();
            entry.conditionLogic = FromDbConditionLogic(fields[6].Get<std::uint8_t>());

            if (entry.isInterrupt)
            {
                interruptIndexByEntryId.emplace(entry.entryId, profile.interruptEntries.size());
                profile.interruptEntries.push_back(std::move(entry));
            }
            else
            {
                rotationIndexByEntryId.emplace(entry.entryId, profile.rotationEntries.size());
                profile.rotationEntries.push_back(std::move(entry));
            }
        } while (entryResult->NextRow());
    }

    auto findEntry = [&](std::uint64_t entryId) -> model::BotCombatEntryDefinition*
    {
        if (auto it = interruptIndexByEntryId.find(entryId); it != interruptIndexByEntryId.end())
            return &profile.interruptEntries[it->second];
        if (auto it = rotationIndexByEntryId.find(entryId); it != rotationIndexByEntryId.end())
            return &profile.rotationEntries[it->second];
        return nullptr;
    };

    QueryResult actionResult = CharacterDatabase.Query(
        "SELECT action_id, slot, action_type, spell_base_id, item_id, "
        "rank_mode, rank_value, target_key, aoe_mode, aoe_min_targets, aoe_radius, entry_id "
        "FROM living_world_bot_combat_profile_action "
        "WHERE entry_id IN ("
            "SELECT entry_id FROM living_world_bot_combat_profile_entry "
            "WHERE profile_id = {}) "
        "ORDER BY entry_id ASC, slot ASC",
        profile.profileId);
    if (actionResult)
    {
        do
        {
            Field const* fields = actionResult->Fetch();
            model::BotCombatActionDefinition action = BuildAction(fields);
            if (model::BotCombatEntryDefinition* entry = findEntry(fields[11].Get<std::uint64_t>()))
            {
                if (action.slot == 0)
                    entry->primaryAction = std::move(action);
                else
                    entry->secondaryAction = std::move(action);
            }
        } while (actionResult->NextRow());
    }

    QueryResult conditionResult = CharacterDatabase.Query(
        "SELECT condition_id, sequence, subject_key, stat_key, comparison, "
        "numeric_value, string_value, entry_id "
        "FROM living_world_bot_combat_profile_condition "
        "WHERE entry_id IN ("
            "SELECT entry_id FROM living_world_bot_combat_profile_entry "
            "WHERE profile_id = {}) "
        "ORDER BY entry_id ASC, sequence ASC",
        profile.profileId);
    if (conditionResult)
    {
        do
        {
            Field const* fields = conditionResult->Fetch();
            if (model::BotCombatEntryDefinition* entry = findEntry(fields[7].Get<std::uint64_t>()))
                entry->conditions.push_back(BuildCondition(fields));
        } while (conditionResult->NextRow());
    }
}

std::uint64_t ResolveProfileId(model::BotCombatProfileRecord const& profile)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT profile_id FROM living_world_bot_combat_profile "
        "WHERE owner_account_id = {} AND source_character_guid = {} AND slot = {} "
        "LIMIT 1",
        profile.ownerAccountId,
        profile.sourceCharacterGuid,
        static_cast<std::uint32_t>(profile.slot));
    if (!result)
        return 0;

    return result->Fetch()[0].Get<std::uint64_t>();
}

void InsertEntry(std::uint64_t profileId, model::BotCombatEntryDefinition const& entry)
{
    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_combat_profile_entry ("
        "profile_id, priority, label, is_interrupt, breaks_current_cast, enabled, condition_logic) "
        "VALUES ({}, {}, {}, {}, {}, {}, {})",
        profileId,
        static_cast<std::uint32_t>(entry.priority),
        QuoteSql(entry.label),
        entry.isInterrupt ? 1 : 0,
        entry.breaksCurrentCast ? 1 : 0,
        entry.enabled ? 1 : 0,
        static_cast<std::uint32_t>(entry.conditionLogic));

    QueryResult entryIdResult = CharacterDatabase.Query(
        "SELECT entry_id FROM living_world_bot_combat_profile_entry "
        "WHERE profile_id = {} AND priority = {} AND label = {} AND is_interrupt = {} "
        "ORDER BY entry_id DESC LIMIT 1",
        profileId,
        static_cast<std::uint32_t>(entry.priority),
        QuoteSql(entry.label),
        entry.isInterrupt ? 1 : 0);
    if (!entryIdResult)
        return;

    std::uint64_t entryId = entryIdResult->Fetch()[0].Get<std::uint64_t>();
    auto saveAction = [&](model::BotCombatActionDefinition const& action)
    {
        CharacterDatabase.Execute(
            "INSERT INTO living_world_bot_combat_profile_action ("
            "entry_id, slot, action_type, spell_base_id, item_id, rank_mode, rank_value, "
            "target_key, aoe_mode, aoe_min_targets, aoe_radius) "
            "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
            entryId,
            static_cast<std::uint32_t>(action.slot),
            static_cast<std::uint32_t>(action.actionType),
            action.spellBaseId,
            action.itemId,
            static_cast<std::uint32_t>(action.rankMode),
            static_cast<std::uint32_t>(action.rankValue),
            QuoteSql(action.targetKey),
            action.aoeMode ? std::to_string(static_cast<std::uint32_t>(*action.aoeMode)) : "NULL",
            action.aoeMinTargets ? std::to_string(static_cast<std::uint32_t>(*action.aoeMinTargets)) : "NULL",
            action.aoeRadius ? std::to_string(*action.aoeRadius) : "NULL");
    };

    saveAction(entry.primaryAction);
    if (entry.secondaryAction)
        saveAction(*entry.secondaryAction);

    for (model::BotCombatConditionDefinition const& condition : entry.conditions)
    {
        CharacterDatabase.Execute(
            "INSERT INTO living_world_bot_combat_profile_condition ("
            "entry_id, sequence, subject_key, stat_key, comparison, numeric_value, string_value) "
            "VALUES ({}, {}, {}, {}, {}, {}, {})",
            entryId,
            static_cast<std::uint32_t>(condition.sequence),
            QuoteSql(condition.subjectKey),
            QuoteSql(condition.statKey),
            static_cast<std::uint32_t>(condition.comparison),
            condition.numericValue,
            QuoteSql(condition.stringValue));
    }
}

void DeleteProfileChildren(std::uint64_t profileId)
{
    CharacterDatabase.Execute(
        "DELETE c FROM living_world_bot_combat_profile_condition c "
        "INNER JOIN living_world_bot_combat_profile_entry e ON e.entry_id = c.entry_id "
        "WHERE e.profile_id = {}",
        profileId);
    CharacterDatabase.Execute(
        "DELETE a FROM living_world_bot_combat_profile_action a "
        "INNER JOIN living_world_bot_combat_profile_entry e ON e.entry_id = a.entry_id "
        "WHERE e.profile_id = {}",
        profileId);
    CharacterDatabase.Execute(
        "DELETE FROM living_world_bot_combat_profile_entry WHERE profile_id = {}",
        profileId);
}
} // namespace

std::vector<model::BotCombatProfileRecord>
SqlBotCombatProfileRepository::ListProfilesForCharacter(
    std::uint32_t ownerAccountId,
    std::uint64_t sourceCharacterGuid) const
{
    std::vector<model::BotCombatProfileRecord> profiles;
    QueryResult result = CharacterDatabase.Query(
        "SELECT profile_id, source_character_guid, owner_account_id, slot, profile_name, "
        "guessed_spec_key, guessed_role_key, spec_override_key, role_override_key, "
        "conservation_mode, resource_low_water, resource_high_water, enable_down_rank, "
        "down_rank_floor, default_aoe_mode, default_aoe_min_targets, default_aoe_scan_radius, "
        "targeting_mode, current_target_bias, assist_target_bias, focus_fire_bias, protect_ally_bias, "
        "prefer_healer_bias, prefer_dps_bias, avoid_tank_bias, "
        "COALESCE(buff_scope,2), COALESCE(buff_reapply_secs,30), COALESCE(buff_on_spawn,1), "
        "follow_dist_override, auto_loot_override, COALESCE(loot_quality_min,0), "
        "COALESCE(gather_nodes,0), COALESCE(gather_skin,0), COALESCE(skin_loot_quality_max,0), "
        "COALESCE(loot_category_flags,0) "
        "FROM living_world_bot_combat_profile "
        "WHERE owner_account_id = {} AND source_character_guid = {} "
        "ORDER BY slot ASC",
        ownerAccountId,
        sourceCharacterGuid);
    if (!result)
        return profiles;

    do
    {
        model::BotCombatProfileRecord profile = BuildProfile(result->Fetch());
        LoadProfileChildren(profile);
        profiles.push_back(std::move(profile));
    } while (result->NextRow());

    return profiles;
}

std::optional<model::BotCombatProfileRecord>
SqlBotCombatProfileRepository::FindProfileForCharacterSlot(
    std::uint32_t ownerAccountId,
    std::uint64_t sourceCharacterGuid,
    std::uint8_t slot) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT profile_id, source_character_guid, owner_account_id, slot, profile_name, "
        "guessed_spec_key, guessed_role_key, spec_override_key, role_override_key, "
        "conservation_mode, resource_low_water, resource_high_water, enable_down_rank, "
        "down_rank_floor, default_aoe_mode, default_aoe_min_targets, default_aoe_scan_radius, "
        "targeting_mode, current_target_bias, assist_target_bias, focus_fire_bias, protect_ally_bias, "
        "prefer_healer_bias, prefer_dps_bias, avoid_tank_bias, "
        "COALESCE(buff_scope,2), COALESCE(buff_reapply_secs,30), COALESCE(buff_on_spawn,1), "
        "follow_dist_override, auto_loot_override, COALESCE(loot_quality_min,0), "
        "COALESCE(gather_nodes,0), COALESCE(gather_skin,0), COALESCE(skin_loot_quality_max,0), "
        "COALESCE(loot_category_flags,0) "
        "FROM living_world_bot_combat_profile "
        "WHERE owner_account_id = {} AND source_character_guid = {} AND slot = {} "
        "LIMIT 1",
        ownerAccountId,
        sourceCharacterGuid,
        static_cast<std::uint32_t>(slot));
    if (!result)
        return std::nullopt;

    model::BotCombatProfileRecord profile = BuildProfile(result->Fetch());
    LoadProfileChildren(profile);
    return profile;
}

void SqlBotCombatProfileRepository::SaveProfile(
    model::BotCombatProfileRecord const& profile)
{
    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_combat_profile ("
        "source_character_guid, owner_account_id, slot, profile_name, guessed_spec_key, "
        "guessed_role_key, spec_override_key, role_override_key, conservation_mode, "
        "resource_low_water, resource_high_water, enable_down_rank, down_rank_floor, default_aoe_mode, "
        "default_aoe_min_targets, default_aoe_scan_radius, targeting_mode, current_target_bias, assist_target_bias, "
        "focus_fire_bias, protect_ally_bias, prefer_healer_bias, prefer_dps_bias, avoid_tank_bias, "
        "buff_scope, buff_reapply_secs, buff_on_spawn, follow_dist_override, "
        "auto_loot_override, loot_quality_min, gather_nodes, gather_skin, skin_loot_quality_max, "
        "loot_category_flags) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "profile_name = VALUES(profile_name), "
        "guessed_spec_key = VALUES(guessed_spec_key), "
        "guessed_role_key = VALUES(guessed_role_key), "
        "spec_override_key = VALUES(spec_override_key), "
        "role_override_key = VALUES(role_override_key), "
        "conservation_mode = VALUES(conservation_mode), "
        "resource_low_water = VALUES(resource_low_water), "
        "resource_high_water = VALUES(resource_high_water), "
        "enable_down_rank = VALUES(enable_down_rank), "
        "down_rank_floor = VALUES(down_rank_floor), "
        "default_aoe_mode = VALUES(default_aoe_mode), "
        "default_aoe_min_targets = VALUES(default_aoe_min_targets), "
        "default_aoe_scan_radius = VALUES(default_aoe_scan_radius), "
        "targeting_mode = VALUES(targeting_mode), "
        "current_target_bias = VALUES(current_target_bias), "
        "assist_target_bias = VALUES(assist_target_bias), "
        "focus_fire_bias = VALUES(focus_fire_bias), "
        "protect_ally_bias = VALUES(protect_ally_bias), "
        "prefer_healer_bias = VALUES(prefer_healer_bias), "
        "prefer_dps_bias = VALUES(prefer_dps_bias), "
        "avoid_tank_bias = VALUES(avoid_tank_bias), "
        "buff_scope = VALUES(buff_scope), "
        "buff_reapply_secs = VALUES(buff_reapply_secs), "
        "buff_on_spawn = VALUES(buff_on_spawn), "
        "follow_dist_override = VALUES(follow_dist_override), "
        "auto_loot_override = VALUES(auto_loot_override), "
        "loot_quality_min = VALUES(loot_quality_min), "
        "gather_nodes = VALUES(gather_nodes), "
        "gather_skin = VALUES(gather_skin), "
        "skin_loot_quality_max = VALUES(skin_loot_quality_max), "
        "loot_category_flags = VALUES(loot_category_flags)",
        profile.sourceCharacterGuid,
        profile.ownerAccountId,
        static_cast<std::uint32_t>(profile.slot),
        QuoteSql(profile.profileName),
        QuoteSql(profile.guessedSpecKey),
        QuoteSql(profile.guessedRoleKey),
        QuoteOptionalString(profile.specOverrideKey),
        QuoteOptionalString(profile.roleOverrideKey),
        static_cast<std::uint32_t>(profile.settings.conservationMode),
        static_cast<std::uint32_t>(profile.settings.resourceLowWater),
        static_cast<std::uint32_t>(profile.settings.resourceHighWater),
        profile.settings.enableDownRank ? 1 : 0,
        static_cast<std::uint32_t>(profile.settings.downRankFloor),
        static_cast<std::uint32_t>(profile.settings.defaultAoEMode),
        static_cast<std::uint32_t>(profile.settings.defaultAoEMinTargets),
        profile.settings.defaultAoEScanRadius,
        static_cast<std::uint32_t>(profile.settings.targeting.mode),
        profile.settings.targeting.currentTargetBias,
        profile.settings.targeting.assistTargetBias,
        profile.settings.targeting.focusFireBias,
        profile.settings.targeting.protectAllyBias,
        profile.settings.targeting.preferHealerBias,
        profile.settings.targeting.preferDpsBias,
        profile.settings.targeting.avoidTankBias,
        static_cast<std::uint32_t>(profile.oocBehavior.buffScope),
        static_cast<std::uint32_t>(profile.oocBehavior.buffReapplySecs),
        profile.oocBehavior.buffOnSpawn ? 1 : 0,
        profile.oocBehavior.followDistOverride.has_value()
            ? std::to_string(*profile.oocBehavior.followDistOverride) : "NULL",
        profile.oocBehavior.autoLootOverride.has_value()
            ? std::to_string(*profile.oocBehavior.autoLootOverride ? 1 : 0) : "NULL",
        static_cast<std::uint32_t>(profile.oocBehavior.lootQualityMin),
        static_cast<std::uint32_t>(profile.oocBehavior.gatherNodes),
        static_cast<std::uint32_t>(profile.oocBehavior.gatherSkin),
        static_cast<std::uint32_t>(profile.oocBehavior.skinLootQualityMax),
        profile.oocBehavior.lootCategoryFlags);

    std::uint64_t profileId = ResolveProfileId(profile);
    if (profileId == 0)
        return;

    DeleteProfileChildren(profileId);
    for (model::BotCombatEntryDefinition const& entry : profile.interruptEntries)
        InsertEntry(profileId, entry);
    for (model::BotCombatEntryDefinition const& entry : profile.rotationEntries)
        InsertEntry(profileId, entry);
}

void SqlBotCombatProfileRepository::DeleteProfile(
    std::uint64_t profileId)
{
    DeleteProfileChildren(profileId);
    CharacterDatabase.Execute(
        "DELETE FROM living_world_bot_combat_profile WHERE profile_id = {}",
        profileId);
}
} // namespace integration
} // namespace living_world
