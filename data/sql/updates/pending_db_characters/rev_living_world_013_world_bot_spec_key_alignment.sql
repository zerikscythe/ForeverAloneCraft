-- rev_living_world_013_world_bot_spec_key_alignment (characters DB)
--
-- Align legacy world-bot identity spec keys with the canonical spec names used
-- by the default combat-profile and talent-template systems.

UPDATE living_world_bot_identity
SET spec_key = CASE spec_key
    WHEN 'warrior_arms'   THEN 'Arms'
    WHEN 'warrior_fury'   THEN 'Fury'
    WHEN 'warrior_prot'   THEN 'Protection'
    WHEN 'paladin_holy'   THEN 'Holy'
    WHEN 'paladin_prot'   THEN 'Protection'
    WHEN 'paladin_ret'    THEN 'Retribution'
    WHEN 'hunter_bm'      THEN 'BeastMastery'
    WHEN 'hunter_mm'      THEN 'Marksmanship'
    WHEN 'hunter_sv'      THEN 'Survival'
    WHEN 'rogue_assa'     THEN 'Assassination'
    WHEN 'rogue_combat'   THEN 'Combat'
    WHEN 'rogue_sub'      THEN 'Subtlety'
    WHEN 'priest_disc'    THEN 'Discipline'
    WHEN 'priest_holy'    THEN 'Holy'
    WHEN 'priest_shadow'  THEN 'Shadow'
    WHEN 'dk_blood'       THEN 'Blood'
    WHEN 'dk_frost'       THEN 'Frost'
    WHEN 'dk_unholy'      THEN 'Unholy'
    WHEN 'shaman_ele'     THEN 'Elemental'
    WHEN 'shaman_enh'     THEN 'Enhancement'
    WHEN 'shaman_resto'   THEN 'Restoration'
    WHEN 'mage_arcane'    THEN 'Arcane'
    WHEN 'mage_fire'      THEN 'Fire'
    WHEN 'mage_frost'     THEN 'Frost'
    WHEN 'warlock_afflic' THEN 'Affliction'
    WHEN 'warlock_demo'   THEN 'Demonology'
    WHEN 'warlock_destro' THEN 'Destruction'
    WHEN 'druid_balance'  THEN 'Balance'
    WHEN 'druid_feral'    THEN 'Feral'
    WHEN 'druid_resto'    THEN 'Restoration'
    ELSE spec_key
END
WHERE spec_key IN (
    'warrior_arms', 'warrior_fury', 'warrior_prot',
    'paladin_holy', 'paladin_prot', 'paladin_ret',
    'hunter_bm', 'hunter_mm', 'hunter_sv',
    'rogue_assa', 'rogue_combat', 'rogue_sub',
    'priest_disc', 'priest_holy', 'priest_shadow',
    'dk_blood', 'dk_frost', 'dk_unholy',
    'shaman_ele', 'shaman_enh', 'shaman_resto',
    'mage_arcane', 'mage_fire', 'mage_frost',
    'warlock_afflic', 'warlock_demo', 'warlock_destro',
    'druid_balance', 'druid_feral', 'druid_resto'
);