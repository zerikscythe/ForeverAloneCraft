#include "integration/SqlBotCombatProfileSelectionRepository.h"

#include "DatabaseEnv.h"
#include "QueryResult.h"

namespace living_world
{
namespace integration
{
std::optional<model::BotCombatRuntimeSelection>
SqlBotCombatProfileSelectionRepository::FindRuntimeSelection(
    std::uint64_t sourceCharacterGuid) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT source_character_guid, active_profile_slot "
        "FROM living_world_bot_combat_runtime_selection "
        "WHERE source_character_guid = {} "
        "LIMIT 1",
        sourceCharacterGuid);
    if (!result)
        return std::nullopt;

    Field const* fields = result->Fetch();
    model::BotCombatRuntimeSelection selection;
    selection.sourceCharacterGuid = fields[0].Get<std::uint64_t>();
    selection.activeProfileSlot = fields[1].Get<std::uint8_t>();
    return selection;
}

void SqlBotCombatProfileSelectionRepository::SaveRuntimeSelection(
    model::BotCombatRuntimeSelection const& selection)
{
    CharacterDatabase.Execute(
        "INSERT INTO living_world_bot_combat_runtime_selection ("
        "source_character_guid, active_profile_slot) "
        "VALUES ({}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "active_profile_slot = VALUES(active_profile_slot)",
        selection.sourceCharacterGuid,
        static_cast<std::uint32_t>(selection.activeProfileSlot));
}
} // namespace integration
} // namespace living_world
