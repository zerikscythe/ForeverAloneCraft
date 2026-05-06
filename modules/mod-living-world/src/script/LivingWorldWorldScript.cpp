#include "Config.h"
#include "IWorld.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "WorldConfig.h"
#include "service/BotQuestRewardService.h"

namespace
{
// Quality-ordered list matching qualityToBuyValueConfig in ObjectMgr.
constexpr ServerConfigs kBuyValueRates[] =
{
    RATE_BUYVALUE_ITEM_POOR,
    RATE_BUYVALUE_ITEM_NORMAL,
    RATE_BUYVALUE_ITEM_UNCOMMON,
    RATE_BUYVALUE_ITEM_RARE,
    RATE_BUYVALUE_ITEM_EPIC,
    RATE_BUYVALUE_ITEM_LEGENDARY,
    RATE_BUYVALUE_ITEM_ARTIFACT,
    RATE_BUYVALUE_ITEM_HEIRLOOM,
};
} // namespace

class LivingWorldWorldScript final : public WorldScript
{
public:
    LivingWorldWorldScript() : WorldScript("LivingWorldWorldScript") { }

    // Fires after worldserver.conf is loaded, before ObjectMgr loads item
    // templates. Setting rates here means itemTemplate.BuyPrice will already
    // carry the scaled value when the world finishes initialising.
    void OnAfterConfigLoad(bool reload) override
    {
        float const scale = sConfigMgr->GetOption<float>("LivingWorld.EconomyScale", 1.0f);
        if (scale <= 0.0f)
        {
            LOG_ERROR(
                "server.worldserver",
                "[LivingWorld] EconomyScale must be > 0 (got {}). Using 1.0.",
                scale);
            return;
        }

        if (scale == 1.0f)
            return;

        // Vendor item buy prices (all eight quality tiers) and repair costs.
        // These are in-memory rates — the item_template DB table is never touched.
        for (ServerConfigs rate : kBuyValueRates)
            sWorld->setRate(rate, scale);

        sWorld->setRate(RATE_REPAIRCOST, scale);

        LOG_INFO(
            "server.worldserver",
            "[LivingWorld] EconomyScale={}{} — vendor prices and repair costs scaled.",
            scale,
            reload ? " (reload: repair cost updated; vendor prices require restart)" : "");
    }

    void OnStartup() override
    {
        living_world::service::BotQuestRewardService().EnsureSchema();
    }
};

void AddSC_LivingWorldWorldScript()
{
    new LivingWorldWorldScript();
}

namespace
{
// Quality-ordered list matching qualityToBuyValueConfig in ObjectMgr.
constexpr ServerConfigs kBuyValueRates[] =
{
    RATE_BUYVALUE_ITEM_POOR,
    RATE_BUYVALUE_ITEM_NORMAL,
    RATE_BUYVALUE_ITEM_UNCOMMON,
    RATE_BUYVALUE_ITEM_RARE,
    RATE_BUYVALUE_ITEM_EPIC,
    RATE_BUYVALUE_ITEM_LEGENDARY,
    RATE_BUYVALUE_ITEM_ARTIFACT,
    RATE_BUYVALUE_ITEM_HEIRLOOM,
};
} // namespace

class LivingWorldWorldScript final : public WorldScript
{
public:
    LivingWorldWorldScript() : WorldScript("LivingWorldWorldScript") { }

    // Fires after worldserver.conf is loaded, before ObjectMgr loads item
    // templates. Setting rates here means itemTemplate.BuyPrice will already
    // carry the scaled value when the world finishes initialising.
    void OnAfterConfigLoad(bool reload) override
    {
        float const scale = sConfigMgr->GetOption<float>("LivingWorld.EconomyScale", 1.0f);
        if (scale <= 0.0f)
        {
            LOG_ERROR(
                "server.worldserver",
                "[LivingWorld] EconomyScale must be > 0 (got {}). Using 1.0.",
                scale);
            return;
        }

        if (scale == 1.0f)
            return;

        // Vendor item buy prices (all eight quality tiers).
        for (ServerConfigs rate : kBuyValueRates)
            sWorld->setRate(rate, scale);

        // Repair costs are read dynamically so this takes effect immediately
        // and also on config reload.
        sWorld->setRate(RATE_REPAIRCOST, scale);

        LOG_INFO(
            "server.worldserver",
            "[LivingWorld] EconomyScale={}{} — vendor prices and repair costs scaled.",
            scale,
            reload ? " (reload: repair cost updated; vendor prices require restart)" : "");
    }

    void OnStartup() override
    {
        living_world::service::BotQuestRewardService().EnsureSchema();
        ScaleTrainerCosts();
    }

private:
    // Trainer spell costs live in the trainer_spell DB table and have no
    // WorldConfig rate. Scale them once at startup by updating the in-DB
    // rows. Original values are preserved in a backup column so the scale
    // can be re-applied correctly on restart without compounding.
    static void ScaleTrainerCosts()
    {
        float const scale = sConfigMgr->GetOption<float>("LivingWorld.EconomyScale", 1.0f);
        if (scale <= 0.0f || scale == 1.0f)
            return;

        // Ensure the backup column exists (idempotent).
        WorldDatabase.Execute(
            "ALTER TABLE trainer_spell "
            "ADD COLUMN IF NOT EXISTS MoneyCostBase INT UNSIGNED NOT NULL DEFAULT 0");

        // On first run, copy original costs into the backup column.
        WorldDatabase.Execute(
            "UPDATE trainer_spell "
            "SET MoneyCostBase = MoneyCost "
            "WHERE MoneyCostBase = 0 AND MoneyCost > 0");

        // Always recompute from the backup so repeated restarts
        // with different scales don't compound the multiplier.
        WorldDatabase.DirectExecute(
            "UPDATE trainer_spell "
            "SET MoneyCost = GREATEST(1, ROUND(MoneyCostBase * {}))",
            scale);

        LOG_INFO(
            "server.worldserver",
            "[LivingWorld] EconomyScale={} — trainer spell costs scaled.",
            scale);
    }
};

void AddSC_LivingWorldWorldScript()
{
    new LivingWorldWorldScript();
}
