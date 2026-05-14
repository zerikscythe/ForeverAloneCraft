-- rev_living_world_014_world_bot_personality (characters DB)
--
-- Add a DB-backed personality value for world-bot ledger identities so future
-- faction-contact behavior and editor/runtime tooling can share one canonical
-- vocabulary.

ALTER TABLE living_world_bot_identity
    ADD COLUMN personality_key VARCHAR(32) NOT NULL DEFAULT 'uninterested' AFTER gear_tier;

UPDATE living_world_bot_identity
SET personality_key = 'uninterested'
WHERE personality_key IS NULL OR personality_key = '';