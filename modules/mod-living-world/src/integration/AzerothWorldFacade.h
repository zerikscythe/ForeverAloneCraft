#pragma once

// Read-only adapter into AzerothCore world state. This is the single choke
// point services use to gather planner inputs. Keeping reads behind a
// virtual interface lets us:
//   - run planners and services in unit tests against a fake facade
//   - swap the real implementation between module-only and future
//     integration-module variants without touching planners
//   - add progression / event / zone caches transparently later
//
// Rules for implementations:
//   - return value types only; never hand out pointers to live server objects
//   - never mutate world state from this interface
//   - treat missing data as empty optionals or empty vectors, not exceptions

#include "integration/WorldReadContext.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace living_world
{
namespace integration
{
class AzerothWorldFacade
{
public:
    virtual ~AzerothWorldFacade() = default;

    // Resolve the player context for a character GUID. Returns std::nullopt
    // when the player is offline or the GUID is unknown; callers should
    // treat either case as "request cannot proceed right now" rather than
    // an error.
    virtual std::optional<PlayerWorldContext> GetPlayerContext(
        std::uint64_t characterGuid) const = 0;

    // List spawn anchors visible to the planner in the given zone. May
    // return an empty vector when the zone has no registered anchors.
    virtual std::vector<SpawnAnchor> GetSpawnAnchorsInZone(
        std::uint32_t zoneId) const = 0;

    // True when a live character with the given GUID is currently online.
    // Used by the service layer to enforce the "alt already online" rule
    // before approving account-alt roster requests.
    virtual bool IsCharacterOnline(std::uint64_t characterGuid) const = 0;
};
} // namespace integration
} // namespace living_world
