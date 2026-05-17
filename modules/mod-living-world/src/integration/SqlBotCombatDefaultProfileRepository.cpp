#include "integration/SqlBotCombatDefaultProfileRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

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

void LoadDefaultProfileChildren(model::BotCombatDefaultProfileRecord& profile)
{
    std::unordered_map<std::uint64_t, std::size_t> interruptIndexByEntryId;
    std::unordered_map<std::uint64_t, std::size_t> rotationIndexByEntryId;

    QueryResult entryResult = WorldDatabase.Query(
        "SELECT entry_id, priority, label, is_interrupt, breaks_current_cast, "
        "enabled, condition_logic "
        "FROM living_world_bot_combat_default_entry "
        "WHERE default_profile_id = {} "
        "ORDER BY is_interrupt DESC, priority ASC, entry_id ASC",
        profile.defaultProfileId);
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

    QueryResult actionResult = WorldDatabase.Query(
        "SELECT action_id, slot, action_type, spell_base_id, item_id, "
        "rank_mode, rank_value, target_key, aoe_mode, aoe_min_targets, aoe_radius, entry_id "
        "FROM living_world_bot_combat_default_action "
        "WHERE entry_id IN ("
            "SELECT entry_id FROM living_world_bot_combat_default_entry "
            "WHERE default_profile_id = {}) "
        "ORDER BY entry_id ASC, slot ASC",
        profile.defaultProfileId);
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

    QueryResult conditionResult = WorldDatabase.Query(
        "SELECT condition_id, sequence, subject_key, stat_key, comparison, "
        "numeric_value, string_value, entry_id "
        "FROM living_world_bot_combat_default_condition "
        "WHERE entry_id IN ("
            "SELECT entry_id FROM living_world_bot_combat_default_entry "
            "WHERE default_profile_id = {}) "
        "ORDER BY entry_id ASC, sequence ASC",
        profile.defaultProfileId);
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

model::BotCombatDefaultProfileRecord BuildDefaultProfile(Field const* fields)
{
    model::BotCombatDefaultProfileRecord profile;
    profile.defaultProfileId = fields[0].Get<std::uint64_t>();
    profile.specKey    = fields[1].Get<std::string>();
    profile.roleKey    = fields[2].Get<std::string>();
    profile.displayName= fields[3].Get<std::string>();
    profile.classKey   = fields[4].IsNull() ? "" : fields[4].Get<std::string>();
    profile.contextKey = fields[5].IsNull() ? "PvE" : fields[5].Get<std::string>();
    profile.variantKey = fields[6].IsNull() ? "" : fields[6].Get<std::string>();
    profile.description = fields[7].IsNull() ? "" : fields[7].Get<std::string>();
    profile.settings   = BuildSettings(fields, 8);
    return profile;
}
} // namespace

std::vector<model::BotCombatDefaultProfileRecord>
SqlBotCombatDefaultProfileRepository::ListDefaultProfiles() const
{
    std::vector<model::BotCombatDefaultProfileRecord> profiles;
    QueryResult result = WorldDatabase.Query(
        "SELECT default_profile_id, spec_key, role_key, display_name, "
        "class_key, context_key, variant_key, description, conservation_mode, resource_low_water, resource_high_water, "
        "enable_down_rank, down_rank_floor, default_aoe_mode, "
        "default_aoe_min_targets, default_aoe_scan_radius, targeting_mode, current_target_bias, assist_target_bias, "
        "focus_fire_bias, protect_ally_bias, prefer_healer_bias, prefer_dps_bias, avoid_tank_bias "
        "FROM living_world_bot_combat_default_profile "
        "ORDER BY spec_key ASC, role_key ASC, class_key ASC, context_key ASC, default_profile_id ASC");
    if (!result)
        return profiles;

    do
    {
        model::BotCombatDefaultProfileRecord profile =
            BuildDefaultProfile(result->Fetch());
        LoadDefaultProfileChildren(profile);
        profiles.push_back(std::move(profile));
    } while (result->NextRow());

    return profiles;
}

