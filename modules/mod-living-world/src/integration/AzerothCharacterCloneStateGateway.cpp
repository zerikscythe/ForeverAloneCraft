#include "integration/AzerothCharacterCloneStateGateway.h"

#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryResult.h"
#include "WorldSessionMgr.h"

namespace living_world
{
namespace integration
{
std::optional<CharacterCloneLoginState>
AzerothCharacterCloneStateGateway::LoadCloneLoginState(
    std::uint64_t cloneCharacterGuid) const
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT name, at_login FROM characters WHERE guid = {} LIMIT 1",
        cloneCharacterGuid);
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();
    CharacterCloneLoginState state;
    state.name = fields[0].Get<std::string>();
    state.atLoginFlags = fields[1].Get<std::uint16_t>();
    state.loginNameValid =
        ObjectMgr::CheckPlayerName(state.name) == CHAR_NAME_SUCCESS;
    return state;
}

bool AzerothCharacterCloneStateGateway::DeleteOfflineCloneCharacter(
    std::uint32_t accountId,
    std::uint64_t cloneCharacterGuid,
    std::string const& cloneCharacterName) const
{
    if (cloneCharacterGuid == 0)
        return false;

    ObjectGuid cloneGuid =
        ObjectGuid::Create<HighGuid::Player>(cloneCharacterGuid);
    if (ObjectAccessor::FindConnectedPlayer(cloneGuid) ||
        sWorldSessionMgr->FindOfflineSessionForCharacterGUID(cloneCharacterGuid))
    {
        return false;
    }

    Player::DeleteFromDB(cloneCharacterGuid, accountId, true, true);
    sCharacterCache->DeleteCharacterCacheEntry(cloneGuid, cloneCharacterName);
    return true;
}
} // namespace integration
} // namespace living_world