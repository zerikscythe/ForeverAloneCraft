#include "service/BotLedgerShellHydratorService.h"

#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryResult.h"
#include "StringFormat.h"
#include "integration/SqlBotAssignedGearRepository.h"
#include "integration/SqlBotAssignedGearTemplateRepository.h"
#include "integration/SqlBotDisplayLoadoutRepository.h"
#include "integration/SqlBotIdentityRepository.h"
#include "integration/SqlBotRebuildLogRepository.h"
#include "integration/SqlBotRuntimeSnapshotRepository.h"
#include "integration/SqlBotShellRuntimeRepository.h"
#include "integration/SqlBotCombatDefaultProfileRepository.h"
#include "integration/SqlBotGlyphTemplateRepository.h"
#include "integration/SqlBotTalentTemplateRepository.h"
#include "integration/SqlBotVirtualLoadoutRepository.h"
#include "service/BotAppearanceResolver.h"
#include "service/WorldBotAssignedGearService.h"
#include "service/WorldBotPreparationService.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
using living_world::integration::BotIdentityRecord;
using living_world::model::BotDisplayLoadoutRecord;
using living_world::model::BotRuntimeSnapshotRecord;
using living_world::model::BotShellRuntimeRecord;
using living_world::model::WorldBotAssignedGearEntry;

struct ShellBinding
{
    std::uint32_t accountId = 0;
    std::uint64_t characterGuid = 0;
};

struct ShellCharacterRow
{
    std::uint32_t accountId = 0;
    std::uint32_t playerFlags = 0;
    std::uint16_t mapId = 0;
    std::uint32_t zoneId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
};

struct PreservedBagItem
{
    std::uint64_t itemGuid = 0;
    std::uint8_t slot = 0;
};

struct EquippedShellItem
{
    std::uint8_t slot = 0;
    std::uint32_t itemId = 0;
    std::string enchantments;
};

struct SeededShellSkill
{
    std::uint16_t skillId = 0;
    std::uint16_t value = 0;
    std::uint16_t maxValue = 0;
};

std::string Quote(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return "'" + value + "'";
}

std::optional<ShellBinding> ResolveShellBinding(
    BotIdentityRecord const& identity,
    living_world::integration::SqlBotShellRuntimeRepository const& runtimeRepo)
{
    if (std::optional<BotShellRuntimeRecord> runtime = runtimeRepo.FindByIdentity(identity.id))
    {
        if (runtime->shellAccountId != 0 && runtime->shellCharacterGuid != 0)
            return ShellBinding{ runtime->shellAccountId, runtime->shellCharacterGuid };
    }

    if (identity.shellAccountId != 0 && identity.shellCharacterGuid != 0)
        return ShellBinding{ identity.shellAccountId, identity.shellCharacterGuid };

    return std::nullopt;
}

std::optional<ShellCharacterRow> LoadShellCharacterRow(std::uint64_t characterGuid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT account, playerFlags, map, zone, position_x, position_y, position_z, orientation "
        "FROM characters WHERE guid = {} LIMIT 1",
        characterGuid);
    if (!result)
        return std::nullopt;

    Field const* fields = result->Fetch();
    ShellCharacterRow row;
    row.accountId = fields[0].Get<std::uint32_t>();
    row.playerFlags = fields[1].Get<std::uint32_t>();
    row.mapId = fields[2].Get<std::uint16_t>();
    row.zoneId = fields[3].Get<std::uint32_t>();
    row.x = fields[4].Get<float>();
    row.y = fields[5].Get<float>();
    row.z = fields[6].Get<float>();
    row.o = fields[7].Get<float>();
    return row;
}

std::vector<PreservedBagItem> LoadPreservedBagItems(std::uint64_t characterGuid)
{
    std::vector<PreservedBagItem> items;
    QueryResult result = CharacterDatabase.Query(
        "SELECT slot, item FROM character_inventory "
        "WHERE guid = {} AND bag = 0 AND slot BETWEEN {} AND {} "
        "ORDER BY slot ASC",
        characterGuid,
        static_cast<std::uint32_t>(INVENTORY_SLOT_BAG_START),
        static_cast<std::uint32_t>(INVENTORY_SLOT_BAG_END - 1));
    if (!result)
        return items;

    do
    {
        Field const* fields = result->Fetch();
        items.push_back(PreservedBagItem{
            fields[1].Get<std::uint64_t>(),
            fields[0].Get<std::uint8_t>()
        });
    } while (result->NextRow());

    return items;
}

