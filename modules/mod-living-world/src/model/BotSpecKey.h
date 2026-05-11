#pragma once

#include <string>
#include <string_view>

namespace living_world
{
namespace model
{

// Normalizes older flavor-style spec keys used by some seeded bot rows
// (e.g. warrior_arms, paladin_ret) into the canonical spec keys used by the
// combat doctrine and talent-template systems (e.g. Arms, Retribution).
inline std::string CanonicalizeBotSpecKey(std::string_view specKey)
{
    if (specKey == "warrior_arms")  return "Arms";
    if (specKey == "warrior_fury")  return "Fury";
    if (specKey == "warrior_prot")  return "Protection";

    if (specKey == "paladin_holy")  return "Holy";
    if (specKey == "paladin_prot")  return "Protection";
    if (specKey == "paladin_ret")   return "Retribution";

    if (specKey == "hunter_bm")     return "BeastMastery";
    if (specKey == "hunter_mm")     return "Marksmanship";
    if (specKey == "hunter_sv")     return "Survival";

    if (specKey == "rogue_assa")    return "Assassination";
    if (specKey == "rogue_combat")  return "Combat";
    if (specKey == "rogue_sub")     return "Subtlety";

    if (specKey == "priest_disc")   return "Discipline";
    if (specKey == "priest_holy")   return "Holy";
    if (specKey == "priest_shadow") return "Shadow";

    if (specKey == "dk_blood")      return "Blood";
    if (specKey == "dk_frost")      return "Frost";
    if (specKey == "dk_unholy")     return "Unholy";

    if (specKey == "shaman_ele")    return "Elemental";
    if (specKey == "shaman_enh")    return "Enhancement";
    if (specKey == "shaman_resto")  return "Restoration";

    if (specKey == "mage_arcane")   return "Arcane";
    if (specKey == "mage_fire")     return "Fire";
    if (specKey == "mage_frost")    return "Frost";

    if (specKey == "warlock_afflic") return "Affliction";
    if (specKey == "warlock_demo")   return "Demonology";
    if (specKey == "warlock_destro") return "Destruction";

    if (specKey == "druid_balance") return "Balance";
    if (specKey == "druid_feral")   return "Feral";
    if (specKey == "druid_resto")   return "Restoration";

    return std::string(specKey);
}

} // namespace model
} // namespace living_world