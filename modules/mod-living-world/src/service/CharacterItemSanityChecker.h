#pragma once

#include "model/AccountAltRuntime.h"
#include "model/CharacterItemSnapshot.h"

namespace living_world
{
namespace service
{
// Conservative item-domain sanity checker. It validates equipment plus
// inventory/bank container shape, duplicate ownership, and storage-domain
// consistency. Only equipment is currently executable; inventory/bank can be
// observed and planned but remain blocked until dedicated write paths exist.
class CharacterItemSanityChecker
{
public:
    model::AccountAltSanityCheckResult Check(
        model::CharacterItemSnapshot const& sourceSnapshot,
        model::CharacterItemSnapshot const& cloneSnapshot) const;
};
} // namespace service
} // namespace living_world
