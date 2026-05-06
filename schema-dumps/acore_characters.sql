-- MySQL dump 10.13  Distrib 8.0.46, for Win64 (x86_64)
--
-- Host: 192.168.0.93    Database: acore_characters
-- ------------------------------------------------------
-- Server version	8.0.46

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `account_data`
--

DROP TABLE IF EXISTS `account_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `account_data` (
  `accountId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Account Identifier',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `time` int unsigned NOT NULL DEFAULT '0',
  `data` blob NOT NULL,
  PRIMARY KEY (`accountId`,`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `account_instance_times`
--

DROP TABLE IF EXISTS `account_instance_times`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `account_instance_times` (
  `accountId` int unsigned NOT NULL,
  `instanceId` int unsigned NOT NULL DEFAULT '0',
  `releaseTime` bigint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`accountId`,`instanceId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `account_tutorial`
--

DROP TABLE IF EXISTS `account_tutorial`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `account_tutorial` (
  `accountId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Account Identifier',
  `tut0` int unsigned NOT NULL DEFAULT '0',
  `tut1` int unsigned NOT NULL DEFAULT '0',
  `tut2` int unsigned NOT NULL DEFAULT '0',
  `tut3` int unsigned NOT NULL DEFAULT '0',
  `tut4` int unsigned NOT NULL DEFAULT '0',
  `tut5` int unsigned NOT NULL DEFAULT '0',
  `tut6` int unsigned NOT NULL DEFAULT '0',
  `tut7` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`accountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `active_arena_season`
--

DROP TABLE IF EXISTS `active_arena_season`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `active_arena_season` (
  `season_id` tinyint unsigned NOT NULL,
  `season_state` tinyint unsigned NOT NULL COMMENT 'Supported 2 states: 0 - disabled; 1 - in progress.'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `addons`
--

DROP TABLE IF EXISTS `addons`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `addons` (
  `name` varchar(120) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `crc` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Addons';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `arena_team`
--

DROP TABLE IF EXISTS `arena_team`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `arena_team` (
  `arenaTeamId` int unsigned NOT NULL DEFAULT '0',
  `name` varchar(24) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `captainGuid` int unsigned NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `rating` smallint unsigned NOT NULL DEFAULT '0',
  `seasonGames` smallint unsigned NOT NULL DEFAULT '0',
  `seasonWins` smallint unsigned NOT NULL DEFAULT '0',
  `weekGames` smallint unsigned NOT NULL DEFAULT '0',
  `weekWins` smallint unsigned NOT NULL DEFAULT '0',
  `rank` int unsigned NOT NULL DEFAULT '0',
  `backgroundColor` int unsigned NOT NULL DEFAULT '0',
  `emblemStyle` tinyint unsigned NOT NULL DEFAULT '0',
  `emblemColor` int unsigned NOT NULL DEFAULT '0',
  `borderStyle` tinyint unsigned NOT NULL DEFAULT '0',
  `borderColor` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`arenaTeamId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `arena_team_member`
--

DROP TABLE IF EXISTS `arena_team_member`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `arena_team_member` (
  `arenaTeamId` int unsigned NOT NULL DEFAULT '0',
  `guid` int unsigned NOT NULL DEFAULT '0',
  `weekGames` smallint unsigned NOT NULL DEFAULT '0',
  `weekWins` smallint unsigned NOT NULL DEFAULT '0',
  `seasonGames` smallint unsigned NOT NULL DEFAULT '0',
  `seasonWins` smallint unsigned NOT NULL DEFAULT '0',
  `personalRating` smallint NOT NULL DEFAULT '0',
  PRIMARY KEY (`arenaTeamId`,`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `auctionhouse`
--

DROP TABLE IF EXISTS `auctionhouse`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `auctionhouse` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `houseid` tinyint unsigned NOT NULL DEFAULT '7',
  `itemguid` int unsigned NOT NULL DEFAULT '0',
  `itemowner` int unsigned NOT NULL DEFAULT '0',
  `buyoutprice` int unsigned NOT NULL DEFAULT '0',
  `time` int unsigned NOT NULL DEFAULT '0',
  `buyguid` int unsigned NOT NULL DEFAULT '0',
  `lastbid` int unsigned NOT NULL DEFAULT '0',
  `startbid` int unsigned NOT NULL DEFAULT '0',
  `deposit` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `item_guid` (`itemguid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `banned_addons`
--

DROP TABLE IF EXISTS `banned_addons`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `banned_addons` (
  `Id` int unsigned NOT NULL AUTO_INCREMENT,
  `Name` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Version` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `Timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`Id`),
  UNIQUE KEY `idx_name_ver` (`Name`,`Version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `battleground_deserters`
--

DROP TABLE IF EXISTS `battleground_deserters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `battleground_deserters` (
  `guid` int unsigned NOT NULL COMMENT 'characters.guid',
  `type` tinyint unsigned NOT NULL COMMENT 'type of the desertion',
  `datetime` datetime NOT NULL COMMENT 'datetime of the desertion'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `bugreport`
--

DROP TABLE IF EXISTS `bugreport`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `bugreport` (
  `id` int unsigned NOT NULL AUTO_INCREMENT COMMENT 'Identifier',
  `type` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `content` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `State` tinyint NOT NULL DEFAULT '1',
  `Assignee` varchar(255) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Comment` longtext COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Debug System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `calendar_events`
--

DROP TABLE IF EXISTS `calendar_events`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `calendar_events` (
  `id` bigint unsigned NOT NULL DEFAULT '0',
  `creator` int unsigned NOT NULL DEFAULT '0',
  `title` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `type` tinyint unsigned NOT NULL DEFAULT '4',
  `dungeon` int NOT NULL DEFAULT '-1',
  `eventtime` int unsigned NOT NULL DEFAULT '0',
  `flags` int unsigned NOT NULL DEFAULT '0',
  `time2` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `calendar_invites`
--

DROP TABLE IF EXISTS `calendar_invites`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `calendar_invites` (
  `id` bigint unsigned NOT NULL DEFAULT '0',
  `event` bigint unsigned NOT NULL DEFAULT '0',
  `invitee` int unsigned NOT NULL DEFAULT '0',
  `sender` int unsigned NOT NULL DEFAULT '0',
  `status` tinyint unsigned NOT NULL DEFAULT '0',
  `statustime` int unsigned NOT NULL DEFAULT '0',
  `rank` tinyint unsigned NOT NULL DEFAULT '0',
  `text` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `channels`
--

DROP TABLE IF EXISTS `channels`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `channels` (
  `channelId` int unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `team` int unsigned NOT NULL,
  `announce` tinyint unsigned NOT NULL DEFAULT '1',
  `ownership` tinyint unsigned NOT NULL DEFAULT '1',
  `password` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `lastUsed` int unsigned NOT NULL,
  PRIMARY KEY (`channelId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Channel System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `channels_bans`
--

DROP TABLE IF EXISTS `channels_bans`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `channels_bans` (
  `channelId` int unsigned NOT NULL,
  `playerGUID` int unsigned NOT NULL,
  `banTime` int unsigned NOT NULL,
  PRIMARY KEY (`channelId`,`playerGUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `channels_rights`
--

DROP TABLE IF EXISTS `channels_rights`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `channels_rights` (
  `name` varchar(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `flags` int unsigned NOT NULL,
  `speakdelay` int unsigned NOT NULL,
  `joinmessage` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `delaymessage` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `moderators` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_account_data`
--

DROP TABLE IF EXISTS `character_account_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_account_data` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `time` int unsigned NOT NULL DEFAULT '0',
  `data` blob NOT NULL,
  PRIMARY KEY (`guid`,`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_achievement`
--

DROP TABLE IF EXISTS `character_achievement`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_achievement` (
  `guid` int unsigned NOT NULL,
  `achievement` smallint unsigned NOT NULL,
  `date` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`achievement`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_achievement_offline_updates`
--

DROP TABLE IF EXISTS `character_achievement_offline_updates`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_achievement_offline_updates` (
  `guid` int unsigned NOT NULL COMMENT 'Character''s GUID',
  `update_type` tinyint unsigned NOT NULL COMMENT 'Supported types: 1 - COMPLETE_ACHIEVEMENT; 2 - UPDATE_CRITERIA',
  `arg1` int unsigned NOT NULL COMMENT 'For type 1: achievement ID; for type 2: ACHIEVEMENT_CRITERIA_TYPE',
  `arg2` int unsigned DEFAULT NULL COMMENT 'For type 2: miscValue1 for updating achievement criteria',
  `arg3` int unsigned DEFAULT NULL COMMENT 'For type 2: miscValue2 for updating achievement criteria',
  KEY `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Stores updates to character achievements when the character was offline';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_achievement_progress`
--

DROP TABLE IF EXISTS `character_achievement_progress`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_achievement_progress` (
  `guid` int unsigned NOT NULL,
  `criteria` smallint unsigned NOT NULL,
  `counter` int unsigned NOT NULL,
  `date` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`criteria`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_action`
--

DROP TABLE IF EXISTS `character_action`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_action` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `spec` tinyint unsigned NOT NULL DEFAULT '0',
  `button` tinyint unsigned NOT NULL DEFAULT '0',
  `action` int unsigned NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`spec`,`button`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_arena_stats`
--

DROP TABLE IF EXISTS `character_arena_stats`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_arena_stats` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `slot` tinyint unsigned NOT NULL DEFAULT '0',
  `matchMakerRating` smallint unsigned NOT NULL DEFAULT '0',
  `maxMMR` smallint NOT NULL,
  PRIMARY KEY (`guid`,`slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_aura`
--

DROP TABLE IF EXISTS `character_aura`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_aura` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `casterGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Full Global Unique Identifier',
  `itemGuid` bigint unsigned NOT NULL DEFAULT '0',
  `spell` int unsigned NOT NULL DEFAULT '0',
  `effectMask` tinyint unsigned NOT NULL DEFAULT '0',
  `recalculateMask` tinyint unsigned NOT NULL DEFAULT '0',
  `stackCount` tinyint unsigned NOT NULL DEFAULT '1',
  `amount0` int NOT NULL DEFAULT '0',
  `amount1` int NOT NULL DEFAULT '0',
  `amount2` int NOT NULL DEFAULT '0',
  `base_amount0` int NOT NULL DEFAULT '0',
  `base_amount1` int NOT NULL DEFAULT '0',
  `base_amount2` int NOT NULL DEFAULT '0',
  `maxDuration` int NOT NULL DEFAULT '0',
  `remainTime` int NOT NULL DEFAULT '0',
  `remainCharges` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`casterGuid`,`itemGuid`,`spell`,`effectMask`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_banned`
--

DROP TABLE IF EXISTS `character_banned`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_banned` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `bandate` int unsigned NOT NULL DEFAULT '0',
  `unbandate` int unsigned NOT NULL DEFAULT '0',
  `bannedby` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `banreason` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `active` tinyint unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`guid`,`bandate`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Ban List';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_battleground_random`
--

DROP TABLE IF EXISTS `character_battleground_random`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_battleground_random` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_bot_profile_slots`
--

DROP TABLE IF EXISTS `character_bot_profile_slots`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_bot_profile_slots` (
  `characterGuid` bigint unsigned NOT NULL,
  `activeSlot` tinyint unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`characterGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Tracks the active moveset profile slot (1-10) per bot character.';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_brew_of_the_month`
--

DROP TABLE IF EXISTS `character_brew_of_the_month`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_brew_of_the_month` (
  `guid` int unsigned NOT NULL,
  `lastEventId` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_declinedname`
--

DROP TABLE IF EXISTS `character_declinedname`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_declinedname` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `genitive` varchar(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `dative` varchar(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `accusative` varchar(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `instrumental` varchar(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `prepositional` varchar(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_entry_point`
--

DROP TABLE IF EXISTS `character_entry_point`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_entry_point` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `joinX` float NOT NULL DEFAULT '0',
  `joinY` float NOT NULL DEFAULT '0',
  `joinZ` float NOT NULL DEFAULT '0',
  `joinO` float NOT NULL DEFAULT '0',
  `joinMapId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Map Identifier',
  `taxiPath0` int unsigned NOT NULL DEFAULT '0',
  `taxiPath1` int unsigned NOT NULL DEFAULT '0',
  `mountSpell` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_equipmentsets`
--

DROP TABLE IF EXISTS `character_equipmentsets`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_equipmentsets` (
  `guid` int NOT NULL DEFAULT '0',
  `setguid` bigint NOT NULL AUTO_INCREMENT,
  `setindex` tinyint unsigned NOT NULL DEFAULT '0',
  `name` varchar(31) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `iconname` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `ignore_mask` int unsigned NOT NULL DEFAULT '0',
  `item0` int unsigned NOT NULL DEFAULT '0',
  `item1` int unsigned NOT NULL DEFAULT '0',
  `item2` int unsigned NOT NULL DEFAULT '0',
  `item3` int unsigned NOT NULL DEFAULT '0',
  `item4` int unsigned NOT NULL DEFAULT '0',
  `item5` int unsigned NOT NULL DEFAULT '0',
  `item6` int unsigned NOT NULL DEFAULT '0',
  `item7` int unsigned NOT NULL DEFAULT '0',
  `item8` int unsigned NOT NULL DEFAULT '0',
  `item9` int unsigned NOT NULL DEFAULT '0',
  `item10` int unsigned NOT NULL DEFAULT '0',
  `item11` int unsigned NOT NULL DEFAULT '0',
  `item12` int unsigned NOT NULL DEFAULT '0',
  `item13` int unsigned NOT NULL DEFAULT '0',
  `item14` int unsigned NOT NULL DEFAULT '0',
  `item15` int unsigned NOT NULL DEFAULT '0',
  `item16` int unsigned NOT NULL DEFAULT '0',
  `item17` int unsigned NOT NULL DEFAULT '0',
  `item18` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`setguid`),
  UNIQUE KEY `idx_set` (`guid`,`setguid`,`setindex`),
  KEY `Idx_setindex` (`setindex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_gifts`
--

DROP TABLE IF EXISTS `character_gifts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_gifts` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `item_guid` int unsigned NOT NULL DEFAULT '0',
  `entry` int unsigned NOT NULL DEFAULT '0',
  `flags` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`item_guid`),
  KEY `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_glyphs`
--

DROP TABLE IF EXISTS `character_glyphs`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_glyphs` (
  `guid` int unsigned NOT NULL,
  `talentGroup` tinyint unsigned NOT NULL DEFAULT '0',
  `glyph1` smallint unsigned DEFAULT '0',
  `glyph2` smallint unsigned DEFAULT '0',
  `glyph3` smallint unsigned DEFAULT '0',
  `glyph4` smallint unsigned DEFAULT '0',
  `glyph5` smallint unsigned DEFAULT '0',
  `glyph6` smallint unsigned DEFAULT '0',
  PRIMARY KEY (`guid`,`talentGroup`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_homebind`
--

DROP TABLE IF EXISTS `character_homebind`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_homebind` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `mapId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Map Identifier',
  `zoneId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Zone Identifier',
  `posX` float NOT NULL DEFAULT '0',
  `posY` float NOT NULL DEFAULT '0',
  `posZ` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_instance`
--

DROP TABLE IF EXISTS `character_instance`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_instance` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `instance` int unsigned NOT NULL DEFAULT '0',
  `permanent` tinyint unsigned NOT NULL DEFAULT '0',
  `extended` tinyint unsigned NOT NULL,
  PRIMARY KEY (`guid`,`instance`),
  KEY `instance` (`instance`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_inventory`
--

DROP TABLE IF EXISTS `character_inventory`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_inventory` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `bag` int unsigned NOT NULL DEFAULT '0',
  `slot` tinyint unsigned NOT NULL DEFAULT '0',
  `item` int unsigned NOT NULL DEFAULT '0' COMMENT 'Item Global Unique Identifier',
  PRIMARY KEY (`item`),
  UNIQUE KEY `guid` (`guid`,`bag`,`slot`),
  KEY `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_pet`
--

DROP TABLE IF EXISTS `character_pet`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_pet` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `entry` int unsigned NOT NULL DEFAULT '0',
  `owner` int unsigned NOT NULL DEFAULT '0',
  `modelid` int unsigned DEFAULT '0',
  `CreatedBySpell` int unsigned DEFAULT '0',
  `PetType` tinyint unsigned NOT NULL DEFAULT '0',
  `level` smallint unsigned NOT NULL DEFAULT '1',
  `exp` int unsigned NOT NULL DEFAULT '0',
  `Reactstate` tinyint unsigned NOT NULL DEFAULT '0',
  `name` varchar(21) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'Pet',
  `renamed` tinyint unsigned NOT NULL DEFAULT '0',
  `slot` tinyint unsigned NOT NULL DEFAULT '0',
  `curhealth` int unsigned NOT NULL DEFAULT '1',
  `curmana` int unsigned NOT NULL DEFAULT '0',
  `curhappiness` int unsigned NOT NULL DEFAULT '0',
  `savetime` int unsigned NOT NULL DEFAULT '0',
  `abdata` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`id`),
  KEY `owner` (`owner`),
  KEY `idx_slot` (`slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Pet System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_pet_declinedname`
--

DROP TABLE IF EXISTS `character_pet_declinedname`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_pet_declinedname` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `owner` int unsigned NOT NULL DEFAULT '0',
  `genitive` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `dative` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `accusative` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `instrumental` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `prepositional` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `owner_key` (`owner`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_queststatus`
--

DROP TABLE IF EXISTS `character_queststatus`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_queststatus` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  `status` tinyint unsigned NOT NULL DEFAULT '0',
  `explored` tinyint unsigned NOT NULL DEFAULT '0',
  `timer` int unsigned NOT NULL DEFAULT '0',
  `mobcount1` smallint unsigned NOT NULL DEFAULT '0',
  `mobcount2` smallint unsigned NOT NULL DEFAULT '0',
  `mobcount3` smallint unsigned NOT NULL DEFAULT '0',
  `mobcount4` smallint unsigned NOT NULL DEFAULT '0',
  `itemcount1` smallint unsigned NOT NULL DEFAULT '0',
  `itemcount2` smallint unsigned NOT NULL DEFAULT '0',
  `itemcount3` smallint unsigned NOT NULL DEFAULT '0',
  `itemcount4` smallint unsigned NOT NULL DEFAULT '0',
  `itemcount5` smallint unsigned NOT NULL DEFAULT '0',
  `itemcount6` smallint unsigned NOT NULL DEFAULT '0',
  `playercount` smallint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_queststatus_daily`
--

DROP TABLE IF EXISTS `character_queststatus_daily`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_queststatus_daily` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  `time` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`quest`),
  KEY `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_queststatus_monthly`
--

DROP TABLE IF EXISTS `character_queststatus_monthly`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_queststatus_monthly` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  PRIMARY KEY (`guid`,`quest`),
  KEY `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_queststatus_rewarded`
--

DROP TABLE IF EXISTS `character_queststatus_rewarded`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_queststatus_rewarded` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  `active` tinyint unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`guid`,`quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_queststatus_seasonal`
--

DROP TABLE IF EXISTS `character_queststatus_seasonal`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_queststatus_seasonal` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  `event` int unsigned NOT NULL DEFAULT '0' COMMENT 'Event Identifier',
  PRIMARY KEY (`guid`,`quest`),
  KEY `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_queststatus_weekly`
--

DROP TABLE IF EXISTS `character_queststatus_weekly`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_queststatus_weekly` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  PRIMARY KEY (`guid`,`quest`),
  KEY `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_reputation`
--

DROP TABLE IF EXISTS `character_reputation`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_reputation` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `faction` smallint unsigned NOT NULL DEFAULT '0',
  `standing` int NOT NULL DEFAULT '0',
  `flags` smallint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`faction`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_settings`
--

DROP TABLE IF EXISTS `character_settings`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_settings` (
  `guid` int unsigned NOT NULL,
  `source` varchar(40) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `data` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`guid`,`source`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player Settings';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_skills`
--

DROP TABLE IF EXISTS `character_skills`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_skills` (
  `guid` int unsigned NOT NULL COMMENT 'Global Unique Identifier',
  `skill` smallint unsigned NOT NULL,
  `value` smallint unsigned NOT NULL,
  `max` smallint unsigned NOT NULL,
  PRIMARY KEY (`guid`,`skill`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_social`
--

DROP TABLE IF EXISTS `character_social`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_social` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `friend` int unsigned NOT NULL DEFAULT '0' COMMENT 'Friend Global Unique Identifier',
  `flags` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Friend Flags',
  `note` varchar(48) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Friend Note',
  PRIMARY KEY (`guid`,`friend`,`flags`),
  KEY `friend` (`friend`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_spell`
--

DROP TABLE IF EXISTS `character_spell`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_spell` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `spell` int unsigned NOT NULL DEFAULT '0' COMMENT 'Spell Identifier',
  `specMask` tinyint unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`guid`,`spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_spell_cooldown`
--

DROP TABLE IF EXISTS `character_spell_cooldown`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_spell_cooldown` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier, Low part',
  `spell` int unsigned NOT NULL DEFAULT '0' COMMENT 'Spell Identifier',
  `category` int unsigned DEFAULT '0',
  `item` int unsigned NOT NULL DEFAULT '0' COMMENT 'Item Identifier',
  `time` int unsigned NOT NULL DEFAULT '0',
  `needSend` tinyint unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`guid`,`spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_stats`
--

DROP TABLE IF EXISTS `character_stats`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_stats` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier, Low part',
  `maxhealth` int unsigned NOT NULL DEFAULT '0',
  `maxpower1` int unsigned NOT NULL DEFAULT '0',
  `maxpower2` int unsigned NOT NULL DEFAULT '0',
  `maxpower3` int unsigned NOT NULL DEFAULT '0',
  `maxpower4` int unsigned NOT NULL DEFAULT '0',
  `maxpower5` int unsigned NOT NULL DEFAULT '0',
  `maxpower6` int unsigned NOT NULL DEFAULT '0',
  `maxpower7` int unsigned NOT NULL DEFAULT '0',
  `strength` int unsigned NOT NULL DEFAULT '0',
  `agility` int unsigned NOT NULL DEFAULT '0',
  `stamina` int unsigned NOT NULL DEFAULT '0',
  `intellect` int unsigned NOT NULL DEFAULT '0',
  `spirit` int unsigned NOT NULL DEFAULT '0',
  `armor` int unsigned NOT NULL DEFAULT '0',
  `resHoly` int unsigned NOT NULL DEFAULT '0',
  `resFire` int unsigned NOT NULL DEFAULT '0',
  `resNature` int unsigned NOT NULL DEFAULT '0',
  `resFrost` int unsigned NOT NULL DEFAULT '0',
  `resShadow` int unsigned NOT NULL DEFAULT '0',
  `resArcane` int unsigned NOT NULL DEFAULT '0',
  `blockPct` float NOT NULL DEFAULT '0',
  `dodgePct` float NOT NULL DEFAULT '0',
  `parryPct` float NOT NULL DEFAULT '0',
  `critPct` float NOT NULL DEFAULT '0',
  `rangedCritPct` float NOT NULL DEFAULT '0',
  `spellCritPct` float NOT NULL DEFAULT '0',
  `attackPower` int unsigned NOT NULL DEFAULT '0',
  `rangedAttackPower` int unsigned NOT NULL DEFAULT '0',
  `spellPower` int unsigned NOT NULL DEFAULT '0',
  `resilience` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`),
  CONSTRAINT `character_stats_chk_1` CHECK (((`blockPct` >= 0) and (`dodgePct` >= 0) and (`parryPct` >= 0) and (`critPct` >= 0) and (`rangedCritPct` >= 0) and (`spellCritPct` >= 0)))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `character_talent`
--

DROP TABLE IF EXISTS `character_talent`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_talent` (
  `guid` int unsigned NOT NULL,
  `spell` int unsigned NOT NULL,
  `specMask` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `characters`
--

DROP TABLE IF EXISTS `characters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `characters` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `account` int unsigned NOT NULL DEFAULT '0' COMMENT 'Account Identifier',
  `name` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  `race` tinyint unsigned NOT NULL DEFAULT '0',
  `class` tinyint unsigned NOT NULL DEFAULT '0',
  `gender` tinyint unsigned NOT NULL DEFAULT '0',
  `level` tinyint unsigned NOT NULL DEFAULT '0',
  `xp` int unsigned NOT NULL DEFAULT '0',
  `money` int unsigned NOT NULL DEFAULT '0',
  `skin` tinyint unsigned NOT NULL DEFAULT '0',
  `face` tinyint unsigned NOT NULL DEFAULT '0',
  `hairStyle` tinyint unsigned NOT NULL DEFAULT '0',
  `hairColor` tinyint unsigned NOT NULL DEFAULT '0',
  `facialStyle` tinyint unsigned NOT NULL DEFAULT '0',
  `bankSlots` tinyint unsigned NOT NULL DEFAULT '0',
  `restState` tinyint unsigned NOT NULL DEFAULT '0',
  `playerFlags` int unsigned NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `map` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Map Identifier',
  `instance_id` int unsigned NOT NULL DEFAULT '0',
  `instance_mode_mask` tinyint unsigned NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `taximask` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `online` tinyint unsigned NOT NULL DEFAULT '0',
  `cinematic` tinyint unsigned NOT NULL DEFAULT '0',
  `totaltime` int unsigned NOT NULL DEFAULT '0',
  `leveltime` int unsigned NOT NULL DEFAULT '0',
  `logout_time` int unsigned NOT NULL DEFAULT '0',
  `is_logout_resting` tinyint unsigned NOT NULL DEFAULT '0',
  `rest_bonus` float NOT NULL DEFAULT '0',
  `resettalents_cost` int unsigned NOT NULL DEFAULT '0',
  `resettalents_time` int unsigned NOT NULL DEFAULT '0',
  `trans_x` float NOT NULL DEFAULT '0',
  `trans_y` float NOT NULL DEFAULT '0',
  `trans_z` float NOT NULL DEFAULT '0',
  `trans_o` float NOT NULL DEFAULT '0',
  `transguid` int DEFAULT '0',
  `extra_flags` smallint unsigned NOT NULL DEFAULT '0',
  `stable_slots` tinyint unsigned NOT NULL DEFAULT '0',
  `at_login` smallint unsigned NOT NULL DEFAULT '0',
  `zone` smallint unsigned NOT NULL DEFAULT '0',
  `death_expire_time` int unsigned NOT NULL DEFAULT '0',
  `taxi_path` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `arenaPoints` int unsigned NOT NULL DEFAULT '0',
  `totalHonorPoints` int unsigned NOT NULL DEFAULT '0',
  `todayHonorPoints` int unsigned NOT NULL DEFAULT '0',
  `yesterdayHonorPoints` int unsigned NOT NULL DEFAULT '0',
  `totalKills` int unsigned NOT NULL DEFAULT '0',
  `todayKills` smallint unsigned NOT NULL DEFAULT '0',
  `yesterdayKills` smallint unsigned NOT NULL DEFAULT '0',
  `chosenTitle` int unsigned NOT NULL DEFAULT '0',
  `knownCurrencies` bigint unsigned NOT NULL DEFAULT '0',
  `watchedFaction` int unsigned NOT NULL DEFAULT '0',
  `drunk` tinyint unsigned NOT NULL DEFAULT '0',
  `health` int unsigned NOT NULL DEFAULT '0',
  `power1` int unsigned NOT NULL DEFAULT '0',
  `power2` int unsigned NOT NULL DEFAULT '0',
  `power3` int unsigned NOT NULL DEFAULT '0',
  `power4` int unsigned NOT NULL DEFAULT '0',
  `power5` int unsigned NOT NULL DEFAULT '0',
  `power6` int unsigned NOT NULL DEFAULT '0',
  `power7` int unsigned NOT NULL DEFAULT '0',
  `latency` int unsigned DEFAULT '0',
  `talentGroupsCount` tinyint unsigned NOT NULL DEFAULT '1',
  `activeTalentGroup` tinyint unsigned NOT NULL DEFAULT '0',
  `exploredZones` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `equipmentCache` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ammoId` int unsigned NOT NULL DEFAULT '0',
  `knownTitles` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `actionBars` tinyint unsigned NOT NULL DEFAULT '0',
  `grantableLevels` tinyint unsigned NOT NULL DEFAULT '0',
  `order` tinyint DEFAULT NULL,
  `creation_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `deleteInfos_Account` int unsigned DEFAULT NULL,
  `deleteInfos_Name` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `deleteDate` int unsigned DEFAULT NULL,
  `innTriggerId` int unsigned NOT NULL,
  `extraBonusTalentCount` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`),
  KEY `idx_account` (`account`),
  KEY `idx_online` (`online`),
  KEY `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `corpse`
--

DROP TABLE IF EXISTS `corpse`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `corpse` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `posX` float NOT NULL DEFAULT '0',
  `posY` float NOT NULL DEFAULT '0',
  `posZ` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `mapId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Map Identifier',
  `phaseMask` int unsigned NOT NULL DEFAULT '1',
  `displayId` int unsigned NOT NULL DEFAULT '0',
  `itemCache` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `bytes1` int unsigned NOT NULL DEFAULT '0',
  `bytes2` int unsigned NOT NULL DEFAULT '0',
  `guildId` int unsigned NOT NULL DEFAULT '0',
  `flags` tinyint unsigned NOT NULL DEFAULT '0',
  `dynFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `time` int unsigned NOT NULL DEFAULT '0',
  `corpseType` tinyint unsigned NOT NULL DEFAULT '0',
  `instanceId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Instance Identifier',
  PRIMARY KEY (`guid`),
  KEY `idx_type` (`corpseType`),
  KEY `idx_instance` (`instanceId`),
  KEY `idx_time` (`time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Death System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_respawn`
--

DROP TABLE IF EXISTS `creature_respawn`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_respawn` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `respawnTime` int unsigned NOT NULL DEFAULT '0',
  `mapId` smallint unsigned NOT NULL DEFAULT '0',
  `instanceId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Instance Identifier',
  PRIMARY KEY (`guid`,`instanceId`),
  KEY `idx_instance` (`instanceId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Grid Loading System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_condition_save`
--

DROP TABLE IF EXISTS `game_event_condition_save`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_condition_save` (
  `eventEntry` tinyint unsigned NOT NULL,
  `condition_id` int unsigned NOT NULL DEFAULT '0',
  `done` float DEFAULT '0',
  PRIMARY KEY (`eventEntry`,`condition_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_save`
--

DROP TABLE IF EXISTS `game_event_save`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_save` (
  `eventEntry` tinyint unsigned NOT NULL,
  `state` tinyint unsigned NOT NULL DEFAULT '1',
  `next_start` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`eventEntry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject_respawn`
--

DROP TABLE IF EXISTS `gameobject_respawn`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject_respawn` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `respawnTime` int unsigned NOT NULL DEFAULT '0',
  `mapId` smallint unsigned NOT NULL DEFAULT '0',
  `instanceId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Instance Identifier',
  PRIMARY KEY (`guid`,`instanceId`),
  KEY `idx_instance` (`instanceId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Grid Loading System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gm_subsurvey`
--

DROP TABLE IF EXISTS `gm_subsurvey`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gm_subsurvey` (
  `surveyId` int unsigned NOT NULL AUTO_INCREMENT,
  `questionId` int unsigned NOT NULL DEFAULT '0',
  `answer` int unsigned NOT NULL DEFAULT '0',
  `answerComment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`surveyId`,`questionId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gm_survey`
--

DROP TABLE IF EXISTS `gm_survey`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gm_survey` (
  `surveyId` int unsigned NOT NULL AUTO_INCREMENT,
  `guid` int unsigned NOT NULL DEFAULT '0',
  `mainSurvey` int unsigned NOT NULL DEFAULT '0',
  `comment` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `createTime` int unsigned NOT NULL DEFAULT '0',
  `maxMMR` smallint NOT NULL,
  PRIMARY KEY (`surveyId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gm_ticket`
--

DROP TABLE IF EXISTS `gm_ticket`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gm_ticket` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `type` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0 open, 1 closed, 2 character deleted',
  `playerGuid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier of ticket creator',
  `name` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Name of ticket creator',
  `description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `createTime` int unsigned NOT NULL DEFAULT '0',
  `mapId` smallint unsigned NOT NULL DEFAULT '0',
  `posX` float NOT NULL DEFAULT '0',
  `posY` float NOT NULL DEFAULT '0',
  `posZ` float NOT NULL DEFAULT '0',
  `lastModifiedTime` int unsigned NOT NULL DEFAULT '0',
  `closedBy` int NOT NULL DEFAULT '0' COMMENT '-1 Closed by Console, >0 GUID of GM',
  `assignedTo` int unsigned NOT NULL DEFAULT '0' COMMENT 'GUID of admin to whom ticket is assigned',
  `comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `response` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `completed` tinyint unsigned NOT NULL DEFAULT '0',
  `escalated` tinyint unsigned NOT NULL DEFAULT '0',
  `viewed` tinyint unsigned NOT NULL DEFAULT '0',
  `needMoreHelp` tinyint unsigned NOT NULL DEFAULT '0',
  `resolvedBy` int NOT NULL DEFAULT '0' COMMENT '-1 Resolved by Console, >0 GUID of GM',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `group_member`
--

DROP TABLE IF EXISTS `group_member`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `group_member` (
  `guid` int unsigned NOT NULL,
  `memberGuid` int unsigned NOT NULL,
  `memberFlags` tinyint unsigned NOT NULL DEFAULT '0',
  `subgroup` tinyint unsigned NOT NULL DEFAULT '0',
  `roles` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`memberGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Groups';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `groups`
--

DROP TABLE IF EXISTS `groups`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `groups` (
  `guid` int unsigned NOT NULL,
  `leaderGuid` int unsigned NOT NULL,
  `lootMethod` tinyint unsigned NOT NULL,
  `looterGuid` int unsigned NOT NULL,
  `lootThreshold` tinyint unsigned NOT NULL,
  `icon1` bigint unsigned NOT NULL,
  `icon2` bigint unsigned NOT NULL,
  `icon3` bigint unsigned NOT NULL,
  `icon4` bigint unsigned NOT NULL,
  `icon5` bigint unsigned NOT NULL,
  `icon6` bigint unsigned NOT NULL,
  `icon7` bigint unsigned NOT NULL,
  `icon8` bigint unsigned NOT NULL,
  `groupType` tinyint unsigned NOT NULL,
  `difficulty` tinyint unsigned NOT NULL DEFAULT '0',
  `raidDifficulty` tinyint unsigned NOT NULL DEFAULT '0',
  `masterLooterGuid` int unsigned NOT NULL,
  PRIMARY KEY (`guid`),
  KEY `leaderGuid` (`leaderGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Groups';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guild`
--

DROP TABLE IF EXISTS `guild`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild` (
  `guildid` int unsigned NOT NULL DEFAULT '0',
  `name` varchar(24) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `leaderguid` int unsigned NOT NULL DEFAULT '0',
  `EmblemStyle` tinyint unsigned NOT NULL DEFAULT '0',
  `EmblemColor` tinyint unsigned NOT NULL DEFAULT '0',
  `BorderStyle` tinyint unsigned NOT NULL DEFAULT '0',
  `BorderColor` tinyint unsigned NOT NULL DEFAULT '0',
  `BackgroundColor` tinyint unsigned NOT NULL DEFAULT '0',
  `info` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `motd` varchar(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `createdate` int unsigned NOT NULL DEFAULT '0',
  `BankMoney` bigint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guildid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Guild System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guild_bank_eventlog`
--

DROP TABLE IF EXISTS `guild_bank_eventlog`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_bank_eventlog` (
  `guildid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Guild Identificator',
  `LogGuid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Log record identificator - auxiliary column',
  `TabId` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Guild bank TabId',
  `EventType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Event type',
  `PlayerGuid` int unsigned NOT NULL DEFAULT '0',
  `ItemOrMoney` int unsigned NOT NULL DEFAULT '0',
  `ItemStackCount` smallint unsigned NOT NULL DEFAULT '0',
  `DestTabId` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Destination Tab Id',
  `TimeStamp` int unsigned NOT NULL DEFAULT '0' COMMENT 'Event UNIX time',
  PRIMARY KEY (`guildid`,`LogGuid`,`TabId`),
  KEY `guildid_key` (`guildid`),
  KEY `Idx_PlayerGuid` (`PlayerGuid`),
  KEY `Idx_LogGuid` (`LogGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guild_bank_item`
--

DROP TABLE IF EXISTS `guild_bank_item`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_bank_item` (
  `guildid` int unsigned NOT NULL DEFAULT '0',
  `TabId` tinyint unsigned NOT NULL DEFAULT '0',
  `SlotId` tinyint unsigned NOT NULL DEFAULT '0',
  `item_guid` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guildid`,`TabId`,`SlotId`),
  KEY `guildid_key` (`guildid`),
  KEY `Idx_item_guid` (`item_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guild_bank_right`
--

DROP TABLE IF EXISTS `guild_bank_right`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_bank_right` (
  `guildid` int unsigned NOT NULL DEFAULT '0',
  `TabId` tinyint unsigned NOT NULL DEFAULT '0',
  `rid` tinyint unsigned NOT NULL DEFAULT '0',
  `gbright` tinyint unsigned NOT NULL DEFAULT '0',
  `SlotPerDay` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guildid`,`TabId`,`rid`),
  KEY `guildid_key` (`guildid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guild_bank_tab`
--

DROP TABLE IF EXISTS `guild_bank_tab`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_bank_tab` (
  `guildid` int unsigned NOT NULL DEFAULT '0',
  `TabId` tinyint unsigned NOT NULL DEFAULT '0',
  `TabName` varchar(16) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `TabIcon` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `TabText` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`guildid`,`TabId`),
  KEY `guildid_key` (`guildid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guild_eventlog`
--

DROP TABLE IF EXISTS `guild_eventlog`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_eventlog` (
  `guildid` int unsigned NOT NULL COMMENT 'Guild Identificator',
  `LogGuid` int unsigned NOT NULL COMMENT 'Log record identificator - auxiliary column',
  `EventType` tinyint unsigned NOT NULL COMMENT 'Event type',
  `PlayerGuid1` int unsigned NOT NULL COMMENT 'Player 1',
  `PlayerGuid2` int unsigned NOT NULL COMMENT 'Player 2',
  `NewRank` tinyint unsigned NOT NULL COMMENT 'New rank(in case promotion/demotion)',
  `TimeStamp` int unsigned NOT NULL COMMENT 'Event UNIX time',
  PRIMARY KEY (`guildid`,`LogGuid`),
  KEY `Idx_PlayerGuid1` (`PlayerGuid1`),
  KEY `Idx_PlayerGuid2` (`PlayerGuid2`),
  KEY `Idx_LogGuid` (`LogGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Guild Eventlog';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guild_member`
--

DROP TABLE IF EXISTS `guild_member`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_member` (
  `guildid` int unsigned NOT NULL COMMENT 'Guild Identificator',
  `guid` int unsigned NOT NULL,
  `rank` tinyint unsigned NOT NULL,
  `pnote` varchar(31) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `offnote` varchar(31) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  UNIQUE KEY `guid_key` (`guid`),
  KEY `guildid_key` (`guildid`),
  KEY `guildid_rank_key` (`guildid`,`rank`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Guild System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guild_member_withdraw`
--

DROP TABLE IF EXISTS `guild_member_withdraw`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_member_withdraw` (
  `guid` int unsigned NOT NULL,
  `tab0` int unsigned NOT NULL DEFAULT '0',
  `tab1` int unsigned NOT NULL DEFAULT '0',
  `tab2` int unsigned NOT NULL DEFAULT '0',
  `tab3` int unsigned NOT NULL DEFAULT '0',
  `tab4` int unsigned NOT NULL DEFAULT '0',
  `tab5` int unsigned NOT NULL DEFAULT '0',
  `money` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Guild Member Daily Withdraws';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `guild_rank`
--

DROP TABLE IF EXISTS `guild_rank`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `guild_rank` (
  `guildid` int unsigned NOT NULL DEFAULT '0',
  `rid` tinyint unsigned NOT NULL,
  `rname` varchar(20) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `rights` int unsigned DEFAULT '0',
  `BankMoneyPerDay` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guildid`,`rid`),
  KEY `Idx_rid` (`rid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Guild System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `instance`
--

DROP TABLE IF EXISTS `instance`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `instance` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `map` smallint unsigned NOT NULL DEFAULT '0',
  `resettime` int unsigned NOT NULL DEFAULT '0',
  `difficulty` tinyint unsigned NOT NULL DEFAULT '0',
  `completedEncounters` int unsigned NOT NULL DEFAULT '0',
  `data` tinytext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`id`),
  KEY `map` (`map`),
  KEY `resettime` (`resettime`),
  KEY `difficulty` (`difficulty`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `instance_reset`
--

DROP TABLE IF EXISTS `instance_reset`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `instance_reset` (
  `mapid` smallint unsigned NOT NULL DEFAULT '0',
  `difficulty` tinyint unsigned NOT NULL DEFAULT '0',
  `resettime` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`mapid`,`difficulty`),
  KEY `difficulty` (`difficulty`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `instance_saved_go_state_data`
--

DROP TABLE IF EXISTS `instance_saved_go_state_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `instance_saved_go_state_data` (
  `id` int unsigned NOT NULL COMMENT 'instance.id',
  `guid` int unsigned NOT NULL COMMENT 'gameobject.guid',
  `state` tinyint unsigned DEFAULT '0' COMMENT 'gameobject.state',
  PRIMARY KEY (`id`,`guid`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_instance`
--

DROP TABLE IF EXISTS `item_instance`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_instance` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `itemEntry` int unsigned DEFAULT '0',
  `owner_guid` int unsigned NOT NULL DEFAULT '0',
  `creatorGuid` int unsigned NOT NULL DEFAULT '0',
  `giftCreatorGuid` int unsigned NOT NULL DEFAULT '0',
  `count` int unsigned NOT NULL DEFAULT '1',
  `duration` int NOT NULL DEFAULT '0',
  `charges` tinytext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `flags` int unsigned DEFAULT '0',
  `enchantments` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `randomPropertyId` smallint NOT NULL DEFAULT '0',
  `durability` smallint unsigned NOT NULL DEFAULT '0',
  `playedTime` int unsigned NOT NULL DEFAULT '0',
  `text` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`guid`),
  KEY `idx_owner_guid` (`owner_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Item System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_loot_storage`
--

DROP TABLE IF EXISTS `item_loot_storage`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_loot_storage` (
  `containerGUID` int unsigned NOT NULL,
  `itemid` int unsigned NOT NULL,
  `count` int unsigned NOT NULL,
  `item_index` int unsigned NOT NULL DEFAULT '0',
  `randomPropertyId` int NOT NULL,
  `randomSuffix` int unsigned NOT NULL,
  `follow_loot_rules` tinyint unsigned NOT NULL,
  `freeforall` tinyint unsigned NOT NULL,
  `is_blocked` tinyint unsigned NOT NULL,
  `is_counted` tinyint unsigned NOT NULL,
  `is_underthreshold` tinyint unsigned NOT NULL,
  `needs_quest` tinyint unsigned NOT NULL,
  `conditionLootId` int NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_refund_instance`
--

DROP TABLE IF EXISTS `item_refund_instance`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_refund_instance` (
  `item_guid` int unsigned NOT NULL COMMENT 'Item GUID',
  `player_guid` int unsigned NOT NULL COMMENT 'Player GUID',
  `paidMoney` int unsigned NOT NULL DEFAULT '0',
  `paidExtendedCost` smallint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`item_guid`,`player_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Item Refund System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_soulbound_trade_data`
--

DROP TABLE IF EXISTS `item_soulbound_trade_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_soulbound_trade_data` (
  `itemGuid` int unsigned NOT NULL COMMENT 'Item GUID',
  `allowedPlayers` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Space separated GUID list of players who can receive this item in trade',
  PRIMARY KEY (`itemGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Item Refund System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `lag_reports`
--

DROP TABLE IF EXISTS `lag_reports`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `lag_reports` (
  `reportId` int unsigned NOT NULL AUTO_INCREMENT,
  `guid` int unsigned NOT NULL DEFAULT '0',
  `lagType` tinyint unsigned NOT NULL DEFAULT '0',
  `mapId` smallint unsigned NOT NULL DEFAULT '0',
  `posX` float NOT NULL DEFAULT '0',
  `posY` float NOT NULL DEFAULT '0',
  `posZ` float NOT NULL DEFAULT '0',
  `latency` int unsigned NOT NULL DEFAULT '0',
  `createTime` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`reportId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `lfg_data`
--

DROP TABLE IF EXISTS `lfg_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `lfg_data` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `dungeon` int unsigned NOT NULL DEFAULT '0',
  `state` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='LFG Data';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `living_world_account_alt_runtime`
--

DROP TABLE IF EXISTS `living_world_account_alt_runtime`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `living_world_account_alt_runtime` (
  `runtime_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `source_account_id` int unsigned NOT NULL,
  `source_character_guid` bigint unsigned NOT NULL,
  `owner_character_guid` bigint unsigned DEFAULT NULL,
  `clone_account_id` int unsigned NOT NULL,
  `clone_character_guid` bigint unsigned DEFAULT NULL,
  `state` tinyint unsigned NOT NULL DEFAULT '0',
  `source_character_name` varchar(12) COLLATE utf8mb4_unicode_ci NOT NULL,
  `reserved_source_character_name` varchar(12) COLLATE utf8mb4_unicode_ci NOT NULL,
  `clone_character_name` varchar(12) COLLATE utf8mb4_unicode_ci NOT NULL,
  `source_level` tinyint unsigned NOT NULL DEFAULT '1',
  `source_experience` int unsigned NOT NULL DEFAULT '0',
  `source_money` int unsigned NOT NULL DEFAULT '0',
  `clone_level` tinyint unsigned NOT NULL DEFAULT '1',
  `clone_experience` int unsigned NOT NULL DEFAULT '0',
  `clone_money` int unsigned NOT NULL DEFAULT '0',
  `last_clean_sync_time` int unsigned DEFAULT NULL,
  `last_recovery_check_time` int unsigned DEFAULT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`runtime_id`),
  UNIQUE KEY `uk_living_world_runtime_source` (`source_account_id`,`source_character_guid`),
  UNIQUE KEY `uk_living_world_runtime_clone` (`clone_account_id`,`clone_character_guid`),
  KEY `idx_living_world_runtime_account_state` (`source_account_id`,`state`)
) ENGINE=InnoDB AUTO_INCREMENT=243 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `living_world_bot_quest_settings`
--

DROP TABLE IF EXISTS `living_world_bot_quest_settings`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `living_world_bot_quest_settings` (
  `owner_character_guid` bigint unsigned NOT NULL,
  `reward_mode` tinyint unsigned NOT NULL DEFAULT '1',
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`owner_character_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `log_arena_fights`
--

DROP TABLE IF EXISTS `log_arena_fights`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `log_arena_fights` (
  `fight_id` int unsigned NOT NULL,
  `time` datetime NOT NULL,
  `type` tinyint unsigned NOT NULL,
  `duration` int unsigned NOT NULL,
  `winner` int unsigned NOT NULL,
  `loser` int unsigned NOT NULL,
  `winner_tr` smallint unsigned NOT NULL,
  `winner_mmr` smallint unsigned NOT NULL,
  `winner_tr_change` smallint NOT NULL,
  `loser_tr` smallint unsigned NOT NULL,
  `loser_mmr` smallint unsigned NOT NULL,
  `loser_tr_change` smallint NOT NULL,
  `currOnline` int unsigned NOT NULL,
  PRIMARY KEY (`fight_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `log_arena_memberstats`
--

DROP TABLE IF EXISTS `log_arena_memberstats`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `log_arena_memberstats` (
  `fight_id` int unsigned NOT NULL,
  `member_id` tinyint unsigned NOT NULL,
  `name` char(20) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `guid` int unsigned NOT NULL,
  `team` int unsigned NOT NULL,
  `account` int unsigned NOT NULL,
  `ip` char(15) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `damage` int unsigned NOT NULL,
  `heal` int unsigned NOT NULL,
  `kblows` int unsigned NOT NULL,
  PRIMARY KEY (`fight_id`,`member_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `log_encounter`
--

DROP TABLE IF EXISTS `log_encounter`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `log_encounter` (
  `time` datetime NOT NULL,
  `map` smallint unsigned NOT NULL,
  `difficulty` tinyint unsigned NOT NULL,
  `creditType` tinyint unsigned NOT NULL,
  `creditEntry` int unsigned NOT NULL,
  `playersInfo` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `log_money`
--

DROP TABLE IF EXISTS `log_money`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `log_money` (
  `sender_acc` int unsigned NOT NULL,
  `sender_guid` int unsigned NOT NULL,
  `sender_name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `sender_ip` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `receiver_acc` int unsigned NOT NULL,
  `receiver_name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `money` bigint unsigned NOT NULL,
  `topic` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `date` datetime NOT NULL,
  `type` tinyint NOT NULL COMMENT '1=COD,2=AH,3=GB DEPOSIT,4=GB WITHDRAW,5=MAIL,6=TRADE'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mail`
--

DROP TABLE IF EXISTS `mail`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `mail` (
  `id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Identifier',
  `messageType` tinyint unsigned NOT NULL DEFAULT '0',
  `stationery` tinyint NOT NULL DEFAULT '41',
  `mailTemplateId` smallint unsigned NOT NULL DEFAULT '0',
  `sender` int unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `receiver` int unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  `subject` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `body` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `has_items` tinyint unsigned NOT NULL DEFAULT '0',
  `expire_time` int unsigned NOT NULL DEFAULT '0',
  `deliver_time` int unsigned NOT NULL DEFAULT '0',
  `money` int unsigned NOT NULL DEFAULT '0',
  `cod` int unsigned NOT NULL DEFAULT '0',
  `checked` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `idx_receiver` (`receiver`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Mail System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mail_items`
--

DROP TABLE IF EXISTS `mail_items`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `mail_items` (
  `mail_id` int unsigned NOT NULL DEFAULT '0',
  `item_guid` int unsigned NOT NULL DEFAULT '0',
  `receiver` int unsigned NOT NULL DEFAULT '0' COMMENT 'Character Global Unique Identifier',
  PRIMARY KEY (`item_guid`),
  KEY `idx_receiver` (`receiver`),
  KEY `idx_mail_id` (`mail_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mail_server_character`
--

DROP TABLE IF EXISTS `mail_server_character`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `mail_server_character` (
  `guid` int unsigned NOT NULL,
  `mailId` int unsigned NOT NULL,
  PRIMARY KEY (`guid`,`mailId`),
  KEY `fk_mail_server_character` (`mailId`),
  CONSTRAINT `fk_mail_server_character` FOREIGN KEY (`mailId`) REFERENCES `mail_server_template` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mail_server_template`
--

DROP TABLE IF EXISTS `mail_server_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `mail_server_template` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `moneyA` int unsigned NOT NULL DEFAULT '0',
  `moneyH` int unsigned NOT NULL DEFAULT '0',
  `subject` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `body` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `active` tinyint unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mail_server_template_conditions`
--

DROP TABLE IF EXISTS `mail_server_template_conditions`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `mail_server_template_conditions` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `templateID` int unsigned NOT NULL,
  `conditionType` enum('Level','PlayTime','Quest','Achievement','Reputation','Faction','Race','Class','AccountFlags') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `conditionValue` int unsigned NOT NULL,
  `conditionState` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `fk_mail_template_conditions` (`templateID`),
  CONSTRAINT `fk_mail_template_conditions` FOREIGN KEY (`templateID`) REFERENCES `mail_server_template` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mail_server_template_items`
--

DROP TABLE IF EXISTS `mail_server_template_items`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `mail_server_template_items` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `templateID` int unsigned NOT NULL,
  `faction` enum('Alliance','Horde') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `item` int unsigned NOT NULL,
  `itemCount` int unsigned NOT NULL,
  PRIMARY KEY (`id`),
  KEY `fk_mail_template` (`templateID`),
  CONSTRAINT `fk_mail_template` FOREIGN KEY (`templateID`) REFERENCES `mail_server_template` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pet_aura`
--

DROP TABLE IF EXISTS `pet_aura`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pet_aura` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `casterGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Full Global Unique Identifier',
  `spell` int unsigned NOT NULL DEFAULT '0',
  `effectMask` tinyint unsigned NOT NULL DEFAULT '0',
  `recalculateMask` tinyint unsigned NOT NULL DEFAULT '0',
  `stackCount` tinyint unsigned NOT NULL DEFAULT '1',
  `amount0` int DEFAULT NULL,
  `amount1` int DEFAULT NULL,
  `amount2` int DEFAULT NULL,
  `base_amount0` int DEFAULT NULL,
  `base_amount1` int DEFAULT NULL,
  `base_amount2` int DEFAULT NULL,
  `maxDuration` int NOT NULL DEFAULT '0',
  `remainTime` int NOT NULL DEFAULT '0',
  `remainCharges` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`casterGuid`,`spell`,`effectMask`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Pet System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pet_spell`
--

DROP TABLE IF EXISTS `pet_spell`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pet_spell` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `spell` int unsigned NOT NULL DEFAULT '0' COMMENT 'Spell Identifier',
  `active` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Pet System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pet_spell_cooldown`
--

DROP TABLE IF EXISTS `pet_spell_cooldown`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pet_spell_cooldown` (
  `guid` int unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier, Low part',
  `spell` int unsigned NOT NULL DEFAULT '0' COMMENT 'Spell Identifier',
  `category` int unsigned DEFAULT '0',
  `time` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `petition`
--

DROP TABLE IF EXISTS `petition`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `petition` (
  `ownerguid` int unsigned NOT NULL,
  `petitionguid` int unsigned DEFAULT '0',
  `petition_id` int unsigned NOT NULL DEFAULT '0',
  `name` varchar(24) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ownerguid`,`type`),
  UNIQUE KEY `index_ownerguid_petitionguid` (`ownerguid`,`petitionguid`),
  KEY `idx_petition_id` (`petition_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Guild System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `petition_sign`
--

DROP TABLE IF EXISTS `petition_sign`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `petition_sign` (
  `ownerguid` int unsigned NOT NULL,
  `petitionguid` int unsigned NOT NULL DEFAULT '0',
  `petition_id` int unsigned NOT NULL DEFAULT '0',
  `playerguid` int unsigned NOT NULL DEFAULT '0',
  `player_account` int unsigned NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`petitionguid`,`playerguid`),
  KEY `Idx_playerguid` (`playerguid`),
  KEY `Idx_ownerguid` (`ownerguid`),
  KEY `idx_petition_id_player` (`petition_id`,`playerguid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Guild System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pool_quest_save`
--

DROP TABLE IF EXISTS `pool_quest_save`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pool_quest_save` (
  `pool_id` int unsigned NOT NULL DEFAULT '0',
  `quest_id` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`pool_id`,`quest_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `profanity_name`
--

DROP TABLE IF EXISTS `profanity_name`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `profanity_name` (
  `name` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pvpstats_battlegrounds`
--

DROP TABLE IF EXISTS `pvpstats_battlegrounds`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pvpstats_battlegrounds` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `winner_faction` tinyint NOT NULL,
  `bracket_id` tinyint unsigned NOT NULL,
  `type` tinyint unsigned NOT NULL,
  `date` datetime NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pvpstats_players`
--

DROP TABLE IF EXISTS `pvpstats_players`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pvpstats_players` (
  `battleground_id` bigint unsigned NOT NULL,
  `character_guid` int unsigned NOT NULL,
  `winner` bit(1) NOT NULL,
  `score_killing_blows` int unsigned DEFAULT NULL,
  `score_deaths` int unsigned DEFAULT NULL,
  `score_honorable_kills` int unsigned DEFAULT NULL,
  `score_bonus_honor` int unsigned DEFAULT NULL,
  `score_damage_done` int unsigned DEFAULT NULL,
  `score_healing_done` int unsigned DEFAULT NULL,
  `attr_1` int unsigned DEFAULT '0',
  `attr_2` int unsigned DEFAULT '0',
  `attr_3` int unsigned DEFAULT '0',
  `attr_4` int unsigned DEFAULT '0',
  `attr_5` int unsigned DEFAULT '0',
  PRIMARY KEY (`battleground_id`,`character_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_tracker`
--

DROP TABLE IF EXISTS `quest_tracker`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_tracker` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `character_guid` int unsigned NOT NULL DEFAULT '0',
  `quest_accept_time` datetime NOT NULL,
  `quest_complete_time` datetime DEFAULT NULL,
  `quest_abandon_time` datetime DEFAULT NULL,
  `completed_by_gm` tinyint NOT NULL DEFAULT '0',
  `core_hash` varchar(120) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '0',
  `core_revision` varchar(120) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '0',
  UNIQUE KEY `idx_latest_quest_for_character` (`id`,`character_guid`,`quest_accept_time` DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `recovery_item`
--

DROP TABLE IF EXISTS `recovery_item`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `recovery_item` (
  `Id` int unsigned NOT NULL AUTO_INCREMENT,
  `Guid` int unsigned NOT NULL DEFAULT '0',
  `ItemEntry` int unsigned DEFAULT '0',
  `Count` int unsigned NOT NULL DEFAULT '0',
  `DeleteDate` int unsigned DEFAULT NULL,
  PRIMARY KEY (`Id`),
  KEY `idx_guid` (`Guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `reserved_name`
--

DROP TABLE IF EXISTS `reserved_name`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `reserved_name` (
  `name` varchar(12) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player Reserved Names';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spam_reports`
--

DROP TABLE IF EXISTS `spam_reports`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spam_reports` (
  `ID` int unsigned NOT NULL AUTO_INCREMENT,
  `SpamType` tinyint unsigned NOT NULL COMMENT '0 = mail, 1 = chat, 2 = calendar',
  `SpammerGuid` int unsigned NOT NULL DEFAULT '0',
  `Unk1` int unsigned DEFAULT '0',
  `MailIdOrMessageType` int unsigned DEFAULT '0',
  `ChannelId` int unsigned DEFAULT NULL COMMENT 'Only used if SpamType = 1',
  `SecondsSinceMessage` int unsigned DEFAULT NULL COMMENT 'Only used if SpamType = 1',
  `Description` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Time` int DEFAULT NULL COMMENT 'Time of report',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `updates`
--

DROP TABLE IF EXISTS `updates`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `updates` (
  `name` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'filename with extension of the update.',
  `hash` char(40) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT '' COMMENT 'sha1 hash of the sql file.',
  `state` enum('RELEASED','CUSTOM','MODULE','ARCHIVED','PENDING') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'RELEASED' COMMENT 'defines if an update is released or archived.',
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'timestamp when the query was applied.',
  `speed` int unsigned NOT NULL DEFAULT '0' COMMENT 'time the query takes to apply in ms.',
  PRIMARY KEY (`name`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='List of all applied updates in this database.';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `updates_include`
--

DROP TABLE IF EXISTS `updates_include`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `updates_include` (
  `path` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'directory to include. $ means relative to the source directory.',
  `state` enum('RELEASED','ARCHIVED','CUSTOM','PENDING') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'RELEASED' COMMENT 'defines if the directory contains released or archived updates.',
  PRIMARY KEY (`path`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='List of directories where we want to include sql updates.';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `warden_action`
--

DROP TABLE IF EXISTS `warden_action`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `warden_action` (
  `wardenId` smallint unsigned NOT NULL,
  `action` tinyint unsigned DEFAULT NULL,
  PRIMARY KEY (`wardenId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `world_state`
--

DROP TABLE IF EXISTS `world_state`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `world_state` (
  `Id` int unsigned NOT NULL COMMENT 'Internal save ID',
  `Data` longtext,
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='WorldState save system';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `worldstates`
--

DROP TABLE IF EXISTS `worldstates`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `worldstates` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `value` int unsigned NOT NULL DEFAULT '0',
  `comment` tinytext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Variable Saves';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping events for database 'acore_characters'
--

--
-- Dumping routines for database 'acore_characters'
--
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2026-05-01 23:54:56