std::vector<std::uint64_t> LoadOwnedPetIds(std::uint64_t characterGuid)
{
    std::vector<std::uint64_t> ids;
    QueryResult result = CharacterDatabase.Query(
        "SELECT id FROM character_pet WHERE owner = {}",
        characterGuid);
    if (!result)
        return ids;

    do
    {
        ids.push_back(result->Fetch()[0].Get<std::uint64_t>());
    } while (result->NextRow());

    return ids;
}

std::vector<EquippedShellItem> BuildEquippedItems(
    std::vector<WorldBotAssignedGearEntry> const& assignedGear,
    std::optional<BotDisplayLoadoutRecord> const& displayLoadout)
{
    std::unordered_map<std::uint8_t, EquippedShellItem> bySlot;
    for (WorldBotAssignedGearEntry const& entry : assignedGear)
    {
        if (entry.itemId == 0)
            continue;

        bySlot[entry.slot] = EquippedShellItem{
            entry.slot,
            entry.itemId,
            entry.enchantments
        };
    }

    if (displayLoadout)
    {
        auto overlay = [&](std::uint8_t slot, std::uint32_t itemId)
        {
            if (itemId == 0)
                return;

            auto itr = bySlot.find(slot);
            std::string enchantments = itr != bySlot.end() ? itr->second.enchantments : std::string();
            bySlot[slot] = EquippedShellItem{ slot, itemId, enchantments };
        };

        overlay(EQUIPMENT_SLOT_HEAD, displayLoadout->helmItemId);
        overlay(EQUIPMENT_SLOT_SHOULDERS, displayLoadout->shoulderItemId);
        overlay(EQUIPMENT_SLOT_BODY, displayLoadout->shirtItemId);
        overlay(EQUIPMENT_SLOT_CHEST, displayLoadout->chestItemId);
        overlay(EQUIPMENT_SLOT_WAIST, displayLoadout->waistItemId);
        overlay(EQUIPMENT_SLOT_LEGS, displayLoadout->legsItemId);
        overlay(EQUIPMENT_SLOT_FEET, displayLoadout->feetItemId);
        overlay(EQUIPMENT_SLOT_WRISTS, displayLoadout->wristItemId);
        overlay(EQUIPMENT_SLOT_HANDS, displayLoadout->handsItemId);
        overlay(EQUIPMENT_SLOT_BACK, displayLoadout->backItemId);
        overlay(EQUIPMENT_SLOT_TABARD, displayLoadout->tabardItemId);
        overlay(EQUIPMENT_SLOT_MAINHAND, displayLoadout->mainHandItemId);
        overlay(EQUIPMENT_SLOT_OFFHAND, displayLoadout->offHandItemId);
        overlay(EQUIPMENT_SLOT_RANGED, displayLoadout->rangedItemId);
    }

    std::vector<EquippedShellItem> equipped;
    equipped.reserve(bySlot.size());
    for (auto const& [slot, item] : bySlot)
    {
        (void)slot;
        equipped.push_back(item);
    }

    std::sort(equipped.begin(), equipped.end(),
        [](EquippedShellItem const& lhs, EquippedShellItem const& rhs)
        {
            return lhs.slot < rhs.slot;
        });
    return equipped;
}

std::uint32_t BuildPlayerFlags(std::optional<BotDisplayLoadoutRecord> const& displayLoadout)
{
    std::uint32_t flags = 0;
    if (!displayLoadout)
        return flags;

    if (displayLoadout->hideHelm)
        flags |= PLAYER_FLAGS_HIDE_HELM;
    if (displayLoadout->hideCloak)
        flags |= PLAYER_FLAGS_HIDE_CLOAK;
    return flags;
}

