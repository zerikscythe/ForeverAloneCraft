#include "service/BotCombatRuntimeEvaluator.h"

#include "service/BotCombatSimulatedItemUse.h"

#include "CellImpl.h"
#include "Creature.h"
#include "DataStores/DBCStores.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "Player.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace living_world
{
namespace service
{
namespace
{
struct ResolvedAoEActionTargeting
{
    bool valid = true;
    std::optional<model::BotCombatAoEMode> aoeMode;
    std::optional<std::uint8_t> aoeMinTargets;
    std::optional<float> aoeRadius;
    bool useDestination = false;
    float destinationX = 0.0f;
    float destinationY = 0.0f;
    float destinationZ = 0.0f;
};

struct AoECandidatePoint
{
    float x = 0.0f;
    float y = 0.0f;
};

struct BestAoEPointResult
{
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::size_t hits = 0;
};

static constexpr std::uint32_t WorldBotEntry = 9900001;
static constexpr float AmbientSupportScanRadius = 45.0f;

// Returns the remaining cooldown in milliseconds for the given spell on any Unit.
// Dispatches to Player::GetSpellCooldownDelay or Creature::GetSpellCooldown.
std::uint32_t GetSpellCooldownRemainingMs(Unit* bot, std::uint32_t spellId)
{
    if (!bot || spellId == 0)
        return 0;
    if (Player* player = bot->ToPlayer())
        return player->GetSpellCooldownDelay(spellId);
    if (Creature* creature = bot->ToCreature())
        return creature->GetSpellCooldown(spellId);
    return 0;
}

// Returns the global cooldown remaining in ms for a given spell on any Unit.
// Creatures do not use the player GCD system; returns 0 for non-Player units.
std::uint32_t GetGlobalCooldownRemainingMs(Unit* bot, SpellInfo const* spellInfo)
{
    if (!bot || !spellInfo)
        return 0;
    if (Player* player = bot->ToPlayer())
        return player->GetGlobalCooldownMgr().GetGlobalCooldown(spellInfo);
    return 0;
}

bool CompareNumeric(
    model::BotCombatConditionOperator op,
    float left,
    float right)
{
    switch (op)
    {
        case model::BotCombatConditionOperator::Equal:
            return std::fabs(left - right) < 0.01f;
        case model::BotCombatConditionOperator::NotEqual:
            return std::fabs(left - right) >= 0.01f;
        case model::BotCombatConditionOperator::LessThan:
            return left < right;
        case model::BotCombatConditionOperator::LessThanOrEqual:
            return left <= right;
        case model::BotCombatConditionOperator::GreaterThan:
            return left > right;
        case model::BotCombatConditionOperator::GreaterThanOrEqual:
            return left >= right;
        case model::BotCombatConditionOperator::Has:
        case model::BotCombatConditionOperator::NotHas:
        case model::BotCombatConditionOperator::Exists:
            return false;
    }

    return false;
}

std::optional<std::uint32_t> ParseConditionSpellId(
    model::BotCombatConditionDefinition const& condition)
{
    if (!condition.stringValue.empty())
    {
        std::uint32_t spellId = 0;
        auto const* begin = condition.stringValue.data();
        auto const* end = begin + condition.stringValue.size();
        auto const result = std::from_chars(begin, end, spellId);
        if (result.ec == std::errc{} && result.ptr == end)
            return spellId;
    }

    if (condition.numericValue > 0.0f)
        return static_cast<std::uint32_t>(condition.numericValue);

    return std::nullopt;
}

float GetManaPct(Unit* unit)
{
    if (!unit)
        return 0.0f;

    if (unit->GetMaxPower(POWER_MANA) == 0)
        return 100.0f;

    return 100.0f * static_cast<float>(unit->GetPower(POWER_MANA)) /
        static_cast<float>(unit->GetMaxPower(POWER_MANA));
}

float NormalizeDisplayedPower(Powers powerType, std::int32_t rawValue)
{
    if (powerType == POWER_RAGE || powerType == POWER_RUNIC_POWER)
        return static_cast<float>(rawValue) / 10.0f;

    return static_cast<float>(rawValue);
}

float GetPower(Unit* unit, Powers powerType)
{
    if (!unit)
        return 0.0f;

    return NormalizeDisplayedPower(powerType, unit->GetPower(powerType));
}

float GetPowerPct(Unit* unit, Powers powerType)
{
    if (!unit)
        return 0.0f;

    std::uint32_t const maxPower = unit->GetMaxPower(powerType);
    if (maxPower == 0)
        return 100.0f;

    return 100.0f * static_cast<float>(unit->GetPower(powerType)) /
        static_cast<float>(maxPower);
}

std::string ToLowerCopy(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string CreatureTypeToKey(Unit* unit)
{
    if (!unit)
        return "";

    switch (unit->GetCreatureType())
    {
        case CREATURE_TYPE_BEAST:
            return "beast";
        case CREATURE_TYPE_DRAGONKIN:
            return "dragonkin";
        case CREATURE_TYPE_DEMON:
            return "demon";
        case CREATURE_TYPE_ELEMENTAL:
            return "elemental";
        case CREATURE_TYPE_GIANT:
            return "giant";
        case CREATURE_TYPE_UNDEAD:
            return "undead";
        case CREATURE_TYPE_HUMANOID:
            return "humanoid";
        case CREATURE_TYPE_CRITTER:
            return "critter";
        case CREATURE_TYPE_MECHANICAL:
            return "mechanical";
        case CREATURE_TYPE_NOT_SPECIFIED:
            return "notspecified";
        case CREATURE_TYPE_TOTEM:
            return "totem";
        case CREATURE_TYPE_NON_COMBAT_PET:
            return "noncombatpet";
        case CREATURE_TYPE_GAS_CLOUD:
            return "gascloud";
        default:
            return "";
    }
}

bool CreatureTypeMatches(std::string const& actualType, std::string expectedValue)
{
    if (actualType.empty())
        return false;

    expectedValue = ToLowerCopy(expectedValue);
    expectedValue.erase(
        std::remove_if(
            expectedValue.begin(),
            expectedValue.end(),
            [](unsigned char c)
            {
                return std::isspace(c) || c == '_' || c == '-';
            }),
        expectedValue.end());

    if (expectedValue.empty())
        return false;

    std::size_t start = 0;
    while (start <= expectedValue.size())
    {
        std::size_t const end = expectedValue.find('|', start);
        std::string token = expectedValue.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (token == actualType)
            return true;

        if (end == std::string::npos)
            break;
        start = end + 1;
    }

    return false;
}

bool TryParseRuneType(
    std::string const& value,
    std::optional<RuneType>& runeType)
{
    std::string const lowered = ToLowerCopy(value);
    if (lowered.empty() || lowered == "any")
    {
        runeType = std::nullopt;
        return true;
    }

    if (lowered == "blood" || lowered == "b")
    {
        runeType = RUNE_BLOOD;
        return true;
    }

    if (lowered == "frost" || lowered == "f")
    {
        runeType = RUNE_FROST;
        return true;
    }

    if (lowered == "unholy" || lowered == "u")
    {
        runeType = RUNE_UNHOLY;
        return true;
    }

    if (lowered == "death" || lowered == "d")
    {
        runeType = RUNE_DEATH;
        return true;
    }

    return false;
}

bool RuneMatchesRequested(RuneType currentRune, RuneType requestedRune)
{
    if (requestedRune == RUNE_DEATH)
        return currentRune == RUNE_DEATH;

    return currentRune == requestedRune || currentRune == RUNE_DEATH;
}

std::uint32_t CountReadyRunes(Player* player, std::optional<RuneType> requestedRune)
{
    if (!player)
        return 0;

    std::uint32_t ready = 0;
    for (std::uint8_t i = 0; i < MAX_RUNES; ++i)
    {
        if (player->GetRuneCooldown(i) != 0)
            continue;

        RuneType const currentRune = player->GetCurrentRune(i);
        if (!requestedRune || RuneMatchesRequested(currentRune, *requestedRune))
            ++ready;
    }

    return ready;
}

bool HasRequiredRunes(Player* player, std::uint32_t runeCostId)
{
    if (!player || runeCostId == 0)
        return true;

    SpellRuneCostEntry const* runeCostData = sSpellRuneCostStore.LookupEntry(runeCostId);
    if (!runeCostData || runeCostData->NoRuneCost())
        return true;

    int32 runeCost[NUM_RUNE_TYPES] = { 0, 0, 0, 0 };
    for (std::uint32_t i = 0; i < RUNE_DEATH; ++i)
        runeCost[i] = runeCostData->RuneCost[i];

    for (std::uint8_t i = 0; i < MAX_RUNES; ++i)
    {
        RuneType const rune = player->GetCurrentRune(i);
        if (player->GetRuneCooldown(i) == 0 && runeCost[rune] > 0)
            --runeCost[rune];
    }

    int32 deathRuneDeficit = 0;
    for (std::uint32_t i = 0; i < RUNE_DEATH; ++i)
    {
        if (runeCost[i] > 0)
            deathRuneDeficit += runeCost[i];
    }

    if (deathRuneDeficit == 0)
        return true;

    for (std::uint8_t i = 0; i < MAX_RUNES; ++i)
    {
        if (player->GetRuneCooldown(i) == 0 && player->GetCurrentRune(i) == RUNE_DEATH)
            --deathRuneDeficit;
    }

    return deathRuneDeficit <= 0;
}

float GetAuraRemainingSecs(Unit* unit, std::uint32_t spellId)
{
    if (!unit || spellId == 0)
        return 0.0f;

    Aura const* aura = unit->GetAura(spellId);
    if (!aura)
        return 0.0f;

    return static_cast<float>(aura->GetDuration()) / 1000.0f;
}

std::uint32_t CountNearbyEnemies(
    BotCombatRuntimeContext const& context,
    Unit* subject,
    float radius)
{
    if (!subject || radius <= 0.0f)
        return 0;

    Unit* hostilityReference = context.bot ? context.bot : subject;
    std::vector<Unit*> targets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(subject, hostilityReference, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(subject, targets, check);
    Cell::VisitObjects(subject, searcher, radius);
    return static_cast<std::uint32_t>(targets.size());
}

std::vector<Unit*> CollectNearbyEnemies(
    BotCombatRuntimeContext const& context,
    Unit* subject,
    float radius)
{
    if (!subject || radius <= 0.0f)
        return {};

    Unit* hostilityReference = context.bot ? context.bot : subject;
    std::vector<Unit*> targets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(subject, hostilityReference, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(subject, targets, check);
    Cell::VisitObjects(subject, searcher, radius);
    targets.erase(
        std::remove_if(
            targets.begin(),
            targets.end(),
            [](Unit* unit)
            {
                return !unit || !unit->IsAlive() || !unit->IsInWorld();
            }),
        targets.end());
    return targets;
}

std::vector<Unit*> CollectEnemiesWithinRadius(
    std::vector<Unit*> const& enemies,
    float x,
    float y,
    float radius)
{
    std::vector<Unit*> hits;
    float const radiusSq = radius * radius;
    for (Unit* enemy : enemies)
    {
        if (!enemy)
            continue;

        float const dx = enemy->GetPositionX() - x;
        float const dy = enemy->GetPositionY() - y;
        float const distSq = dx * dx + dy * dy;
        if (distSq <= radiusSq + 0.01f)
            hits.push_back(enemy);
    }

    return hits;
}

AoECandidatePoint ComputeCentroid2d(std::vector<Unit*> const& enemies)
{
    AoECandidatePoint point;
    if (enemies.empty())
        return point;

    for (Unit* enemy : enemies)
    {
        point.x += enemy->GetPositionX();
        point.y += enemy->GetPositionY();
    }

    float const divisor = static_cast<float>(enemies.size());
    point.x /= divisor;
    point.y /= divisor;
    return point;
}

float ComputeAverageZ(std::vector<Unit*> const& enemies)
{
    if (enemies.empty())
        return 0.0f;

    float z = 0.0f;
    for (Unit* enemy : enemies)
        z += enemy->GetPositionZ();

    return z / static_cast<float>(enemies.size());
}

std::vector<AoECandidatePoint> BuildPairCircleCenters(
    Unit* left,
    Unit* right,
    float radius)
{
    if (!left || !right || radius <= 0.0f)
        return {};

    float const x1 = left->GetPositionX();
    float const y1 = left->GetPositionY();
    float const x2 = right->GetPositionX();
    float const y2 = right->GetPositionY();
    float const dx = x2 - x1;
    float const dy = y2 - y1;
    float const distSq = dx * dx + dy * dy;
    if (distSq <= 0.0001f)
        return { { x1, y1 } };

    float const dist = std::sqrt(distSq);
    float const diameter = radius * 2.0f;
    if (dist > diameter)
        return {};

    float const midX = (x1 + x2) * 0.5f;
    float const midY = (y1 + y2) * 0.5f;
    float const halfDist = dist * 0.5f;
    float const heightSq = std::max(0.0f, radius * radius - halfDist * halfDist);
    float const height = std::sqrt(heightSq);
    float const invDist = 1.0f / dist;
    float const perpX = -dy * invDist;
    float const perpY = dx * invDist;

    std::vector<AoECandidatePoint> candidates;
    candidates.push_back({ midX + perpX * height, midY + perpY * height });
    if (height > 0.0001f)
        candidates.push_back({ midX - perpX * height, midY - perpY * height });
    return candidates;
}

BestAoEPointResult FindBestAoECenter(
    std::vector<Unit*> const& enemies,
    float radius,
    Unit* anchorTarget)
{
    BestAoEPointResult best;
    if (enemies.empty() || radius <= 0.0f)
        return best;

    std::vector<AoECandidatePoint> candidates;
    candidates.reserve(enemies.size() * enemies.size());
    for (Unit* enemy : enemies)
        candidates.push_back({ enemy->GetPositionX(), enemy->GetPositionY() });
    candidates.push_back(ComputeCentroid2d(enemies));

    for (std::size_t i = 0; i < enemies.size(); ++i)
    {
        for (std::size_t j = i + 1; j < enemies.size(); ++j)
        {
            std::vector<AoECandidatePoint> pairCandidates =
                BuildPairCircleCenters(enemies[i], enemies[j], radius);
            candidates.insert(candidates.end(), pairCandidates.begin(), pairCandidates.end());
        }
    }

    float bestSpreadScore = std::numeric_limits<float>::max();
    float bestAnchorScore = std::numeric_limits<float>::max();

    for (AoECandidatePoint const& candidate : candidates)
    {
        std::vector<Unit*> hits = CollectEnemiesWithinRadius(enemies, candidate.x, candidate.y, radius);
        if (hits.empty())
            continue;

        float spreadScore = 0.0f;
        for (Unit* hit : hits)
        {
            float const dx = hit->GetPositionX() - candidate.x;
            float const dy = hit->GetPositionY() - candidate.y;
            spreadScore += dx * dx + dy * dy;
        }

        float anchorScore = 0.0f;
        if (anchorTarget)
        {
            float const dx = anchorTarget->GetPositionX() - candidate.x;
            float const dy = anchorTarget->GetPositionY() - candidate.y;
            anchorScore = dx * dx + dy * dy;
        }

        bool const betterHits = hits.size() > best.hits;
        bool const betterSpread = hits.size() == best.hits && spreadScore < bestSpreadScore;
        bool const betterAnchor =
            hits.size() == best.hits &&
            std::fabs(spreadScore - bestSpreadScore) < 0.01f &&
            anchorScore < bestAnchorScore;

        if (!best.valid || betterHits || betterSpread || betterAnchor)
        {
            best.valid = true;
            best.x = candidate.x;
            best.y = candidate.y;
            best.z = ComputeAverageZ(hits);
            best.hits = hits.size();
            bestSpreadScore = spreadScore;
            bestAnchorScore = anchorScore;
        }
    }

    if (!best.valid)
        return best;

    std::vector<Unit*> bestHits = CollectEnemiesWithinRadius(enemies, best.x, best.y, radius);
    AoECandidatePoint const refinedCentroid = ComputeCentroid2d(bestHits);
    std::vector<Unit*> refinedHits =
        CollectEnemiesWithinRadius(enemies, refinedCentroid.x, refinedCentroid.y, radius);
    if (refinedHits.size() >= best.hits)
    {
        best.x = refinedCentroid.x;
        best.y = refinedCentroid.y;
        best.z = ComputeAverageZ(refinedHits);
        best.hits = refinedHits.size();
    }

    return best;
}

bool SpellUsesDestinationTarget(SpellInfo const* spellInfo)
{
    return spellInfo && (spellInfo->GetExplicitTargetMask() & TARGET_FLAG_DEST_LOCATION);
}

ResolvedAoEActionTargeting ResolveAoEActionTargeting(
    model::BotCombatActionDefinition const& action,
    BotCombatRuntimeContext const& context,
    Unit* target,
    SpellInfo const* spellInfo)
{
    ResolvedAoEActionTargeting resolved;
    resolved.aoeMode = action.aoeMode;
    if (!resolved.aoeMode && (action.aoeMinTargets || action.aoeRadius))
        resolved.aoeMode = context.defaultAoEMode;

    if (!resolved.aoeMode)
        return resolved;

    resolved.aoeMinTargets = action.aoeMinTargets.value_or(context.defaultAoEMinTargets);
    resolved.aoeRadius = action.aoeRadius.value_or(context.defaultAoEScanRadius);

    if (!target || !resolved.aoeRadius || *resolved.aoeRadius <= 0.0f)
    {
        resolved.valid = false;
        return resolved;
    }

    std::vector<Unit*> const nearbyEnemies = CollectNearbyEnemies(
        context,
        target,
        *resolved.aoeRadius);
    std::uint8_t const minTargets = std::max<std::uint8_t>(1, *resolved.aoeMinTargets);
    if (nearbyEnemies.size() < minTargets)
    {
        resolved.valid = false;
        return resolved;
    }

    switch (*resolved.aoeMode)
    {
        case model::BotCombatAoEMode::Centroid:
        {
            BestAoEPointResult const bestPoint =
                FindBestAoECenter(nearbyEnemies, *resolved.aoeRadius, target);
            if (!bestPoint.valid || bestPoint.hits < minTargets)
            {
                resolved.valid = false;
                return resolved;
            }

            resolved.destinationX = bestPoint.x;
            resolved.destinationY = bestPoint.y;
            resolved.destinationZ = bestPoint.z;
            break;
        }
        case model::BotCombatAoEMode::Feet:
            resolved.destinationX = target->GetPositionX();
            resolved.destinationY = target->GetPositionY();
            resolved.destinationZ = target->GetPositionZ();
            break;
    }

    resolved.useDestination = SpellUsesDestinationTarget(spellInfo);
    if (resolved.useDestination && context.bot)
    {
        float const maxRange = context.bot->GetSpellMaxRangeForTarget(target, spellInfo);
        if (maxRange > 0.0f &&
            context.bot->GetDistance(
                resolved.destinationX,
                resolved.destinationY,
                resolved.destinationZ) > maxRange)
        {
            resolved.valid = false;
        }
    }

    return resolved;
}

std::uint32_t CountPartyMembersBelowHealthPctImpl(
    Unit* bot,
    Player* owner,
    float thresholdPct)
{
    std::uint32_t count = 0;

    auto consider = [&](Unit* candidate)
    {
        if (!candidate || !candidate->IsAlive())
            return;

        if (candidate->GetHealthPct() <= thresholdPct)
            ++count;
    };

    consider(owner);
    consider(bot);

    auto isAmbientWorldBot = [](Unit* unit) -> bool
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        return creature && creature->GetEntry() == WorldBotEntry;
    };

    auto collectNearbyAmbientAllies = [&](bool includeSelf) -> std::vector<Unit*>
    {
        std::vector<Unit*> allies;
        if (!bot)
            return allies;

        if (includeSelf)
            allies.push_back(bot);

        Acore::AnyFriendlyUnitInObjectRangeCheck check(bot, bot, AmbientSupportScanRadius);
        Acore::UnitListSearcher<Acore::AnyFriendlyUnitInObjectRangeCheck> searcher(bot, allies, check);
        Cell::VisitObjects(bot, searcher, AmbientSupportScanRadius);

        allies.erase(
            std::remove_if(
                allies.begin(),
                allies.end(),
                [&](Unit* candidate)
                {
                    if (!candidate || !candidate->IsAlive() || !candidate->IsInWorld())
                        return true;
                    if (candidate == bot)
                        return !includeSelf;
                    if (!isAmbientWorldBot(candidate))
                        return true;
                    return !bot->IsFriendlyTo(candidate);
                }),
            allies.end());
        std::sort(allies.begin(), allies.end());
        allies.erase(std::unique(allies.begin(), allies.end()), allies.end());
        return allies;
    };

    if (owner)
    {
        if (Group const* group = owner->GetGroup())
        {
            for (Group::MemberSlot const& slot : group->GetMemberSlots())
            {
                Player* member = ObjectAccessor::FindConnectedPlayer(slot.guid);
                if (!member || member == owner || member == bot)
                    continue;

                consider(member);
            }
        }
    }
    else if (bot && bot->ToCreature() && isAmbientWorldBot(bot))
    {
        for (Unit* ally : collectNearbyAmbientAllies(false))
            consider(ally);
    }

    return count;
}

std::vector<Unit*> CollectNearbyFriendlyAmbientWorldBots(Unit* bot, float radius, bool includeSelf)
{
    std::vector<Unit*> allies;
    if (!bot || radius <= 0.0f)
        return allies;

    if (includeSelf)
        allies.push_back(bot);

    Acore::AnyFriendlyUnitInObjectRangeCheck check(bot, bot, radius);
    Acore::UnitListSearcher<Acore::AnyFriendlyUnitInObjectRangeCheck> searcher(bot, allies, check);
    Cell::VisitObjects(bot, searcher, radius);

    allies.erase(
        std::remove_if(
            allies.begin(),
            allies.end(),
            [&](Unit* candidate)
            {
                if (!candidate || !candidate->IsAlive() || !candidate->IsInWorld())
                    return true;
                if (candidate == bot)
                    return !includeSelf;

                Creature* creature = candidate->ToCreature();
                if (!creature || creature->GetEntry() != WorldBotEntry)
                    return true;

                return !bot->IsFriendlyTo(candidate);
            }),
        allies.end());

    std::sort(allies.begin(), allies.end());
    allies.erase(std::unique(allies.begin(), allies.end()), allies.end());
    return allies;
}

std::uint64_t ComputeSupportTankAnchorScore(Unit* candidate)
{
    if (!candidate)
        return 0;

    return candidate->GetMaxHealth();
}

Unit* FindSupportTankAnchor(Unit* bot, Player* owner)
{
    Unit* best = nullptr;
    auto consider = [&](Unit* candidate)
    {
        if (!candidate || !candidate->IsAlive() || !candidate->IsInWorld())
            return;

        if (!best || ComputeSupportTankAnchorScore(candidate) > ComputeSupportTankAnchorScore(best))
            best = candidate;
    };

    consider(owner);

    if (owner)
    {
        if (Group const* group = owner->GetGroup())
        {
            for (Group::MemberSlot const& slot : group->GetMemberSlots())
            {
                consider(ObjectAccessor::FindConnectedPlayer(slot.guid));
            }
        }
    }
    else
    {
        for (Unit* ally : CollectNearbyFriendlyAmbientWorldBots(bot, AmbientSupportScanRadius, false))
            consider(ally);
    }

    return best;
}

Unit* FindLowestHealthPartyTarget(Unit* bot, Player* owner)
{
    Unit* lowest = nullptr;
    auto consider = [&](Unit* candidate)
    {
        if (!candidate || !candidate->IsAlive() || !candidate->IsInWorld())
            return;
        if (!lowest || candidate->GetHealthPct() < lowest->GetHealthPct())
            lowest = candidate;
    };

    consider(owner);
    consider(bot);

    if (owner)
    {
        if (Group const* group = owner->GetGroup())
        {
            for (Group::MemberSlot const& slot : group->GetMemberSlots())
                consider(ObjectAccessor::FindConnectedPlayer(slot.guid));
        }
    }
    else
    {
        for (Unit* ally : CollectNearbyFriendlyAmbientWorldBots(bot, AmbientSupportScanRadius, false))
            consider(ally);
    }

    return lowest;
}

Unit* FindTrashTarget(BotCombatRuntimeContext const& context)
{
    if (!context.owner)
        return nullptr;

    for (Unit* attacker : context.owner->getAttackers())
    {
        if (!attacker || !attacker->IsAlive())
            continue;
        if (attacker == context.primaryTarget)
            continue;
        if (context.owner->IsFriendlyTo(attacker))
            continue;
        return attacker;
    }

    return nullptr;
}

Spell* GetCurrentNonMeleeSpell(Unit* unit)
{
    if (!unit)
        return nullptr;

    if (Spell* spell = unit->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        return spell;

    if (Spell* spell = unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        return spell;

    return nullptr;
}

Item* ResolveUsableCombatItem(Player* player, std::uint32_t itemId)
{
    if (!player || itemId == 0)
        return nullptr;

    Item* item = player->GetItemByEntry(itemId);
    if (!item)
        return nullptr;

    if (player->CanUseItem(item) != EQUIP_ERR_OK)
        return nullptr;

    return item;
}
} // namespace

std::uint32_t BotCombatRuntimeEvaluator::CountPartyMembersBelowHealthPct(
    Unit* bot,
    Player* owner,
    float thresholdPct)
{
    return CountPartyMembersBelowHealthPctImpl(bot, owner, thresholdPct);
}

BotCombatEvaluationResult BotCombatRuntimeEvaluator::EvaluateInterrupts(
    BotCombatPreparedProfile const& preparedProfile,
    BotCombatRuntimeContext const& context) const
{
    return EvaluateEntries(preparedProfile.interruptEntries, context);
}

BotCombatEvaluationResult BotCombatRuntimeEvaluator::EvaluateRotation(
    BotCombatPreparedProfile const& preparedProfile,
    BotCombatRuntimeContext const& context) const
{
    return EvaluateEntries(preparedProfile.rotationEntries, context);
}

BotCombatEvaluationResult BotCombatRuntimeEvaluator::EvaluateEntries(
    std::vector<model::BotCombatEntryDefinition> const& sourceEntries,
    BotCombatRuntimeContext const& context)
{
    if (context.syntheticGlobalCooldownRemainingMs > 0)
    {
        BotCombatEvaluationResult result;
        result.disposition = BotCombatEvaluationDisposition::Wait;
        result.waitMs = std::min(
            context.syntheticGlobalCooldownRemainingMs,
            std::max<std::uint32_t>(context.rotationWaitMs, 1u));
        result.traceReason = "synthetic_gcd";
        return result;
    }

    std::vector<model::BotCombatEntryDefinition> entries = sourceEntries;
    std::stable_sort(
        entries.begin(),
        entries.end(),
        [](model::BotCombatEntryDefinition const& left,
           model::BotCombatEntryDefinition const& right)
        {
            return left.priority < right.priority;
        });

    for (model::BotCombatEntryDefinition const& entry : entries)
    {
        if (!EvaluateEntryConditions(entry, context))
            continue;

        if (auto evaluated = EvaluateAction(entry, entry.primaryAction, context))
        {
            BotCombatEvaluationResult result;
            result.disposition = BotCombatEvaluationDisposition::Cast;
            result.action = std::move(evaluated);
            return result;
        }

        std::uint32_t const currentCastPrimaryWaitMs =
            GetCurrentCastHoldWaitMs(entry, entry.primaryAction, context);
        if (currentCastPrimaryWaitMs > 0)
        {
            BotCombatEvaluationResult result;
            result.disposition = BotCombatEvaluationDisposition::Wait;
            result.waitMs = currentCastPrimaryWaitMs;
            result.traceEntryId = entry.entryId;
            result.traceActionId = entry.primaryAction.actionId;
            result.traceEntryLabel = entry.label;
            result.traceTargetKey = entry.primaryAction.targetKey;
            result.traceReason = "current_cast_hold";
            return result;
        }

        std::uint32_t const primaryWaitMs = entry.isInterrupt
            ? 0
            : GetActionWaitMs(entry.primaryAction, context);
        if (primaryWaitMs > 0)
        {
            BotCombatEvaluationResult result;
            result.disposition = BotCombatEvaluationDisposition::Wait;
            result.waitMs = primaryWaitMs;
            result.traceEntryId = entry.entryId;
            result.traceActionId = entry.primaryAction.actionId;
            result.traceEntryLabel = entry.label;
            result.traceTargetKey = entry.primaryAction.targetKey;
            result.traceReason = "cooldown_or_gcd";
            return result;
        }

        if (entry.secondaryAction)
        {
            if (auto evaluated = EvaluateAction(entry, *entry.secondaryAction, context))
            {
                BotCombatEvaluationResult result;
                result.disposition = BotCombatEvaluationDisposition::Cast;
                result.action = std::move(evaluated);
                return result;
            }

            std::uint32_t const currentCastSecondaryWaitMs =
                GetCurrentCastHoldWaitMs(entry, *entry.secondaryAction, context);
            if (currentCastSecondaryWaitMs > 0)
            {
                BotCombatEvaluationResult result;
                result.disposition = BotCombatEvaluationDisposition::Wait;
                result.waitMs = currentCastSecondaryWaitMs;
                result.traceEntryId = entry.entryId;
                result.traceActionId = entry.secondaryAction->actionId;
                result.traceEntryLabel = entry.label;
                result.traceTargetKey = entry.secondaryAction->targetKey;
                result.traceReason = "current_cast_hold";
                return result;
            }

            std::uint32_t const secondaryWaitMs = entry.isInterrupt
                ? 0
                : GetActionWaitMs(*entry.secondaryAction, context);
            if (secondaryWaitMs > 0)
            {
                BotCombatEvaluationResult result;
                result.disposition = BotCombatEvaluationDisposition::Wait;
                result.waitMs = secondaryWaitMs;
                result.traceEntryId = entry.entryId;
                result.traceActionId = entry.secondaryAction->actionId;
                result.traceEntryLabel = entry.label;
                result.traceTargetKey = entry.secondaryAction->targetKey;
                result.traceReason = "cooldown_or_gcd";
                return result;
            }
        }
    }

    return {};
}

bool BotCombatRuntimeEvaluator::EvaluateEntryConditions(
    model::BotCombatEntryDefinition const& entry,
    BotCombatRuntimeContext const& context)
{
    if (entry.conditions.empty())
        return true;

    if (entry.conditionLogic == model::BotCombatConditionLogic::All)
    {
        for (model::BotCombatConditionDefinition const& condition : entry.conditions)
        {
            if (!EvaluateCondition(condition, context))
                return false;
        }
        return true;
    }

    for (model::BotCombatConditionDefinition const& condition : entry.conditions)
    {
        if (EvaluateCondition(condition, context))
            return true;
    }

    return false;
}

bool BotCombatRuntimeEvaluator::EvaluateCondition(
    model::BotCombatConditionDefinition const& condition,
    BotCombatRuntimeContext const& context)
{
    if (condition.statKey == "exists")
    {
        bool const exists = ResolveConditionSubject(condition.subjectKey, context) != nullptr;
        if (condition.comparison == model::BotCombatConditionOperator::Exists)
            return exists;
        return CompareNumeric(
            condition.comparison,
            exists ? 1.0f : 0.0f,
            condition.numericValue);
    }

    Unit* subject = ResolveConditionSubject(condition.subjectKey, context);
    if (!subject)
        return false;

    if (condition.statKey == "hp_pct")
        return CompareNumeric(condition.comparison, subject->GetHealthPct(), condition.numericValue);

    if (condition.statKey == "is_moving" || condition.statKey == "moving")
        return CompareNumeric(
            condition.comparison,
            subject->isMoving() ? 1.0f : 0.0f,
            condition.numericValue);

    if (condition.statKey == "mana_pct")
        return CompareNumeric(condition.comparison, GetManaPct(subject), condition.numericValue);

    if (condition.statKey == "power_pct")
        return CompareNumeric(
            condition.comparison,
            GetPowerPct(subject, subject->getPowerType()),
            condition.numericValue);

    if (condition.statKey == "power")
        return CompareNumeric(
            condition.comparison,
            GetPower(subject, subject->getPowerType()),
            condition.numericValue);

    if (condition.statKey == "runic_power_pct")
        return CompareNumeric(
            condition.comparison,
            GetPowerPct(subject, POWER_RUNIC_POWER),
            condition.numericValue);

    if (condition.statKey == "runic_power")
        return CompareNumeric(
            condition.comparison,
            GetPower(subject, POWER_RUNIC_POWER),
            condition.numericValue);

    if (condition.statKey == "distance")
    {
        if (!context.bot)
            return false;
        return CompareNumeric(
            condition.comparison,
            context.bot->GetDistance(subject),
            condition.numericValue);
    }

    if (condition.statKey == "creature_type")
    {
        std::string const actualType = CreatureTypeToKey(subject);
        bool const matches = CreatureTypeMatches(actualType, condition.stringValue);
        switch (condition.comparison)
        {
            case model::BotCombatConditionOperator::Equal:
            case model::BotCombatConditionOperator::Has:
                return matches;
            case model::BotCombatConditionOperator::NotEqual:
            case model::BotCombatConditionOperator::NotHas:
                return !matches;
            case model::BotCombatConditionOperator::Exists:
                return !actualType.empty();
            case model::BotCombatConditionOperator::LessThan:
            case model::BotCombatConditionOperator::LessThanOrEqual:
            case model::BotCombatConditionOperator::GreaterThan:
            case model::BotCombatConditionOperator::GreaterThanOrEqual:
                return CompareNumeric(
                    condition.comparison,
                    matches ? 1.0f : 0.0f,
                    condition.numericValue);
        }
    }

    if (condition.statKey == "aura" || condition.statKey == "has_aura")
    {
        std::optional<std::uint32_t> spellId = ParseConditionSpellId(condition);
        if (!spellId)
            return false;

        bool const hasAura = subject->HasAura(*spellId);
        switch (condition.comparison)
        {
            case model::BotCombatConditionOperator::Has:
                return hasAura;
            case model::BotCombatConditionOperator::NotHas:
                return !hasAura;
            case model::BotCombatConditionOperator::Equal:
            case model::BotCombatConditionOperator::NotEqual:
            case model::BotCombatConditionOperator::LessThan:
            case model::BotCombatConditionOperator::LessThanOrEqual:
            case model::BotCombatConditionOperator::GreaterThan:
            case model::BotCombatConditionOperator::GreaterThanOrEqual:
            case model::BotCombatConditionOperator::Exists:
                return CompareNumeric(
                    condition.comparison,
                    hasAura ? 1.0f : 0.0f,
                    condition.numericValue);
        }
    }

    if (condition.statKey == "aura_remaining_secs")
    {
        std::optional<std::uint32_t> spellId = ParseConditionSpellId(condition);
        if (!spellId)
            return false;

        return CompareNumeric(
            condition.comparison,
            GetAuraRemainingSecs(subject, *spellId),
            condition.numericValue);
    }

    if (condition.statKey == "combo_points")
        return CompareNumeric(
            condition.comparison,
            static_cast<float>(subject->GetComboPoints()),
            condition.numericValue);

    if (condition.statKey == "threat_pct")
    {
        if (!context.primaryTarget)
            return false;
        ThreatManager& mgr = context.primaryTarget->GetThreatMgr();
        float const subjectThreat = mgr.GetThreat(subject);
        float topThreat = 0.0f;
        for (ThreatReference const* ref : mgr.GetSortedThreatList())
        {
            if (ref->IsOnline())
            {
                topThreat = ref->GetThreat();
                break;
            }
        }
        float const pct = (topThreat > 0.0f) ? (subjectThreat / topThreat * 100.0f) : 0.0f;
        return CompareNumeric(condition.comparison, pct, condition.numericValue);
    }

    if (condition.statKey == "is_aggro_holder")
    {
        if (!context.primaryTarget)
            return false;
        bool const holds = (context.primaryTarget->GetThreatMgr().GetCurrentVictim() == subject);
        return CompareNumeric(condition.comparison, holds ? 1.0f : 0.0f, condition.numericValue);
    }

    if (condition.statKey == "aura_stacks")
    {
        if (condition.stringValue.empty())
            return false;
        uint32_t spellId = 0;
        try { spellId = static_cast<uint32_t>(std::stoul(condition.stringValue)); }
        catch (...) { return false; }
        uint32_t stacks = 0;
        if (Aura const* aura = subject->GetAura(spellId))
            stacks = aura->GetStackAmount();
        return CompareNumeric(
            condition.comparison,
            static_cast<float>(stacks),
            condition.numericValue);
    }

    if (condition.statKey == "runes_ready" || condition.statKey == "runes_available")
    {
        Player* player = subject->ToPlayer();
        if (!player)
            return false;

        std::optional<RuneType> requestedRune;
        if (!TryParseRuneType(condition.stringValue, requestedRune))
            return false;

        return CompareNumeric(
            condition.comparison,
            static_cast<float>(CountReadyRunes(player, requestedRune)),
            condition.numericValue);
    }

    if (condition.statKey == "nearby_enemies")
    {
        float radius = 10.0f;
        if (!condition.stringValue.empty())
        {
            try
            {
                radius = std::stof(condition.stringValue);
            }
            catch (...)
            {
                return false;
            }
        }

        return CompareNumeric(
            condition.comparison,
            static_cast<float>(CountNearbyEnemies(context, subject, radius)),
            condition.numericValue);
    }

    if (condition.statKey == "party_members_below_hp_pct")
    {
        float thresholdPct = 0.0f;
        if (!condition.stringValue.empty())
        {
            try
            {
                thresholdPct = std::stof(condition.stringValue);
            }
            catch (...)
            {
                return false;
            }
        }
        else
        {
            thresholdPct = condition.numericValue;
        }

        std::uint32_t const memberCount =
            CountPartyMembersBelowHealthPctImpl(context.bot, context.owner, thresholdPct);

        return CompareNumeric(
            condition.comparison,
            static_cast<float>(memberCount),
            condition.numericValue);
    }

    return false;
}

std::optional<BotCombatEvaluatedAction> BotCombatRuntimeEvaluator::EvaluateAction(
    model::BotCombatEntryDefinition const& entry,
    model::BotCombatActionDefinition const& action,
    BotCombatRuntimeContext const& context)
{
    if (!context.bot)
        return std::nullopt;

    Unit* target = ResolveActionTarget(action.targetKey, context);
    if (!target)
        return std::nullopt;

    if (action.actionType == model::BotCombatActionType::Item)
    {
        if (action.itemId == 0)
            return std::nullopt;

        // First pass scope: self-use item actions only.
        if (target != context.bot)
            return std::nullopt;

        BotCombatEvaluatedAction evaluated;
        evaluated.entryId = entry.entryId;
        evaluated.actionId = action.actionId;
        evaluated.actionType = action.actionType;
        evaluated.itemId = action.itemId;
        evaluated.target = target;
        evaluated.targetKey = action.targetKey;
        evaluated.entryLabel = entry.label;
        evaluated.isInterrupt = entry.isInterrupt;
        evaluated.actionSlot = action.slot;
        evaluated.breaksCurrentCast = entry.breaksCurrentCast;

        if (Player* player = context.bot->ToPlayer())
        {
            if (!ResolveUsableCombatItem(player, action.itemId))
                return std::nullopt;
        }
        else
        {
            if (!CanUseSimulatedCombatItem(
                    context.bot,
                    target,
                    action.itemId,
                    context.usedSimulatedItemsThisCombat))
            {
                return std::nullopt;
            }
            evaluated.simulatedItemUse = true;
        }

        if (!CanBreakCurrentCast(context.bot, evaluated))
            return std::nullopt;

        return evaluated;
    }

    if (action.actionType != model::BotCombatActionType::Spell)
        return std::nullopt;

    std::uint32_t const spellId =
        BotCombatProfilePreparationService::ResolveKnownSpellForAction(
            context.availableSpells,
            action);
    if (spellId == 0)
        return std::nullopt;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return std::nullopt;

    ResolvedAoEActionTargeting const aoeTargeting =
        ResolveAoEActionTargeting(action, context, target, spellInfo);
    if (!aoeTargeting.valid)
        return std::nullopt;

    if (entry.isInterrupt && !HasInterruptibleEnemyCast(target))
        return std::nullopt;

    if (!CanExecuteSpell(
            context.bot,
            target,
            spellId,
            context.allowHardCasts,
            context.syntheticGlobalCooldownRemainingMs))
        return std::nullopt;

    BotCombatEvaluatedAction evaluated;
    evaluated.entryId = entry.entryId;
    evaluated.actionId = action.actionId;
    evaluated.actionType = action.actionType;
    evaluated.spellId = spellId;
    evaluated.target = target;
    evaluated.targetKey = action.targetKey;
    evaluated.entryLabel = entry.label;
    evaluated.isInterrupt = entry.isInterrupt;
    evaluated.actionSlot = action.slot;
    evaluated.aoeMode = aoeTargeting.aoeMode;
    evaluated.aoeMinTargets = aoeTargeting.aoeMinTargets;
    evaluated.aoeRadius = aoeTargeting.aoeRadius;
    evaluated.useDestination = aoeTargeting.useDestination;
    evaluated.destinationX = aoeTargeting.destinationX;
    evaluated.destinationY = aoeTargeting.destinationY;
    evaluated.destinationZ = aoeTargeting.destinationZ;
    evaluated.breaksCurrentCast = entry.breaksCurrentCast;

    if (!CanBreakCurrentCast(context.bot, evaluated))
        return std::nullopt;

    return evaluated;
}

std::uint32_t BotCombatRuntimeEvaluator::GetActionWaitMs(
    model::BotCombatActionDefinition const& action,
    BotCombatRuntimeContext const& context)
{
    if (!context.bot)
        return 0;

    if (action.actionType == model::BotCombatActionType::Item)
        return 0;

    if (action.actionType != model::BotCombatActionType::Spell)
        return 0;

    std::uint32_t const spellId =
        BotCombatProfilePreparationService::ResolveKnownSpellForAction(
            context.availableSpells,
            action);
    if (spellId == 0)
        return 0;

    Unit* target = ResolveActionTarget(action.targetKey, context);
    if (!target)
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    ResolvedAoEActionTargeting const aoeTargeting =
        ResolveAoEActionTargeting(action, context, target, spellInfo);
    if (!aoeTargeting.valid)
        return 0;

    std::uint32_t const waitMs = std::max(
        GetSpellWaitMs(context.bot, target, spellId),
        context.syntheticGlobalCooldownRemainingMs);
    if (waitMs == 0 || waitMs > context.rotationWaitMs)
        return 0;

    return waitMs;
}

std::uint32_t BotCombatRuntimeEvaluator::GetCurrentCastHoldWaitMs(
    model::BotCombatEntryDefinition const& entry,
    model::BotCombatActionDefinition const& action,
    BotCombatRuntimeContext const& context)
{
    if (!context.bot || !context.bot->IsNonMeleeSpellCast(false) || entry.breaksCurrentCast)
        return 0;

    if (action.actionType == model::BotCombatActionType::Item)
        return context.rotationWaitMs;

    if (action.actionType != model::BotCombatActionType::Spell)
        return 0;

    std::uint32_t const spellId =
        BotCombatProfilePreparationService::ResolveKnownSpellForAction(
            context.availableSpells,
            action);
    if (spellId == 0)
        return 0;

    Unit* target = ResolveActionTarget(action.targetKey, context);
    if (!target)
        return 0;

    if (entry.isInterrupt && !HasInterruptibleEnemyCast(target))
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    ResolvedAoEActionTargeting const aoeTargeting =
        ResolveAoEActionTargeting(action, context, target, spellInfo);
    if (!aoeTargeting.valid)
        return 0;

    if (context.bot->HasSpellCooldown(spellId))
        return 0;

    if (GetGlobalCooldownRemainingMs(context.bot, spellInfo) > 0
        || context.syntheticGlobalCooldownRemainingMs > 0)
        return 0;

    float const maxRange = context.bot->GetSpellMaxRangeForTarget(target, spellInfo);
    if (maxRange > 0.0f && !context.bot->IsWithinCombatRange(target, maxRange))
        return 0;

    Spell* currentSpell = GetCurrentNonMeleeSpell(context.bot);
    if (!currentSpell)
        return context.rotationWaitMs;

    std::uint32_t const remainingCastMs = std::max(currentSpell->GetCastTimeRemaining(), 0);
    return std::min(remainingCastMs, context.rotationWaitMs);
}

Unit* BotCombatRuntimeEvaluator::ResolveConditionSubject(
    std::string const& subjectKey,
    BotCombatRuntimeContext const& context)
{
    return ResolveActionTarget(subjectKey, context);
}

Unit* BotCombatRuntimeEvaluator::ResolveActionTarget(
    std::string const& targetKey,
    BotCombatRuntimeContext const& context)
{
    if (targetKey.empty() || targetKey == "enemy" || targetKey == "enemy_primary")
        return context.primaryTarget;

    if (targetKey == "self")
        return context.bot;

    if (targetKey == "owner")
        return context.owner;

    if (targetKey == "ally_tank")
        return FindSupportTankAnchor(context.bot, context.owner);

    if (targetKey == "lowest_hp_party")
        return FindLowestHealthPartyTarget(context.bot, context.owner);

    if (targetKey == "lowest_hp_ally")
        return FindLowestHealthPartyTarget(context.bot, context.owner);

    if (targetKey == "enemy_trash")
        return FindTrashTarget(context);

    if (targetKey == "enemy_primary_victim")
        return context.primaryTarget ? context.primaryTarget->GetVictim() : nullptr;

    return nullptr;
}

bool BotCombatRuntimeEvaluator::CanExecuteSpell(
    Unit* bot,
    Unit* target,
    std::uint32_t spellId,
    bool allowHardCasts,
    std::uint32_t syntheticGlobalCooldownRemainingMs)
{
    if (!bot || !target || spellId == 0)
        return false;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    if (!allowHardCasts && spellInfo->CalcCastTime(bot) > 0)
        return false;

    if (bot->isMoving() &&
        spellInfo->CalcCastTime(bot) > 0 &&
        (spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT))
    {
        return false;
    }

    if (Creature* creature = bot->ToCreature())
    {
        if (!creature->CanCastSpell(spellId))
            return false;
    }

    if (Player* player = bot->ToPlayer())
    {
        int32 const powerCost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
        if (powerCost > 0)
        {
            if (spellInfo->PowerType == POWER_RUNE)
            {
                if (!HasRequiredRunes(player, spellInfo->RuneCostID))
                    return false;
            }
            else if (spellInfo->PowerType >= POWER_MANA && spellInfo->PowerType <= POWER_RUNIC_POWER)
            {
                Powers const powerType = static_cast<Powers>(spellInfo->PowerType);
                if (player->GetPower(powerType) < powerCost)
                    return false;
            }
        }
    }

    if (bot->HasSpellCooldown(spellId))
        return false;

    if (GetGlobalCooldownRemainingMs(bot, spellInfo) > 0
        || syntheticGlobalCooldownRemainingMs > 0)
        return false;

    float const maxRange = bot->GetSpellMaxRangeForTarget(target, spellInfo);
    if (maxRange > 0.0f && !bot->IsWithinCombatRange(target, maxRange))
        return false;

    return true;
}

std::uint32_t BotCombatRuntimeEvaluator::GetSpellWaitMs(
    Unit* bot,
    Unit* target,
    std::uint32_t spellId)
{
    if (!bot || !target || spellId == 0)
        return 0;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 0;

    float const maxRange = bot->GetSpellMaxRangeForTarget(target, spellInfo);
    if (maxRange > 0.0f && !bot->IsWithinCombatRange(target, maxRange))
        return 0;

    std::uint32_t const spellCooldownMs = GetSpellCooldownRemainingMs(bot, spellId);
    std::uint32_t const globalCooldownMs = GetGlobalCooldownRemainingMs(bot, spellInfo);
    std::uint32_t const waitMs = std::max(spellCooldownMs, globalCooldownMs);
    if (waitMs == 0)
        return 0;

    return waitMs;
}

bool BotCombatRuntimeEvaluator::CanBreakCurrentCast(
    Unit* bot,
    BotCombatEvaluatedAction const& evaluatedAction)
{
    if (!bot || !bot->IsNonMeleeSpellCast(false))
        return true;

    if (!evaluatedAction.breaksCurrentCast)
        return false;

    return true;
}

bool BotCombatRuntimeEvaluator::HasInterruptibleEnemyCast(Unit* target)
{
    Spell* currentSpell = GetCurrentNonMeleeSpell(target);
    if (!currentSpell)
        return false;

    return currentSpell->IsInterruptable();
}
} // namespace service
} // namespace living_world
