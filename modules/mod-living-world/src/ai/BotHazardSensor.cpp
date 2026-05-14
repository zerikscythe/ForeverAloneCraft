#include "ai/BotHazardSensor.h"

#include "Group.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Unit.h"
#include "integration/SqlBotHazardConfigRepository.h"
#include "service/BotHazardConfigService.h"
#include "service/SharedHazardEvaluation.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace living_world
{
namespace ai
{
namespace BotHazardSensor
{
namespace
{

// ---------------------------------------------------------------
// Singleton service — loads config from DB with TTL caching.
// ---------------------------------------------------------------
service::BotHazardConfigService& GetHazardConfigService()
{
    static integration::SqlBotHazardConfigRepository repo;
    static service::BotHazardConfigService            service(repo);
    return service;
}

// ---------------------------------------------------------------
// Per-bot tracking state
// ---------------------------------------------------------------

struct BotHazardTracking
{
    BotHazardMovementState moveState;
    service::SharedHazardEvaluationState hazardState;
};

static std::mutex                                       s_hazardMutex;
static std::unordered_map<uint64_t, BotHazardTracking> s_tracking;

// ---------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------

// Maps a bot's class to its hazard role key.
// Warriors and DKs → TANK (aggro check applied separately).
// Pure Priest    → HEALER.
// Paladin/Shaman/Druid → HYBRID_HEALER (may be healing or DPS spec;
//   the owner-HP gate suppresses escape only when they are in a healing role).
// Ranged casters/hunters → RANGED_DPS.
// Everything else → MELEE_DPS.
std::string GetHazardRoleKey(Player const* bot)
{
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:      return "TANK";
        case CLASS_DEATH_KNIGHT: return "TANK";
        case CLASS_PRIEST:       return "HEALER";
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
        case CLASS_DRUID:        return "HYBRID_HEALER";
        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:      return "RANGED_DPS";
        default:                 return "MELEE_DPS";
    }
}

// Returns true if bot should participate in hazard escape based on
// role rules loaded from the DB.
bool ShouldEscapeHazard(Player const* bot, Player const* owner)
{
    std::string const roleKey = GetHazardRoleKey(bot);
    model::HazardRoleRule const* rule =
        GetHazardConfigService().GetRoleRule(roleKey);

    if (rule && rule->skipEscape)
    {
        if (rule->requiresAggroToSkip)
        {
            Unit* target = bot->GetVictim();
            bool const holdsAggro = target &&
                target->GetThreatMgr().GetCurrentVictim() == bot;
            if (holdsAggro)
                return false;
        }
        else
        {
            return false;
        }
    }

    if (rule && rule->ownerHpGatePct > 0.0f)
    {
        if (owner->GetHealthPct() < rule->ownerHpGatePct)
            return false;
    }

    return true;
}

// Check Layer 1: bot has at least one known hazard aura.
bool HasKnownHazardAura(Player const* bot, uint32_t& outSpellId)
{
    std::unordered_set<uint32_t> const auraIds =
        GetHazardConfigService().GetHazardAuraIds();
    for (uint32_t id : auraIds)
    {
        if (bot->HasAura(id))
        {
            outSpellId = id;
            return true;
        }
    }
    outSpellId = 0;
    return false;
}

// Find the nearest alive, hazard-free party member to use as a
// movement anchor. Falls back to the owner when the bot is ungrouped.
Player* FindNearestCleanPartyMember(Player* bot, Player* owner)
{
    float const searchRadius =
        GetHazardConfigService().GetTuning().anchorSearchRadius;

    auto IsCleanCandidate = [&](Player* candidate) -> bool
    {
        if (!candidate || candidate == bot)
            return false;
        if (!candidate->IsAlive() || !candidate->IsInWorld())
            return false;
        if (bot->GetDistance(candidate) > searchRadius)
            return false;
        uint32_t unused = 0;
        return !HasKnownHazardAura(candidate, unused);
    };

    Group const* group = owner->GetGroup();
    if (!group)
        return IsCleanCandidate(owner) ? owner : nullptr;

    Player* best     = nullptr;
    float   bestDist = searchRadius + 1.0f;

    for (Group::MemberSlot const& slot : group->GetMemberSlots())
    {
        Player* member = ObjectAccessor::FindConnectedPlayer(slot.guid);
        if (!IsCleanCandidate(member))
            continue;

        float const dist = bot->GetDistance(member);
        if (dist < bestDist)
        {
            best     = member;
            bestDist = dist;
        }
    }

    return best;
}

// Project a HazardEscapeStepYards step from the bot toward `anchor`,
// stopping at the first vmap collision so the destination is never inside
// a wall or over a ledge edge. Correct ground height is also resolved.
void IssueEscapeStep(Player* bot, Player* anchor)
{
    float const stepYards =
        GetHazardConfigService().GetTuning().escapeStepYards;
    float const angle = bot->GetAngle(anchor);
    Position dest = bot->GetPosition();
    bot->MovePositionToFirstCollision(dest, stepYards, angle);
    bot->GetMotionMaster()->MovePoint(0, dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ());
}

// Move away from the hazard center (used when no clean anchor exists).
void IssueEscapeFromHazardCenter(Player* bot, Player* owner)
{
    // Best fallback: follow the owner, who is likely not in the
    // same fire patch.
    constexpr float FallbackFollowDist  = 2.0f;
    constexpr float FallbackFollowAngle = 3.14159265358979323846f;
    bot->GetMotionMaster()->MoveFollow(owner, FallbackFollowDist, FallbackFollowAngle);
}

} // anonymous namespace

// ---------------------------------------------------------------
// Public API
// ---------------------------------------------------------------

bool ProcessHazardTick(Player* bot, Player* owner)
{
    if (!bot || !owner || !bot->IsAlive())
        return false;

    if (!ShouldEscapeHazard(bot, owner))
        return false;

    auto const now = std::chrono::steady_clock::now();
    uint64_t const key = bot->GetGUID().GetCounter();
    model::HazardTuning const tuning = GetHazardConfigService().GetTuning();

    // Copy tracking state out under mutex so mutations are safe
    // even if ClearHazardState races from a dismissal path.
    BotHazardTracking tracking;
    {
        std::lock_guard<std::mutex> lk(s_hazardMutex);
        auto it = s_tracking.find(key);
        if (it != s_tracking.end())
            tracking = it->second;
        // If not found: tracking is default-constructed (zeroes). Correct.
    }

    float const    currentHpPct = bot->GetHealthPct();
    Position const currentPos   = bot->GetPosition();

    // -----------------------------------------------------------
    // Layer 1: explicit hazard aura
    // -----------------------------------------------------------
    uint32_t hazardSpellId = 0;
    bool const hasKnownAura = HasKnownHazardAura(bot, hazardSpellId);

    // -----------------------------------------------------------
    // Layer 2: repeated damage at the same position
    // -----------------------------------------------------------
    service::SharedHazardEvaluationResult const hazardResult =
        service::EvaluateSharedHazard(
            tracking.hazardState,
            now,
            currentHpPct,
            currentPos,
            hasKnownAura,
            hazardSpellId,
            hasKnownAura ? 1.0f : 0.0f,
            tuning);

    bool const layer2Triggered = hazardResult.repeatedDamageTriggered;
    bool const inDanger = hazardResult.dangerDetectedNow;

    // -----------------------------------------------------------
    // Not in danger: stop escaping if we were
    // -----------------------------------------------------------
    if (!inDanger)
    {
        if (tracking.moveState.IsEscapingHazard)
        {
            LOG_INFO(
                "server.worldserver",
                "[LivingWorldHazard] EscapeCleared bot='{}' guid={}",
                bot->GetName(),
                key);
            tracking.moveState.IsEscapingHazard = false;
            tracking.moveState.SafeAnchorGuid   = ObjectGuid::Empty;
        }

        std::lock_guard<std::mutex> lk(s_hazardMutex);
        s_tracking[key] = tracking;
        return false;
    }

    // -----------------------------------------------------------
    // In danger: decide escape behaviour
    // -----------------------------------------------------------
    LOG_INFO(
        "server.worldserver",
        "[LivingWorldHazard] DangerDetected bot='{}' guid={} layer1={} spellId={} layer2={} consec={}",
        bot->GetName(),
        key,
        hasKnownAura,
        hazardSpellId,
        layer2Triggered,
        hazardResult.consecutiveDamageTicks);

    // Check whether we are still within the commitment window for
    // an existing anchor — avoids changing direction every tick.
    bool const inCommitWindow = tracking.moveState.IsEscapingHazard
        && (now < tracking.moveState.NextHazardDecisionTime);

    Player* anchor = nullptr;

    if (inCommitWindow && tracking.moveState.SafeAnchorGuid)
    {
        // Re-validate the stored anchor (it may have died or caught the aura).
        anchor = ObjectAccessor::FindConnectedPlayer(tracking.moveState.SafeAnchorGuid);
        if (anchor)
        {
            uint32_t unused = 0;
            if (!anchor->IsAlive() || !anchor->IsInWorld() || HasKnownHazardAura(anchor, unused))
                anchor = nullptr;
        }
    }

    if (!anchor)
    {
        // Pick a new anchor and reset the commitment window.
        anchor = FindNearestCleanPartyMember(bot, owner);
        tracking.moveState.IsEscapingHazard      = true;
        tracking.moveState.NextHazardDecisionTime =
            now + std::chrono::milliseconds(tuning.commitWindowMs);
        tracking.moveState.EscapeStartedTime      = now;
        tracking.moveState.SafeAnchorGuid =
            anchor ? anchor->GetGUID() : ObjectGuid::Empty;

        LOG_INFO(
            "server.worldserver",
            "[LivingWorldHazard] NewAnchor bot='{}' guid={} anchor='{}'",
            bot->GetName(),
            key,
            anchor ? anchor->GetName() : "(none — fallback to owner follow)");
    }

    {
        std::lock_guard<std::mutex> lk(s_hazardMutex);
        s_tracking[key] = tracking;
    }

    if (anchor)
        IssueEscapeStep(bot, anchor);
    else
        IssueEscapeFromHazardCenter(bot, owner);

    return true;
}

void ClearHazardState(ObjectGuid botGuid)
{
    if (!botGuid)
        return;

    std::lock_guard<std::mutex> lk(s_hazardMutex);
    s_tracking.erase(botGuid.GetCounter());
}

} // namespace BotHazardSensor
} // namespace ai
} // namespace living_world
