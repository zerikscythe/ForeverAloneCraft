#include "service/BotLedgerShellSanitizerService.h"

#include "Player.h"

namespace living_world
{
namespace service
{
namespace
{
void SetReason(std::string* reason, std::string value)
{
    if (reason)
        *reason = std::move(value);
}
} // namespace

bool BotLedgerShellSanitizerService::IsCompatibleShell(
    Player const* shell,
    integration::BotIdentityRecord const& identity,
    std::string* reason) const
{
    if (!shell)
    {
        SetReason(reason, "missing shell player");
        return false;
    }

    if (shell->getClass() != identity.classId)
    {
        SetReason(
            reason,
            "shell class mismatch: shell="
                + std::to_string(static_cast<std::uint32_t>(shell->getClass()))
                + " ledger="
                + std::to_string(static_cast<std::uint32_t>(identity.classId)));
        return false;
    }

    if (shell->getRace() != identity.raceId)
    {
        SetReason(
            reason,
            "shell race mismatch: shell="
                + std::to_string(static_cast<std::uint32_t>(shell->getRace()))
                + " ledger="
                + std::to_string(static_cast<std::uint32_t>(identity.raceId)));
        return false;
    }

    if (shell->getGender() != identity.gender)
    {
        SetReason(
            reason,
            "shell gender mismatch: shell="
                + std::to_string(static_cast<std::uint32_t>(shell->getGender()))
                + " ledger="
                + std::to_string(static_cast<std::uint32_t>(identity.gender)));
        return false;
    }

    return true;
}

bool BotLedgerShellSanitizerService::SanitizeForIdentity(
    Player* shell,
    integration::BotIdentityRecord const& identity,
    std::string* reason) const
{
    if (!IsCompatibleShell(shell, identity, reason))
        return false;

    shell->InterruptNonMeleeSpells(true);
    shell->RemoveAllAuras();
    shell->RemoveAllSpellCooldown();
    shell->resetSpells();
    shell->resetTalents(true);
    shell->InitTalentForLevel();

    for (std::uint8_t button = 0; button < MAX_ACTION_BUTTONS; ++button)
        shell->removeActionButton(button);

    shell->SendInitialActionButtons();
    return true;
}

} // namespace service
} // namespace living_world