living_world::service::WorldBotPreparationService& GetShellPreparationService()
{
    static living_world::integration::SqlBotCombatDefaultProfileRepository defaultProfileRepository;
    static living_world::integration::SqlBotGlyphTemplateRepository glyphTemplateRepository;
    static living_world::integration::SqlBotTalentTemplateRepository talentTemplateRepository;
    static living_world::integration::SqlBotVirtualLoadoutRepository virtualLoadoutRepository;
    static living_world::service::WorldBotPreparationService preparationService(
        defaultProfileRepository,
        glyphTemplateRepository,
        talentTemplateRepository,
        virtualLoadoutRepository);
    return preparationService;
}

std::uint16_t ResolveDefaultSkillMaxValueForLevel(std::uint8_t level)
{
    return static_cast<std::uint16_t>(std::max<std::uint32_t>(1u, static_cast<std::uint32_t>(level) * 5u));
}

std::vector<SeededShellSkill> BuildDefaultShellSkills(BotIdentityRecord const& identity)
{
    std::vector<SeededShellSkill> skills;

    PlayerInfo const* playerInfo = sObjectMgr->GetPlayerInfo(identity.raceId, identity.classId);
    if (!playerInfo)
        return skills;

    std::unordered_map<std::uint16_t, SeededShellSkill> bySkillId;
    std::uint16_t const maxSkillForLevel = ResolveDefaultSkillMaxValueForLevel(identity.level);

    for (PlayerCreateInfoSkill const& skill : playerInfo->skills)
    {
        SkillRaceClassInfoEntry const* rcInfo =
            GetSkillRaceClassInfo(skill.SkillId, identity.raceId, identity.classId);
        if (!rcInfo)
            continue;

        SeededShellSkill seeded;
        seeded.skillId = static_cast<std::uint16_t>(skill.SkillId);

        switch (GetSkillRangeType(rcInfo))
        {
            case SKILL_RANGE_LANGUAGE:
                seeded.value = 300;
                seeded.maxValue = 300;
                break;
            case SKILL_RANGE_MONO:
                seeded.value = 1;
                seeded.maxValue = 1;
                break;
            case SKILL_RANGE_RANK:
            {
                if (skill.Rank == 0)
                    continue;

                SkillTiersEntry const* tier = sSkillTiersStore.LookupEntry(rcInfo->SkillTierID);
                if (!tier)
                    continue;

                std::uint16_t const tierValue =
                    tier->Value[std::max<int32>(static_cast<int32>(skill.Rank) - 1, 0)];
                seeded.value = std::max<std::uint16_t>(tierValue, 1u);
                seeded.maxValue = std::max<std::uint16_t>(tierValue, 1u);
                break;
            }
            case SKILL_RANGE_LEVEL:
            default:
                seeded.value = maxSkillForLevel;
                seeded.maxValue = maxSkillForLevel;
                break;
        }

        auto const [itr, inserted] = bySkillId.emplace(seeded.skillId, seeded);
        if (!inserted)
        {
            itr->second.value = std::max(itr->second.value, seeded.value);
            itr->second.maxValue = std::max(itr->second.maxValue, seeded.maxValue);
        }
    }

    skills.reserve(bySkillId.size());
    for (auto const& [skillId, seeded] : bySkillId)
    {
        (void)skillId;
        skills.push_back(seeded);
    }

    std::sort(skills.begin(), skills.end(),
        [](SeededShellSkill const& lhs, SeededShellSkill const& rhs)
        {
            return lhs.skillId < rhs.skillId;
        });

    return skills;
}

std::string BuildInList(std::vector<std::uint64_t> const& values)
{
    std::string list;
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
            list += ',';
        list += std::to_string(values[i]);
    }
    return list;
}
} // namespace

