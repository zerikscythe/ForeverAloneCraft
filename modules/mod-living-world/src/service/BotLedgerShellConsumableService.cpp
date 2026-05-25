#include "service/BotLedgerShellConsumableService.h"

#include "Player.h"
#include "Log.h"
#include "integration/SqlBotIdentityRepository.h"
#include "integration/SqlBotShellRuntimeRepository.h"
#include "model/BotRuntimeKind.h"
#include "service/BotCombatSimulatedItemUse.h"
#include "service/BotPlayerRegistry.h"

#include <algorithm>
#include <array>

namespace
{
bool EnsurePlayerItemCount(Player* player, std::uint32_t itemId, std::uint32_t desiredCount)
{
    if (!player || itemId == 0 || desiredCount == 0)
        return false;

    std::uint32_t const currentCount = player->GetItemCount(itemId, false);
    if (currentCount >= desiredCount)
        return false;

    std::uint32_t const missingCount = desiredCount - currentCount;
    if (!player->AddItem(itemId, missingCount))
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] LedgerShellConsumables failed_add bot='{}' guid={} item={} desired={} current={}",
            player->GetName(),
            player->GetGUID().GetCounter(),
            itemId,
            desiredCount,
            currentCount);
        return false;
    }

    return true;
}
bool IsManaUsingClass(Player const* player)
{
    return player && player->GetMaxPower(POWER_MANA) > 0;
}
}

namespace living_world
{
namespace service
{

bool BotLedgerShellConsumableService::IsLedgerShellBot(Player const* player) const
{
    if (!player)
        return false;

    return BotPlayerRegistry::Instance().GetBotRuntimeKind(player->GetGUID())
        == model::BotRuntimeKind::LedgerShell;
}

bool BotLedgerShellConsumableService::PrimeLedgerShellConsumables(Player* player) const
{
    if (!player || !player->GetSession() || !IsLedgerShellBot(player))
        return false;

    integration::SqlBotShellRuntimeRepository shellRuntimeRepo;
    std::optional<model::BotShellRuntimeRecord> shellRuntime =
        shellRuntimeRepo.FindByShell(
            player->GetSession()->GetAccountId(),
            player->GetGUID().GetCounter());
    if (!shellRuntime)
        return false;

    integration::SqlBotIdentityRepository identityRepo;
    std::optional<integration::BotIdentityRecord> identity =
        identityRepo.FindById(shellRuntime->identityId);
    if (!identity)
        return false;

    bool changed = false;
    std::uint32_t const potionBudget =
        std::max<std::uint32_t>(1u, identity->genericPotionCharges);
    std::uint32_t const healingPotionId =
        ResolveGenericHealingPotionItemIdForLevel(identity->level);
    std::uint32_t const manaPotionId =
        ResolveGenericManaPotionItemIdForLevel(identity->level);

    if (healingPotionId != 0)
    {
        std::uint32_t const desiredHealingCount = IsManaUsingClass(player)
            ? std::max<std::uint32_t>(1u, (potionBudget + 1u) / 2u)
            : potionBudget;
        changed |= EnsurePlayerItemCount(player, healingPotionId, desiredHealingCount);
    }

    if (manaPotionId != 0 && IsManaUsingClass(player))
    {
        std::uint32_t const desiredManaCount = potionBudget / 2u;
        if (desiredManaCount > 0)
            changed |= EnsurePlayerItemCount(player, manaPotionId, desiredManaCount);
    }

    if (changed)
    {
        LOG_INFO(
            "server.worldserver",
            "[LivingWorldDebug] LedgerShellConsumables primed bot='{}' guid={} identityId={} class={} level={} potionBudget={}",
            player->GetName(),
            player->GetGUID().GetCounter(),
            identity->id,
            static_cast<std::uint32_t>(identity->classId),
            static_cast<std::uint32_t>(identity->level),
            potionBudget);
    }

    return changed;
}

} // namespace service
} // namespace living_world
