#pragma once

// Persistent-roster read adapter. Services need a way to resolve "what
// roster entries can this player ask for?" without knowing where the data
// lives (config file, SQL row, account-derived alt list, in-memory seed).
// Keeping that lookup behind an interface lets us:
//   - wire the command/service flow end-to-end before SQL schema exists
//   - unit-test services against an in-memory fake
//   - swap the real backing implementation later without touching callers
//
// Rules for implementations:
//   - scope every lookup by requesting account; never leak another
//     player's entries across accounts
//   - return empty optionals / empty vectors for missing data; do not
//     throw on "not found"
//   - never mutate world state from this interface

#include "model/RosterEntry.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace living_world
{
namespace integration
{
class RosterRepository
{
public:
    virtual ~RosterRepository() = default;

    // List every roster entry the given account is allowed to summon.
    // Used by `.lwbot roster list` and by future planners that need to
    // see the full roster (e.g. role-balance planning across the party).
    virtual std::vector<model::RosterEntry> GetRosterEntriesForAccount(
        std::uint32_t accountId) const = 0;

    // Look up a single entry by ID, scoped to the requesting account for
    // safety. Returns std::nullopt when the entry does not exist or is
    // not visible to that account. The caller still needs to route the
    // result through the planner for ownership / enabled / summoned /
    // party-full validation - this is only a fetch, not an authorisation.
    virtual std::optional<model::RosterEntry> FindRosterEntryForAccount(
        std::uint32_t accountId,
        std::uint64_t rosterEntryId) const = 0;
};
} // namespace integration
} // namespace living_world