std::optional<model::BotCombatDefaultProfileRecord>
SqlBotCombatDefaultProfileRepository::FindDefaultProfile(
    std::string const& specKey,
    std::string const& roleKey,
    std::string const& classKey,
    std::string const& contextKey,
    std::string const& variantKey) const
{
    std::string escapedSpecKey  = specKey;  WorldDatabase.EscapeString(escapedSpecKey);
    std::string escapedRoleKey  = roleKey;  WorldDatabase.EscapeString(escapedRoleKey);
    std::string escapedClassKey = classKey; WorldDatabase.EscapeString(escapedClassKey);
    std::string escapedContextKey = contextKey; WorldDatabase.EscapeString(escapedContextKey);
    std::string escapedVariantKey = variantKey; WorldDatabase.EscapeString(escapedVariantKey);

    QueryResult result;
    if (escapedClassKey.empty())
    {
        if (escapedVariantKey.empty())
        {
            result = WorldDatabase.Query(
                "SELECT default_profile_id, spec_key, role_key, display_name, "
                "class_key, context_key, variant_key, description, conservation_mode, resource_low_water, resource_high_water, "
                "enable_down_rank, down_rank_floor, default_aoe_mode, "
                "default_aoe_min_targets, default_aoe_scan_radius, targeting_mode, current_target_bias, assist_target_bias, "
                "focus_fire_bias, protect_ally_bias, prefer_healer_bias, prefer_dps_bias, avoid_tank_bias "
                "FROM living_world_bot_combat_default_profile "
                "WHERE spec_key = '{}' AND role_key = '{}' "
                "AND (context_key = '{}' OR context_key IS NULL OR context_key = '') "
                "AND (class_key IS NULL OR class_key = '') "
                "AND (variant_key IS NULL OR variant_key = '') "
                "ORDER BY CASE "
                    "WHEN context_key = '{}' THEN 0 "
                    "WHEN context_key IS NULL OR context_key = '' THEN 1 "
                    "ELSE 2 END, "
                    "default_profile_id ASC "
                "LIMIT 1",
                escapedSpecKey, escapedRoleKey, escapedContextKey, escapedContextKey);
        }
        else
        {
            result = WorldDatabase.Query(
                "SELECT default_profile_id, spec_key, role_key, display_name, "
                "class_key, context_key, variant_key, description, conservation_mode, resource_low_water, resource_high_water, "
                "enable_down_rank, down_rank_floor, default_aoe_mode, "
                "default_aoe_min_targets, default_aoe_scan_radius, targeting_mode, current_target_bias, assist_target_bias, "
                "focus_fire_bias, protect_ally_bias, prefer_healer_bias, prefer_dps_bias, avoid_tank_bias "
                "FROM living_world_bot_combat_default_profile "
                "WHERE spec_key = '{}' "
                "AND (context_key = '{}' OR context_key IS NULL OR context_key = '') "
                "AND (class_key IS NULL OR class_key = '') "
                "AND (variant_key = '{}' OR variant_key IS NULL OR variant_key = '') "
                "ORDER BY CASE "
                    "WHEN variant_key = '{}' THEN 0 "
                    "WHEN variant_key IS NULL OR variant_key = '' THEN 1 "
                    "ELSE 2 END, "
                    "CASE "
                    "WHEN role_key = '{}' THEN 0 "
                    "ELSE 1 END, "
                    "CASE "
                    "WHEN context_key = '{}' THEN 0 "
                    "WHEN context_key IS NULL OR context_key = '' THEN 1 "
                    "ELSE 2 END, "
                    "default_profile_id ASC "
                "LIMIT 1",
                escapedSpecKey,
                escapedContextKey,
                escapedVariantKey,
                escapedVariantKey,
                escapedRoleKey,
                escapedContextKey);
        }
    }
    else
    {
        if (escapedVariantKey.empty())
        {
            result = WorldDatabase.Query(
                "SELECT default_profile_id, spec_key, role_key, display_name, "
                "class_key, context_key, variant_key, description, conservation_mode, resource_low_water, resource_high_water, "
                "enable_down_rank, down_rank_floor, default_aoe_mode, "
                "default_aoe_min_targets, default_aoe_scan_radius, targeting_mode, current_target_bias, assist_target_bias, "
                "focus_fire_bias, protect_ally_bias, prefer_healer_bias, prefer_dps_bias, avoid_tank_bias "
                "FROM living_world_bot_combat_default_profile "
                "WHERE spec_key = '{}' AND role_key = '{}' "
                "AND (context_key = '{}' OR context_key IS NULL OR context_key = '') "
                "AND (class_key = '{}' OR class_key IS NULL OR class_key = '') "
                "AND (variant_key IS NULL OR variant_key = '') "
                "ORDER BY CASE "
                    "WHEN class_key = '{}' THEN 0 "
                    "WHEN class_key IS NULL OR class_key = '' THEN 1 "
                    "ELSE 2 END, "
                    "CASE "
                    "WHEN context_key = '{}' THEN 0 "
                    "WHEN context_key IS NULL OR context_key = '' THEN 1 "
                    "ELSE 2 END, "
                    "default_profile_id ASC "
                "LIMIT 1",
                escapedSpecKey, escapedRoleKey, escapedContextKey,
                escapedClassKey, escapedClassKey, escapedContextKey);
        }
        else
        {
            result = WorldDatabase.Query(
                "SELECT default_profile_id, spec_key, role_key, display_name, "
                "class_key, context_key, variant_key, description, conservation_mode, resource_low_water, resource_high_water, "
                "enable_down_rank, down_rank_floor, default_aoe_mode, "
                "default_aoe_min_targets, default_aoe_scan_radius, targeting_mode, current_target_bias, assist_target_bias, "
                "focus_fire_bias, protect_ally_bias, prefer_healer_bias, prefer_dps_bias, avoid_tank_bias "
                "FROM living_world_bot_combat_default_profile "
                "WHERE spec_key = '{}' "
                "AND (context_key = '{}' OR context_key IS NULL OR context_key = '') "
                "AND (class_key = '{}' OR class_key IS NULL OR class_key = '') "
                "AND (variant_key = '{}' OR variant_key IS NULL OR variant_key = '') "
                "ORDER BY CASE "
                    "WHEN variant_key = '{}' THEN 0 "
                    "WHEN variant_key IS NULL OR variant_key = '' THEN 1 "
                    "ELSE 2 END, "
                    "CASE "
                    "WHEN role_key = '{}' THEN 0 "
                    "ELSE 1 END, "
                    "CASE "
                    "WHEN class_key = '{}' THEN 0 "
                    "WHEN class_key IS NULL OR class_key = '' THEN 1 "
                    "ELSE 2 END, "
                    "CASE "
                    "WHEN context_key = '{}' THEN 0 "
                    "WHEN context_key IS NULL OR context_key = '' THEN 1 "
                    "ELSE 2 END, "
                    "default_profile_id ASC "
                "LIMIT 1",
                escapedSpecKey,
                escapedContextKey,
                escapedClassKey,
                escapedVariantKey,
                escapedVariantKey,
                escapedRoleKey,
                escapedClassKey,
                escapedContextKey);
        }
    }
    if (!result)
        return std::nullopt;

    model::BotCombatDefaultProfileRecord profile =
        BuildDefaultProfile(result->Fetch());
    LoadDefaultProfileChildren(profile);
    return profile;
}
} // namespace integration
} // namespace living_world
