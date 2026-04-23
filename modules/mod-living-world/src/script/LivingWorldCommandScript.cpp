#include "script/LivingWorldCommandGrammar.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "TemporarySummon.h"
#include "integration/AzerothWorldFacade.h"
#include "integration/BotSessionFactory.h"
#include "integration/RosterRepository.h"
#include "integration/WorldCommitAction.h"
#include "model/PlayerRosterRequest.h"
#include "model/RosterEntry.h"
#include "planner/PlannerTypes.h"
#include "planner/SimplePartyRosterPlanner.h"
#include "service/BotPlayerRegistry.h"
#include "service/PartyBotService.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace Acore::ChatCommands;

namespace living_world
{
namespace script
{
namespace
{
constexpr std::uint32_t MaleAltCompanionTemplateEntry = 111;
constexpr float AltCompanionFollowDistance = 2.5f;
constexpr float AltCompanionFollowAngle = 3.14159f;

struct CommitExecutionSummary
{
    std::uint32_t spawned = 0;
    std::uint32_t attachedToFollow = 0;
    std::uint32_t skipped = 0;
    std::uint32_t failed = 0;
};

std::unordered_map<std::uint64_t, ObjectGuid> SpawnedRosterBodies;

model::BotFaction ToBotFaction(std::uint8_t raceId)
{
    TeamId const teamId = Player::TeamIdForRace(raceId);
    if (teamId == TEAM_ALLIANCE)
    {
        return model::BotFaction::Alliance;
    }

    if (teamId == TEAM_HORDE)
    {
        return model::BotFaction::Horde;
    }

    return model::BotFaction::Neutral;
}

model::BotRole ToDefaultRole(std::uint8_t classId)
{
    switch (classId)
    {
        case CLASS_PRIEST:
        case CLASS_DRUID:
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
            return model::BotRole::Support;
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
            return model::BotRole::Tank;
        default:
            return model::BotRole::Damage;
    }
}

std::string_view ToClassName(std::uint8_t classId)
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
            return "Unknown";
    }
}

std::string_view ToFailureText(
    planner::PartyRosterFailureReason reason)
{
    switch (reason)
    {
        case planner::PartyRosterFailureReason::None:
            return "none";
        case planner::PartyRosterFailureReason::RequesterUnavailable:
            return "requester unavailable";
        case planner::PartyRosterFailureReason::RosterEntryNotFound:
            return "roster entry not found";
        case planner::PartyRosterFailureReason::RosterEntryDisabled:
            return "roster entry disabled";
        case planner::PartyRosterFailureReason::RosterEntryAlreadySummoned:
            return "roster entry already online/summoned";
        case planner::PartyRosterFailureReason::PartyFull:
            return "party is full";
        case planner::PartyRosterFailureReason::OwnershipMismatch:
            return "roster entry belongs to another account";
        case planner::PartyRosterFailureReason::DirectControlNotSupported:
            return "direct control not supported";
    }

    return "unknown failure";
}

std::string_view ToParseErrorText(CommandParseErrorKind kind)
{
    switch (kind)
    {
        case CommandParseErrorKind::Empty:
            return "empty command";
        case CommandParseErrorKind::UnknownSubsystem:
            return "unknown subsystem";
        case CommandParseErrorKind::UnknownVerb:
            return "unknown verb";
        case CommandParseErrorKind::MissingArgument:
            return "missing argument";
        case CommandParseErrorKind::InvalidArgument:
            return "invalid argument";
    }

    return "parse error";
}

std::string_view ToSourceText(model::RosterEntrySource source)
{
    switch (source)
    {
        case model::RosterEntrySource::GenericBot:
            return "generic";
        case model::RosterEntrySource::AccountAlt:
            return "account-alt";
    }

    return "unknown";
}

std::string_view ToActionText(integration::WorldCommitAction const& action)
{
    if (std::holds_alternative<integration::SpawnRosterBodyAction>(action))
    {
        return "spawn roster body";
    }

    if (std::holds_alternative<integration::AttachToPartyAction>(action))
    {
        return "attach to party";
    }

    if (std::holds_alternative<integration::UpdateAbstractStateAction>(action))
    {
        return "update abstract state";
    }

    return "unknown action";
}

std::string_view ToBotSpawnStatusText(
    integration::BotSessionSpawnStatus status)
{
    switch (status)
    {
        case integration::BotSessionSpawnStatus::SpawnQueued:
            return "spawn queued";
        case integration::BotSessionSpawnStatus::NoAvailableBotAccount:
            return "no available bot account";
        case integration::BotSessionSpawnStatus::BotAccountNotFound:
            return "bot account not found";
        case integration::BotSessionSpawnStatus::InvalidCharacterGuid:
            return "invalid character guid";
    }

    return "unknown bot spawn status";
}

Creature* FindActiveRosterBody(
    Player* player,
    std::uint64_t rosterEntryId)
{
    auto const itr = SpawnedRosterBodies.find(rosterEntryId);
    if (itr == SpawnedRosterBodies.end())
    {
        return nullptr;
    }

    Creature* body = player->GetMap()->GetCreature(itr->second);
    if (!body || !body->IsAlive())
    {
        SpawnedRosterBodies.erase(itr);
        return nullptr;
    }

    return body;
}

bool ExecuteSpawnRosterBodyAction(
    ChatHandler* handler,
    Player* requester,
    integration::SpawnRosterBodyAction const& action)
{
    if (action.source != model::RosterEntrySource::AccountAlt)
    {
        if (Creature* existing = FindActiveRosterBody(requester, action.rosterEntryId))
        {
            existing->GetMotionMaster()->MoveFollow(
                requester,
                AltCompanionFollowDistance,
                AltCompanionFollowAngle);
            handler->PSendSysMessage(
                "LivingWorld generic roster entry {} is already spawned and following.",
                action.rosterEntryId);
            return true;
        }

        std::uint32_t const templateEntry = MaleAltCompanionTemplateEntry;
        Position const spawnPosition = requester->GetNearPosition(2.0f, 0.0f);
        TempSummon* summon = requester->GetMap()->SummonCreature(
            templateEntry,
            spawnPosition,
            nullptr,
            0,
            requester);
        if (!summon)
        {
            handler->PSendSysMessage(
                "LivingWorld failed to summon generic roster entry {}.",
                action.rosterEntryId);
            return false;
        }

        summon->SetFaction(requester->GetFaction());
        summon->SetReactState(REACT_PASSIVE);
        summon->SetFullHealth();
        summon->GetMotionMaster()->MoveFollow(
            requester,
            AltCompanionFollowDistance,
            AltCompanionFollowAngle);

        SpawnedRosterBodies[action.rosterEntryId] = summon->GetGUID();
        handler->PSendSysMessage(
            "LivingWorld spawned generic roster entry {} with the temporary fallback.",
            action.rosterEntryId);
        return true;
    }

    Player* existingBot =
        service::BotPlayerRegistry::Instance().FindBotForOwner(
            requester->GetGUID());
    if (existingBot)
    {
        existingBot->GetMotionMaster()->MoveFollow(
            requester,
            AltCompanionFollowDistance,
            AltCompanionFollowAngle);
        handler->PSendSysMessage(
            "LivingWorld roster entry {} is already active as a bot player and following.",
            action.rosterEntryId);
        return true;
    }

    integration::BotSessionSpawnResult const spawnResult =
        integration::BotSessionFactory::SpawnBotPlayer(
            ObjectGuid::Create<HighGuid::Player>(action.characterGuid),
            requester->GetGUID());
    if (spawnResult.status != integration::BotSessionSpawnStatus::SpawnQueued)
    {
        handler->PSendSysMessage(
            "LivingWorld failed to queue account-alt bot login for entry {}: {}.",
            action.rosterEntryId,
            ToBotSpawnStatusText(spawnResult.status));
        return false;
    }

    handler->PSendSysMessage(
        "LivingWorld queued account-alt bot login for entry {} using bot account {}.",
        action.rosterEntryId,
        spawnResult.botAccountId);
    return true;
}

bool ExecuteAttachToPartyAction(
    ChatHandler* handler,
    Player* requester,
    integration::AttachToPartyAction const& action)
{
    Creature* body = FindActiveRosterBody(requester, action.rosterEntryId);
    if (!body)
    {
        if (Player* bot =
            service::BotPlayerRegistry::Instance().FindBotForOwner(
                requester->GetGUID()))
        {
            bot->GetMotionMaster()->MoveFollow(
                requester,
                AltCompanionFollowDistance,
                AltCompanionFollowAngle);
            handler->PSendSysMessage(
                "LivingWorld attached roster entry {} as a bot-player follower. Real party membership is not wired yet.",
                action.rosterEntryId);
            return true;
        }

        handler->PSendSysMessage(
            "LivingWorld queued roster entry {}; bot login is still pending.",
            action.rosterEntryId);
        return true;
    }

    body->GetMotionMaster()->MoveFollow(
        requester,
        AltCompanionFollowDistance,
        AltCompanionFollowAngle);
    handler->PSendSysMessage(
        "LivingWorld attached roster entry {} as a follower. Real party membership is not wired yet.",
        action.rosterEntryId);
    return true;
}

CommitExecutionSummary ExecuteCommitActions(
    ChatHandler* handler,
    Player* requester,
    std::vector<integration::WorldCommitAction> const& actions)
{
    CommitExecutionSummary summary;
    for (integration::WorldCommitAction const& action : actions)
    {
        if (integration::SpawnRosterBodyAction const* spawn =
            std::get_if<integration::SpawnRosterBodyAction>(&action))
        {
            if (ExecuteSpawnRosterBodyAction(handler, requester, *spawn))
            {
                ++summary.spawned;
            }
            else
            {
                ++summary.failed;
            }
            continue;
        }

        if (integration::AttachToPartyAction const* attach =
            std::get_if<integration::AttachToPartyAction>(&action))
        {
            if (ExecuteAttachToPartyAction(handler, requester, *attach))
            {
                ++summary.attachedToFollow;
            }
            else
            {
                ++summary.failed;
            }
            continue;
        }

        ++summary.skipped;
    }

    return summary;
}

model::RosterEntry BuildRosterEntry(
    Field const* fields,
    std::uint32_t ownerAccountId)
{
    std::uint64_t const guid = fields[0].Get<std::uint64_t>();
    std::string const name = fields[1].Get<std::string>();
    std::uint8_t const raceId = fields[2].Get<std::uint8_t>();
    std::uint8_t const classId = fields[3].Get<std::uint8_t>();
    std::uint8_t const level = fields[4].Get<std::uint8_t>();

    model::RosterEntry entry;
    entry.rosterEntryId = guid;
    entry.source = model::RosterEntrySource::AccountAlt;
    entry.ownerAccountId = ownerAccountId;
    entry.characterGuid = guid;
    entry.isEnabled = true;
    entry.isAlreadySummoned =
        ObjectAccessor::FindPlayerByLowGUID(static_cast<ObjectGuid::LowType>(guid)) != nullptr;

    entry.controllableProfile.profile.botId = guid;
    entry.controllableProfile.profile.name = name;
    entry.controllableProfile.profile.raceId = raceId;
    entry.controllableProfile.profile.classId = classId;
    entry.controllableProfile.profile.faction = ToBotFaction(raceId);
    entry.controllableProfile.profile.level = level;
    entry.controllableProfile.profile.guildName = "Account Alts";
    entry.controllableProfile.profile.personality =
        model::BotPersonality::Indifferent;
    entry.controllableProfile.profile.preferredRole = ToDefaultRole(classId);
    entry.controllableProfile.canBePlayerControlled = false;
    entry.controllableProfile.canEarnProgression = true;
    entry.controllableProfile.canJoinPlayerParty = true;
    return entry;
}

class AccountAltRosterRepository final : public integration::RosterRepository
{
public:
    std::vector<model::RosterEntry> GetRosterEntriesForAccount(
        std::uint32_t accountId) const override
    {
        std::vector<model::RosterEntry> entries;
        QueryResult result = CharacterDatabase.Query(
            "SELECT guid, name, race, class, level FROM characters "
            "WHERE account = {} ORDER BY name ASC",
            accountId);

        if (!result)
        {
            return entries;
        }

        do
        {
            entries.push_back(BuildRosterEntry(result->Fetch(), accountId));
        } while (result->NextRow());

        return entries;
    }

