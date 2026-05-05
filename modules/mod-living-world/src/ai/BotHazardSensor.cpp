#include "ai/BotHazardSensor.h"

#include "Group.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Unit.h"

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
// Tuning constants
// ---------------------------------------------------------------

// HP% drop per 500ms tick to count as "taking damage"
constexpr float HazardDamageThreshold       = 2.0f;
// Consecutive damage ticks needed before declaring danger (Layer 2)
constexpr int   HazardConsecutiveTicks       = 2;
// If bot moved more than this between ticks, ignore HP loss
// (prevents false positives from fall damage while moving)
constexpr float HazardMaxMovementYards       = 2.0f;
// How far to search for a clean party anchor
constexpr float HazardSafeAnchorSearchRadius = 40.0f;
// How far to step toward the anchor each tick
constexpr float HazardEscapeStepYards        = 5.0f;
// Once an anchor is chosen, keep it for this long to prevent jitter
constexpr auto  HazardCommitWindowMs         = std::chrono::milliseconds(2000);

// ---------------------------------------------------------------
// Layer 1 — known hazard aura IDs
// These are ground effect / persistent AoE debuffs. Expand as
// new raid content is encountered.
// ---------------------------------------------------------------
std::unordered_set<uint32_t> const& GetKnownHazardAuras()
{
    static std::unordered_set<uint32_t> const s_hazardAuras =
    {
        // --- Naxxramas ---
        28524,  // Slime Pool (Grobbulus)
        26575,  // Void Zone (Instructor Razuvious / generic)
        // --- Serpentshrine Cavern ---
        37591,  // Toxic Spores
        // --- Black Temple ---
        40923,  // Fel Eruption
        // --- Sunwell ---
        46228,  // Dark Decay (Eredar Twins)
        // --- Ulduar ---
        63018,  // Searing Flames (Ignis)
        64290,  // Saronite Vapors (General Vezax)
        // --- Trial of the Crusader ---
        67480,  // Firebomb (Jaraxxus)
        // --- Icecrown Citadel ---
        70952,  // Defile (Lich King)
        72754,  // Frozen Orb ground effect
        // --- Ruby Sanctum ---
        74527,  // Combustion ground fire
    };
    return s_hazardAuras;
}

// ---------------------------------------------------------------
// Per-bot tracking state
// ---------------------------------------------------------------

struct BotHazardTracking
{
    BotHazardMovementState moveState;
    float    lastHealthPct          = 100.0f;
    Position lastPosition;                  // zero-initialised: first-tick guard works
    int      consecutiveDamageTicks = 0;    // because initial distance to (0,0,0) >> 2y
    std::chrono::steady_clock::time_point lastDamageTick = {};
};

static std::mutex                                       s_hazardMutex;
static std::unordered_map<uint64_t, BotHazardTracking> s_tracking;

// ---------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------

// Warriors and Death Knights are tanking-capable; skip hazard escape
// for them. The BotCombatDoctrine role does not distinguish TANK from
// Melee DPS (both map to Melee), so we use the class as a proxy.
bool IsTankClass(Player const* bot)
{
    uint8_t const c = bot->getClass();
    return c == CLASS_WARRIOR || c == CLASS_DEATH_KNIGHT;
}

// Returns true if bot should participate in hazard escape.
// Hybrid healers suppressed when the owner is critically low (they
// need to stay in range to cast emergency heals).
bool ShouldEscapeHazard(Player const* bot, Player const* owner)
{
    if (IsTankClass(bot))
        return false;

    // Mirrors HealOwnerCritical = 50.0f from CompanionAI.cpp
    uint8_t const c = bot->getClass();
    bool const isHybridHealer = (c == CLASS_DRUID
                                || c == CLASS_PALADIN
                                || c == CLASS_SHAMAN);
    if (isHybridHealer && owner->GetHealthPct() < 50.0f)
        return false;

    return true;
}

// Check Layer 1: bot has at least one known hazard aura.
bool HasKnownHazardAura(Player const* bot, uint32_t& outSpellId)
{
    for (uint32_t id : GetKnownHazardAuras())
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
    // Helper lambda — checks a candidate and returns distance if valid.
    auto IsCleanCandidate = [&](Player* candidate) -> bool
    {
        if (!candidate || candidate == bot)
            return false;
        if (!candidate->IsAlive() || !candidate->IsInWorld())
            return false;
        if (bot->GetDistance(candidate) > HazardSafeAnchorSearchRadius)
            return false;
        uint32_t unused = 0;
        return !HasKnownHazardAura(candidate, unused);
    };

    Group const* group = owner->GetGroup();
    if (!group)
        return IsCleanCandidate(owner) ? owner : nullptr;

    Player* best     = nullptr;
    float   bestDist = HazardSafeAnchorSearchRadius + 1.0f;

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
    float const angle = bot->GetAngle(anchor);
    Position dest = bot->GetPosition();
    bot->MovePositionToFirstCollision(dest, HazardEscapeStepYards, angle);
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
    bool layer2Triggered = false;
    float const hpDrop = tracking.lastHealthPct - currentHpPct;
    float const moved  = tracking.lastPosition.GetExactDist2d(currentPos);

    if (hpDrop > HazardDamageThreshold && moved < HazardMaxMovementYards)
    {
        // Took significant damage while barely moving.
        // Only count as a consecutive tick if it arrived within ~750ms
        // of the previous one (1.5× the 500ms tick period).
        auto const msSinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - tracking.lastDamageTick).count();

        if (msSinceLast < 750 && tracking.consecutiveDamageTicks > 0)
            ++tracking.consecutiveDamageTicks;
        else
            tracking.consecutiveDamageTicks = 1;

        tracking.lastDamageTick = now;

        if (tracking.consecutiveDamageTicks >= HazardConsecutiveTicks)
            layer2Triggered = true;
    }
    else if (hpDrop <= 0.0f)
    {
        // HP not dropping: reset consecutive counter.
        tracking.consecutiveDamageTicks = 0;
    }

    // Update baseline for next tick.
    tracking.lastHealthPct = currentHpPct;
    tracking.lastPosition  = currentPos;

    bool const inDanger = hasKnownAura || layer2Triggered;

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
        tracking.consecutiveDamageTicks);

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
        tracking.moveState.NextHazardDecisionTime = now + HazardCommitWindowMs;
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
