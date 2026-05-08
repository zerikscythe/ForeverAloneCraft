-- living_world_world_bot_template (acore_world)
--
-- A single generic creature_template entry used as the base for all
-- creature-based world bots. Level, display_id, and class are overridden
-- at runtime by WorldBotCreatureAI::ApplyIdentityToCreature().
--
-- Entry ID 9900001 is in the custom NPC range used by this project.
-- ScriptName "worldbot_ai" matches the CreatureScript registration.

DELETE FROM creature_template WHERE entry = 9900001;

INSERT INTO creature_template
    (entry, name, subname, IconName, gossip_menu_id, minlevel, maxlevel,
     exp, faction, npcflag, speed_walk, speed_run, speed_swim, speed_flight,
     detection_range, `rank`, dmgschool, DamageModifier,
     BaseAttackTime, RangeAttackTime, BaseVariance, RangeVariance,
     unit_class, unit_flags, unit_flags2, dynamicflags, family, type, type_flags,
     lootid, pickpocketloot, skinloot,
     PetSpellDataId, VehicleId, mingold, maxgold, AIName, MovementType,
     HoverHeight, HealthModifier, ManaModifier, ArmorModifier, ExperienceModifier,
     RacialLeader, movementId, RegenHealth, CreatureImmunitiesId, flags_extra,
     ScriptName)
VALUES
    (9900001, 'Wanderer', '', '', 0, 1, 80,
     2, 35, 0, 1.0, 1.14286, 1.0, 1.0,
     20.0, 0, 0, 1.0,
     2000, 2000, 1.0, 1.0,
     1, 33024, 2048, 0, 0, 7, 0,
     0, 0, 0,
     0, 0, 0, 0, '', 1,
     1.0, 1.0, 1.0, 1.0, 1.0,
     0, 0, 1, 0, 0,
     'worldbot_ai');