    std::optional<model::RosterEntry> FindRosterEntryForAccount(
        std::uint32_t accountId,
        std::uint64_t rosterEntryId) const override
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT guid, name, race, class, level FROM characters "
            "WHERE account = {} AND guid = {} LIMIT 1",
            accountId,
            rosterEntryId);

        if (!result)
        {
            return std::nullopt;
        }

        return BuildRosterEntry(result->Fetch(), accountId);
    }
};

class LiveAzerothWorldFacade final : public integration::AzerothWorldFacade
{
public:
    std::optional<integration::PlayerWorldContext> GetPlayerContext(
        std::uint64_t characterGuid) const override
    {
        Player* player = ObjectAccessor::FindPlayerByLowGUID(
            static_cast<ObjectGuid::LowType>(characterGuid));
        if (!player || !player->GetSession())
        {
            return std::nullopt;
        }

        integration::PlayerWorldContext context;
        context.identity.characterGuid = player->GetGUID().GetCounter();
        context.identity.accountId = player->GetSession()->GetAccountId();
        context.position.mapId = player->GetMapId();
        context.position.zoneId = player->GetZoneId();
        context.position.areaId = player->GetAreaId();
        context.position.x = player->GetPositionX();
        context.position.y = player->GetPositionY();
        context.position.z = player->GetPositionZ();
        context.position.orientation = player->GetOrientation();
        context.isInWorld = player->IsInWorld();
        context.isInCombat = player->IsInCombat();
        context.isDead = player->isDead();
        context.canControlCompanions = !context.isInCombat;

        if (Group const* group = player->GetGroup())
        {
            context.party.maxPartyMembers = group->isRaidGroup()
                ? MAXRAIDSIZE
                : MAXGROUPSIZE;

            for (Group::MemberSlot const& member : group->GetMemberSlots())
            {
                integration::PartyMemberSnapshot snapshot;
                snapshot.characterGuid = member.guid.GetCounter();
                snapshot.isOnline =
                    ObjectAccessor::FindPlayer(member.guid) != nullptr;
                snapshot.isBotControlled = false;
                context.party.members.push_back(snapshot);
            }
        }
        else
        {
            context.party.maxPartyMembers = MAXGROUPSIZE;
        }

        return context;
    }

