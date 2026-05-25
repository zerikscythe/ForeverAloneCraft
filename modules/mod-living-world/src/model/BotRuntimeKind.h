#pragma once

#include <cstdint>

namespace living_world
{
namespace model
{

enum class BotRuntimeKind : std::uint8_t
{
    Companion = 0,
    Hostile = 1,
    Ambient = 2,
    LedgerShell = 3
};

} // namespace model
} // namespace living_world