namespace living_world
{
namespace service
{

bool BotLedgerShellHydratorService::RehydrateIdentity(
    std::uint32_t identityId,
    std::uint32_t shellAccountId,
    std::uint64_t shellCharacterGuid,
    std::string* failureReason) const
{
    integration::SqlBotIdentityRepository identityRepo;
    integration::SqlBotAssignedGearRepository assignedGearRepo;
    integration::SqlBotAssignedGearTemplateRepository assignedGearTemplateRepo;
    integration::SqlBotDisplayLoadoutRepository displayLoadoutRepo;
    integration::SqlBotRuntimeSnapshotRepository snapshotRepo;
    integration::SqlBotShellRuntimeRepository shellRuntimeRepo;
    integration::SqlBotRebuildLogRepository rebuildLogRepo;

    std::optional<BotIdentityRecord> identity = identityRepo.FindById(identityId);
    if (!identity)
    {
        if (failureReason)
            *failureReason = "ledger identity not found";
        return false;
    }

    if (!identity->appearanceResolved)
    {
        BotAppearanceResolver appearanceResolver;
        if (!appearanceResolver.ResolveAndPersist(*identity, identityRepo))
        {
            if (failureReason)
                *failureReason = "appearance could not be resolved";
            return false;
        }
    }

    std::optional<ShellBinding> shellBinding;
    if (shellAccountId != 0 && shellCharacterGuid != 0)
    {
        shellBinding = ShellBinding{ shellAccountId, shellCharacterGuid };
    }
    else
    {
        shellBinding = ResolveShellBinding(*identity, shellRuntimeRepo);
    }
    if (!shellBinding)
    {
        if (failureReason)
            *failureReason = "identity has no assigned shell";
        return false;
    }

    std::optional<ShellCharacterRow> shellRow = LoadShellCharacterRow(shellBinding->characterGuid);
    if (!shellRow)
    {
        if (failureReason)
            *failureReason = "assigned shell character row not found";
        return false;
    }

    if (shellRow->accountId != shellBinding->accountId)
    {
        if (failureReason)
            *failureReason = "assigned shell account mismatch";
        return false;
    }

    if (ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(shellBinding->characterGuid)))
    {
        if (failureReason)
            *failureReason = "assigned shell is already loaded";
        return false;
    }

    std::optional<BotRuntimeSnapshotRecord> snapshot = snapshotRepo.LoadByIdentity(identityId);
    std::optional<BotDisplayLoadoutRecord> displayLoadout =
        displayLoadoutRepo.LoadByIdentity(identityId);
    model::WorldBotPreparedBuild preparedBuild =
        GetShellPreparationService().Prepare(*identity, "PvP");
    if (!preparedBuild.IsReady())
    {
        if (failureReason)
            *failureReason = "prepared build not ready";
        return false;
    }

    WorldBotAssignedGearService assignedGearService(
        assignedGearRepo,
        assignedGearTemplateRepo);
    WorldBotAssignedGearResult assignedGearResult = assignedGearService.EnsureAssignedGear(
        *identity,
        preparedBuild.canonicalSpecKey,
        preparedBuild.resolvedRoleKey);
    if (assignedGearResult.refreshed)
    {
        identityRepo.UpdateGearRefreshState(
            identityId,
            identity->gearRefreshPending,
            identity->lastGearRefreshBand);
    }

    std::vector<WorldBotAssignedGearEntry> assignedGear = assignedGearResult.entries;
    if (assignedGear.empty())
        assignedGear = assignedGearRepo.LoadAssignments(identityId);

    std::vector<SeededShellSkill> seededSkills = BuildDefaultShellSkills(*identity);
    std::vector<EquippedShellItem> equippedItems =
        BuildEquippedItems(assignedGear, displayLoadout);
    std::vector<PreservedBagItem> preservedBags =
        LoadPreservedBagItems(shellBinding->characterGuid);
    std::vector<std::uint64_t> petIds =
        LoadOwnedPetIds(shellBinding->characterGuid);

