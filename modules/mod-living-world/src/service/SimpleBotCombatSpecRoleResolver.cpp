#include "service/SimpleBotCombatSpecRoleResolver.h"

#include "DatabaseEnv.h"
#include "DataStores/DBCStores.h"
#include "DataStores/DBCStructure.h"
#include "QueryResult.h"
#include "SharedDefines.h"

#include <array>
#include <unordered_map>

namespace living_world
{
namespace service
{
namespace
{
// Maps class + dominant talent tree tab page (0/1/2) to { specKey, roleKey }.
// Tab page order matches the in-game left-to-right tree order for each class.
std::pair<std::string, std::string> SpecRoleForClassAndTab(
    std::uint8_t classId,
    int dominantTab)
{
    // Tab layout per class (WotLK 3.3.5):
    //   Warrior:    0=Arms  1=Fury       2=Protection
    //   Paladin:    0=Holy  1=Protection 2=Retribution
    //   Hunter:     0=BM    1=MM         2=Survival
    //   Rogue:      0=Assa  1=Combat     2=Subtlety
    //   Priest:     0=Disc  1=Holy       2=Shadow
    //   DK:         0=Blood 1=Frost      2=Unholy
    //   Shaman:     0=Ele   1=Enh        2=Restoration
    //   Mage:       0=Arc   1=Fire       2=Frost
    //   Warlock:    0=Affl  1=Demo       2=Destr
    //   Druid:      0=Bal   1=Feral      2=Restoration
    static std::pair<std::string,std::string> const kFallback = { "", "DPS" };

    switch (classId)
    {
        case CLASS_WARRIOR:
            if (dominantTab == 2) return { "Protection", "TANK" };
            return { dominantTab == 0 ? "Arms" : "Fury", "DPS" };
        case CLASS_PALADIN:
            if (dominantTab == 0) return { "Holy",         "HEAL" };
            if (dominantTab == 1) return { "Protection",   "TANK" };
            return { "Retribution", "DPS" };
        case CLASS_HUNTER:
        {
            static char const* const specs[] = { "BeastMastery", "Marksmanship", "Survival" };
            return { dominantTab >= 0 && dominantTab <= 2 ? specs[dominantTab] : "BeastMastery", "DPS" };
        }
        case CLASS_ROGUE:
        {
            static char const* const specs[] = { "Assassination", "Combat", "Subtlety" };
            return { dominantTab >= 0 && dominantTab <= 2 ? specs[dominantTab] : "Combat", "DPS" };
        }
        case CLASS_PRIEST:
            if (dominantTab == 0) return { "Holy", "HEAL" }; // Discipline → heal role
            if (dominantTab == 1) return { "Holy", "HEAL" };
            return { "Shadow", "DPS" };
        case CLASS_DEATH_KNIGHT:
            if (dominantTab == 0) return { "Blood",  "TANK" };
            if (dominantTab == 1) return { "Frost",  "DPS" };
            return { "Unholy", "DPS" };
        case CLASS_SHAMAN:
            if (dominantTab == 2) return { "Restoration", "HEAL" };
            return { dominantTab == 0 ? "Elemental" : "Enhancement", "DPS" };
        case CLASS_MAGE:
        {
            static char const* const specs[] = { "Arcane", "Fire", "Frost" };
            return { dominantTab >= 0 && dominantTab <= 2 ? specs[dominantTab] : "Frost", "DPS" };
        }
        case CLASS_WARLOCK:
        {
            static char const* const specs[] = { "Affliction", "Demonology", "Destruction" };
            return { dominantTab >= 0 && dominantTab <= 2 ? specs[dominantTab] : "Affliction", "DPS" };
        }
        case CLASS_DRUID:
            if (dominantTab == 1) return { "Feral",        "TANK" };
            if (dominantTab == 2) return { "Restoration",  "HEAL" };
            return { "Balance", "DPS" };
        default:
            return kFallback;
    }
}

// Builds a spell-id → tabpage map from sTalentStore/sTalentTabStore once
// and caches it for the process lifetime.
std::unordered_map<std::uint32_t, int> const& GetSpellToTabPageMap()
{
    static std::unordered_map<std::uint32_t, int> s_map = []{
        std::unordered_map<std::uint32_t, int> m;
        for (std::uint32_t i = 0; i < sTalentStore.GetNumRows(); ++i)
        {
            TalentEntry const* talent = sTalentStore.LookupEntry(i);
            if (!talent)
                continue;
            TalentTabEntry const* tab = sTalentTabStore.LookupEntry(talent->TalentTab);
            if (!tab)
                continue;
            int const tabPage = static_cast<int>(tab->tabpage);
            for (std::uint32_t spellId : talent->RankID)
                if (spellId)
                    m[spellId] = tabPage;
        }
        return m;
    }();
    return s_map;
}

// Returns the dominant talent tree tab page (0/1/2) for the active spec of the
// given source character, or -1 if no talent data is found.
int LoadDominantTalentTab(std::uint64_t sourceCharacterGuid)
{
    if (sourceCharacterGuid == 0)
        return -1;

    QueryResult result = CharacterDatabase.Query(
        "SELECT spell FROM character_talent WHERE guid = {} AND (specMask & 1) <> 0",
        sourceCharacterGuid);
    if (!result)
        return -1;

    std::unordered_map<std::uint32_t, int> const& spellToTab = GetSpellToTabPageMap();
    std::array<int, 3> tabPoints = { 0, 0, 0 };

    do
    {
        std::uint32_t const spellId = result->Fetch()[0].Get<std::uint32_t>();
        auto const it = spellToTab.find(spellId);
        if (it != spellToTab.end() && it->second >= 0 && it->second <= 2)
            ++tabPoints[it->second];
    } while (result->NextRow());

    int dominant = -1;
    int maxPoints = 0;
    for (int tab = 0; tab < 3; ++tab)
    {
        if (tabPoints[tab] > maxPoints)
        {
            maxPoints = tabPoints[tab];
            dominant = tab;
        }
    }
    return dominant;
}

std::pair<std::string, std::string> GetGuessedSpecAndRole(
    std::uint8_t classId,
    std::uint64_t sourceCharacterGuid)
{
    int const dominantTab = LoadDominantTalentTab(sourceCharacterGuid);
    return SpecRoleForClassAndTab(classId, dominantTab);
}
} // namespace

BotCombatSpecRoleResolutionResult SimpleBotCombatSpecRoleResolver::Resolve(
    BotCombatSpecRoleResolutionRequest const& request) const
{
    BotCombatSpecRoleResolutionResult result;
    auto const [guessedSpec, guessedRole] =
        GetGuessedSpecAndRole(request.classId, request.sourceCharacterGuid);

    result.guessedSpecKey = guessedSpec;
    result.guessedRoleKey = guessedRole;
    result.effectiveSpecKey =
        request.specOverrideKey && !request.specOverrideKey->empty()
            ? *request.specOverrideKey
            : guessedSpec;
    result.effectiveRoleKey =
        request.roleOverrideKey && !request.roleOverrideKey->empty()
            ? *request.roleOverrideKey
            : guessedRole;
    return result;
}
} // namespace service
} // namespace living_world