    std::vector<integration::SpawnAnchor> GetSpawnAnchorsInZone(
        std::uint32_t) const override
    {
        return {};
    }

    bool IsCharacterOnline(std::uint64_t characterGuid) const override
    {
        return ObjectAccessor::FindPlayerByLowGUID(
            static_cast<ObjectGuid::LowType>(characterGuid)) != nullptr;
    }
};

void RenderRosterList(ChatHandler* handler)
{
    WorldSession* session = handler->GetSession();
    if (!session)
    {
        handler->SendErrorMessage("LivingWorld roster commands require an in-game session.");
        return;
    }

    AccountAltRosterRepository repository;
    std::vector<model::RosterEntry> entries =
        repository.GetRosterEntriesForAccount(session->GetAccountId());

    if (entries.empty())
    {
        handler->PSendSysMessage("LivingWorld roster: no account characters found.");
        return;
    }

    handler->PSendSysMessage("LivingWorld roster entries:");
    for (model::RosterEntry const& entry : entries)
    {
        model::BotProfile const& profile = entry.controllableProfile.profile;
        handler->PSendSysMessage(
            "  [{}] {} lvl {} {} ({}){}",
            entry.rosterEntryId,
            profile.name,
            static_cast<std::uint32_t>(profile.level),
            ToClassName(profile.classId),
            ToSourceText(entry.source),
            entry.isAlreadySummoned ? " online" : "");
    }
}