    std::uint32_t const nextShellStateVersion = identity->shellStateVersion + 1;
    identityRepo.UpdateShellState(
        identityId,
        shellBinding->accountId,
        shellBinding->characterGuid,
        nextShellStateVersion,
        "rehydrate");

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    std::vector<std::uint64_t> preservedBagGuids;
    preservedBagGuids.reserve(preservedBags.size());
    for (PreservedBagItem const& bag : preservedBags)
        preservedBagGuids.push_back(bag.itemGuid);

    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "DELETE FROM character_action WHERE guid = {}",
            shellBinding->characterGuid));
    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "DELETE FROM character_spell WHERE guid = {}",
            shellBinding->characterGuid));
    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "DELETE FROM character_spell_cooldown WHERE guid = {}",
            shellBinding->characterGuid));
    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "DELETE FROM character_skills WHERE guid = {}",
            shellBinding->characterGuid));
    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "DELETE FROM character_talent WHERE guid = {}",
            shellBinding->characterGuid));
    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "DELETE FROM character_aura WHERE guid = {}",
            shellBinding->characterGuid));

    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "DELETE FROM character_inventory WHERE guid = {} "
            "AND NOT (bag = 0 AND slot BETWEEN {} AND {})",
            shellBinding->characterGuid,
            static_cast<std::uint32_t>(INVENTORY_SLOT_BAG_START),
            static_cast<std::uint32_t>(INVENTORY_SLOT_BAG_END - 1)));

    if (preservedBagGuids.empty())
    {
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "DELETE FROM item_instance WHERE owner_guid = {}",
                shellBinding->characterGuid));
    }
    else
    {
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "DELETE FROM item_instance WHERE owner_guid = {} AND guid NOT IN ({})",
                shellBinding->characterGuid,
                BuildInList(preservedBagGuids)));
    }

    if (!petIds.empty())
    {
        std::string const petIdList = BuildInList(petIds);
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "DELETE FROM pet_aura WHERE guid IN ({})",
                petIdList));
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "DELETE FROM pet_spell WHERE guid IN ({})",
                petIdList));
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "DELETE FROM pet_spell_cooldown WHERE guid IN ({})",
                petIdList));
    }
    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "DELETE FROM character_pet_declinedname WHERE owner = {}",
            shellBinding->characterGuid));
    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "DELETE FROM character_pet WHERE owner = {}",
            shellBinding->characterGuid));

    std::uint16_t const mapId = snapshot ? snapshot->mapId : shellRow->mapId;
    std::uint32_t const zoneId = snapshot ? snapshot->zoneId : shellRow->zoneId;
    float const posX = snapshot ? snapshot->x : shellRow->x;
    float const posY = snapshot ? snapshot->y : shellRow->y;
    float const posZ = snapshot ? snapshot->z : shellRow->z;
    float const orientation = snapshot ? snapshot->o : shellRow->o;
    std::uint32_t const playerFlags = BuildPlayerFlags(displayLoadout);
    std::string escapedName = identity->name;
    CharacterDatabase.EscapeString(escapedName);

    CharacterDatabase.ExecuteOrAppend(
        trans,
        Acore::StringFormat(
            "UPDATE characters SET name = '{}', race = {}, class = {}, gender = {}, level = {}, xp = 0, "
            "skin = {}, face = {}, hairStyle = {}, hairColor = {}, facialStyle = {}, "
            "playerFlags = {}, position_x = {}, position_y = {}, position_z = {}, map = {}, zone = {}, orientation = {}, "
            "online = 0, restState = 0, is_logout_resting = 0, rest_bonus = 0, "
            "resettalents_cost = 0, resettalents_time = 0, "
            "trans_x = 0, trans_y = 0, trans_z = 0, trans_o = 0, transguid = 0, "
            "at_login = 0, death_expire_time = 0, taxi_path = NULL, "
            "health = GREATEST(health, 1), activeTalentGroup = 0, talentGroupsCount = 1, "
            "equipmentCache = '', ammoId = 0, actionBars = 0, logout_time = UNIX_TIMESTAMP() "
            "WHERE guid = {}",
            escapedName,
            static_cast<std::uint32_t>(identity->raceId),
            static_cast<std::uint32_t>(identity->classId),
            static_cast<std::uint32_t>(identity->gender),
            static_cast<std::uint32_t>(identity->level),
            static_cast<std::uint32_t>(identity->skin),
            static_cast<std::uint32_t>(identity->face),
            static_cast<std::uint32_t>(identity->hairStyle),
            static_cast<std::uint32_t>(identity->hairColor),
            static_cast<std::uint32_t>(identity->facialStyle),
            playerFlags,
            posX,
            posY,
            posZ,
            static_cast<std::uint32_t>(mapId),
            zoneId,
            orientation,
            shellBinding->characterGuid));

    for (std::uint32_t spellId : preparedBuild.knownSpellIds)
    {
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "INSERT IGNORE INTO character_spell (guid, spell, specMask) "
                "VALUES ({}, {}, 255)",
                shellBinding->characterGuid,
                spellId));
    }

    for (SeededShellSkill const& skill : seededSkills)
    {
        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "INSERT INTO character_skills (guid, skill, value, max) "
                "VALUES ({}, {}, {}, {})",
                shellBinding->characterGuid,
                static_cast<std::uint32_t>(skill.skillId),
                static_cast<std::uint32_t>(skill.value),
                static_cast<std::uint32_t>(skill.maxValue)));
    }

    std::uint32_t equippedCount = 0;
    for (EquippedShellItem const& item : equippedItems)
    {
        if (item.itemId == 0)
            continue;

        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item.itemId);
        if (!itemTemplate)
        {
            LOG_WARN(
                "server.worldserver",
                "[LivingWorldDebug] ShellHydrate identityId={} shellGuid={} slot={} missing item template {}.",
                identityId,
                shellBinding->characterGuid,
                static_cast<std::uint32_t>(item.slot),
                item.itemId);
            continue;
        }

        std::uint64_t const itemGuid =
            sObjectMgr->GetGenerator<HighGuid::Item>().Generate();
        std::string enchantments = item.enchantments;
        CharacterDatabase.EscapeString(enchantments);

        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "REPLACE INTO item_instance "
                "(itemEntry, owner_guid, creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, "
                "randomPropertyId, durability, playedTime, text, guid) "
                "VALUES ({}, {}, 0, 0, 1, 0, '', 0, '{}', 0, {}, 0, '', {})",
                item.itemId,
                shellBinding->characterGuid,
                enchantments,
                itemTemplate->MaxDurability,
                itemGuid));

        CharacterDatabase.ExecuteOrAppend(
            trans,
            Acore::StringFormat(
                "REPLACE INTO character_inventory (guid, bag, slot, item) "
                "VALUES ({}, 0, {}, {})",
                shellBinding->characterGuid,
                static_cast<std::uint32_t>(item.slot),
                itemGuid));
        ++equippedCount;
    }

    CharacterDatabase.CommitTransaction(trans);

    BotShellRuntimeRecord runtime;
    if (std::optional<BotShellRuntimeRecord> existing = shellRuntimeRepo.FindByIdentity(identityId))
        runtime = *existing;
    runtime.identityId = identityId;
    runtime.shellAccountId = shellBinding->accountId;
    runtime.shellCharacterGuid = shellBinding->characterGuid;
    runtime.isMaterialized = false;
    runtime.shellStateVersion = nextShellStateVersion;
    shellRuntimeRepo.Upsert(runtime);

    identityRepo.MarkShellRehydrated(identityId, nextShellStateVersion);

    model::BotRebuildLogEntry rebuildLog;
    rebuildLog.identityId = identityId;
    rebuildLog.shellAccountId = shellBinding->accountId;
    rebuildLog.shellCharacterGuid = shellBinding->characterGuid;
    rebuildLog.rebuildReason = "rehydrate";
    rebuildLog.level = identity->level;
    rebuildLog.gearTier = identity->gearTier;
    rebuildLog.specKey = identity->specKey;
    rebuildLog.loadoutKey = identity->loadoutKey;
    rebuildLog.displayLoadoutKey = identity->displayLoadoutKey;
    rebuildLog.doctrineProfileKey = identity->doctrineProfileKey;
    rebuildLog.shellStateVersion = nextShellStateVersion;
    rebuildLog.notes = Acore::StringFormat(
        "equipped={} preserved_bags={}",
        equippedCount,
        preservedBags.size());
    rebuildLogRepo.Append(rebuildLog);

    LOG_INFO(
        "server.worldserver",
        "[LivingWorldDebug] ShellHydrate identityId={} shellGuid={} accountId={} race={} class={} gender={} level={} equipped={}",
        identityId,
        shellBinding->characterGuid,
        shellBinding->accountId,
        static_cast<std::uint32_t>(identity->raceId),
        static_cast<std::uint32_t>(identity->classId),
        static_cast<std::uint32_t>(identity->gender),
        static_cast<std::uint32_t>(identity->level),
        equippedCount);
    return true;
}

} // namespace service
} // namespace living_world
