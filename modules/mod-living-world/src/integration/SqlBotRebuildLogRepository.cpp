#include "integration/SqlBotRebuildLogRepository.h"

#include "DatabaseEnv.h"

namespace living_world
{
namespace integration
{
void SqlBotRebuildLogRepository::EnsureSchema() const
{
    CharacterDatabase.Execute(
        "CREATE TABLE IF NOT EXISTS living_world_bot_rebuild_log ("
        " id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        " identity_id INT UNSIGNED NOT NULL,"
        " shell_account_id INT UNSIGNED NULL,"
        " shell_character_guid BIGINT UNSIGNED NULL,"
        " rebuild_reason VARCHAR(64) NOT NULL DEFAULT '',"
        " level TINYINT UNSIGNED NOT NULL DEFAULT 1,"
        " gear_tier TINYINT UNSIGNED NOT NULL DEFAULT 1,"
        " spec_key VARCHAR(32) NOT NULL DEFAULT '',"
        " loadout_key VARCHAR(64) NOT NULL DEFAULT '',"
        " display_loadout_key VARCHAR(64) NOT NULL DEFAULT '',"
        " doctrine_profile_key VARCHAR(64) NOT NULL DEFAULT '',"
        " shell_state_version INT UNSIGNED NOT NULL DEFAULT 0,"
        " notes VARCHAR(255) NOT NULL DEFAULT '',"
        " rebuilt_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        " PRIMARY KEY (id),"
        " KEY idx_identity_time (identity_id, rebuilt_at)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
}

void SqlBotRebuildLogRepository::Append(
    model::BotRebuildLogEntry const& entry) const
{
    std::string rebuildReason = entry.rebuildReason;
    std::string specKey = entry.specKey;
    std::string loadoutKey = entry.loadoutKey;
    std::string displayLoadoutKey = entry.displayLoadoutKey;
    std::string doctrineProfileKey = entry.doctrineProfileKey;
    std::string notes = entry.notes;
    CharacterDatabase.EscapeString(rebuildReason);
    CharacterDatabase.EscapeString(specKey);
    CharacterDatabase.EscapeString(loadoutKey);
    CharacterDatabase.EscapeString(displayLoadoutKey);
    CharacterDatabase.EscapeString(doctrineProfileKey);
    CharacterDatabase.EscapeString(notes);

    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_rebuild_log "
        "(identity_id, shell_account_id, shell_character_guid, rebuild_reason, level, gear_tier, spec_key, "
        "loadout_key, display_loadout_key, doctrine_profile_key, shell_state_version, notes) "
        "VALUES ({}, {}, {}, '{}', {}, {}, '{}', '{}', '{}', '{}', {}, '{}')",
        entry.identityId,
        entry.shellAccountId ? std::to_string(*entry.shellAccountId) : std::string("NULL"),
        entry.shellCharacterGuid ? std::to_string(*entry.shellCharacterGuid) : std::string("NULL"),
        rebuildReason,
        static_cast<std::uint32_t>(entry.level),
        static_cast<std::uint32_t>(entry.gearTier),
        specKey,
        loadoutKey,
        displayLoadoutKey,
        doctrineProfileKey,
        entry.shellStateVersion,
        notes);
}
} // namespace integration
} // namespace living_world