void RenderRosterRequest(
    ChatHandler* handler,
    RosterRequestCommand const& command)
{
    WorldSession* session = handler->GetSession();
    Player* player = session ? session->GetPlayer() : nullptr;
    if (!session || !player)
    {
        handler->SendErrorMessage("LivingWorld roster requests require an in-game player.");
        return;
    }

    LiveAzerothWorldFacade facade;
    AccountAltRosterRepository repository;
    planner::SimplePartyRosterPlanner planner;
    service::PartyBotService service(facade, repository, planner);

    model::PlayerRosterRequest request;
    request.requesterCharacterGuid = player->GetGUID().GetCounter();
    request.requesterAccountId = session->GetAccountId();
    request.requestedRosterEntryId = command.rosterEntryId;

    service::PartyBotDispatchResult result =
        service.DispatchRosterRequest(request);

    if (!result.isApproved)
    {
        handler->PSendSysMessage(
            "LivingWorld roster request rejected: {}.",
            ToFailureText(result.failureReason));
        return;
    }

    handler->PSendSysMessage(
        "LivingWorld roster request approved for entry {}.",
        command.rosterEntryId);
    CommitExecutionSummary const execution = ExecuteCommitActions(
        handler,
        player,
        result.commitActions);
    handler->PSendSysMessage(
        "LivingWorld commit result: spawned {}, attached {}, skipped {}, failed {}.",
        execution.spawned,
        execution.attachedToFollow,
        execution.skipped,
        execution.failed);
}

