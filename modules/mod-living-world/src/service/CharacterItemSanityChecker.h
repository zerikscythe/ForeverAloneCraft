#pragma once

#include "model/AccountAltRuntime.h"
#include "model/CharacterItemSnapshot.h"

namespace living_world
{
namespace service
{
// Conservative item-domain sanity checker. For now it only approves the
// Equipment sync domain when both snapshots have structurally valid equipment
// state. Inventory and bank remain observed-but-blocked until they have their
// own dedicated duplicate/loss prevention rules.
class CharacterItemSanityChecker
{
public:
    model::AccountAltSanityCheckResult Check(
        model::CharacterItemSnapshot const& sourceSnapshot,
        model::CharacterItemSnapshot const& cloneSnapshot) const;
};
} // namespace service
} // namespace living_world
