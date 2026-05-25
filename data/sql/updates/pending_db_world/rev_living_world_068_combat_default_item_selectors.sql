ALTER TABLE `living_world_bot_combat_default_action`
    ADD COLUMN `item_selector` VARCHAR(32) NOT NULL DEFAULT '' AFTER `item_id`;
