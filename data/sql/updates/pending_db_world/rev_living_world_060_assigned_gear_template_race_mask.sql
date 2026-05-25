-- rev_living_world_060_assigned_gear_template_race_mask (world DB)
--
-- Extends curated level-80 assigned gear templates with a race-aware selector.
-- This allows stage templates to carry mirrored faction/race-locked catch-up
-- pieces while preserving shared fallback rows for neutral items.

ALTER TABLE `living_world_bot_assigned_gear_template`
    ADD COLUMN `race_mask` int unsigned NOT NULL DEFAULT 0 AFTER `loadout_key`;

ALTER TABLE `living_world_bot_assigned_gear_template`
    DROP INDEX `uk_lw_bot_assigned_gear_template`,
    ADD UNIQUE KEY `uk_lw_bot_assigned_gear_template`
        (`class_id`, `spec_key`, `loadout_key`, `race_mask`, `endgame_stage`, `slot_id`);

ALTER TABLE `living_world_bot_assigned_gear_template`
    DROP INDEX `idx_lw_bot_assigned_gear_template_lookup`,
    ADD KEY `idx_lw_bot_assigned_gear_template_lookup`
        (`class_id`, `endgame_stage`, `spec_key`, `loadout_key`, `race_mask`);