void RenderDismissPlaceholder(
    ChatHandler* handler,
    RosterDismissCommand const& command)
{
    handler->PSendSysMessage(
        "LivingWorld dismiss for entry {} is parsed but not implemented yet.",
        command.rosterEntryId);
}

bool HandleParsedCommand(
    ChatHandler* handler,
    ParsedCommand const& parsed)
{
    if (CommandParseError const* error =
        std::get_if<CommandParseError>(&parsed))
    {
        handler->PSendSysMessage(
            "LivingWorld command error: {} ({})",
            ToParseErrorText(error->kind),
            error->detail);
        return false;
    }

    if (std::get_if<RosterListCommand>(&parsed))
    {
        RenderRosterList(handler);
        return true;
    }

    if (RosterRequestCommand const* command =
        std::get_if<RosterRequestCommand>(&parsed))
    {
        RenderRosterRequest(handler, *command);
        return true;
    }

    if (RosterDismissCommand const* command =
        std::get_if<RosterDismissCommand>(&parsed))
    {
        RenderDismissPlaceholder(handler, *command);
        return true;
    }

    return false;
}
} // namespace
} // namespace script
} // namespace living_world

class LivingWorldCommandScript final : public CommandScript
{
public:
    LivingWorldCommandScript() : CommandScript("LivingWorldCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "lwbot", HandleLivingWorldCommand, SEC_PLAYER, Console::No }
        };

        return commandTable;
    }

private:
    static bool HandleLivingWorldCommand(ChatHandler* handler, char const* args)
    {
        return living_world::script::HandleParsedCommand(
            handler,
            living_world::script::ParseLivingWorldCommand(args ? args : ""));
    }
};

void AddSC_LivingWorldCommandScript()
{
    new LivingWorldCommandScript();
}
