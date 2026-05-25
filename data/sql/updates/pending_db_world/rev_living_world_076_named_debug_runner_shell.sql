DELETE FROM creature_template_model WHERE CreatureID = 9900002;
DELETE FROM creature_template WHERE entry = 9900002;

INSERT INTO creature_template (
    entry, difficulty_entry_1, difficulty_entry_2, difficulty_entry_3,
    KillCredit1, KillCredit2, `name`, `subname`, IconName, gossip_menu_id,
    minlevel, maxlevel, `exp`, faction, npcflag, speed_walk, speed_run,
    speed_swim, speed_flight, detection_range, `rank`, dmgschool,
    DamageModifier, BaseAttackTime, RangeAttackTime, BaseVariance,
    RangeVariance, unit_class, unit_flags, unit_flags2, dynamicflags,
    family, `type`, type_flags, lootid, pickpocketloot, skinloot,
    PetSpellDataId, VehicleId, mingold, maxgold, AIName, MovementType,
    HoverHeight, HealthModifier, ManaModifier, ArmorModifier,
    ExperienceModifier, RacialLeader, movementId, RegenHealth,
    CreatureImmunitiesId, flags_extra, ScriptName, VerifiedBuild
)
SELECT
    9900002, difficulty_entry_1, difficulty_entry_2, difficulty_entry_3,
    KillCredit1, KillCredit2, 'Taskmaster', 'Debug Runner', IconName, gossip_menu_id,
    1, 1, `exp`, 35, 0, speed_walk, speed_run,
    speed_swim, speed_flight, detection_range, `rank`, dmgschool,
    DamageModifier, BaseAttackTime, RangeAttackTime, BaseVariance,
    RangeVariance, unit_class, unit_flags, unit_flags2, dynamicflags,
    family, `type`, type_flags, lootid, pickpocketloot, skinloot,
    PetSpellDataId, VehicleId, mingold, maxgold, AIName, MovementType,
    HoverHeight, HealthModifier, ManaModifier, ArmorModifier,
    ExperienceModifier, RacialLeader, movementId, RegenHealth,
    CreatureImmunitiesId, flags_extra, ScriptName, VerifiedBuild
FROM creature_template
WHERE entry = 9900001;

INSERT INTO creature_template_model (
    CreatureID, Idx, CreatureDisplayID, DisplayScale, Probability, VerifiedBuild
)
VALUES
    (9900002, 0, 17246, 1, 1, NULL);
