-- MySQL dump 10.13  Distrib 8.0.46, for Win64 (x86_64)
--
-- Host: 192.168.0.93    Database: acore_world
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
-- Table structure for table `achievement_category_dbc`
--

DROP TABLE IF EXISTS `achievement_category_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `achievement_category_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Parent` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Ui_Order` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `achievement_criteria_data`
--

DROP TABLE IF EXISTS `achievement_criteria_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `achievement_criteria_data` (
  `criteria_id` int NOT NULL,
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `value1` int unsigned NOT NULL DEFAULT '0',
  `value2` int unsigned NOT NULL DEFAULT '0',
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`criteria_id`,`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Achievment system';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `achievement_criteria_dbc`
--

DROP TABLE IF EXISTS `achievement_criteria_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `achievement_criteria_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Achievement_Id` int NOT NULL DEFAULT '0',
  `Type` int NOT NULL DEFAULT '0',
  `Asset_Id` int NOT NULL DEFAULT '0',
  `Quantity` int NOT NULL DEFAULT '0',
  `Start_Event` int NOT NULL DEFAULT '0',
  `Start_Asset` int NOT NULL DEFAULT '0',
  `Fail_Event` int NOT NULL DEFAULT '0',
  `Fail_Asset` int NOT NULL DEFAULT '0',
  `Description_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `Timer_Start_Event` int NOT NULL DEFAULT '0',
  `Timer_Asset_Id` int NOT NULL DEFAULT '0',
  `Timer_Time` int NOT NULL DEFAULT '0',
  `Ui_Order` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `achievement_dbc`
--

DROP TABLE IF EXISTS `achievement_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `achievement_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Faction` int NOT NULL DEFAULT '0',
  `Instance_Id` int NOT NULL DEFAULT '0',
  `Supercedes` int NOT NULL DEFAULT '0',
  `Title_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Title_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Description_Lang_enUS` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enGB` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_koKR` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_frFR` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_deDE` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enCN` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhCN` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enTW` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhTW` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esES` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esMX` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ruRU` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptPT` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptBR` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_itIT` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Category` int NOT NULL DEFAULT '0',
  `Points` int NOT NULL DEFAULT '0',
  `Ui_Order` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `IconID` int NOT NULL DEFAULT '0',
  `Reward_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Reward_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Minimum_Criteria` int NOT NULL DEFAULT '0',
  `Shares_Criteria` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `achievement_reward`
--

DROP TABLE IF EXISTS `achievement_reward`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `achievement_reward` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `TitleA` int unsigned NOT NULL DEFAULT '0',
  `TitleH` int unsigned NOT NULL DEFAULT '0',
  `ItemID` int unsigned NOT NULL DEFAULT '0',
  `Sender` int unsigned NOT NULL DEFAULT '0',
  `Subject` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MailTemplateID` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `achievement_reward_locale`
--

DROP TABLE IF EXISTS `achievement_reward_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `achievement_reward_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Subject` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`ID`,`Locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `acore_string`
--

DROP TABLE IF EXISTS `acore_string`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `acore_string` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `content_default` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `locale_koKR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `locale_frFR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `locale_deDE` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `locale_zhCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `locale_zhTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `locale_esES` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `locale_esMX` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `locale_ruRU` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `antidos_opcode_policies`
--

DROP TABLE IF EXISTS `antidos_opcode_policies`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `antidos_opcode_policies` (
  `Opcode` smallint unsigned NOT NULL,
  `Policy` tinyint unsigned NOT NULL,
  `MaxAllowedCount` smallint unsigned NOT NULL,
  PRIMARY KEY (`Opcode`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `areagroup_dbc`
--

DROP TABLE IF EXISTS `areagroup_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `areagroup_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `AreaID_1` int NOT NULL DEFAULT '0',
  `AreaID_2` int NOT NULL DEFAULT '0',
  `AreaID_3` int NOT NULL DEFAULT '0',
  `AreaID_4` int NOT NULL DEFAULT '0',
  `AreaID_5` int NOT NULL DEFAULT '0',
  `AreaID_6` int NOT NULL DEFAULT '0',
  `NextAreaID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `areapoi_dbc`
--

DROP TABLE IF EXISTS `areapoi_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `areapoi_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Importance` int NOT NULL DEFAULT '0',
  `Icon_1` int NOT NULL DEFAULT '0',
  `Icon_2` int NOT NULL DEFAULT '0',
  `Icon_3` int NOT NULL DEFAULT '0',
  `Icon_4` int NOT NULL DEFAULT '0',
  `Icon_5` int NOT NULL DEFAULT '0',
  `Icon_6` int NOT NULL DEFAULT '0',
  `Icon_7` int NOT NULL DEFAULT '0',
  `Icon_8` int NOT NULL DEFAULT '0',
  `Icon_9` int NOT NULL DEFAULT '0',
  `FactionID` int NOT NULL DEFAULT '0',
  `X` float NOT NULL DEFAULT '0',
  `Y` float NOT NULL DEFAULT '0',
  `Z` float NOT NULL DEFAULT '0',
  `ContinentID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `AreaID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Description_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `WorldStateID` int NOT NULL DEFAULT '0',
  `WorldMapLink` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `areatable_dbc`
--

DROP TABLE IF EXISTS `areatable_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `areatable_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `ContinentID` int NOT NULL DEFAULT '0',
  `ParentAreaID` int NOT NULL DEFAULT '0',
  `AreaBit` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `SoundProviderPref` int NOT NULL DEFAULT '0',
  `SoundProviderPrefUnderwater` int NOT NULL DEFAULT '0',
  `AmbienceID` int NOT NULL DEFAULT '0',
  `ZoneMusic` int NOT NULL DEFAULT '0',
  `IntroSound` int NOT NULL DEFAULT '0',
  `ExplorationLevel` int NOT NULL DEFAULT '0',
  `AreaName_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `FactionGroupMask` int NOT NULL DEFAULT '0',
  `LiquidTypeID_1` int NOT NULL DEFAULT '0',
  `LiquidTypeID_2` int NOT NULL DEFAULT '0',
  `LiquidTypeID_3` int NOT NULL DEFAULT '0',
  `LiquidTypeID_4` int NOT NULL DEFAULT '0',
  `MinElevation` float NOT NULL DEFAULT '0',
  `Ambient_Multiplier` float NOT NULL DEFAULT '0',
  `Lightid` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `areatrigger`
--

DROP TABLE IF EXISTS `areatrigger`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `areatrigger` (
  `entry` int unsigned NOT NULL AUTO_INCREMENT,
  `map` int unsigned NOT NULL DEFAULT '0',
  `x` float NOT NULL DEFAULT '0',
  `y` float NOT NULL DEFAULT '0',
  `z` float NOT NULL DEFAULT '0',
  `radius` float NOT NULL DEFAULT '0' COMMENT 'Seems to be a box of size yards with center at x,y,z',
  `length` float NOT NULL DEFAULT '0' COMMENT 'Most commonly used when size is 0, but not always',
  `width` float NOT NULL DEFAULT '0' COMMENT 'Most commonly used when size is 0, but not always',
  `height` float NOT NULL DEFAULT '0' COMMENT 'Most commonly used when size is 0, but not always',
  `orientation` float NOT NULL DEFAULT '0' COMMENT 'Most commonly used when size is 0, but not always',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB AUTO_INCREMENT=5873 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `areatrigger_involvedrelation`
--

DROP TABLE IF EXISTS `areatrigger_involvedrelation`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `areatrigger_involvedrelation` (
  `id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Identifier',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Trigger System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `areatrigger_scripts`
--

DROP TABLE IF EXISTS `areatrigger_scripts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `areatrigger_scripts` (
  `entry` int NOT NULL,
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `areatrigger_tavern`
--

DROP TABLE IF EXISTS `areatrigger_tavern`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `areatrigger_tavern` (
  `id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Identifier',
  `name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `faction` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Trigger System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `areatrigger_teleport`
--

DROP TABLE IF EXISTS `areatrigger_teleport`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `areatrigger_teleport` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `target_map` smallint unsigned NOT NULL DEFAULT '0',
  `target_position_x` float NOT NULL DEFAULT '0',
  `target_position_y` float NOT NULL DEFAULT '0',
  `target_position_z` float NOT NULL DEFAULT '0',
  `target_orientation` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`),
  FULLTEXT KEY `name` (`Name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Trigger System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `arena_season_reward`
--

DROP TABLE IF EXISTS `arena_season_reward`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `arena_season_reward` (
  `group_id` int NOT NULL COMMENT 'id from arena_season_reward_group table',
  `type` enum('achievement','item') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'achievement',
  `entry` int unsigned NOT NULL COMMENT 'For item type - item entry, for achievement - achevement id.',
  PRIMARY KEY (`group_id`,`type`,`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `arena_season_reward_group`
--

DROP TABLE IF EXISTS `arena_season_reward_group`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `arena_season_reward_group` (
  `id` int NOT NULL AUTO_INCREMENT,
  `arena_season` tinyint unsigned NOT NULL,
  `criteria_type` enum('pct','abs') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'pct' COMMENT 'Determines how rankings are evaluated: "pct" - percentage-based (e.g., top 20% of the ladder), "abs" - absolute position-based (e.g., top 10 players).',
  `min_criteria` float NOT NULL,
  `max_criteria` float NOT NULL,
  `reward_mail_template_id` int unsigned DEFAULT NULL,
  `reward_mail_subject` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `reward_mail_body` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `gold_reward` int unsigned DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=40 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `auctionhouse_dbc`
--

DROP TABLE IF EXISTS `auctionhouse_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `auctionhouse_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `FactionID` int NOT NULL DEFAULT '0',
  `DepositRate` int NOT NULL DEFAULT '0',
  `ConsignmentRate` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `bankbagslotprices_dbc`
--

DROP TABLE IF EXISTS `bankbagslotprices_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `bankbagslotprices_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Cost` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `barbershopstyle_dbc`
--

DROP TABLE IF EXISTS `barbershopstyle_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `barbershopstyle_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Type` int NOT NULL DEFAULT '0',
  `DisplayName_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Description_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Cost_Modifier` float NOT NULL DEFAULT '0',
  `Race` int NOT NULL DEFAULT '0',
  `Sex` int NOT NULL DEFAULT '0',
  `Data` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `battleground_template`
--

DROP TABLE IF EXISTS `battleground_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `battleground_template` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `MinPlayersPerTeam` smallint unsigned NOT NULL DEFAULT '0',
  `MaxPlayersPerTeam` smallint unsigned NOT NULL DEFAULT '0',
  `MinLvl` tinyint unsigned NOT NULL DEFAULT '0',
  `MaxLvl` tinyint unsigned NOT NULL DEFAULT '0',
  `AllianceStartLoc` int unsigned DEFAULT NULL,
  `AllianceStartO` float NOT NULL,
  `HordeStartLoc` int unsigned DEFAULT NULL,
  `HordeStartO` float NOT NULL,
  `StartMaxDist` float NOT NULL DEFAULT '0',
  `Weight` tinyint unsigned NOT NULL DEFAULT '1',
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `Comment` char(38) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `battlemaster_entry`
--

DROP TABLE IF EXISTS `battlemaster_entry`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `battlemaster_entry` (
  `entry` int unsigned NOT NULL DEFAULT '0' COMMENT 'Entry of a creature',
  `bg_template` int unsigned NOT NULL DEFAULT '0' COMMENT 'Battleground template id',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `battlemasterlist_dbc`
--

DROP TABLE IF EXISTS `battlemasterlist_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `battlemasterlist_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `MapID_1` int NOT NULL DEFAULT '0',
  `MapID_2` int NOT NULL DEFAULT '0',
  `MapID_3` int NOT NULL DEFAULT '0',
  `MapID_4` int NOT NULL DEFAULT '0',
  `MapID_5` int NOT NULL DEFAULT '0',
  `MapID_6` int NOT NULL DEFAULT '0',
  `MapID_7` int NOT NULL DEFAULT '0',
  `MapID_8` int NOT NULL DEFAULT '0',
  `InstanceType` int NOT NULL DEFAULT '0',
  `GroupsAllowed` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `MaxGroupSize` int NOT NULL DEFAULT '0',
  `HolidayWorldState` int NOT NULL DEFAULT '0',
  `Minlevel` int NOT NULL DEFAULT '0',
  `Maxlevel` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `broadcast_text`
--

DROP TABLE IF EXISTS `broadcast_text`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `broadcast_text` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `LanguageID` int DEFAULT NULL,
  `MaleText` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `FemaleText` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `EmoteID1` int DEFAULT NULL,
  `EmoteID2` int DEFAULT NULL,
  `EmoteID3` int DEFAULT NULL,
  `EmoteDelay1` int DEFAULT NULL,
  `EmoteDelay2` int DEFAULT NULL,
  `EmoteDelay3` int DEFAULT NULL,
  `SoundEntriesId` int DEFAULT NULL,
  `EmotesID` int DEFAULT NULL,
  `Flags` int DEFAULT NULL,
  `VerifiedBuild` smallint DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `broadcast_text_locale`
--

DROP TABLE IF EXISTS `broadcast_text_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `broadcast_text_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `MaleText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `FemaleText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` smallint DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `charstartoutfit_dbc`
--

DROP TABLE IF EXISTS `charstartoutfit_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `charstartoutfit_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `RaceID` tinyint unsigned NOT NULL DEFAULT '0',
  `ClassID` tinyint unsigned NOT NULL DEFAULT '0',
  `SexID` tinyint unsigned NOT NULL DEFAULT '0',
  `OutfitID` tinyint unsigned NOT NULL DEFAULT '0',
  `ItemID_1` int NOT NULL DEFAULT '0',
  `ItemID_2` int NOT NULL DEFAULT '0',
  `ItemID_3` int NOT NULL DEFAULT '0',
  `ItemID_4` int NOT NULL DEFAULT '0',
  `ItemID_5` int NOT NULL DEFAULT '0',
  `ItemID_6` int NOT NULL DEFAULT '0',
  `ItemID_7` int NOT NULL DEFAULT '0',
  `ItemID_8` int NOT NULL DEFAULT '0',
  `ItemID_9` int NOT NULL DEFAULT '0',
  `ItemID_10` int NOT NULL DEFAULT '0',
  `ItemID_11` int NOT NULL DEFAULT '0',
  `ItemID_12` int NOT NULL DEFAULT '0',
  `ItemID_13` int NOT NULL DEFAULT '0',
  `ItemID_14` int NOT NULL DEFAULT '0',
  `ItemID_15` int NOT NULL DEFAULT '0',
  `ItemID_16` int NOT NULL DEFAULT '0',
  `ItemID_17` int NOT NULL DEFAULT '0',
  `ItemID_18` int NOT NULL DEFAULT '0',
  `ItemID_19` int NOT NULL DEFAULT '0',
  `ItemID_20` int NOT NULL DEFAULT '0',
  `ItemID_21` int NOT NULL DEFAULT '0',
  `ItemID_22` int NOT NULL DEFAULT '0',
  `ItemID_23` int NOT NULL DEFAULT '0',
  `ItemID_24` int NOT NULL DEFAULT '0',
  `DisplayItemID_1` int NOT NULL DEFAULT '0',
  `DisplayItemID_2` int NOT NULL DEFAULT '0',
  `DisplayItemID_3` int NOT NULL DEFAULT '0',
  `DisplayItemID_4` int NOT NULL DEFAULT '0',
  `DisplayItemID_5` int NOT NULL DEFAULT '0',
  `DisplayItemID_6` int NOT NULL DEFAULT '0',
  `DisplayItemID_7` int NOT NULL DEFAULT '0',
  `DisplayItemID_8` int NOT NULL DEFAULT '0',
  `DisplayItemID_9` int NOT NULL DEFAULT '0',
  `DisplayItemID_10` int NOT NULL DEFAULT '0',
  `DisplayItemID_11` int NOT NULL DEFAULT '0',
  `DisplayItemID_12` int NOT NULL DEFAULT '0',
  `DisplayItemID_13` int NOT NULL DEFAULT '0',
  `DisplayItemID_14` int NOT NULL DEFAULT '0',
  `DisplayItemID_15` int NOT NULL DEFAULT '0',
  `DisplayItemID_16` int NOT NULL DEFAULT '0',
  `DisplayItemID_17` int NOT NULL DEFAULT '0',
  `DisplayItemID_18` int NOT NULL DEFAULT '0',
  `DisplayItemID_19` int NOT NULL DEFAULT '0',
  `DisplayItemID_20` int NOT NULL DEFAULT '0',
  `DisplayItemID_21` int NOT NULL DEFAULT '0',
  `DisplayItemID_22` int NOT NULL DEFAULT '0',
  `DisplayItemID_23` int NOT NULL DEFAULT '0',
  `DisplayItemID_24` int NOT NULL DEFAULT '0',
  `InventoryType_1` int NOT NULL DEFAULT '0',
  `InventoryType_2` int NOT NULL DEFAULT '0',
  `InventoryType_3` int NOT NULL DEFAULT '0',
  `InventoryType_4` int NOT NULL DEFAULT '0',
  `InventoryType_5` int NOT NULL DEFAULT '0',
  `InventoryType_6` int NOT NULL DEFAULT '0',
  `InventoryType_7` int NOT NULL DEFAULT '0',
  `InventoryType_8` int NOT NULL DEFAULT '0',
  `InventoryType_9` int NOT NULL DEFAULT '0',
  `InventoryType_10` int NOT NULL DEFAULT '0',
  `InventoryType_11` int NOT NULL DEFAULT '0',
  `InventoryType_12` int NOT NULL DEFAULT '0',
  `InventoryType_13` int NOT NULL DEFAULT '0',
  `InventoryType_14` int NOT NULL DEFAULT '0',
  `InventoryType_15` int NOT NULL DEFAULT '0',
  `InventoryType_16` int NOT NULL DEFAULT '0',
  `InventoryType_17` int NOT NULL DEFAULT '0',
  `InventoryType_18` int NOT NULL DEFAULT '0',
  `InventoryType_19` int NOT NULL DEFAULT '0',
  `InventoryType_20` int NOT NULL DEFAULT '0',
  `InventoryType_21` int NOT NULL DEFAULT '0',
  `InventoryType_22` int NOT NULL DEFAULT '0',
  `InventoryType_23` int NOT NULL DEFAULT '0',
  `InventoryType_24` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `chartitles_dbc`
--

DROP TABLE IF EXISTS `chartitles_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `chartitles_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Condition_ID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Name1_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name1_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Mask_ID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `chatchannels_dbc`
--

DROP TABLE IF EXISTS `chatchannels_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `chatchannels_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `FactionGroup` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Shortcut_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Shortcut_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `chrclasses_dbc`
--

DROP TABLE IF EXISTS `chrclasses_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `chrclasses_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Field01` int NOT NULL DEFAULT '0',
  `DisplayPower` int NOT NULL DEFAULT '0',
  `PetNameToken` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Name_Female_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Name_Male_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Filename` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SpellClassSet` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `CinematicSequenceID` int NOT NULL DEFAULT '0',
  `Required_Expansion` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `chrraces_dbc`
--

DROP TABLE IF EXISTS `chrraces_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `chrraces_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `FactionID` int NOT NULL DEFAULT '0',
  `ExplorationSoundID` int NOT NULL DEFAULT '0',
  `MaleDisplayId` int NOT NULL DEFAULT '0',
  `FemaleDisplayId` int NOT NULL DEFAULT '0',
  `ClientPrefix` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `BaseLanguage` int NOT NULL DEFAULT '0',
  `CreatureType` int NOT NULL DEFAULT '0',
  `ResSicknessSpellID` int NOT NULL DEFAULT '0',
  `SplashSoundID` int NOT NULL DEFAULT '0',
  `ClientFilestring` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `CinematicSequenceID` int NOT NULL DEFAULT '0',
  `Alliance` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Name_Female_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Female_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Name_Male_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Male_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `FacialHairCustomization_1` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `FacialHairCustomization_2` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `HairCustomization` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Required_Expansion` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `cinematiccamera_dbc`
--

DROP TABLE IF EXISTS `cinematiccamera_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `cinematiccamera_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `model` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `soundEntry` int NOT NULL DEFAULT '0',
  `locationX` float NOT NULL DEFAULT '0',
  `locationY` float NOT NULL DEFAULT '0',
  `locationZ` float NOT NULL DEFAULT '0',
  `rotation` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Cinematic camera DBC';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `cinematicsequences_dbc`
--

DROP TABLE IF EXISTS `cinematicsequences_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `cinematicsequences_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `SoundID` int NOT NULL DEFAULT '0',
  `Camera_1` int NOT NULL DEFAULT '0',
  `Camera_2` int NOT NULL DEFAULT '0',
  `Camera_3` int NOT NULL DEFAULT '0',
  `Camera_4` int NOT NULL DEFAULT '0',
  `Camera_5` int NOT NULL DEFAULT '0',
  `Camera_6` int NOT NULL DEFAULT '0',
  `Camera_7` int NOT NULL DEFAULT '0',
  `Camera_8` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `command`
--

DROP TABLE IF EXISTS `command`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `command` (
  `name` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `security` tinyint unsigned NOT NULL DEFAULT '0',
  `help` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Chat System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `conditions`
--

DROP TABLE IF EXISTS `conditions`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `conditions` (
  `SourceTypeOrReferenceId` int NOT NULL DEFAULT '0',
  `SourceGroup` int unsigned NOT NULL DEFAULT '0',
  `SourceEntry` int NOT NULL DEFAULT '0',
  `SourceId` int NOT NULL DEFAULT '0',
  `ElseGroup` int unsigned NOT NULL DEFAULT '0',
  `ConditionTypeOrReference` int NOT NULL DEFAULT '0',
  `ConditionTarget` tinyint unsigned NOT NULL DEFAULT '0',
  `ConditionValue1` int unsigned NOT NULL DEFAULT '0',
  `ConditionValue2` int unsigned NOT NULL DEFAULT '0',
  `ConditionValue3` int unsigned NOT NULL DEFAULT '0',
  `NegativeCondition` tinyint unsigned NOT NULL DEFAULT '0',
  `ErrorType` int unsigned NOT NULL DEFAULT '0',
  `ErrorTextId` int unsigned NOT NULL DEFAULT '0',
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`SourceTypeOrReferenceId`,`SourceGroup`,`SourceEntry`,`SourceId`,`ElseGroup`,`ConditionTypeOrReference`,`ConditionTarget`,`ConditionValue1`,`ConditionValue2`,`ConditionValue3`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Condition System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature`
--

DROP TABLE IF EXISTS `creature`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature` (
  `guid` int unsigned NOT NULL AUTO_INCREMENT COMMENT 'Global Unique Identifier',
  `id1` int unsigned NOT NULL DEFAULT '0' COMMENT 'Creature Identifier',
  `id2` int unsigned NOT NULL DEFAULT '0' COMMENT 'Creature Identifier',
  `id3` int unsigned NOT NULL DEFAULT '0' COMMENT 'Creature Identifier',
  `map` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Map Identifier',
  `zoneId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Zone Identifier',
  `areaId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Area Identifier',
  `spawnMask` tinyint unsigned NOT NULL DEFAULT '1',
  `phaseMask` int unsigned NOT NULL DEFAULT '1',
  `equipment_id` tinyint NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `spawntimesecs` int unsigned NOT NULL DEFAULT '120',
  `wander_distance` float NOT NULL DEFAULT '0',
  `currentwaypoint` int unsigned NOT NULL DEFAULT '0',
  `curhealth` int unsigned NOT NULL DEFAULT '1',
  `curmana` int unsigned NOT NULL DEFAULT '0',
  `MovementType` tinyint unsigned NOT NULL DEFAULT '0',
  `npcflag` int unsigned NOT NULL DEFAULT '0',
  `unit_flags` int unsigned NOT NULL DEFAULT '0',
  `dynamicflags` int unsigned NOT NULL DEFAULT '0',
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT '',
  `VerifiedBuild` int DEFAULT NULL,
  `CreateObject` tinyint unsigned NOT NULL DEFAULT '0',
  `Comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`guid`),
  KEY `idx_map` (`map`),
  KEY `idx_id` (`id1`)
) ENGINE=InnoDB AUTO_INCREMENT=5300679 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Creature System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_addon`
--

DROP TABLE IF EXISTS `creature_addon`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_addon` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `path_id` int unsigned NOT NULL DEFAULT '0',
  `mount` int unsigned NOT NULL DEFAULT '0',
  `bytes1` int unsigned NOT NULL DEFAULT '0',
  `bytes2` int unsigned NOT NULL DEFAULT '0',
  `emote` int unsigned NOT NULL DEFAULT '0',
  `visibilityDistanceType` tinyint unsigned NOT NULL DEFAULT '0',
  `auras` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_classlevelstats`
--

DROP TABLE IF EXISTS `creature_classlevelstats`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_classlevelstats` (
  `level` tinyint unsigned NOT NULL,
  `class` tinyint unsigned NOT NULL,
  `basehp0` int unsigned NOT NULL DEFAULT '1',
  `basehp1` int unsigned NOT NULL DEFAULT '1',
  `basehp2` int unsigned NOT NULL DEFAULT '1',
  `basemana` int unsigned NOT NULL DEFAULT '0',
  `basearmor` int unsigned NOT NULL DEFAULT '1',
  `attackpower` int unsigned NOT NULL DEFAULT '0',
  `rangedattackpower` int unsigned NOT NULL DEFAULT '0',
  `damage_base` float NOT NULL DEFAULT '0',
  `damage_exp1` float NOT NULL DEFAULT '0',
  `damage_exp2` float NOT NULL DEFAULT '0',
  `Strength` int NOT NULL DEFAULT '0',
  `Agility` int NOT NULL DEFAULT '0',
  `Stamina` int NOT NULL DEFAULT '0',
  `Intellect` int NOT NULL DEFAULT '0',
  `Spirit` int NOT NULL DEFAULT '0',
  `comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`level`,`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_default_trainer`
--

DROP TABLE IF EXISTS `creature_default_trainer`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_default_trainer` (
  `CreatureId` int unsigned NOT NULL,
  `TrainerId` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`CreatureId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_equip_template`
--

DROP TABLE IF EXISTS `creature_equip_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_equip_template` (
  `CreatureID` int unsigned NOT NULL DEFAULT '0',
  `ID` tinyint unsigned NOT NULL DEFAULT '1',
  `ItemID1` int unsigned NOT NULL DEFAULT '0',
  `ItemID2` int unsigned NOT NULL DEFAULT '0',
  `ItemID3` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`CreatureID`,`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_formations`
--

DROP TABLE IF EXISTS `creature_formations`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_formations` (
  `leaderGUID` int unsigned NOT NULL DEFAULT '0',
  `memberGUID` int unsigned NOT NULL DEFAULT '0',
  `dist` float NOT NULL DEFAULT '0',
  `angle` float NOT NULL DEFAULT '0',
  `groupAI` int unsigned NOT NULL DEFAULT '0',
  `point_1` smallint unsigned NOT NULL DEFAULT '0',
  `point_2` smallint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`memberGUID`),
  CONSTRAINT `creature_formations_chk_1` CHECK (((`dist` >= 0) and (`angle` >= 0)))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_immunities`
--

DROP TABLE IF EXISTS `creature_immunities`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_immunities` (
  `ID` int NOT NULL,
  `SchoolMask` tinyint NOT NULL DEFAULT '0',
  `DispelTypeMask` smallint NOT NULL DEFAULT '0',
  `MechanicsMask` bigint NOT NULL DEFAULT '0',
  `Effects` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Auras` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `ImmuneAoE` tinyint(1) NOT NULL DEFAULT '0',
  `ImmuneChain` tinyint(1) NOT NULL DEFAULT '0',
  `Comment` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_loot_template`
--

DROP TABLE IF EXISTS `creature_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`,`Reference`,`GroupId`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_model_info`
--

DROP TABLE IF EXISTS `creature_model_info`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_model_info` (
  `DisplayID` int unsigned NOT NULL DEFAULT '0',
  `BoundingRadius` float NOT NULL DEFAULT '0',
  `CombatReach` float NOT NULL DEFAULT '0',
  `Gender` tinyint unsigned NOT NULL DEFAULT '2',
  `DisplayID_Other_Gender` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` mediumint DEFAULT NULL,
  PRIMARY KEY (`DisplayID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Creature System (Model related info)';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_movement_override`
--

DROP TABLE IF EXISTS `creature_movement_override`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_movement_override` (
  `SpawnId` int unsigned NOT NULL DEFAULT '0',
  `Ground` tinyint unsigned DEFAULT NULL,
  `Swim` tinyint unsigned DEFAULT NULL,
  `Flight` tinyint unsigned DEFAULT NULL,
  `Rooted` tinyint unsigned DEFAULT NULL,
  `Chase` tinyint unsigned DEFAULT NULL,
  `Random` tinyint unsigned DEFAULT NULL,
  `InteractionPauseTimer` int unsigned DEFAULT NULL COMMENT 'Time (in milliseconds) during which creature will not move after interaction with player',
  PRIMARY KEY (`SpawnId`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_onkill_reputation`
--

DROP TABLE IF EXISTS `creature_onkill_reputation`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_onkill_reputation` (
  `creature_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Creature Identifier',
  `RewOnKillRepFaction1` smallint NOT NULL DEFAULT '0',
  `RewOnKillRepFaction2` smallint NOT NULL DEFAULT '0',
  `MaxStanding1` tinyint NOT NULL DEFAULT '0',
  `IsTeamAward1` tinyint NOT NULL DEFAULT '0',
  `RewOnKillRepValue1` float NOT NULL DEFAULT '0',
  `MaxStanding2` tinyint NOT NULL DEFAULT '0',
  `IsTeamAward2` tinyint NOT NULL DEFAULT '0',
  `RewOnKillRepValue2` float NOT NULL DEFAULT '0',
  `TeamDependent` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`creature_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Creature OnKill Reputation gain';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_questender`
--

DROP TABLE IF EXISTS `creature_questender`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_questender` (
  `id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Identifier',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  PRIMARY KEY (`id`,`quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Creature System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_questitem`
--

DROP TABLE IF EXISTS `creature_questitem`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_questitem` (
  `CreatureEntry` int unsigned NOT NULL DEFAULT '0',
  `Idx` int unsigned NOT NULL DEFAULT '0',
  `ItemId` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`CreatureEntry`,`Idx`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_queststarter`
--

DROP TABLE IF EXISTS `creature_queststarter`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_queststarter` (
  `id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Identifier',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  PRIMARY KEY (`id`,`quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Creature System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_sparring`
--

DROP TABLE IF EXISTS `creature_sparring`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_sparring` (
  `GUID` int unsigned NOT NULL,
  `SparringPCT` float NOT NULL,
  PRIMARY KEY (`GUID`),
  CONSTRAINT `creature_sparring_ibfk_1` FOREIGN KEY (`GUID`) REFERENCES `creature` (`guid`),
  CONSTRAINT `creature_sparring_chk_1` CHECK ((`SparringPCT` between 0 and 100))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_summon_groups`
--

DROP TABLE IF EXISTS `creature_summon_groups`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_summon_groups` (
  `summonerId` int unsigned NOT NULL DEFAULT '0',
  `summonerType` tinyint unsigned NOT NULL DEFAULT '0',
  `groupId` tinyint unsigned NOT NULL DEFAULT '0',
  `entry` int unsigned NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `summonType` tinyint unsigned NOT NULL DEFAULT '0',
  `summonTime` int unsigned NOT NULL DEFAULT '0',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_template`
--

DROP TABLE IF EXISTS `creature_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_template` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `difficulty_entry_1` int unsigned NOT NULL DEFAULT '0',
  `difficulty_entry_2` int unsigned NOT NULL DEFAULT '0',
  `difficulty_entry_3` int unsigned NOT NULL DEFAULT '0',
  `KillCredit1` int unsigned NOT NULL DEFAULT '0',
  `KillCredit2` int unsigned NOT NULL DEFAULT '0',
  `name` char(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '0',
  `subname` char(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `IconName` char(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `gossip_menu_id` int unsigned NOT NULL DEFAULT '0',
  `minlevel` tinyint unsigned NOT NULL DEFAULT '1',
  `maxlevel` tinyint unsigned NOT NULL DEFAULT '1',
  `exp` smallint NOT NULL DEFAULT '0',
  `faction` smallint unsigned NOT NULL DEFAULT '0',
  `npcflag` int unsigned NOT NULL DEFAULT '0',
  `speed_walk` float NOT NULL DEFAULT '1' COMMENT 'Result of 2.5/2.5, most common value',
  `speed_run` float NOT NULL DEFAULT '1.14286' COMMENT 'Result of 8.0/7.0, most common value',
  `speed_swim` float NOT NULL DEFAULT '1',
  `speed_flight` float NOT NULL DEFAULT '1',
  `detection_range` float NOT NULL DEFAULT '20',
  `rank` tinyint unsigned NOT NULL DEFAULT '0',
  `dmgschool` tinyint NOT NULL DEFAULT '0',
  `DamageModifier` float NOT NULL DEFAULT '1',
  `BaseAttackTime` int unsigned NOT NULL DEFAULT '0',
  `RangeAttackTime` int unsigned NOT NULL DEFAULT '0',
  `BaseVariance` float NOT NULL DEFAULT '1',
  `RangeVariance` float NOT NULL DEFAULT '1',
  `unit_class` tinyint unsigned NOT NULL DEFAULT '0',
  `unit_flags` int unsigned NOT NULL DEFAULT '0',
  `unit_flags2` int unsigned NOT NULL DEFAULT '0',
  `dynamicflags` int unsigned NOT NULL DEFAULT '0',
  `family` tinyint NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `type_flags` int unsigned NOT NULL DEFAULT '0',
  `lootid` int unsigned NOT NULL DEFAULT '0',
  `pickpocketloot` int unsigned NOT NULL DEFAULT '0',
  `skinloot` int unsigned NOT NULL DEFAULT '0',
  `PetSpellDataId` int unsigned NOT NULL DEFAULT '0',
  `VehicleId` int unsigned NOT NULL DEFAULT '0',
  `mingold` int unsigned NOT NULL DEFAULT '0',
  `maxgold` int unsigned NOT NULL DEFAULT '0',
  `AIName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `MovementType` tinyint unsigned NOT NULL DEFAULT '0',
  `HoverHeight` float NOT NULL DEFAULT '1',
  `HealthModifier` float NOT NULL DEFAULT '1',
  `ManaModifier` float NOT NULL DEFAULT '1',
  `ArmorModifier` float NOT NULL DEFAULT '1',
  `ExperienceModifier` float NOT NULL DEFAULT '1',
  `RacialLeader` tinyint unsigned NOT NULL DEFAULT '0',
  `movementId` int unsigned NOT NULL DEFAULT '0',
  `RegenHealth` tinyint unsigned NOT NULL DEFAULT '1',
  `CreatureImmunitiesId` int NOT NULL DEFAULT '0',
  `flags_extra` int unsigned NOT NULL DEFAULT '0',
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`entry`),
  KEY `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Creature System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_template_addon`
--

DROP TABLE IF EXISTS `creature_template_addon`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_template_addon` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `path_id` int unsigned NOT NULL DEFAULT '0',
  `mount` int unsigned NOT NULL DEFAULT '0',
  `bytes1` int unsigned NOT NULL DEFAULT '0',
  `bytes2` int unsigned NOT NULL DEFAULT '0',
  `emote` int unsigned NOT NULL DEFAULT '0',
  `visibilityDistanceType` tinyint unsigned NOT NULL DEFAULT '0',
  `auras` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_template_locale`
--

DROP TABLE IF EXISTS `creature_template_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_template_locale` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Title` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`entry`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_template_model`
--

DROP TABLE IF EXISTS `creature_template_model`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_template_model` (
  `CreatureID` int unsigned NOT NULL,
  `Idx` smallint unsigned NOT NULL DEFAULT '0',
  `CreatureDisplayID` int unsigned NOT NULL,
  `DisplayScale` float NOT NULL DEFAULT '1',
  `Probability` float NOT NULL DEFAULT '0',
  `VerifiedBuild` smallint unsigned DEFAULT NULL,
  PRIMARY KEY (`CreatureID`,`Idx`),
  CONSTRAINT `creature_template_model_chk_1` CHECK ((`Idx` <= 3))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_template_movement`
--

DROP TABLE IF EXISTS `creature_template_movement`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_template_movement` (
  `CreatureId` int unsigned NOT NULL DEFAULT '0',
  `Ground` tinyint unsigned DEFAULT NULL,
  `Swim` tinyint unsigned DEFAULT NULL,
  `Flight` tinyint unsigned DEFAULT NULL,
  `Rooted` tinyint unsigned DEFAULT NULL,
  `Chase` tinyint unsigned DEFAULT NULL,
  `Random` tinyint unsigned DEFAULT NULL,
  `InteractionPauseTimer` int unsigned DEFAULT NULL COMMENT 'Time (in milliseconds) during which creature will not move after interaction with player',
  PRIMARY KEY (`CreatureId`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_template_resistance`
--

DROP TABLE IF EXISTS `creature_template_resistance`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_template_resistance` (
  `CreatureID` int unsigned NOT NULL,
  `School` tinyint unsigned NOT NULL,
  `Resistance` smallint DEFAULT NULL,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`CreatureID`,`School`),
  CONSTRAINT `creature_template_resistance_chk_1` CHECK (((`School` >= 1) and (`School` <= 6)))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_template_spell`
--

DROP TABLE IF EXISTS `creature_template_spell`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_template_spell` (
  `CreatureID` int unsigned NOT NULL,
  `Index` tinyint unsigned NOT NULL DEFAULT '0',
  `Spell` int unsigned DEFAULT NULL,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`CreatureID`,`Index`),
  CONSTRAINT `creature_template_spell_chk_1` CHECK (((`Index` >= 0) and (`Index` <= 7)))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_text`
--

DROP TABLE IF EXISTS `creature_text`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_text` (
  `CreatureID` int unsigned NOT NULL DEFAULT '0',
  `GroupID` tinyint unsigned NOT NULL DEFAULT '0',
  `ID` tinyint unsigned NOT NULL DEFAULT '0',
  `Text` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Type` tinyint unsigned NOT NULL DEFAULT '0',
  `Language` tinyint NOT NULL DEFAULT '0',
  `Probability` float NOT NULL DEFAULT '0',
  `Emote` int unsigned NOT NULL DEFAULT '0',
  `Duration` int unsigned NOT NULL DEFAULT '0',
  `Sound` int unsigned NOT NULL DEFAULT '0',
  `BroadcastTextId` int NOT NULL DEFAULT '0',
  `TextRange` tinyint unsigned NOT NULL DEFAULT '0',
  `comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT '',
  PRIMARY KEY (`CreatureID`,`GroupID`,`ID`),
  CONSTRAINT `creature_text_chk_1` CHECK ((`Probability` >= 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creature_text_locale`
--

DROP TABLE IF EXISTS `creature_text_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creature_text_locale` (
  `CreatureID` int unsigned NOT NULL DEFAULT '0',
  `GroupID` tinyint unsigned NOT NULL DEFAULT '0',
  `ID` tinyint unsigned NOT NULL DEFAULT '0',
  `Locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Text` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`CreatureID`,`GroupID`,`ID`,`Locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creaturedisplayinfo_dbc`
--

DROP TABLE IF EXISTS `creaturedisplayinfo_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creaturedisplayinfo_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `ModelID` int NOT NULL DEFAULT '0',
  `SoundID` int NOT NULL DEFAULT '0',
  `ExtendedDisplayInfoID` int NOT NULL DEFAULT '0',
  `CreatureModelScale` float NOT NULL DEFAULT '0',
  `CreatureModelAlpha` int NOT NULL DEFAULT '0',
  `TextureVariation_1` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `TextureVariation_2` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `TextureVariation_3` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `PortraitTextureName` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `BloodLevel` int NOT NULL DEFAULT '0',
  `BloodID` int NOT NULL DEFAULT '0',
  `NPCSoundID` int NOT NULL DEFAULT '0',
  `ParticleColorID` int NOT NULL DEFAULT '0',
  `CreatureGeosetData` int NOT NULL DEFAULT '0',
  `ObjectEffectPackageID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creaturedisplayinfoextra_dbc`
--

DROP TABLE IF EXISTS `creaturedisplayinfoextra_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creaturedisplayinfoextra_dbc` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `DisplayRaceID` int unsigned NOT NULL DEFAULT '0',
  `DisplaySexID` int unsigned NOT NULL DEFAULT '0',
  `SkinID` int unsigned NOT NULL DEFAULT '0',
  `FaceID` int unsigned NOT NULL DEFAULT '0',
  `HairStyleID` int unsigned NOT NULL DEFAULT '0',
  `HairColorID` int unsigned NOT NULL DEFAULT '0',
  `FacialHairID` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay1` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay2` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay3` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay4` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay5` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay6` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay7` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay8` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay9` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay10` int unsigned NOT NULL DEFAULT '0',
  `NPCItemDisplay11` int unsigned NOT NULL DEFAULT '0',
  `Flags` int unsigned NOT NULL DEFAULT '0',
  `BakeName` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creaturefamily_dbc`
--

DROP TABLE IF EXISTS `creaturefamily_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creaturefamily_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `MinScale` float NOT NULL DEFAULT '0',
  `MinScaleLevel` int NOT NULL DEFAULT '0',
  `MaxScale` float NOT NULL DEFAULT '0',
  `MaxScaleLevel` int NOT NULL DEFAULT '0',
  `SkillLine_1` int NOT NULL DEFAULT '0',
  `SkillLine_2` int NOT NULL DEFAULT '0',
  `PetFoodMask` int NOT NULL DEFAULT '0',
  `PetTalentType` int NOT NULL DEFAULT '0',
  `CategoryEnumID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `IconFile` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creaturemodeldata_dbc`
--

DROP TABLE IF EXISTS `creaturemodeldata_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creaturemodeldata_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `ModelName` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SizeClass` int NOT NULL DEFAULT '0',
  `ModelScale` float NOT NULL DEFAULT '0',
  `BloodID` int NOT NULL DEFAULT '0',
  `FootprintTextureID` int NOT NULL DEFAULT '0',
  `FootprintTextureLength` float NOT NULL DEFAULT '0',
  `FootprintTextureWidth` float NOT NULL DEFAULT '0',
  `FootprintParticleScale` float NOT NULL DEFAULT '0',
  `FoleyMaterialID` int NOT NULL DEFAULT '0',
  `FootstepShakeSize` int NOT NULL DEFAULT '0',
  `DeathThudShakeSize` int NOT NULL DEFAULT '0',
  `SoundID` int NOT NULL DEFAULT '0',
  `CollisionWidth` float NOT NULL DEFAULT '0',
  `CollisionHeight` float NOT NULL DEFAULT '0',
  `MountHeight` float NOT NULL DEFAULT '0',
  `GeoBoxMinX` float NOT NULL DEFAULT '0',
  `GeoBoxMinY` float NOT NULL DEFAULT '0',
  `GeoBoxMinZ` float NOT NULL DEFAULT '0',
  `GeoBoxMaxX` float NOT NULL DEFAULT '0',
  `GeoBoxMaxY` float NOT NULL DEFAULT '0',
  `GeoBoxMaxZ` float NOT NULL DEFAULT '0',
  `WorldEffectScale` float NOT NULL DEFAULT '0',
  `AttachedEffectScale` float NOT NULL DEFAULT '0',
  `MissileCollisionRadius` float NOT NULL DEFAULT '0',
  `MissileCollisionPush` float NOT NULL DEFAULT '0',
  `MissileCollisionRaise` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creaturespelldata_dbc`
--

DROP TABLE IF EXISTS `creaturespelldata_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creaturespelldata_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Spells_1` int NOT NULL DEFAULT '0',
  `Spells_2` int NOT NULL DEFAULT '0',
  `Spells_3` int NOT NULL DEFAULT '0',
  `Spells_4` int NOT NULL DEFAULT '0',
  `Availability_1` int NOT NULL DEFAULT '0',
  `Availability_2` int NOT NULL DEFAULT '0',
  `Availability_3` int NOT NULL DEFAULT '0',
  `Availability_4` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `creaturetype_dbc`
--

DROP TABLE IF EXISTS `creaturetype_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `creaturetype_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `currencytypes_dbc`
--

DROP TABLE IF EXISTS `currencytypes_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `currencytypes_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `CategoryID` int NOT NULL DEFAULT '0',
  `BitIndex` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `destructiblemodeldata_dbc`
--

DROP TABLE IF EXISTS `destructiblemodeldata_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `destructiblemodeldata_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `State0Wmo` int NOT NULL DEFAULT '0',
  `State0DestructionDoodadSet` int NOT NULL DEFAULT '0',
  `State0ImpactEffectDoodadSet` int NOT NULL DEFAULT '0',
  `State0AmbientDoodadSet` int NOT NULL DEFAULT '0',
  `State1Wmo` int NOT NULL DEFAULT '0',
  `State1DestructionDoodadSet` int NOT NULL DEFAULT '0',
  `State1ImpactEffectDoodadSet` int NOT NULL DEFAULT '0',
  `State1AmbientDoodadSet` int NOT NULL DEFAULT '0',
  `State2Wmo` int NOT NULL DEFAULT '0',
  `State2DestructionDoodadSet` int NOT NULL DEFAULT '0',
  `State2ImpactEffectDoodadSet` int NOT NULL DEFAULT '0',
  `State2AmbientDoodadSet` int NOT NULL DEFAULT '0',
  `State3Wmo` int NOT NULL DEFAULT '0',
  `State3DestructionDoodadSet` int NOT NULL DEFAULT '0',
  `State3ImpactEffectDoodadSet` int NOT NULL DEFAULT '0',
  `State3AmbientDoodadSet` int NOT NULL DEFAULT '0',
  `Field17` int NOT NULL DEFAULT '0',
  `Field18` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `disables`
--

DROP TABLE IF EXISTS `disables`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `disables` (
  `sourceType` int unsigned NOT NULL,
  `entry` int unsigned NOT NULL,
  `flags` tinyint unsigned NOT NULL DEFAULT '0',
  `params_0` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `params_1` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`sourceType`,`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `disenchant_loot_template`
--

DROP TABLE IF EXISTS `disenchant_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `disenchant_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `dungeon_access_requirements`
--

DROP TABLE IF EXISTS `dungeon_access_requirements`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `dungeon_access_requirements` (
  `dungeon_access_id` tinyint unsigned NOT NULL COMMENT 'ID from dungeon_access_template',
  `requirement_type` tinyint unsigned NOT NULL COMMENT '0 = achiev, 1 = quest, 2 = item',
  `requirement_id` int unsigned NOT NULL COMMENT 'Achiev/quest/item ID',
  `requirement_note` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Optional msg shown ingame to player if he cannot enter. You can add extra info',
  `faction` tinyint unsigned NOT NULL DEFAULT '2' COMMENT '0 = Alliance, 1 = Horde, 2 = Both factions',
  `priority` tinyint unsigned DEFAULT NULL COMMENT 'Priority order for the requirement, sorted by type. 0 is the highest priority',
  `leader_only` tinyint NOT NULL DEFAULT '0' COMMENT '0 = check the requirement for the player trying to enter, 1 = check the requirement for the party leader',
  `comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`dungeon_access_id`,`requirement_type`,`requirement_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Add (multiple) requirements before being able to enter a dungeon/raid';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `dungeon_access_template`
--

DROP TABLE IF EXISTS `dungeon_access_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `dungeon_access_template` (
  `id` tinyint unsigned NOT NULL AUTO_INCREMENT COMMENT 'The dungeon template ID',
  `map_id` int unsigned DEFAULT NULL COMMENT 'Map ID from instance_template',
  `difficulty` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '5 man: 0 = normal, 1 = heroic, 2 = epic (not implemented) | 10 man: 0 = normal, 2 = heroic | 25 man: 1 = normal, 3 = heroic',
  `min_level` tinyint unsigned DEFAULT NULL,
  `max_level` tinyint unsigned DEFAULT NULL,
  `min_avg_item_level` smallint unsigned DEFAULT NULL COMMENT 'Min average ilvl required to enter',
  `comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Dungeon Name 5/10/25/40 man - Normal/Heroic',
  PRIMARY KEY (`id`),
  KEY `FK_dungeon_access_template__instance_template` (`map_id`)
) ENGINE=InnoDB AUTO_INCREMENT=122 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Dungeon/raid access template and single requirements';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `dungeonencounter_dbc`
--

DROP TABLE IF EXISTS `dungeonencounter_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `dungeonencounter_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `MapID` int NOT NULL DEFAULT '0',
  `Difficulty` int NOT NULL DEFAULT '0',
  `OrderIndex` int NOT NULL DEFAULT '0',
  `Bit` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `SpellIconID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `durabilitycosts_dbc`
--

DROP TABLE IF EXISTS `durabilitycosts_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `durabilitycosts_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_1` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_2` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_3` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_4` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_5` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_6` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_7` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_8` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_9` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_10` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_11` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_12` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_13` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_14` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_15` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_16` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_17` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_18` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_19` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_20` int NOT NULL DEFAULT '0',
  `WeaponSubClassCost_21` int NOT NULL DEFAULT '0',
  `ArmorSubClassCost_1` int NOT NULL DEFAULT '0',
  `ArmorSubClassCost_2` int NOT NULL DEFAULT '0',
  `ArmorSubClassCost_3` int NOT NULL DEFAULT '0',
  `ArmorSubClassCost_4` int NOT NULL DEFAULT '0',
  `ArmorSubClassCost_5` int NOT NULL DEFAULT '0',
  `ArmorSubClassCost_6` int NOT NULL DEFAULT '0',
  `ArmorSubClassCost_7` int NOT NULL DEFAULT '0',
  `ArmorSubClassCost_8` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `durabilityquality_dbc`
--

DROP TABLE IF EXISTS `durabilityquality_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `durabilityquality_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `emotes_dbc`
--

DROP TABLE IF EXISTS `emotes_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `emotes_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `EmoteSlashCommand` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AnimID` int NOT NULL DEFAULT '0',
  `EmoteFlags` int NOT NULL DEFAULT '0',
  `EmoteSpecProc` int NOT NULL DEFAULT '0',
  `EmoteSpecProcParam` int NOT NULL DEFAULT '0',
  `EventSoundID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `emotestext_dbc`
--

DROP TABLE IF EXISTS `emotestext_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `emotestext_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `EmoteID` int NOT NULL DEFAULT '0',
  `EmoteText_1` int NOT NULL DEFAULT '0',
  `EmoteText_2` int NOT NULL DEFAULT '0',
  `EmoteText_3` int NOT NULL DEFAULT '0',
  `EmoteText_4` int NOT NULL DEFAULT '0',
  `EmoteText_5` int NOT NULL DEFAULT '0',
  `EmoteText_6` int NOT NULL DEFAULT '0',
  `EmoteText_7` int NOT NULL DEFAULT '0',
  `EmoteText_8` int NOT NULL DEFAULT '0',
  `EmoteText_9` int NOT NULL DEFAULT '0',
  `EmoteText_10` int NOT NULL DEFAULT '0',
  `EmoteText_11` int NOT NULL DEFAULT '0',
  `EmoteText_12` int NOT NULL DEFAULT '0',
  `EmoteText_13` int NOT NULL DEFAULT '0',
  `EmoteText_14` int NOT NULL DEFAULT '0',
  `EmoteText_15` int NOT NULL DEFAULT '0',
  `EmoteText_16` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `event_scripts`
--

DROP TABLE IF EXISTS `event_scripts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `event_scripts` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `delay` int unsigned NOT NULL DEFAULT '0',
  `command` int unsigned NOT NULL DEFAULT '0',
  `datalong` int unsigned NOT NULL DEFAULT '0',
  `datalong2` int unsigned NOT NULL DEFAULT '0',
  `dataint` int NOT NULL DEFAULT '0',
  `x` float NOT NULL DEFAULT '0',
  `y` float NOT NULL DEFAULT '0',
  `z` float NOT NULL DEFAULT '0',
  `o` float NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `exploration_basexp`
--

DROP TABLE IF EXISTS `exploration_basexp`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `exploration_basexp` (
  `level` tinyint unsigned NOT NULL DEFAULT '0',
  `basexp` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Exploration System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `faction_dbc`
--

DROP TABLE IF EXISTS `faction_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `faction_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `ReputationIndex` int NOT NULL DEFAULT '0',
  `ReputationRaceMask_1` int NOT NULL DEFAULT '0',
  `ReputationRaceMask_2` int NOT NULL DEFAULT '0',
  `ReputationRaceMask_3` int NOT NULL DEFAULT '0',
  `ReputationRaceMask_4` int NOT NULL DEFAULT '0',
  `ReputationClassMask_1` int NOT NULL DEFAULT '0',
  `ReputationClassMask_2` int NOT NULL DEFAULT '0',
  `ReputationClassMask_3` int NOT NULL DEFAULT '0',
  `ReputationClassMask_4` int NOT NULL DEFAULT '0',
  `ReputationBase_1` int NOT NULL DEFAULT '0',
  `ReputationBase_2` int NOT NULL DEFAULT '0',
  `ReputationBase_3` int NOT NULL DEFAULT '0',
  `ReputationBase_4` int NOT NULL DEFAULT '0',
  `ReputationFlags_1` int NOT NULL DEFAULT '0',
  `ReputationFlags_2` int NOT NULL DEFAULT '0',
  `ReputationFlags_3` int NOT NULL DEFAULT '0',
  `ReputationFlags_4` int NOT NULL DEFAULT '0',
  `ParentFactionID` int NOT NULL DEFAULT '0',
  `ParentFactionMod_1` float NOT NULL DEFAULT '0',
  `ParentFactionMod_2` float NOT NULL DEFAULT '0',
  `ParentFactionCap_1` int NOT NULL DEFAULT '0',
  `ParentFactionCap_2` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Description_Lang_enUS` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enGB` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_koKR` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_frFR` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_deDE` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enCN` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhCN` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enTW` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhTW` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esES` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esMX` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ruRU` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptPT` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptBR` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_itIT` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `factiontemplate_dbc`
--

DROP TABLE IF EXISTS `factiontemplate_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `factiontemplate_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Faction` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `FactionGroup` int NOT NULL DEFAULT '0',
  `FriendGroup` int NOT NULL DEFAULT '0',
  `EnemyGroup` int NOT NULL DEFAULT '0',
  `Enemies_1` int NOT NULL DEFAULT '0',
  `Enemies_2` int NOT NULL DEFAULT '0',
  `Enemies_3` int NOT NULL DEFAULT '0',
  `Enemies_4` int NOT NULL DEFAULT '0',
  `Friend_1` int NOT NULL DEFAULT '0',
  `Friend_2` int NOT NULL DEFAULT '0',
  `Friend_3` int NOT NULL DEFAULT '0',
  `Friend_4` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `fishing_loot_template`
--

DROP TABLE IF EXISTS `fishing_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `fishing_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event`
--

DROP TABLE IF EXISTS `game_event`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event` (
  `eventEntry` tinyint unsigned NOT NULL COMMENT 'Entry of the game event',
  `start_time` timestamp NULL DEFAULT '2000-01-01 18:00:00' COMMENT 'Absolute start date, the event will never start before',
  `end_time` timestamp NULL DEFAULT '2000-01-01 18:00:00' COMMENT 'Absolute end date, the event will never start after',
  `occurence` bigint unsigned NOT NULL DEFAULT '5184000' COMMENT 'Delay in minutes between occurences of the event',
  `length` bigint unsigned NOT NULL DEFAULT '2592000' COMMENT 'Length in minutes of the event',
  `holiday` int unsigned NOT NULL DEFAULT '0' COMMENT 'Client side holiday id',
  `holidayStage` tinyint unsigned NOT NULL DEFAULT '0',
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Description of the event displayed in console',
  `world_event` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0 if normal event, 1 if world event',
  `announce` tinyint unsigned NOT NULL DEFAULT '2' COMMENT '0 dont announce, 1 announce, 2 value from config',
  PRIMARY KEY (`eventEntry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_arena_seasons`
--

DROP TABLE IF EXISTS `game_event_arena_seasons`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_arena_seasons` (
  `eventEntry` tinyint unsigned NOT NULL COMMENT 'Entry of the game event',
  `season` tinyint unsigned NOT NULL COMMENT 'Arena season number',
  UNIQUE KEY `season` (`season`,`eventEntry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_battleground_holiday`
--

DROP TABLE IF EXISTS `game_event_battleground_holiday`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_battleground_holiday` (
  `eventEntry` tinyint unsigned NOT NULL COMMENT 'Entry of the game event',
  `bgflag` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`eventEntry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_condition`
--

DROP TABLE IF EXISTS `game_event_condition`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_condition` (
  `eventEntry` tinyint unsigned NOT NULL COMMENT 'Entry of the game event',
  `condition_id` int unsigned NOT NULL DEFAULT '0',
  `req_num` float DEFAULT '0',
  `max_world_state_field` smallint unsigned NOT NULL DEFAULT '0',
  `done_world_state_field` smallint unsigned NOT NULL DEFAULT '0',
  `description` varchar(25) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`eventEntry`,`condition_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_creature`
--

DROP TABLE IF EXISTS `game_event_creature`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_creature` (
  `eventEntry` smallint NOT NULL COMMENT 'Entry of the game event. Put negative entry to remove during event.',
  `guid` int unsigned NOT NULL,
  PRIMARY KEY (`guid`,`eventEntry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_creature_quest`
--

DROP TABLE IF EXISTS `game_event_creature_quest`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_creature_quest` (
  `eventEntry` tinyint unsigned NOT NULL COMMENT 'Entry of the game event.',
  `id` int unsigned NOT NULL DEFAULT '0',
  `quest` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`,`quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_gameobject`
--

DROP TABLE IF EXISTS `game_event_gameobject`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_gameobject` (
  `eventEntry` smallint NOT NULL COMMENT 'Entry of the game event. Put negative entry to remove during event.',
  `guid` int unsigned NOT NULL,
  PRIMARY KEY (`guid`,`eventEntry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_gameobject_quest`
--

DROP TABLE IF EXISTS `game_event_gameobject_quest`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_gameobject_quest` (
  `eventEntry` tinyint unsigned NOT NULL COMMENT 'Entry of the game event',
  `id` int unsigned NOT NULL DEFAULT '0',
  `quest` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`,`quest`,`eventEntry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_model_equip`
--

DROP TABLE IF EXISTS `game_event_model_equip`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_model_equip` (
  `eventEntry` tinyint unsigned NOT NULL COMMENT 'Entry of the game event.',
  `guid` int unsigned NOT NULL DEFAULT '0',
  `modelid` int unsigned NOT NULL DEFAULT '0',
  `equipment_id` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_npc_vendor`
--

DROP TABLE IF EXISTS `game_event_npc_vendor`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_npc_vendor` (
  `eventEntry` smallint NOT NULL COMMENT 'Entry of the game event.',
  `guid` int unsigned NOT NULL DEFAULT '0',
  `slot` smallint NOT NULL DEFAULT '0',
  `item` int unsigned NOT NULL DEFAULT '0',
  `maxcount` int unsigned NOT NULL DEFAULT '0',
  `incrtime` int unsigned NOT NULL DEFAULT '0',
  `ExtendedCost` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`eventEntry`,`guid`,`item`) USING BTREE,
  KEY `slot` (`slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_npcflag`
--

DROP TABLE IF EXISTS `game_event_npcflag`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_npcflag` (
  `eventEntry` tinyint unsigned NOT NULL COMMENT 'Entry of the game event',
  `guid` int unsigned NOT NULL DEFAULT '0',
  `npcflag` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`eventEntry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_pool`
--

DROP TABLE IF EXISTS `game_event_pool`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_pool` (
  `eventEntry` smallint NOT NULL COMMENT 'Entry of the game event. Put negative entry to remove during event.',
  `pool_entry` int unsigned NOT NULL DEFAULT '0' COMMENT 'Id of the pool',
  PRIMARY KEY (`pool_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_prerequisite`
--

DROP TABLE IF EXISTS `game_event_prerequisite`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_prerequisite` (
  `eventEntry` tinyint unsigned NOT NULL COMMENT 'Entry of the game event',
  `prerequisite_event` int unsigned NOT NULL,
  PRIMARY KEY (`eventEntry`,`prerequisite_event`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_quest_condition`
--

DROP TABLE IF EXISTS `game_event_quest_condition`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_quest_condition` (
  `eventEntry` tinyint unsigned NOT NULL COMMENT 'Entry of the game event.',
  `quest` int unsigned NOT NULL DEFAULT '0',
  `condition_id` int unsigned NOT NULL DEFAULT '0',
  `num` float DEFAULT '0',
  PRIMARY KEY (`quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_event_seasonal_questrelation`
--

DROP TABLE IF EXISTS `game_event_seasonal_questrelation`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_event_seasonal_questrelation` (
  `questId` int unsigned NOT NULL COMMENT 'Quest Identifier',
  `eventEntry` int unsigned NOT NULL DEFAULT '0' COMMENT 'Entry of the game event',
  PRIMARY KEY (`questId`,`eventEntry`),
  KEY `idx_quest` (`questId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Player System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_graveyard`
--

DROP TABLE IF EXISTS `game_graveyard`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_graveyard` (
  `ID` int NOT NULL DEFAULT '0',
  `Map` int NOT NULL DEFAULT '0',
  `x` float NOT NULL DEFAULT '0',
  `y` float NOT NULL DEFAULT '0',
  `z` float NOT NULL DEFAULT '0',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_tele`
--

DROP TABLE IF EXISTS `game_tele`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_tele` (
  `id` int unsigned NOT NULL,
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `map` smallint unsigned NOT NULL DEFAULT '0',
  `name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Tele Command';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `game_weather`
--

DROP TABLE IF EXISTS `game_weather`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `game_weather` (
  `zone` int unsigned NOT NULL DEFAULT '0',
  `spring_rain_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `spring_snow_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `spring_storm_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `summer_rain_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `summer_snow_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `summer_storm_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `fall_rain_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `fall_snow_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `fall_storm_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `winter_rain_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `winter_snow_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `winter_storm_chance` tinyint unsigned NOT NULL DEFAULT '25',
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`zone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Weather System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject`
--

DROP TABLE IF EXISTS `gameobject`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject` (
  `guid` int unsigned NOT NULL AUTO_INCREMENT COMMENT 'Global Unique Identifier',
  `id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Gameobject Identifier',
  `map` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Map Identifier',
  `zoneId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Zone Identifier',
  `areaId` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'Area Identifier',
  `spawnMask` tinyint unsigned NOT NULL DEFAULT '1',
  `phaseMask` int unsigned NOT NULL DEFAULT '1',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `rotation0` float NOT NULL DEFAULT '0',
  `rotation1` float NOT NULL DEFAULT '0',
  `rotation2` float NOT NULL DEFAULT '0',
  `rotation3` float NOT NULL DEFAULT '0',
  `spawntimesecs` int NOT NULL DEFAULT '0',
  `animprogress` tinyint unsigned NOT NULL DEFAULT '0',
  `state` tinyint unsigned NOT NULL DEFAULT '0',
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT '',
  `VerifiedBuild` int DEFAULT NULL,
  `Comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB AUTO_INCREMENT=5714438 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Gameobject System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject_addon`
--

DROP TABLE IF EXISTS `gameobject_addon`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject_addon` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `parent_rotation0` float NOT NULL DEFAULT '0',
  `parent_rotation1` float NOT NULL DEFAULT '0',
  `parent_rotation2` float NOT NULL DEFAULT '0',
  `parent_rotation3` float NOT NULL DEFAULT '1',
  `invisibilityType` tinyint unsigned NOT NULL DEFAULT '0',
  `invisibilityValue` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject_loot_template`
--

DROP TABLE IF EXISTS `gameobject_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject_questender`
--

DROP TABLE IF EXISTS `gameobject_questender`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject_questender` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  PRIMARY KEY (`id`,`quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject_questitem`
--

DROP TABLE IF EXISTS `gameobject_questitem`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject_questitem` (
  `GameObjectEntry` int unsigned NOT NULL DEFAULT '0',
  `Idx` int unsigned NOT NULL DEFAULT '0',
  `ItemId` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`GameObjectEntry`,`Idx`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject_queststarter`
--

DROP TABLE IF EXISTS `gameobject_queststarter`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject_queststarter` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `quest` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest Identifier',
  PRIMARY KEY (`id`,`quest`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject_summon_groups`
--

DROP TABLE IF EXISTS `gameobject_summon_groups`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject_summon_groups` (
  `summonerId` int unsigned NOT NULL DEFAULT '0',
  `summonerType` tinyint unsigned NOT NULL DEFAULT '0',
  `groupId` tinyint unsigned NOT NULL DEFAULT '0',
  `entry` int unsigned NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `rotation0` float NOT NULL DEFAULT '0',
  `rotation1` float NOT NULL DEFAULT '0',
  `rotation2` float NOT NULL DEFAULT '0',
  `rotation3` float NOT NULL DEFAULT '0',
  `respawnTime` int unsigned NOT NULL DEFAULT '120',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject_template`
--

DROP TABLE IF EXISTS `gameobject_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject_template` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `displayId` int unsigned NOT NULL DEFAULT '0',
  `name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `IconName` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `castBarCaption` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `unk1` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `size` float NOT NULL DEFAULT '1',
  `Data0` int unsigned NOT NULL DEFAULT '0',
  `Data1` int NOT NULL DEFAULT '0',
  `Data2` int unsigned NOT NULL DEFAULT '0',
  `Data3` int unsigned NOT NULL DEFAULT '0',
  `Data4` int unsigned NOT NULL DEFAULT '0',
  `Data5` int unsigned NOT NULL DEFAULT '0',
  `Data6` int NOT NULL DEFAULT '0',
  `Data7` int unsigned NOT NULL DEFAULT '0',
  `Data8` int unsigned NOT NULL DEFAULT '0',
  `Data9` int unsigned NOT NULL DEFAULT '0',
  `Data10` int unsigned NOT NULL DEFAULT '0',
  `Data11` int unsigned NOT NULL DEFAULT '0',
  `Data12` int unsigned NOT NULL DEFAULT '0',
  `Data13` int unsigned NOT NULL DEFAULT '0',
  `Data14` int unsigned NOT NULL DEFAULT '0',
  `Data15` int unsigned NOT NULL DEFAULT '0',
  `Data16` int unsigned NOT NULL DEFAULT '0',
  `Data17` int unsigned NOT NULL DEFAULT '0',
  `Data18` int unsigned NOT NULL DEFAULT '0',
  `Data19` int unsigned NOT NULL DEFAULT '0',
  `Data20` int unsigned NOT NULL DEFAULT '0',
  `Data21` int unsigned NOT NULL DEFAULT '0',
  `Data22` int unsigned NOT NULL DEFAULT '0',
  `Data23` int unsigned NOT NULL DEFAULT '0',
  `AIName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `ScriptName` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`entry`),
  KEY `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Gameobject System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject_template_addon`
--

DROP TABLE IF EXISTS `gameobject_template_addon`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject_template_addon` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `faction` smallint unsigned NOT NULL DEFAULT '0',
  `flags` int unsigned NOT NULL DEFAULT '0',
  `mingold` int unsigned NOT NULL DEFAULT '0',
  `maxgold` int unsigned NOT NULL DEFAULT '0',
  `artkit0` int NOT NULL DEFAULT '0',
  `artkit1` int NOT NULL DEFAULT '0',
  `artkit2` int NOT NULL DEFAULT '0',
  `artkit3` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobject_template_locale`
--

DROP TABLE IF EXISTS `gameobject_template_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobject_template_locale` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `castBarCaption` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`entry`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobjectartkit_dbc`
--

DROP TABLE IF EXISTS `gameobjectartkit_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobjectartkit_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Texture_1` int NOT NULL DEFAULT '0',
  `Texture_2` int NOT NULL DEFAULT '0',
  `Texture_3` int NOT NULL DEFAULT '0',
  `Attach_Model_1` int NOT NULL DEFAULT '0',
  `Attach_Model_2` int NOT NULL DEFAULT '0',
  `Attach_Model_3` int NOT NULL DEFAULT '0',
  `Attach_Model_4` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gameobjectdisplayinfo_dbc`
--

DROP TABLE IF EXISTS `gameobjectdisplayinfo_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gameobjectdisplayinfo_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `ModelName` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Sound_1` int NOT NULL DEFAULT '0',
  `Sound_2` int NOT NULL DEFAULT '0',
  `Sound_3` int NOT NULL DEFAULT '0',
  `Sound_4` int NOT NULL DEFAULT '0',
  `Sound_5` int NOT NULL DEFAULT '0',
  `Sound_6` int NOT NULL DEFAULT '0',
  `Sound_7` int NOT NULL DEFAULT '0',
  `Sound_8` int NOT NULL DEFAULT '0',
  `Sound_9` int NOT NULL DEFAULT '0',
  `Sound_10` int NOT NULL DEFAULT '0',
  `GeoBoxMinX` float NOT NULL DEFAULT '0',
  `GeoBoxMinY` float NOT NULL DEFAULT '0',
  `GeoBoxMinZ` float NOT NULL DEFAULT '0',
  `GeoBoxMaxX` float NOT NULL DEFAULT '0',
  `GeoBoxMaxY` float NOT NULL DEFAULT '0',
  `GeoBoxMaxZ` float NOT NULL DEFAULT '0',
  `ObjectEffectPackageID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gemproperties_dbc`
--

DROP TABLE IF EXISTS `gemproperties_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gemproperties_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Enchant_Id` int NOT NULL DEFAULT '0',
  `Maxcount_Inv` int NOT NULL DEFAULT '0',
  `Maxcount_Item` int NOT NULL DEFAULT '0',
  `Type` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `glyphproperties_dbc`
--

DROP TABLE IF EXISTS `glyphproperties_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `glyphproperties_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `GlyphSlotFlags` int NOT NULL DEFAULT '0',
  `SpellIconID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `glyphslot_dbc`
--

DROP TABLE IF EXISTS `glyphslot_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `glyphslot_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Type` int NOT NULL DEFAULT '0',
  `Tooltip` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gossip_menu`
--

DROP TABLE IF EXISTS `gossip_menu`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gossip_menu` (
  `MenuID` int unsigned NOT NULL DEFAULT '0',
  `TextID` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`MenuID`,`TextID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gossip_menu_option`
--

DROP TABLE IF EXISTS `gossip_menu_option`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gossip_menu_option` (
  `MenuID` int unsigned NOT NULL DEFAULT '0',
  `OptionID` smallint unsigned NOT NULL DEFAULT '0',
  `OptionIcon` int unsigned NOT NULL DEFAULT '0',
  `OptionText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `OptionBroadcastTextID` int NOT NULL DEFAULT '0',
  `OptionType` tinyint unsigned NOT NULL DEFAULT '0',
  `OptionNpcFlag` int unsigned NOT NULL DEFAULT '0',
  `ActionMenuID` int unsigned NOT NULL DEFAULT '0',
  `ActionPoiID` int unsigned NOT NULL DEFAULT '0',
  `BoxCoded` tinyint unsigned NOT NULL DEFAULT '0',
  `BoxMoney` int unsigned NOT NULL DEFAULT '0',
  `BoxText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BoxBroadcastTextID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`MenuID`,`OptionID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gossip_menu_option_locale`
--

DROP TABLE IF EXISTS `gossip_menu_option_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gossip_menu_option_locale` (
  `MenuID` int unsigned NOT NULL DEFAULT '0',
  `OptionID` smallint unsigned NOT NULL DEFAULT '0',
  `Locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `OptionText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BoxText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`MenuID`,`OptionID`,`Locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `graveyard_zone`
--

DROP TABLE IF EXISTS `graveyard_zone`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `graveyard_zone` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GhostZone` int unsigned NOT NULL DEFAULT '0',
  `Faction` smallint unsigned NOT NULL DEFAULT '0',
  `Comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`ID`,`GhostZone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Trigger System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtbarbershopcostbase_dbc`
--

DROP TABLE IF EXISTS `gtbarbershopcostbase_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtbarbershopcostbase_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtchancetomeleecrit_dbc`
--

DROP TABLE IF EXISTS `gtchancetomeleecrit_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtchancetomeleecrit_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtchancetomeleecritbase_dbc`
--

DROP TABLE IF EXISTS `gtchancetomeleecritbase_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtchancetomeleecritbase_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtchancetospellcrit_dbc`
--

DROP TABLE IF EXISTS `gtchancetospellcrit_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtchancetospellcrit_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtchancetospellcritbase_dbc`
--

DROP TABLE IF EXISTS `gtchancetospellcritbase_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtchancetospellcritbase_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtcombatratings_dbc`
--

DROP TABLE IF EXISTS `gtcombatratings_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtcombatratings_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtnpcmanacostscaler_dbc`
--

DROP TABLE IF EXISTS `gtnpcmanacostscaler_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtnpcmanacostscaler_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtoctclasscombatratingscalar_dbc`
--

DROP TABLE IF EXISTS `gtoctclasscombatratingscalar_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtoctclasscombatratingscalar_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtoctregenhp_dbc`
--

DROP TABLE IF EXISTS `gtoctregenhp_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtoctregenhp_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtregenhpperspt_dbc`
--

DROP TABLE IF EXISTS `gtregenhpperspt_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtregenhpperspt_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `gtregenmpperspt_dbc`
--

DROP TABLE IF EXISTS `gtregenmpperspt_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `gtregenmpperspt_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `holiday_dates`
--

DROP TABLE IF EXISTS `holiday_dates`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `holiday_dates` (
  `id` int unsigned NOT NULL,
  `date_id` tinyint unsigned NOT NULL,
  `date_value` int unsigned NOT NULL,
  `holiday_duration` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`,`date_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `holidays_dbc`
--

DROP TABLE IF EXISTS `holidays_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `holidays_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Duration_1` int NOT NULL DEFAULT '0',
  `Duration_2` int NOT NULL DEFAULT '0',
  `Duration_3` int NOT NULL DEFAULT '0',
  `Duration_4` int NOT NULL DEFAULT '0',
  `Duration_5` int NOT NULL DEFAULT '0',
  `Duration_6` int NOT NULL DEFAULT '0',
  `Duration_7` int NOT NULL DEFAULT '0',
  `Duration_8` int NOT NULL DEFAULT '0',
  `Duration_9` int NOT NULL DEFAULT '0',
  `Duration_10` int NOT NULL DEFAULT '0',
  `Date_1` int NOT NULL DEFAULT '0',
  `Date_2` int NOT NULL DEFAULT '0',
  `Date_3` int NOT NULL DEFAULT '0',
  `Date_4` int NOT NULL DEFAULT '0',
  `Date_5` int NOT NULL DEFAULT '0',
  `Date_6` int NOT NULL DEFAULT '0',
  `Date_7` int NOT NULL DEFAULT '0',
  `Date_8` int NOT NULL DEFAULT '0',
  `Date_9` int NOT NULL DEFAULT '0',
  `Date_10` int NOT NULL DEFAULT '0',
  `Date_11` int NOT NULL DEFAULT '0',
  `Date_12` int NOT NULL DEFAULT '0',
  `Date_13` int NOT NULL DEFAULT '0',
  `Date_14` int NOT NULL DEFAULT '0',
  `Date_15` int NOT NULL DEFAULT '0',
  `Date_16` int NOT NULL DEFAULT '0',
  `Date_17` int NOT NULL DEFAULT '0',
  `Date_18` int NOT NULL DEFAULT '0',
  `Date_19` int NOT NULL DEFAULT '0',
  `Date_20` int NOT NULL DEFAULT '0',
  `Date_21` int NOT NULL DEFAULT '0',
  `Date_22` int NOT NULL DEFAULT '0',
  `Date_23` int NOT NULL DEFAULT '0',
  `Date_24` int NOT NULL DEFAULT '0',
  `Date_25` int NOT NULL DEFAULT '0',
  `Date_26` int NOT NULL DEFAULT '0',
  `Region` int NOT NULL DEFAULT '0',
  `Looping` int NOT NULL DEFAULT '0',
  `CalendarFlags_1` int NOT NULL DEFAULT '0',
  `CalendarFlags_2` int NOT NULL DEFAULT '0',
  `CalendarFlags_3` int NOT NULL DEFAULT '0',
  `CalendarFlags_4` int NOT NULL DEFAULT '0',
  `CalendarFlags_5` int NOT NULL DEFAULT '0',
  `CalendarFlags_6` int NOT NULL DEFAULT '0',
  `CalendarFlags_7` int NOT NULL DEFAULT '0',
  `CalendarFlags_8` int NOT NULL DEFAULT '0',
  `CalendarFlags_9` int NOT NULL DEFAULT '0',
  `CalendarFlags_10` int NOT NULL DEFAULT '0',
  `HolidayNameID` int NOT NULL DEFAULT '0',
  `HolidayDescriptionID` int NOT NULL DEFAULT '0',
  `TextureFilename` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Priority` int NOT NULL DEFAULT '0',
  `CalendarFilterType` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `instance_encounters`
--

DROP TABLE IF EXISTS `instance_encounters`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `instance_encounters` (
  `entry` int unsigned NOT NULL COMMENT 'Unique entry from DungeonEncounter.dbc',
  `creditType` tinyint unsigned NOT NULL DEFAULT '0',
  `creditEntry` int unsigned NOT NULL DEFAULT '0',
  `lastEncounterDungeon` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'If not 0, LfgDungeon.dbc entry for the instance it is last encounter in',
  `comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `instance_template`
--

DROP TABLE IF EXISTS `instance_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `instance_template` (
  `map` smallint unsigned NOT NULL,
  `parent` smallint unsigned NOT NULL,
  `script` varchar(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `allowMount` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`map`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_dbc`
--

DROP TABLE IF EXISTS `item_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `ClassID` int NOT NULL DEFAULT '0',
  `SubclassID` int NOT NULL DEFAULT '0',
  `Sound_Override_Subclassid` int NOT NULL DEFAULT '0',
  `Material` int NOT NULL DEFAULT '0',
  `DisplayInfoID` int NOT NULL DEFAULT '0',
  `InventoryType` int NOT NULL DEFAULT '0',
  `SheatheType` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_enchantment_template`
--

DROP TABLE IF EXISTS `item_enchantment_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_enchantment_template` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `ench` int unsigned NOT NULL DEFAULT '0',
  `chance` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`entry`,`ench`),
  CONSTRAINT `item_enchantment_template_chk_1` CHECK ((`chance` >= 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Item Random Enchantment System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_loot_template`
--

DROP TABLE IF EXISTS `item_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_set_names`
--

DROP TABLE IF EXISTS `item_set_names`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_set_names` (
  `entry` int unsigned NOT NULL,
  `name` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `InventoryType` tinyint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_set_names_locale`
--

DROP TABLE IF EXISTS `item_set_names_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_set_names_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`,`locale`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_template`
--

DROP TABLE IF EXISTS `item_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_template` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `class` tinyint unsigned NOT NULL DEFAULT '0',
  `subclass` tinyint unsigned NOT NULL DEFAULT '0',
  `SoundOverrideSubclass` tinyint NOT NULL DEFAULT '-1',
  `name` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `displayid` int unsigned NOT NULL DEFAULT '0',
  `Quality` tinyint unsigned NOT NULL DEFAULT '0',
  `Flags` int unsigned NOT NULL DEFAULT '0',
  `FlagsExtra` int unsigned NOT NULL DEFAULT '0',
  `BuyCount` tinyint unsigned NOT NULL DEFAULT '1',
  `BuyPrice` bigint NOT NULL DEFAULT '0',
  `SellPrice` int unsigned NOT NULL DEFAULT '0',
  `InventoryType` tinyint unsigned NOT NULL DEFAULT '0',
  `AllowableClass` int NOT NULL DEFAULT '-1',
  `AllowableRace` int NOT NULL DEFAULT '-1',
  `ItemLevel` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredLevel` tinyint unsigned NOT NULL DEFAULT '0',
  `RequiredSkill` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredSkillRank` smallint unsigned NOT NULL DEFAULT '0',
  `requiredspell` int unsigned NOT NULL DEFAULT '0',
  `requiredhonorrank` int unsigned NOT NULL DEFAULT '0',
  `RequiredCityRank` int unsigned NOT NULL DEFAULT '0',
  `RequiredReputationFaction` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredReputationRank` smallint unsigned NOT NULL DEFAULT '0',
  `maxcount` int NOT NULL DEFAULT '0',
  `stackable` int DEFAULT '1',
  `ContainerSlots` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_type1` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_value1` int NOT NULL DEFAULT '0',
  `stat_type2` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_value2` int NOT NULL DEFAULT '0',
  `stat_type3` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_value3` int NOT NULL DEFAULT '0',
  `stat_type4` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_value4` int NOT NULL DEFAULT '0',
  `stat_type5` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_value5` int NOT NULL DEFAULT '0',
  `stat_type6` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_value6` int NOT NULL DEFAULT '0',
  `stat_type7` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_value7` int NOT NULL DEFAULT '0',
  `stat_type8` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_value8` int NOT NULL DEFAULT '0',
  `stat_type9` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_value9` int NOT NULL DEFAULT '0',
  `stat_type10` tinyint unsigned NOT NULL DEFAULT '0',
  `stat_value10` int NOT NULL DEFAULT '0',
  `ScalingStatDistribution` smallint NOT NULL DEFAULT '0',
  `ScalingStatValue` int unsigned NOT NULL DEFAULT '0',
  `dmg_min1` float NOT NULL DEFAULT '0',
  `dmg_max1` float NOT NULL DEFAULT '0',
  `dmg_type1` tinyint unsigned NOT NULL DEFAULT '0',
  `dmg_min2` float NOT NULL DEFAULT '0',
  `dmg_max2` float NOT NULL DEFAULT '0',
  `dmg_type2` tinyint unsigned NOT NULL DEFAULT '0',
  `armor` int unsigned NOT NULL DEFAULT '0',
  `holy_res` smallint DEFAULT NULL,
  `fire_res` smallint DEFAULT NULL,
  `nature_res` smallint DEFAULT NULL,
  `frost_res` smallint DEFAULT NULL,
  `shadow_res` smallint DEFAULT NULL,
  `arcane_res` smallint DEFAULT NULL,
  `delay` smallint unsigned NOT NULL DEFAULT '1000',
  `ammo_type` tinyint unsigned NOT NULL DEFAULT '0',
  `RangedModRange` float NOT NULL DEFAULT '0',
  `spellid_1` int NOT NULL DEFAULT '0',
  `spelltrigger_1` tinyint unsigned NOT NULL DEFAULT '0',
  `spellcharges_1` smallint NOT NULL DEFAULT '0',
  `spellppmRate_1` float NOT NULL DEFAULT '0',
  `spellcooldown_1` int NOT NULL DEFAULT '-1',
  `spellcategory_1` smallint unsigned NOT NULL DEFAULT '0',
  `spellcategorycooldown_1` int NOT NULL DEFAULT '-1',
  `spellid_2` int NOT NULL DEFAULT '0',
  `spelltrigger_2` tinyint unsigned NOT NULL DEFAULT '0',
  `spellcharges_2` smallint NOT NULL DEFAULT '0',
  `spellppmRate_2` float NOT NULL DEFAULT '0',
  `spellcooldown_2` int NOT NULL DEFAULT '-1',
  `spellcategory_2` smallint unsigned NOT NULL DEFAULT '0',
  `spellcategorycooldown_2` int NOT NULL DEFAULT '-1',
  `spellid_3` int NOT NULL DEFAULT '0',
  `spelltrigger_3` tinyint unsigned NOT NULL DEFAULT '0',
  `spellcharges_3` smallint NOT NULL DEFAULT '0',
  `spellppmRate_3` float NOT NULL DEFAULT '0',
  `spellcooldown_3` int NOT NULL DEFAULT '-1',
  `spellcategory_3` smallint unsigned NOT NULL DEFAULT '0',
  `spellcategorycooldown_3` int NOT NULL DEFAULT '-1',
  `spellid_4` int NOT NULL DEFAULT '0',
  `spelltrigger_4` tinyint unsigned NOT NULL DEFAULT '0',
  `spellcharges_4` smallint NOT NULL DEFAULT '0',
  `spellppmRate_4` float NOT NULL DEFAULT '0',
  `spellcooldown_4` int NOT NULL DEFAULT '-1',
  `spellcategory_4` smallint unsigned NOT NULL DEFAULT '0',
  `spellcategorycooldown_4` int NOT NULL DEFAULT '-1',
  `spellid_5` int NOT NULL DEFAULT '0',
  `spelltrigger_5` tinyint unsigned NOT NULL DEFAULT '0',
  `spellcharges_5` smallint NOT NULL DEFAULT '0',
  `spellppmRate_5` float NOT NULL DEFAULT '0',
  `spellcooldown_5` int NOT NULL DEFAULT '-1',
  `spellcategory_5` smallint unsigned NOT NULL DEFAULT '0',
  `spellcategorycooldown_5` int NOT NULL DEFAULT '-1',
  `bonding` tinyint unsigned NOT NULL DEFAULT '0',
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `PageText` int unsigned NOT NULL DEFAULT '0',
  `LanguageID` tinyint unsigned NOT NULL DEFAULT '0',
  `PageMaterial` tinyint unsigned NOT NULL DEFAULT '0',
  `startquest` int unsigned NOT NULL DEFAULT '0',
  `lockid` int unsigned NOT NULL DEFAULT '0',
  `Material` tinyint NOT NULL DEFAULT '0',
  `sheath` tinyint unsigned NOT NULL DEFAULT '0',
  `RandomProperty` int NOT NULL DEFAULT '0',
  `RandomSuffix` int unsigned NOT NULL DEFAULT '0',
  `block` int unsigned NOT NULL DEFAULT '0',
  `itemset` int unsigned NOT NULL DEFAULT '0',
  `MaxDurability` smallint unsigned NOT NULL DEFAULT '0',
  `area` int unsigned NOT NULL DEFAULT '0',
  `Map` smallint NOT NULL DEFAULT '0',
  `BagFamily` int NOT NULL DEFAULT '0',
  `TotemCategory` int NOT NULL DEFAULT '0',
  `socketColor_1` tinyint NOT NULL DEFAULT '0',
  `socketContent_1` int NOT NULL DEFAULT '0',
  `socketColor_2` tinyint NOT NULL DEFAULT '0',
  `socketContent_2` int NOT NULL DEFAULT '0',
  `socketColor_3` tinyint NOT NULL DEFAULT '0',
  `socketContent_3` int NOT NULL DEFAULT '0',
  `socketBonus` int NOT NULL DEFAULT '0',
  `GemProperties` int NOT NULL DEFAULT '0',
  `RequiredDisenchantSkill` smallint NOT NULL DEFAULT '-1',
  `ArmorDamageModifier` float NOT NULL DEFAULT '0',
  `duration` int unsigned NOT NULL DEFAULT '0',
  `ItemLimitCategory` smallint NOT NULL DEFAULT '0',
  `HolidayId` int unsigned NOT NULL DEFAULT '0',
  `ScriptName` varchar(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `DisenchantID` int unsigned NOT NULL DEFAULT '0',
  `FoodType` tinyint unsigned NOT NULL DEFAULT '0',
  `minMoneyLoot` int unsigned NOT NULL DEFAULT '0',
  `maxMoneyLoot` int unsigned NOT NULL DEFAULT '0',
  `flagsCustom` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`entry`),
  KEY `idx_name` (`name`(250)),
  KEY `items_index` (`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Item System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `item_template_locale`
--

DROP TABLE IF EXISTS `item_template_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_template_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `itembagfamily_dbc`
--

DROP TABLE IF EXISTS `itembagfamily_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `itembagfamily_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `itemdisplayinfo_dbc`
--

DROP TABLE IF EXISTS `itemdisplayinfo_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `itemdisplayinfo_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `ModelName_1` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `ModelName_2` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `ModelTexture_1` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `ModelTexture_2` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `InventoryIcon_1` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `InventoryIcon_2` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `GeosetGroup_1` int NOT NULL DEFAULT '0',
  `GeosetGroup_2` int NOT NULL DEFAULT '0',
  `GeosetGroup_3` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `SpellVisualID` int NOT NULL DEFAULT '0',
  `GroupSoundIndex` int NOT NULL DEFAULT '0',
  `HelmetGeosetVis_1` int NOT NULL DEFAULT '0',
  `HelmetGeosetVis_2` int NOT NULL DEFAULT '0',
  `Texture_1` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_2` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_3` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_4` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_5` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_6` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_7` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_8` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `ItemVisual` int NOT NULL DEFAULT '0',
  `ParticleColorID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `itemextendedcost_dbc`
--

DROP TABLE IF EXISTS `itemextendedcost_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `itemextendedcost_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `HonorPoints` int NOT NULL DEFAULT '0',
  `ArenaPoints` int NOT NULL DEFAULT '0',
  `ArenaBracket` int NOT NULL DEFAULT '0',
  `ItemID_1` int NOT NULL DEFAULT '0',
  `ItemID_2` int NOT NULL DEFAULT '0',
  `ItemID_3` int NOT NULL DEFAULT '0',
  `ItemID_4` int NOT NULL DEFAULT '0',
  `ItemID_5` int NOT NULL DEFAULT '0',
  `ItemCount_1` int NOT NULL DEFAULT '0',
  `ItemCount_2` int NOT NULL DEFAULT '0',
  `ItemCount_3` int NOT NULL DEFAULT '0',
  `ItemCount_4` int NOT NULL DEFAULT '0',
  `ItemCount_5` int NOT NULL DEFAULT '0',
  `RequiredArenaRating` int NOT NULL DEFAULT '0',
  `ItemPurchaseGroup` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `itemlimitcategory_dbc`
--

DROP TABLE IF EXISTS `itemlimitcategory_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `itemlimitcategory_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Quantity` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `itemrandomproperties_dbc`
--

DROP TABLE IF EXISTS `itemrandomproperties_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `itemrandomproperties_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Enchantment_1` int NOT NULL DEFAULT '0',
  `Enchantment_2` int NOT NULL DEFAULT '0',
  `Enchantment_3` int NOT NULL DEFAULT '0',
  `Enchantment_4` int NOT NULL DEFAULT '0',
  `Enchantment_5` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `itemrandomsuffix_dbc`
--

DROP TABLE IF EXISTS `itemrandomsuffix_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `itemrandomsuffix_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `InternalName` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Enchantment_1` int NOT NULL DEFAULT '0',
  `Enchantment_2` int NOT NULL DEFAULT '0',
  `Enchantment_3` int NOT NULL DEFAULT '0',
  `Enchantment_4` int NOT NULL DEFAULT '0',
  `Enchantment_5` int NOT NULL DEFAULT '0',
  `AllocationPct_1` int NOT NULL DEFAULT '0',
  `AllocationPct_2` int NOT NULL DEFAULT '0',
  `AllocationPct_3` int NOT NULL DEFAULT '0',
  `AllocationPct_4` int NOT NULL DEFAULT '0',
  `AllocationPct_5` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `itemset_dbc`
--

DROP TABLE IF EXISTS `itemset_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `itemset_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `ItemID_1` int NOT NULL DEFAULT '0',
  `ItemID_2` int NOT NULL DEFAULT '0',
  `ItemID_3` int NOT NULL DEFAULT '0',
  `ItemID_4` int NOT NULL DEFAULT '0',
  `ItemID_5` int NOT NULL DEFAULT '0',
  `ItemID_6` int NOT NULL DEFAULT '0',
  `ItemID_7` int NOT NULL DEFAULT '0',
  `ItemID_8` int NOT NULL DEFAULT '0',
  `ItemID_9` int NOT NULL DEFAULT '0',
  `ItemID_10` int NOT NULL DEFAULT '0',
  `ItemID_11` int NOT NULL DEFAULT '0',
  `ItemID_12` int NOT NULL DEFAULT '0',
  `ItemID_13` int NOT NULL DEFAULT '0',
  `ItemID_14` int NOT NULL DEFAULT '0',
  `ItemID_15` int NOT NULL DEFAULT '0',
  `ItemID_16` int NOT NULL DEFAULT '0',
  `ItemID_17` int NOT NULL DEFAULT '0',
  `SetSpellID_1` int NOT NULL DEFAULT '0',
  `SetSpellID_2` int NOT NULL DEFAULT '0',
  `SetSpellID_3` int NOT NULL DEFAULT '0',
  `SetSpellID_4` int NOT NULL DEFAULT '0',
  `SetSpellID_5` int NOT NULL DEFAULT '0',
  `SetSpellID_6` int NOT NULL DEFAULT '0',
  `SetSpellID_7` int NOT NULL DEFAULT '0',
  `SetSpellID_8` int NOT NULL DEFAULT '0',
  `SetThreshold_1` int NOT NULL DEFAULT '0',
  `SetThreshold_2` int NOT NULL DEFAULT '0',
  `SetThreshold_3` int NOT NULL DEFAULT '0',
  `SetThreshold_4` int NOT NULL DEFAULT '0',
  `SetThreshold_5` int NOT NULL DEFAULT '0',
  `SetThreshold_6` int NOT NULL DEFAULT '0',
  `SetThreshold_7` int NOT NULL DEFAULT '0',
  `SetThreshold_8` int NOT NULL DEFAULT '0',
  `RequiredSkill` int NOT NULL DEFAULT '0',
  `RequiredSkillRank` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `lfg_dungeon_rewards`
--

DROP TABLE IF EXISTS `lfg_dungeon_rewards`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `lfg_dungeon_rewards` (
  `dungeonId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Dungeon entry from dbc',
  `maxLevel` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Max level at which this reward is rewarded',
  `firstQuestId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest id with rewards for first dungeon this day',
  `otherQuestId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Quest id with rewards for Nth dungeon this day',
  PRIMARY KEY (`dungeonId`,`maxLevel`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `lfg_dungeon_template`
--

DROP TABLE IF EXISTS `lfg_dungeon_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `lfg_dungeon_template` (
  `dungeonId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unique id from LFGDungeons.dbc',
  `name` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`dungeonId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `lfgdungeons_dbc`
--

DROP TABLE IF EXISTS `lfgdungeons_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `lfgdungeons_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_enGB` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_koKR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_frFR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_deDE` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_enCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_zhCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_enTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_zhTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_esES` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_esMX` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_ruRU` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_ptPT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_ptBR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_itIT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_Unk` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `MinLevel` int NOT NULL DEFAULT '0',
  `MaxLevel` int NOT NULL DEFAULT '0',
  `Target_Level` int NOT NULL DEFAULT '0',
  `Target_Level_Min` int NOT NULL DEFAULT '0',
  `Target_Level_Max` int NOT NULL DEFAULT '0',
  `MapID` int NOT NULL DEFAULT '0',
  `Difficulty` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `TypeID` int NOT NULL DEFAULT '0',
  `Faction` int NOT NULL DEFAULT '0',
  `TextureFilename` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ExpansionLevel` int NOT NULL DEFAULT '0',
  `Order_Index` int NOT NULL DEFAULT '0',
  `Group_Id` int NOT NULL DEFAULT '0',
  `Description_Lang_enUS` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_enGB` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_koKR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_frFR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_deDE` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_enCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_zhCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_enTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_zhTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_esES` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_esMX` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_ruRU` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_ptPT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_ptBR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_itIT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_Unk` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `light_dbc`
--

DROP TABLE IF EXISTS `light_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `light_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `ContinentID` int NOT NULL DEFAULT '0',
  `X` float NOT NULL DEFAULT '0',
  `Y` float NOT NULL DEFAULT '0',
  `Z` float NOT NULL DEFAULT '0',
  `FalloffStart` float NOT NULL DEFAULT '0',
  `FalloffEnd` float NOT NULL DEFAULT '0',
  `LightParamsID_1` int NOT NULL DEFAULT '0',
  `LightParamsID_2` int NOT NULL DEFAULT '0',
  `LightParamsID_3` int NOT NULL DEFAULT '0',
  `LightParamsID_4` int NOT NULL DEFAULT '0',
  `LightParamsID_5` int NOT NULL DEFAULT '0',
  `LightParamsID_6` int NOT NULL DEFAULT '0',
  `LightParamsID_7` int NOT NULL DEFAULT '0',
  `LightParamsID_8` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `linked_respawn`
--

DROP TABLE IF EXISTS `linked_respawn`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `linked_respawn` (
  `guid` int unsigned NOT NULL COMMENT 'dependent creature',
  `linkedGuid` int unsigned NOT NULL COMMENT 'master creature',
  `linkType` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`linkType`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Creature Respawn Link System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `liquidtype_dbc`
--

DROP TABLE IF EXISTS `liquidtype_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `liquidtype_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Flags` int NOT NULL DEFAULT '0',
  `Type` int NOT NULL DEFAULT '0',
  `SoundID` int NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `MaxDarkenDepth` float NOT NULL DEFAULT '0',
  `FogDarkenintensity` float NOT NULL DEFAULT '0',
  `AmbDarkenintensity` float NOT NULL DEFAULT '0',
  `DirDarkenintensity` float NOT NULL DEFAULT '0',
  `LightID` int NOT NULL DEFAULT '0',
  `ParticleScale` float NOT NULL DEFAULT '0',
  `ParticleMovement` int NOT NULL DEFAULT '0',
  `ParticleTexSlots` int NOT NULL DEFAULT '0',
  `MaterialID` int NOT NULL DEFAULT '0',
  `Texture_1` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_2` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_3` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_4` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_5` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Texture_6` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Color_1` int NOT NULL DEFAULT '0',
  `Color_2` int NOT NULL DEFAULT '0',
  `Float_1` float NOT NULL DEFAULT '0',
  `Float_2` float NOT NULL DEFAULT '0',
  `Float_3` float NOT NULL DEFAULT '0',
  `Float_4` float NOT NULL DEFAULT '0',
  `Float_5` float NOT NULL DEFAULT '0',
  `Float_6` float NOT NULL DEFAULT '0',
  `Float_7` float NOT NULL DEFAULT '0',
  `Float_8` float NOT NULL DEFAULT '0',
  `Float_9` float NOT NULL DEFAULT '0',
  `Float_10` float NOT NULL DEFAULT '0',
  `Float_11` float NOT NULL DEFAULT '0',
  `Float_12` float NOT NULL DEFAULT '0',
  `Float_13` float NOT NULL DEFAULT '0',
  `Float_14` float NOT NULL DEFAULT '0',
  `Float_15` float NOT NULL DEFAULT '0',
  `Float_16` float NOT NULL DEFAULT '0',
  `Float_17` float NOT NULL DEFAULT '0',
  `Float_18` float NOT NULL DEFAULT '0',
  `Int_1` int NOT NULL DEFAULT '0',
  `Int_2` int NOT NULL DEFAULT '0',
  `Int_3` int NOT NULL DEFAULT '0',
  `Int_4` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `lock_dbc`
--

DROP TABLE IF EXISTS `lock_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `lock_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Type_1` int NOT NULL DEFAULT '0',
  `Type_2` int NOT NULL DEFAULT '0',
  `Type_3` int NOT NULL DEFAULT '0',
  `Type_4` int NOT NULL DEFAULT '0',
  `Type_5` int NOT NULL DEFAULT '0',
  `Type_6` int NOT NULL DEFAULT '0',
  `Type_7` int NOT NULL DEFAULT '0',
  `Type_8` int NOT NULL DEFAULT '0',
  `Index_1` int NOT NULL DEFAULT '0',
  `Index_2` int NOT NULL DEFAULT '0',
  `Index_3` int NOT NULL DEFAULT '0',
  `Index_4` int NOT NULL DEFAULT '0',
  `Index_5` int NOT NULL DEFAULT '0',
  `Index_6` int NOT NULL DEFAULT '0',
  `Index_7` int NOT NULL DEFAULT '0',
  `Index_8` int NOT NULL DEFAULT '0',
  `Skill_1` int NOT NULL DEFAULT '0',
  `Skill_2` int NOT NULL DEFAULT '0',
  `Skill_3` int NOT NULL DEFAULT '0',
  `Skill_4` int NOT NULL DEFAULT '0',
  `Skill_5` int NOT NULL DEFAULT '0',
  `Skill_6` int NOT NULL DEFAULT '0',
  `Skill_7` int NOT NULL DEFAULT '0',
  `Skill_8` int NOT NULL DEFAULT '0',
  `Action_1` int NOT NULL DEFAULT '0',
  `Action_2` int NOT NULL DEFAULT '0',
  `Action_3` int NOT NULL DEFAULT '0',
  `Action_4` int NOT NULL DEFAULT '0',
  `Action_5` int NOT NULL DEFAULT '0',
  `Action_6` int NOT NULL DEFAULT '0',
  `Action_7` int NOT NULL DEFAULT '0',
  `Action_8` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mail_level_reward`
--

DROP TABLE IF EXISTS `mail_level_reward`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `mail_level_reward` (
  `level` tinyint unsigned NOT NULL DEFAULT '0',
  `raceMask` int unsigned NOT NULL DEFAULT '0',
  `mailTemplateId` int unsigned NOT NULL DEFAULT '0',
  `senderEntry` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`level`,`raceMask`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Mail System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mail_loot_template`
--

DROP TABLE IF EXISTS `mail_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `mail_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mailtemplate_dbc`
--

DROP TABLE IF EXISTS `mailtemplate_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `mailtemplate_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Subject_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Subject_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Body_Lang_enUS` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_enGB` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_koKR` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_frFR` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_deDE` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_enCN` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_zhCN` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_enTW` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_zhTW` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_esES` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_esMX` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_ruRU` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_ptPT` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_ptBR` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_itIT` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Body_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `map_dbc`
--

DROP TABLE IF EXISTS `map_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `map_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Directory` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `InstanceType` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `PVP` int NOT NULL DEFAULT '0',
  `MapName_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapName_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `AreaTableID` int NOT NULL DEFAULT '0',
  `MapDescription0_Lang_enUS` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_enGB` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_koKR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_frFR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_deDE` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_enCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_zhCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_enTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_zhTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_esES` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_esMX` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_ruRU` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_ptPT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_ptBR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_itIT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription0_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapDescription0_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `MapDescription1_Lang_enUS` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_enGB` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_koKR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_frFR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_deDE` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_enCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_zhCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_enTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_zhTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_esES` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_esMX` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_ruRU` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_ptPT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_ptBR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_itIT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `MapDescription1_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MapDescription1_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `LoadingScreenID` int NOT NULL DEFAULT '0',
  `MinimapIconScale` float NOT NULL DEFAULT '0',
  `CorpseMapID` int NOT NULL DEFAULT '0',
  `CorpseX` float NOT NULL DEFAULT '0',
  `CorpseY` float NOT NULL DEFAULT '0',
  `TimeOfDayOverride` int NOT NULL DEFAULT '0',
  `ExpansionID` int NOT NULL DEFAULT '0',
  `RaidOffset` int NOT NULL DEFAULT '0',
  `MaxPlayers` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mapdifficulty_dbc`
--

DROP TABLE IF EXISTS `mapdifficulty_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `mapdifficulty_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `MapID` int NOT NULL DEFAULT '0',
  `Difficulty` int NOT NULL DEFAULT '0',
  `Message_Lang_enUS` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_enGB` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_koKR` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_frFR` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_deDE` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_enCN` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_zhCN` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_enTW` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_zhTW` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_esES` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_esMX` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_ruRU` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_ptPT` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_ptBR` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_itIT` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Message_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `RaidDuration` int NOT NULL DEFAULT '0',
  `MaxPlayers` int NOT NULL DEFAULT '0',
  `Difficultystring` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `milling_loot_template`
--

DROP TABLE IF EXISTS `milling_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `milling_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `module_string`
--

DROP TABLE IF EXISTS `module_string`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `module_string` (
  `module` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'module dir name, eg mod-cfbg',
  `id` int unsigned NOT NULL,
  `string` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`module`,`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `module_string_locale`
--

DROP TABLE IF EXISTS `module_string_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `module_string_locale` (
  `module` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Corresponds to an existing entry in module_string',
  `id` int unsigned NOT NULL COMMENT 'Corresponds to an existing entry in module_string',
  `locale` enum('koKR','frFR','deDE','zhCN','zhTW','esES','esMX','ruRU') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `string` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`module`,`id`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `movie_dbc`
--

DROP TABLE IF EXISTS `movie_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `movie_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Filename` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Volume` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `namesprofanity_dbc`
--

DROP TABLE IF EXISTS `namesprofanity_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `namesprofanity_dbc` (
  `ID` int unsigned NOT NULL,
  `Pattern` tinytext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `LanguagueID` tinyint NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `namesreserved_dbc`
--

DROP TABLE IF EXISTS `namesreserved_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `namesreserved_dbc` (
  `ID` int unsigned NOT NULL,
  `Pattern` tinytext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `LanguagueID` tinyint NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `npc_spellclick_spells`
--

DROP TABLE IF EXISTS `npc_spellclick_spells`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `npc_spellclick_spells` (
  `npc_entry` int unsigned NOT NULL COMMENT 'reference to creature_template',
  `spell_id` int unsigned NOT NULL COMMENT 'spell which should be casted ',
  `cast_flags` tinyint unsigned NOT NULL COMMENT 'first bit defines caster: 1=player, 0=creature; second bit defines target, same mapping as caster bit',
  `user_type` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'relation with summoner: 0-no 1-friendly 2-raid 3-party player can click',
  PRIMARY KEY (`npc_entry`,`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `npc_text`
--

DROP TABLE IF EXISTS `npc_text`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `npc_text` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `text0_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text0_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID0` int NOT NULL DEFAULT '0',
  `lang0` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability0` float NOT NULL DEFAULT '0',
  `em0_0` smallint unsigned NOT NULL DEFAULT '0',
  `em0_1` smallint unsigned NOT NULL DEFAULT '0',
  `em0_2` smallint unsigned NOT NULL DEFAULT '0',
  `em0_3` smallint unsigned NOT NULL DEFAULT '0',
  `em0_4` smallint unsigned NOT NULL DEFAULT '0',
  `em0_5` smallint unsigned NOT NULL DEFAULT '0',
  `text1_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text1_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID1` int NOT NULL DEFAULT '0',
  `lang1` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability1` float NOT NULL DEFAULT '0',
  `em1_0` smallint unsigned NOT NULL DEFAULT '0',
  `em1_1` smallint unsigned NOT NULL DEFAULT '0',
  `em1_2` smallint unsigned NOT NULL DEFAULT '0',
  `em1_3` smallint unsigned NOT NULL DEFAULT '0',
  `em1_4` smallint unsigned NOT NULL DEFAULT '0',
  `em1_5` smallint unsigned NOT NULL DEFAULT '0',
  `text2_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text2_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID2` int NOT NULL DEFAULT '0',
  `lang2` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability2` float NOT NULL DEFAULT '0',
  `em2_0` smallint unsigned NOT NULL DEFAULT '0',
  `em2_1` smallint unsigned NOT NULL DEFAULT '0',
  `em2_2` smallint unsigned NOT NULL DEFAULT '0',
  `em2_3` smallint unsigned NOT NULL DEFAULT '0',
  `em2_4` smallint unsigned NOT NULL DEFAULT '0',
  `em2_5` smallint unsigned NOT NULL DEFAULT '0',
  `text3_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text3_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID3` int NOT NULL DEFAULT '0',
  `lang3` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability3` float NOT NULL DEFAULT '0',
  `em3_0` smallint unsigned NOT NULL DEFAULT '0',
  `em3_1` smallint unsigned NOT NULL DEFAULT '0',
  `em3_2` smallint unsigned NOT NULL DEFAULT '0',
  `em3_3` smallint unsigned NOT NULL DEFAULT '0',
  `em3_4` smallint unsigned NOT NULL DEFAULT '0',
  `em3_5` smallint unsigned NOT NULL DEFAULT '0',
  `text4_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text4_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID4` int NOT NULL DEFAULT '0',
  `lang4` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability4` float NOT NULL DEFAULT '0',
  `em4_0` smallint unsigned NOT NULL DEFAULT '0',
  `em4_1` smallint unsigned NOT NULL DEFAULT '0',
  `em4_2` smallint unsigned NOT NULL DEFAULT '0',
  `em4_3` smallint unsigned NOT NULL DEFAULT '0',
  `em4_4` smallint unsigned NOT NULL DEFAULT '0',
  `em4_5` smallint unsigned NOT NULL DEFAULT '0',
  `text5_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text5_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID5` int NOT NULL DEFAULT '0',
  `lang5` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability5` float NOT NULL DEFAULT '0',
  `em5_0` smallint unsigned NOT NULL DEFAULT '0',
  `em5_1` smallint unsigned NOT NULL DEFAULT '0',
  `em5_2` smallint unsigned NOT NULL DEFAULT '0',
  `em5_3` smallint unsigned NOT NULL DEFAULT '0',
  `em5_4` smallint unsigned NOT NULL DEFAULT '0',
  `em5_5` smallint unsigned NOT NULL DEFAULT '0',
  `text6_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text6_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID6` int NOT NULL DEFAULT '0',
  `lang6` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability6` float NOT NULL DEFAULT '0',
  `em6_0` smallint unsigned NOT NULL DEFAULT '0',
  `em6_1` smallint unsigned NOT NULL DEFAULT '0',
  `em6_2` smallint unsigned NOT NULL DEFAULT '0',
  `em6_3` smallint unsigned NOT NULL DEFAULT '0',
  `em6_4` smallint unsigned NOT NULL DEFAULT '0',
  `em6_5` smallint unsigned NOT NULL DEFAULT '0',
  `text7_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `text7_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `BroadcastTextID7` int NOT NULL DEFAULT '0',
  `lang7` tinyint unsigned NOT NULL DEFAULT '0',
  `Probability7` float NOT NULL DEFAULT '0',
  `em7_0` smallint unsigned NOT NULL DEFAULT '0',
  `em7_1` smallint unsigned NOT NULL DEFAULT '0',
  `em7_2` smallint unsigned NOT NULL DEFAULT '0',
  `em7_3` smallint unsigned NOT NULL DEFAULT '0',
  `em7_4` smallint unsigned NOT NULL DEFAULT '0',
  `em7_5` smallint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `npc_text_locale`
--

DROP TABLE IF EXISTS `npc_text_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `npc_text_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Text0_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text0_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text1_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text1_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text2_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text2_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text3_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text3_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text4_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text4_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text5_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text5_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text6_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text6_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text7_0` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Text7_1` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`ID`,`Locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `npc_trainer`
--

DROP TABLE IF EXISTS `npc_trainer`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `npc_trainer` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `MoneyCost` int unsigned NOT NULL DEFAULT '0',
  `ReqSkillLine` smallint unsigned NOT NULL DEFAULT '0',
  `ReqSkillRank` smallint unsigned NOT NULL DEFAULT '0',
  `ReqLevel` tinyint unsigned NOT NULL DEFAULT '0',
  `ReqSpell` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`SpellID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `npc_vendor`
--

DROP TABLE IF EXISTS `npc_vendor`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `npc_vendor` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `slot` smallint NOT NULL DEFAULT '0',
  `item` int NOT NULL DEFAULT '0',
  `maxcount` int unsigned NOT NULL DEFAULT '0',
  `incrtime` int unsigned NOT NULL DEFAULT '0',
  `ExtendedCost` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`entry`,`item`,`ExtendedCost`),
  KEY `slot` (`slot`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Npc System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `outdoorpvp_template`
--

DROP TABLE IF EXISTS `outdoorpvp_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `outdoorpvp_template` (
  `TypeId` tinyint unsigned NOT NULL,
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`TypeId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='OutdoorPvP Templates';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `overridespelldata_dbc`
--

DROP TABLE IF EXISTS `overridespelldata_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `overridespelldata_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Spells_1` int NOT NULL DEFAULT '0',
  `Spells_2` int NOT NULL DEFAULT '0',
  `Spells_3` int NOT NULL DEFAULT '0',
  `Spells_4` int NOT NULL DEFAULT '0',
  `Spells_5` int NOT NULL DEFAULT '0',
  `Spells_6` int NOT NULL DEFAULT '0',
  `Spells_7` int NOT NULL DEFAULT '0',
  `Spells_8` int NOT NULL DEFAULT '0',
  `Spells_9` int NOT NULL DEFAULT '0',
  `Spells_10` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `page_text`
--

DROP TABLE IF EXISTS `page_text`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `page_text` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Text` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `NextPageID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Item System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `page_text_locale`
--

DROP TABLE IF EXISTS `page_text_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `page_text_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Text` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pet_levelstats`
--

DROP TABLE IF EXISTS `pet_levelstats`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pet_levelstats` (
  `creature_entry` int unsigned NOT NULL,
  `level` tinyint unsigned NOT NULL,
  `hp` int unsigned NOT NULL DEFAULT '0',
  `mana` int unsigned NOT NULL DEFAULT '0',
  `armor` int unsigned NOT NULL DEFAULT '0',
  `str` int unsigned NOT NULL DEFAULT '0',
  `agi` int unsigned NOT NULL DEFAULT '0',
  `sta` int unsigned NOT NULL DEFAULT '0',
  `inte` int unsigned NOT NULL DEFAULT '0',
  `spi` int unsigned NOT NULL DEFAULT '0',
  `min_dmg` int unsigned NOT NULL DEFAULT '0',
  `max_dmg` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`creature_entry`,`level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci PACK_KEYS=0 COMMENT='Stores pet levels stats.';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pet_name_generation`
--

DROP TABLE IF EXISTS `pet_name_generation`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pet_name_generation` (
  `id` int unsigned NOT NULL,
  `word` tinytext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `entry` int unsigned NOT NULL DEFAULT '0',
  `half` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pet_name_generation_locale`
--

DROP TABLE IF EXISTS `pet_name_generation_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pet_name_generation_locale` (
  `ID` int unsigned NOT NULL,
  `Locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Word` tinytext CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Half` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`Locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pickpocketing_loot_template`
--

DROP TABLE IF EXISTS `pickpocketing_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pickpocketing_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_class_stats`
--

DROP TABLE IF EXISTS `player_class_stats`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_class_stats` (
  `Class` tinyint unsigned NOT NULL,
  `Level` tinyint unsigned NOT NULL,
  `BaseHP` int unsigned NOT NULL DEFAULT '1',
  `BaseMana` int unsigned NOT NULL DEFAULT '1',
  `Strength` int unsigned NOT NULL DEFAULT '0',
  `Agility` int unsigned NOT NULL DEFAULT '0',
  `Stamina` int unsigned NOT NULL DEFAULT '0',
  `Intellect` int unsigned NOT NULL DEFAULT '0',
  `Spirit` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`Class`,`Level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci PACK_KEYS=0 COMMENT='Stores levels stats.';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_factionchange_achievement`
--

DROP TABLE IF EXISTS `player_factionchange_achievement`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_factionchange_achievement` (
  `alliance_id` int unsigned NOT NULL,
  `alliance_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `horde_id` int unsigned NOT NULL,
  `horde_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`alliance_id`,`horde_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_factionchange_items`
--

DROP TABLE IF EXISTS `player_factionchange_items`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_factionchange_items` (
  `alliance_id` int unsigned NOT NULL,
  `alliance_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `horde_id` int unsigned NOT NULL,
  `horde_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`alliance_id`,`horde_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_factionchange_quests`
--

DROP TABLE IF EXISTS `player_factionchange_quests`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_factionchange_quests` (
  `alliance_id` int unsigned NOT NULL,
  `horde_id` int unsigned NOT NULL,
  PRIMARY KEY (`alliance_id`,`horde_id`),
  UNIQUE KEY `alliance_uniq` (`alliance_id`),
  UNIQUE KEY `horde_uniq` (`horde_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_factionchange_reputations`
--

DROP TABLE IF EXISTS `player_factionchange_reputations`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_factionchange_reputations` (
  `alliance_id` int unsigned NOT NULL,
  `alliance_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `horde_id` int unsigned NOT NULL,
  `horde_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`alliance_id`,`horde_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_factionchange_spells`
--

DROP TABLE IF EXISTS `player_factionchange_spells`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_factionchange_spells` (
  `alliance_id` int unsigned NOT NULL,
  `alliance_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `horde_id` int unsigned NOT NULL,
  `horde_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`alliance_id`,`horde_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_factionchange_titles`
--

DROP TABLE IF EXISTS `player_factionchange_titles`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_factionchange_titles` (
  `alliance_id` int NOT NULL,
  `alliance_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `horde_id` int NOT NULL,
  `horde_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`alliance_id`,`horde_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_loot_template`
--

DROP TABLE IF EXISTS `player_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_race_stats`
--

DROP TABLE IF EXISTS `player_race_stats`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_race_stats` (
  `Race` tinyint unsigned NOT NULL,
  `Strength` int NOT NULL DEFAULT '0',
  `Agility` int NOT NULL DEFAULT '0',
  `Stamina` int NOT NULL DEFAULT '0',
  `Intellect` int NOT NULL DEFAULT '0',
  `Spirit` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`Race`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci PACK_KEYS=0 COMMENT='Stores race stats.';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_shapeshift_model`
--

DROP TABLE IF EXISTS `player_shapeshift_model`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_shapeshift_model` (
  `ShapeshiftID` tinyint unsigned NOT NULL,
  `RaceID` tinyint unsigned NOT NULL,
  `CustomizationID` tinyint unsigned NOT NULL,
  `GenderID` tinyint unsigned NOT NULL,
  `ModelID` int unsigned NOT NULL,
  PRIMARY KEY (`ShapeshiftID`,`RaceID`,`CustomizationID`,`GenderID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci PACK_KEYS=0;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_totem_model`
--

DROP TABLE IF EXISTS `player_totem_model`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_totem_model` (
  `TotemID` tinyint unsigned NOT NULL,
  `RaceID` tinyint unsigned NOT NULL,
  `ModelID` int unsigned NOT NULL,
  PRIMARY KEY (`TotemID`,`RaceID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci PACK_KEYS=0;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `player_xp_for_level`
--

DROP TABLE IF EXISTS `player_xp_for_level`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `player_xp_for_level` (
  `Level` tinyint unsigned NOT NULL,
  `Experience` int unsigned NOT NULL,
  PRIMARY KEY (`Level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `playercreateinfo`
--

DROP TABLE IF EXISTS `playercreateinfo`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playercreateinfo` (
  `race` tinyint unsigned NOT NULL DEFAULT '0',
  `class` tinyint unsigned NOT NULL DEFAULT '0',
  `map` smallint unsigned NOT NULL DEFAULT '0',
  `zone` int unsigned NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`,`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `playercreateinfo_action`
--

DROP TABLE IF EXISTS `playercreateinfo_action`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playercreateinfo_action` (
  `race` tinyint unsigned NOT NULL DEFAULT '0',
  `class` tinyint unsigned NOT NULL DEFAULT '0',
  `button` smallint unsigned NOT NULL DEFAULT '0',
  `action` int unsigned NOT NULL DEFAULT '0',
  `type` smallint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`race`,`class`,`button`),
  KEY `playercreateinfo_race_class_index` (`race`,`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `playercreateinfo_cast_spell`
--

DROP TABLE IF EXISTS `playercreateinfo_cast_spell`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playercreateinfo_cast_spell` (
  `raceMask` int unsigned NOT NULL DEFAULT '0',
  `classMask` int unsigned NOT NULL DEFAULT '0',
  `spell` int unsigned NOT NULL DEFAULT '0',
  `note` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `playercreateinfo_item`
--

DROP TABLE IF EXISTS `playercreateinfo_item`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playercreateinfo_item` (
  `race` tinyint unsigned NOT NULL DEFAULT '0',
  `class` tinyint unsigned NOT NULL DEFAULT '0',
  `itemid` int unsigned NOT NULL DEFAULT '0',
  `amount` int NOT NULL DEFAULT '1',
  `Note` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`race`,`class`,`itemid`),
  KEY `playercreateinfo_race_class_index` (`race`,`class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `playercreateinfo_skills`
--

DROP TABLE IF EXISTS `playercreateinfo_skills`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playercreateinfo_skills` (
  `raceMask` int unsigned NOT NULL,
  `classMask` int unsigned NOT NULL,
  `skill` smallint unsigned NOT NULL,
  `rank` smallint unsigned NOT NULL DEFAULT '0',
  `comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`raceMask`,`classMask`,`skill`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `playercreateinfo_spell_custom`
--

DROP TABLE IF EXISTS `playercreateinfo_spell_custom`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `playercreateinfo_spell_custom` (
  `racemask` int unsigned NOT NULL DEFAULT '0',
  `classmask` int unsigned NOT NULL DEFAULT '0',
  `Spell` int unsigned NOT NULL DEFAULT '0',
  `Note` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`racemask`,`classmask`,`Spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `points_of_interest`
--

DROP TABLE IF EXISTS `points_of_interest`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `points_of_interest` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `Icon` int unsigned NOT NULL DEFAULT '0',
  `Flags` int unsigned NOT NULL DEFAULT '0',
  `Importance` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `points_of_interest_locale`
--

DROP TABLE IF EXISTS `points_of_interest_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `points_of_interest_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pool_creature`
--

DROP TABLE IF EXISTS `pool_creature`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pool_creature` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `pool_entry` int unsigned NOT NULL DEFAULT '0',
  `chance` float NOT NULL DEFAULT '0',
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`guid`),
  KEY `idx_guid` (`guid`),
  CONSTRAINT `pool_creature_chk_1` CHECK ((`chance` >= 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pool_gameobject`
--

DROP TABLE IF EXISTS `pool_gameobject`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pool_gameobject` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `pool_entry` int unsigned NOT NULL DEFAULT '0',
  `chance` float NOT NULL DEFAULT '0',
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`guid`),
  KEY `idx_guid` (`guid`),
  CONSTRAINT `pool_gameobject_chk_1` CHECK ((`chance` >= 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pool_pool`
--

DROP TABLE IF EXISTS `pool_pool`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pool_pool` (
  `pool_id` int unsigned NOT NULL DEFAULT '0',
  `mother_pool` int unsigned NOT NULL DEFAULT '0',
  `chance` float NOT NULL DEFAULT '0',
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`pool_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pool_quest`
--

DROP TABLE IF EXISTS `pool_quest`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pool_quest` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `pool_entry` int unsigned NOT NULL DEFAULT '0',
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`entry`),
  KEY `idx_guid` (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pool_template`
--

DROP TABLE IF EXISTS `pool_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pool_template` (
  `entry` int unsigned NOT NULL DEFAULT '0' COMMENT 'Pool entry',
  `max_limit` int unsigned NOT NULL DEFAULT '0' COMMENT 'Max number of objects (0) is no limit',
  `description` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `powerdisplay_dbc`
--

DROP TABLE IF EXISTS `powerdisplay_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `powerdisplay_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `ActualType` int NOT NULL DEFAULT '0',
  `GlobalstringBaseTag` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Red` tinyint unsigned NOT NULL DEFAULT '0',
  `Green` tinyint unsigned NOT NULL DEFAULT '0',
  `Blue` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `prospecting_loot_template`
--

DROP TABLE IF EXISTS `prospecting_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `prospecting_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `pvpdifficulty_dbc`
--

DROP TABLE IF EXISTS `pvpdifficulty_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `pvpdifficulty_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `MapID` int NOT NULL DEFAULT '0',
  `RangeIndex` int NOT NULL DEFAULT '0',
  `MinLevel` int NOT NULL DEFAULT '0',
  `MaxLevel` int NOT NULL DEFAULT '0',
  `Difficulty` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_details`
--

DROP TABLE IF EXISTS `quest_details`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_details` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Emote1` smallint unsigned NOT NULL DEFAULT '0',
  `Emote2` smallint unsigned NOT NULL DEFAULT '0',
  `Emote3` smallint unsigned NOT NULL DEFAULT '0',
  `Emote4` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay1` int unsigned NOT NULL DEFAULT '0',
  `EmoteDelay2` int unsigned NOT NULL DEFAULT '0',
  `EmoteDelay3` int unsigned NOT NULL DEFAULT '0',
  `EmoteDelay4` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_greeting`
--

DROP TABLE IF EXISTS `quest_greeting`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_greeting` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `GreetEmoteType` smallint unsigned NOT NULL DEFAULT '0',
  `GreetEmoteDelay` int unsigned NOT NULL DEFAULT '0',
  `Greeting` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`,`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_greeting_locale`
--

DROP TABLE IF EXISTS `quest_greeting_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_greeting_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Greeting` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`,`type`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_mail_sender`
--

DROP TABLE IF EXISTS `quest_mail_sender`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_mail_sender` (
  `QuestId` int unsigned NOT NULL DEFAULT '0',
  `RewardMailSenderEntry` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`QuestId`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_money_reward`
--

DROP TABLE IF EXISTS `quest_money_reward`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_money_reward` (
  `Level` int NOT NULL DEFAULT '0',
  `Money0` int NOT NULL DEFAULT '0',
  `Money1` int NOT NULL DEFAULT '0',
  `Money2` int NOT NULL DEFAULT '0',
  `Money3` int NOT NULL DEFAULT '0',
  `Money4` int NOT NULL DEFAULT '0',
  `Money5` int NOT NULL DEFAULT '0',
  `Money6` int NOT NULL DEFAULT '0',
  `Money7` int NOT NULL DEFAULT '0',
  `Money8` int NOT NULL DEFAULT '0',
  `Money9` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`Level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_offer_reward`
--

DROP TABLE IF EXISTS `quest_offer_reward`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_offer_reward` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Emote1` smallint unsigned NOT NULL DEFAULT '0',
  `Emote2` smallint unsigned NOT NULL DEFAULT '0',
  `Emote3` smallint unsigned NOT NULL DEFAULT '0',
  `Emote4` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteDelay1` int unsigned NOT NULL DEFAULT '0',
  `EmoteDelay2` int unsigned NOT NULL DEFAULT '0',
  `EmoteDelay3` int unsigned NOT NULL DEFAULT '0',
  `EmoteDelay4` int unsigned NOT NULL DEFAULT '0',
  `RewardText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_offer_reward_locale`
--

DROP TABLE IF EXISTS `quest_offer_reward_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_offer_reward_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `RewardText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_poi`
--

DROP TABLE IF EXISTS `quest_poi`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_poi` (
  `QuestID` int unsigned NOT NULL DEFAULT '0',
  `id` int unsigned NOT NULL DEFAULT '0',
  `ObjectiveIndex` int NOT NULL DEFAULT '0',
  `MapID` int unsigned NOT NULL DEFAULT '0',
  `WorldMapAreaId` int unsigned NOT NULL DEFAULT '0',
  `Floor` int unsigned NOT NULL DEFAULT '0',
  `Priority` int unsigned NOT NULL DEFAULT '0',
  `Flags` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`QuestID`,`id`),
  KEY `idx` (`QuestID`,`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_poi_points`
--

DROP TABLE IF EXISTS `quest_poi_points`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_poi_points` (
  `QuestID` int unsigned NOT NULL DEFAULT '0',
  `Idx1` int unsigned NOT NULL DEFAULT '0',
  `Idx2` int unsigned NOT NULL DEFAULT '0',
  `X` int NOT NULL DEFAULT '0',
  `Y` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`QuestID`,`Idx1`,`Idx2`),
  KEY `questId_id` (`QuestID`,`Idx1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_request_items`
--

DROP TABLE IF EXISTS `quest_request_items`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_request_items` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `EmoteOnComplete` smallint unsigned NOT NULL DEFAULT '0',
  `EmoteOnIncomplete` smallint unsigned NOT NULL DEFAULT '0',
  `CompletionText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_request_items_locale`
--

DROP TABLE IF EXISTS `quest_request_items_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_request_items_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `CompletionText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_template`
--

DROP TABLE IF EXISTS `quest_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_template` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `QuestType` tinyint unsigned NOT NULL DEFAULT '2',
  `QuestLevel` smallint NOT NULL DEFAULT '1',
  `MinLevel` tinyint unsigned NOT NULL DEFAULT '0',
  `QuestSortID` smallint NOT NULL DEFAULT '0',
  `QuestInfoID` smallint unsigned NOT NULL DEFAULT '0',
  `SuggestedGroupNum` tinyint unsigned NOT NULL DEFAULT '0',
  `RequiredFactionId1` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredFactionId2` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredFactionValue1` int NOT NULL DEFAULT '0',
  `RequiredFactionValue2` int NOT NULL DEFAULT '0',
  `RewardNextQuest` int unsigned NOT NULL DEFAULT '0',
  `RewardXPDifficulty` tinyint unsigned NOT NULL DEFAULT '0',
  `RewardMoney` int NOT NULL DEFAULT '0',
  `RewardMoneyDifficulty` int unsigned NOT NULL DEFAULT '0',
  `RewardDisplaySpell` int unsigned NOT NULL DEFAULT '0',
  `RewardSpell` int NOT NULL DEFAULT '0',
  `RewardHonor` int NOT NULL DEFAULT '0',
  `RewardKillHonor` float NOT NULL DEFAULT '0',
  `StartItem` int unsigned NOT NULL DEFAULT '0',
  `Flags` int unsigned NOT NULL DEFAULT '0',
  `RequiredPlayerKills` tinyint unsigned NOT NULL DEFAULT '0',
  `RewardItem1` int unsigned NOT NULL DEFAULT '0',
  `RewardAmount1` smallint unsigned NOT NULL DEFAULT '0',
  `RewardItem2` int unsigned NOT NULL DEFAULT '0',
  `RewardAmount2` smallint unsigned NOT NULL DEFAULT '0',
  `RewardItem3` int unsigned NOT NULL DEFAULT '0',
  `RewardAmount3` smallint unsigned NOT NULL DEFAULT '0',
  `RewardItem4` int unsigned NOT NULL DEFAULT '0',
  `RewardAmount4` smallint unsigned NOT NULL DEFAULT '0',
  `ItemDrop1` int unsigned NOT NULL DEFAULT '0',
  `ItemDropQuantity1` smallint unsigned NOT NULL DEFAULT '0',
  `ItemDrop2` int unsigned NOT NULL DEFAULT '0',
  `ItemDropQuantity2` smallint unsigned NOT NULL DEFAULT '0',
  `ItemDrop3` int unsigned NOT NULL DEFAULT '0',
  `ItemDropQuantity3` smallint unsigned NOT NULL DEFAULT '0',
  `ItemDrop4` int unsigned NOT NULL DEFAULT '0',
  `ItemDropQuantity4` smallint unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemID1` int unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemQuantity1` smallint unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemID2` int unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemQuantity2` smallint unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemID3` int unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemQuantity3` smallint unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemID4` int unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemQuantity4` smallint unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemID5` int unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemQuantity5` smallint unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemID6` int unsigned NOT NULL DEFAULT '0',
  `RewardChoiceItemQuantity6` smallint unsigned NOT NULL DEFAULT '0',
  `POIContinent` smallint unsigned NOT NULL DEFAULT '0',
  `POIx` float NOT NULL DEFAULT '0',
  `POIy` float NOT NULL DEFAULT '0',
  `POIPriority` int unsigned NOT NULL DEFAULT '0',
  `RewardTitle` tinyint unsigned NOT NULL DEFAULT '0',
  `RewardTalents` tinyint unsigned NOT NULL DEFAULT '0',
  `RewardArenaPoints` smallint unsigned NOT NULL DEFAULT '0',
  `RewardFactionID1` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'faction id from Faction.dbc in this case',
  `RewardFactionValue1` int NOT NULL DEFAULT '0',
  `RewardFactionOverride1` int NOT NULL DEFAULT '0',
  `RewardFactionID2` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'faction id from Faction.dbc in this case',
  `RewardFactionValue2` int NOT NULL DEFAULT '0',
  `RewardFactionOverride2` int NOT NULL DEFAULT '0',
  `RewardFactionID3` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'faction id from Faction.dbc in this case',
  `RewardFactionValue3` int NOT NULL DEFAULT '0',
  `RewardFactionOverride3` int NOT NULL DEFAULT '0',
  `RewardFactionID4` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'faction id from Faction.dbc in this case',
  `RewardFactionValue4` int NOT NULL DEFAULT '0',
  `RewardFactionOverride4` int NOT NULL DEFAULT '0',
  `RewardFactionID5` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'faction id from Faction.dbc in this case',
  `RewardFactionValue5` int NOT NULL DEFAULT '0',
  `RewardFactionOverride5` int NOT NULL DEFAULT '0',
  `TimeAllowed` int unsigned NOT NULL DEFAULT '0',
  `AllowableRaces` int unsigned NOT NULL DEFAULT '0',
  `LogTitle` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `LogDescription` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `QuestDescription` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `AreaDescription` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `QuestCompletionLog` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `RequiredNpcOrGo1` int NOT NULL DEFAULT '0',
  `RequiredNpcOrGo2` int NOT NULL DEFAULT '0',
  `RequiredNpcOrGo3` int NOT NULL DEFAULT '0',
  `RequiredNpcOrGo4` int NOT NULL DEFAULT '0',
  `RequiredNpcOrGoCount1` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredNpcOrGoCount2` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredNpcOrGoCount3` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredNpcOrGoCount4` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredItemId1` int unsigned NOT NULL DEFAULT '0',
  `RequiredItemId2` int unsigned NOT NULL DEFAULT '0',
  `RequiredItemId3` int unsigned NOT NULL DEFAULT '0',
  `RequiredItemId4` int unsigned NOT NULL DEFAULT '0',
  `RequiredItemId5` int unsigned NOT NULL DEFAULT '0',
  `RequiredItemId6` int unsigned NOT NULL DEFAULT '0',
  `RequiredItemCount1` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredItemCount2` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredItemCount3` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredItemCount4` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredItemCount5` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredItemCount6` smallint unsigned NOT NULL DEFAULT '0',
  `Unknown0` tinyint unsigned NOT NULL DEFAULT '0',
  `ObjectiveText1` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ObjectiveText2` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ObjectiveText3` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ObjectiveText4` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Quest System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_template_addon`
--

DROP TABLE IF EXISTS `quest_template_addon`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_template_addon` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `MaxLevel` tinyint unsigned NOT NULL DEFAULT '0',
  `AllowableClasses` int unsigned NOT NULL DEFAULT '0',
  `SourceSpellID` int unsigned NOT NULL DEFAULT '0',
  `PrevQuestID` int NOT NULL DEFAULT '0',
  `NextQuestID` int unsigned NOT NULL DEFAULT '0',
  `ExclusiveGroup` int NOT NULL DEFAULT '0',
  `BreadcrumbForQuestId` mediumint unsigned NOT NULL DEFAULT '0',
  `RewardMailTemplateID` int unsigned NOT NULL DEFAULT '0',
  `RewardMailDelay` int unsigned NOT NULL DEFAULT '0',
  `RequiredSkillID` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredSkillPoints` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredMinRepFaction` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredMaxRepFaction` smallint unsigned NOT NULL DEFAULT '0',
  `RequiredMinRepValue` int NOT NULL DEFAULT '0',
  `RequiredMaxRepValue` int NOT NULL DEFAULT '0',
  `ProvidedItemCount` tinyint unsigned NOT NULL DEFAULT '0',
  `SpecialFlags` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `quest_template_locale`
--

DROP TABLE IF EXISTS `quest_template_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `quest_template_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Title` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Details` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Objectives` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `EndText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `CompletedText` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ObjectiveText1` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ObjectiveText2` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ObjectiveText3` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ObjectiveText4` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `questfactionreward_dbc`
--

DROP TABLE IF EXISTS `questfactionreward_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `questfactionreward_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Difficulty_1` int NOT NULL DEFAULT '0',
  `Difficulty_2` int NOT NULL DEFAULT '0',
  `Difficulty_3` int NOT NULL DEFAULT '0',
  `Difficulty_4` int NOT NULL DEFAULT '0',
  `Difficulty_5` int NOT NULL DEFAULT '0',
  `Difficulty_6` int NOT NULL DEFAULT '0',
  `Difficulty_7` int NOT NULL DEFAULT '0',
  `Difficulty_8` int NOT NULL DEFAULT '0',
  `Difficulty_9` int NOT NULL DEFAULT '0',
  `Difficulty_10` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `questsort_dbc`
--

DROP TABLE IF EXISTS `questsort_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `questsort_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `SortName_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `SortName_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `questxp_dbc`
--

DROP TABLE IF EXISTS `questxp_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `questxp_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Difficulty_1` int NOT NULL DEFAULT '0',
  `Difficulty_2` int NOT NULL DEFAULT '0',
  `Difficulty_3` int NOT NULL DEFAULT '0',
  `Difficulty_4` int NOT NULL DEFAULT '0',
  `Difficulty_5` int NOT NULL DEFAULT '0',
  `Difficulty_6` int NOT NULL DEFAULT '0',
  `Difficulty_7` int NOT NULL DEFAULT '0',
  `Difficulty_8` int NOT NULL DEFAULT '0',
  `Difficulty_9` int NOT NULL DEFAULT '0',
  `Difficulty_10` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `randproppoints_dbc`
--

DROP TABLE IF EXISTS `randproppoints_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `randproppoints_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Epic_1` int NOT NULL DEFAULT '0',
  `Epic_2` int NOT NULL DEFAULT '0',
  `Epic_3` int NOT NULL DEFAULT '0',
  `Epic_4` int NOT NULL DEFAULT '0',
  `Epic_5` int NOT NULL DEFAULT '0',
  `Superior_1` int NOT NULL DEFAULT '0',
  `Superior_2` int NOT NULL DEFAULT '0',
  `Superior_3` int NOT NULL DEFAULT '0',
  `Superior_4` int NOT NULL DEFAULT '0',
  `Superior_5` int NOT NULL DEFAULT '0',
  `Good_1` int NOT NULL DEFAULT '0',
  `Good_2` int NOT NULL DEFAULT '0',
  `Good_3` int NOT NULL DEFAULT '0',
  `Good_4` int NOT NULL DEFAULT '0',
  `Good_5` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `reference_loot_template`
--

DROP TABLE IF EXISTS `reference_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `reference_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `reputation_reward_rate`
--

DROP TABLE IF EXISTS `reputation_reward_rate`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `reputation_reward_rate` (
  `faction` int unsigned NOT NULL DEFAULT '0',
  `quest_rate` float NOT NULL DEFAULT '1',
  `quest_daily_rate` float NOT NULL DEFAULT '1',
  `quest_weekly_rate` float NOT NULL DEFAULT '1',
  `quest_monthly_rate` float NOT NULL DEFAULT '1',
  `quest_repeatable_rate` float NOT NULL DEFAULT '1',
  `creature_rate` float NOT NULL DEFAULT '1',
  `spell_rate` float NOT NULL DEFAULT '1',
  PRIMARY KEY (`faction`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `reputation_spillover_template`
--

DROP TABLE IF EXISTS `reputation_spillover_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `reputation_spillover_template` (
  `faction` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'faction entry',
  `faction1` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'faction to give spillover for',
  `rate_1` float NOT NULL DEFAULT '0' COMMENT 'the given rep points * rate',
  `rank_1` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'max rank,above this will not give any spillover',
  `faction2` smallint unsigned NOT NULL DEFAULT '0',
  `rate_2` float NOT NULL DEFAULT '0',
  `rank_2` tinyint unsigned NOT NULL DEFAULT '0',
  `faction3` smallint unsigned NOT NULL DEFAULT '0',
  `rate_3` float NOT NULL DEFAULT '0',
  `rank_3` tinyint unsigned NOT NULL DEFAULT '0',
  `faction4` smallint unsigned NOT NULL DEFAULT '0',
  `rate_4` float NOT NULL DEFAULT '0',
  `rank_4` tinyint unsigned NOT NULL DEFAULT '0',
  `faction5` smallint unsigned NOT NULL DEFAULT '0',
  `rate_5` float NOT NULL DEFAULT '0',
  `rank_5` tinyint unsigned NOT NULL DEFAULT '0',
  `faction6` smallint unsigned NOT NULL DEFAULT '0',
  `rate_6` float NOT NULL DEFAULT '0',
  `rank_6` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`faction`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Reputation spillover reputation gain';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `scalingstatdistribution_dbc`
--

DROP TABLE IF EXISTS `scalingstatdistribution_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `scalingstatdistribution_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `StatID_1` int NOT NULL DEFAULT '0',
  `StatID_2` int NOT NULL DEFAULT '0',
  `StatID_3` int NOT NULL DEFAULT '0',
  `StatID_4` int NOT NULL DEFAULT '0',
  `StatID_5` int NOT NULL DEFAULT '0',
  `StatID_6` int NOT NULL DEFAULT '0',
  `StatID_7` int NOT NULL DEFAULT '0',
  `StatID_8` int NOT NULL DEFAULT '0',
  `StatID_9` int NOT NULL DEFAULT '0',
  `StatID_10` int NOT NULL DEFAULT '0',
  `Bonus_1` int NOT NULL DEFAULT '0',
  `Bonus_2` int NOT NULL DEFAULT '0',
  `Bonus_3` int NOT NULL DEFAULT '0',
  `Bonus_4` int NOT NULL DEFAULT '0',
  `Bonus_5` int NOT NULL DEFAULT '0',
  `Bonus_6` int NOT NULL DEFAULT '0',
  `Bonus_7` int NOT NULL DEFAULT '0',
  `Bonus_8` int NOT NULL DEFAULT '0',
  `Bonus_9` int NOT NULL DEFAULT '0',
  `Bonus_10` int NOT NULL DEFAULT '0',
  `Maxlevel` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `scalingstatvalues_dbc`
--

DROP TABLE IF EXISTS `scalingstatvalues_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `scalingstatvalues_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Charlevel` int NOT NULL DEFAULT '0',
  `ShoulderBudget` int NOT NULL DEFAULT '0',
  `TrinketBudget` int NOT NULL DEFAULT '0',
  `WeaponBudget1H` int NOT NULL DEFAULT '0',
  `RangedBudget` int NOT NULL DEFAULT '0',
  `ClothShoulderArmor` int NOT NULL DEFAULT '0',
  `LeatherShoulderArmor` int NOT NULL DEFAULT '0',
  `MailShoulderArmor` int NOT NULL DEFAULT '0',
  `PlateShoulderArmor` int NOT NULL DEFAULT '0',
  `WeaponDPS1H` int NOT NULL DEFAULT '0',
  `WeaponDPS2H` int NOT NULL DEFAULT '0',
  `SpellcasterDPS1H` int NOT NULL DEFAULT '0',
  `SpellcasterDPS2H` int NOT NULL DEFAULT '0',
  `RangedDPS` int NOT NULL DEFAULT '0',
  `WandDPS` int NOT NULL DEFAULT '0',
  `SpellPower` int NOT NULL DEFAULT '0',
  `PrimaryBudget` int NOT NULL DEFAULT '0',
  `TertiaryBudget` int NOT NULL DEFAULT '0',
  `ClothCloakArmor` int NOT NULL DEFAULT '0',
  `ClothChestArmor` int NOT NULL DEFAULT '0',
  `LeatherChestArmor` int NOT NULL DEFAULT '0',
  `MailChestArmor` int NOT NULL DEFAULT '0',
  `PlateChestArmor` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `script_waypoint`
--

DROP TABLE IF EXISTS `script_waypoint`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `script_waypoint` (
  `entry` int unsigned NOT NULL DEFAULT '0' COMMENT 'creature_template entry',
  `pointid` int unsigned NOT NULL DEFAULT '0',
  `location_x` float NOT NULL DEFAULT '0',
  `location_y` float NOT NULL DEFAULT '0',
  `location_z` float NOT NULL DEFAULT '0',
  `waittime` int unsigned NOT NULL DEFAULT '0' COMMENT 'waittime in millisecs',
  `point_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`entry`,`pointid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Script Creature waypoints';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `skill_discovery_template`
--

DROP TABLE IF EXISTS `skill_discovery_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `skill_discovery_template` (
  `spellId` int unsigned NOT NULL DEFAULT '0' COMMENT 'SpellId of the discoverable spell',
  `reqSpell` int unsigned NOT NULL DEFAULT '0' COMMENT 'spell requirement',
  `reqSkillValue` smallint unsigned NOT NULL DEFAULT '0' COMMENT 'skill points requirement',
  `chance` float NOT NULL DEFAULT '0' COMMENT 'chance to discover',
  PRIMARY KEY (`spellId`,`reqSpell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Skill Discovery System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `skill_extra_item_template`
--

DROP TABLE IF EXISTS `skill_extra_item_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `skill_extra_item_template` (
  `spellId` int unsigned NOT NULL DEFAULT '0' COMMENT 'SpellId of the item creation spell',
  `requiredSpecialization` int unsigned NOT NULL DEFAULT '0' COMMENT 'Specialization spell id',
  `additionalCreateChance` float NOT NULL DEFAULT '0' COMMENT 'chance to create add',
  `additionalMaxNum` tinyint NOT NULL DEFAULT '0',
  PRIMARY KEY (`spellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Skill Specialization System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `skill_fishing_base_level`
--

DROP TABLE IF EXISTS `skill_fishing_base_level`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `skill_fishing_base_level` (
  `entry` int unsigned NOT NULL DEFAULT '0' COMMENT 'Area identifier',
  `skill` smallint NOT NULL DEFAULT '0' COMMENT 'Base skill level requirement',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Fishing system';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `skill_perfect_item_template`
--

DROP TABLE IF EXISTS `skill_perfect_item_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `skill_perfect_item_template` (
  `spellId` int unsigned NOT NULL DEFAULT '0' COMMENT 'SpellId of the item creation spell',
  `requiredSpecialization` int unsigned NOT NULL DEFAULT '0' COMMENT 'Specialization spell id',
  `perfectCreateChance` float NOT NULL DEFAULT '0' COMMENT 'chance to create the perfect item instead',
  `perfectItemType` int unsigned NOT NULL DEFAULT '0' COMMENT 'perfect item type to create instead',
  PRIMARY KEY (`spellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Crafting Perfection System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `skillline_dbc`
--

DROP TABLE IF EXISTS `skillline_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `skillline_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `CategoryID` int NOT NULL DEFAULT '0',
  `SkillCostsID` int NOT NULL DEFAULT '0',
  `DisplayName_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `DisplayName_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Description_Lang_enUS` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enGB` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_koKR` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_frFR` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_deDE` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enCN` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhCN` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_enTW` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_zhTW` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esES` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_esMX` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ruRU` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptPT` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_ptBR` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_itIT` varchar(300) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Description_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `SpellIconID` int NOT NULL DEFAULT '0',
  `AlternateVerb_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AlternateVerb_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `CanLink` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `skilllineability_dbc`
--

DROP TABLE IF EXISTS `skilllineability_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `skilllineability_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `SkillLine` int NOT NULL DEFAULT '0',
  `Spell` int NOT NULL DEFAULT '0',
  `RaceMask` int NOT NULL DEFAULT '0',
  `ClassMask` int NOT NULL DEFAULT '0',
  `ExcludeRace` int NOT NULL DEFAULT '0',
  `ExcludeClass` int NOT NULL DEFAULT '0',
  `MinSkillLineRank` int NOT NULL DEFAULT '0',
  `SupercededBySpell` int NOT NULL DEFAULT '0',
  `AcquireMethod` int NOT NULL DEFAULT '0',
  `TrivialSkillLineRankHigh` int NOT NULL DEFAULT '0',
  `TrivialSkillLineRankLow` int NOT NULL DEFAULT '0',
  `CharacterPoints_1` int NOT NULL DEFAULT '0',
  `CharacterPoints_2` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `skillraceclassinfo_dbc`
--

DROP TABLE IF EXISTS `skillraceclassinfo_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `skillraceclassinfo_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `SkillID` int NOT NULL DEFAULT '0',
  `RaceMask` int NOT NULL DEFAULT '0',
  `ClassMask` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `MinLevel` int NOT NULL DEFAULT '0',
  `SkillTierID` int NOT NULL DEFAULT '0',
  `SkillCostIndex` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `skilltiers_dbc`
--

DROP TABLE IF EXISTS `skilltiers_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `skilltiers_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Cost_1` int NOT NULL DEFAULT '0',
  `Cost_2` int NOT NULL DEFAULT '0',
  `Cost_3` int NOT NULL DEFAULT '0',
  `Cost_4` int NOT NULL DEFAULT '0',
  `Cost_5` int NOT NULL DEFAULT '0',
  `Cost_6` int NOT NULL DEFAULT '0',
  `Cost_7` int NOT NULL DEFAULT '0',
  `Cost_8` int NOT NULL DEFAULT '0',
  `Cost_9` int NOT NULL DEFAULT '0',
  `Cost_10` int NOT NULL DEFAULT '0',
  `Cost_11` int NOT NULL DEFAULT '0',
  `Cost_12` int NOT NULL DEFAULT '0',
  `Cost_13` int NOT NULL DEFAULT '0',
  `Cost_14` int NOT NULL DEFAULT '0',
  `Cost_15` int NOT NULL DEFAULT '0',
  `Cost_16` int NOT NULL DEFAULT '0',
  `Value_1` int NOT NULL DEFAULT '0',
  `Value_2` int NOT NULL DEFAULT '0',
  `Value_3` int NOT NULL DEFAULT '0',
  `Value_4` int NOT NULL DEFAULT '0',
  `Value_5` int NOT NULL DEFAULT '0',
  `Value_6` int NOT NULL DEFAULT '0',
  `Value_7` int NOT NULL DEFAULT '0',
  `Value_8` int NOT NULL DEFAULT '0',
  `Value_9` int NOT NULL DEFAULT '0',
  `Value_10` int NOT NULL DEFAULT '0',
  `Value_11` int NOT NULL DEFAULT '0',
  `Value_12` int NOT NULL DEFAULT '0',
  `Value_13` int NOT NULL DEFAULT '0',
  `Value_14` int NOT NULL DEFAULT '0',
  `Value_15` int NOT NULL DEFAULT '0',
  `Value_16` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `skinning_loot_template`
--

DROP TABLE IF EXISTS `skinning_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `skinning_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `smart_scripts`
--

DROP TABLE IF EXISTS `smart_scripts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `smart_scripts` (
  `entryorguid` int NOT NULL,
  `source_type` tinyint unsigned NOT NULL DEFAULT '0',
  `id` smallint unsigned NOT NULL DEFAULT '0',
  `link` smallint unsigned NOT NULL DEFAULT '0',
  `event_type` tinyint unsigned NOT NULL DEFAULT '0',
  `event_phase_mask` smallint unsigned NOT NULL DEFAULT '0',
  `event_chance` tinyint unsigned NOT NULL DEFAULT '100',
  `event_flags` smallint unsigned NOT NULL DEFAULT '0',
  `event_param1` int unsigned NOT NULL DEFAULT '0',
  `event_param2` int unsigned NOT NULL DEFAULT '0',
  `event_param3` int unsigned NOT NULL DEFAULT '0',
  `event_param4` int unsigned NOT NULL DEFAULT '0',
  `event_param5` int unsigned NOT NULL DEFAULT '0',
  `event_param6` int unsigned NOT NULL DEFAULT '0',
  `action_type` tinyint unsigned NOT NULL DEFAULT '0',
  `action_param1` int unsigned NOT NULL DEFAULT '0',
  `action_param2` int unsigned NOT NULL DEFAULT '0',
  `action_param3` int unsigned NOT NULL DEFAULT '0',
  `action_param4` int unsigned NOT NULL DEFAULT '0',
  `action_param5` int unsigned NOT NULL DEFAULT '0',
  `action_param6` int unsigned NOT NULL DEFAULT '0',
  `target_type` tinyint unsigned NOT NULL DEFAULT '0',
  `target_param1` int unsigned NOT NULL DEFAULT '0',
  `target_param2` int unsigned NOT NULL DEFAULT '0',
  `target_param3` int unsigned NOT NULL DEFAULT '0',
  `target_param4` int unsigned NOT NULL DEFAULT '0',
  `target_x` float NOT NULL DEFAULT '0',
  `target_y` float NOT NULL DEFAULT '0',
  `target_z` float NOT NULL DEFAULT '0',
  `target_o` float NOT NULL DEFAULT '0',
  `comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Event Comment',
  PRIMARY KEY (`entryorguid`,`source_type`,`id`,`link`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `soundentries_dbc`
--

DROP TABLE IF EXISTS `soundentries_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `soundentries_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `SoundType` int NOT NULL DEFAULT '0',
  `Name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `File_1` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `File_2` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `File_3` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `File_4` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `File_5` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `File_6` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `File_7` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `File_8` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `File_9` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `File_10` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Freq_1` int NOT NULL DEFAULT '0',
  `Freq_2` int NOT NULL DEFAULT '0',
  `Freq_3` int NOT NULL DEFAULT '0',
  `Freq_4` int NOT NULL DEFAULT '0',
  `Freq_5` int NOT NULL DEFAULT '0',
  `Freq_6` int NOT NULL DEFAULT '0',
  `Freq_7` int NOT NULL DEFAULT '0',
  `Freq_8` int NOT NULL DEFAULT '0',
  `Freq_9` int NOT NULL DEFAULT '0',
  `Freq_10` int NOT NULL DEFAULT '0',
  `DirectoryBase` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Volumefloat` float NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `MinDistance` float NOT NULL DEFAULT '0',
  `DistanceCutoff` float NOT NULL DEFAULT '0',
  `EAXDef` int NOT NULL DEFAULT '0',
  `SoundEntriesAdvancedID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spawn_group`
--

DROP TABLE IF EXISTS `spawn_group`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spawn_group` (
  `groupId` int unsigned NOT NULL,
  `spawnType` tinyint unsigned NOT NULL,
  `spawnId` int unsigned NOT NULL,
  PRIMARY KEY (`groupId`,`spawnType`,`spawnId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spawn_group_template`
--

DROP TABLE IF EXISTS `spawn_group_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spawn_group_template` (
  `groupId` int unsigned NOT NULL,
  `groupName` varchar(100) NOT NULL,
  `groupFlags` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`groupId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_area`
--

DROP TABLE IF EXISTS `spell_area`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_area` (
  `spell` int unsigned NOT NULL DEFAULT '0',
  `area` int unsigned NOT NULL DEFAULT '0',
  `quest_start` int unsigned NOT NULL DEFAULT '0',
  `quest_end` int unsigned NOT NULL DEFAULT '0',
  `aura_spell` int NOT NULL DEFAULT '0',
  `racemask` int unsigned NOT NULL DEFAULT '0',
  `gender` tinyint unsigned NOT NULL DEFAULT '2',
  `autocast` tinyint unsigned NOT NULL DEFAULT '0',
  `quest_start_status` int NOT NULL DEFAULT '64',
  `quest_end_status` int NOT NULL DEFAULT '11',
  PRIMARY KEY (`spell`,`area`,`quest_start`,`aura_spell`,`racemask`,`gender`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_bonus_data`
--

DROP TABLE IF EXISTS `spell_bonus_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_bonus_data` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `direct_bonus` float NOT NULL DEFAULT '0',
  `dot_bonus` float NOT NULL DEFAULT '0',
  `ap_bonus` float NOT NULL DEFAULT '0',
  `ap_dot_bonus` float NOT NULL DEFAULT '0',
  `comments` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_cooldown_overrides`
--

DROP TABLE IF EXISTS `spell_cooldown_overrides`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_cooldown_overrides` (
  `Id` int unsigned NOT NULL,
  `RecoveryTime` int unsigned NOT NULL DEFAULT '0',
  `CategoryRecoveryTime` int unsigned NOT NULL DEFAULT '0',
  `StartRecoveryTime` int unsigned NOT NULL DEFAULT '0',
  `StartRecoveryCategory` int unsigned NOT NULL DEFAULT '0',
  `Comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_custom_attr`
--

DROP TABLE IF EXISTS `spell_custom_attr`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_custom_attr` (
  `spell_id` int unsigned NOT NULL DEFAULT '0' COMMENT 'spell id',
  `attributes` int unsigned NOT NULL DEFAULT '0' COMMENT 'SpellCustomAttributes',
  PRIMARY KEY (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='SpellInfo custom attributes';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_dbc`
--

DROP TABLE IF EXISTS `spell_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Category` int unsigned NOT NULL DEFAULT '0',
  `DispelType` int unsigned NOT NULL DEFAULT '0',
  `Mechanic` int unsigned NOT NULL DEFAULT '0',
  `Attributes` int unsigned NOT NULL DEFAULT '0',
  `AttributesEx` int unsigned NOT NULL DEFAULT '0',
  `AttributesEx2` int unsigned NOT NULL DEFAULT '0',
  `AttributesEx3` int unsigned NOT NULL DEFAULT '0',
  `AttributesEx4` int unsigned NOT NULL DEFAULT '0',
  `AttributesEx5` int unsigned NOT NULL DEFAULT '0',
  `AttributesEx6` int unsigned NOT NULL DEFAULT '0',
  `AttributesEx7` int unsigned NOT NULL DEFAULT '0',
  `ShapeshiftMask` bigint unsigned NOT NULL DEFAULT '0',
  `unk_320_2` int NOT NULL DEFAULT '0',
  `ShapeshiftExclude` bigint unsigned NOT NULL DEFAULT '0',
  `unk_320_3` int NOT NULL DEFAULT '0',
  `Targets` int unsigned NOT NULL DEFAULT '0',
  `TargetCreatureType` int unsigned NOT NULL DEFAULT '0',
  `RequiresSpellFocus` int unsigned NOT NULL DEFAULT '0',
  `FacingCasterFlags` int unsigned NOT NULL DEFAULT '0',
  `CasterAuraState` int unsigned NOT NULL DEFAULT '0',
  `TargetAuraState` int unsigned NOT NULL DEFAULT '0',
  `ExcludeCasterAuraState` int unsigned NOT NULL DEFAULT '0',
  `ExcludeTargetAuraState` int unsigned NOT NULL DEFAULT '0',
  `CasterAuraSpell` int unsigned NOT NULL DEFAULT '0',
  `TargetAuraSpell` int unsigned NOT NULL DEFAULT '0',
  `ExcludeCasterAuraSpell` int unsigned NOT NULL DEFAULT '0',
  `ExcludeTargetAuraSpell` int unsigned NOT NULL DEFAULT '0',
  `CastingTimeIndex` int unsigned NOT NULL DEFAULT '0',
  `RecoveryTime` int unsigned NOT NULL DEFAULT '0',
  `CategoryRecoveryTime` int unsigned NOT NULL DEFAULT '0',
  `InterruptFlags` int unsigned NOT NULL DEFAULT '0',
  `AuraInterruptFlags` int unsigned NOT NULL DEFAULT '0',
  `ChannelInterruptFlags` int unsigned NOT NULL DEFAULT '0',
  `ProcTypeMask` int unsigned NOT NULL DEFAULT '0',
  `ProcChance` int unsigned NOT NULL DEFAULT '0',
  `ProcCharges` int unsigned NOT NULL DEFAULT '0',
  `MaxLevel` int unsigned NOT NULL DEFAULT '0',
  `BaseLevel` int unsigned NOT NULL DEFAULT '0',
  `SpellLevel` int unsigned NOT NULL DEFAULT '0',
  `DurationIndex` int unsigned NOT NULL DEFAULT '0',
  `PowerType` int NOT NULL DEFAULT '0',
  `ManaCost` int unsigned NOT NULL DEFAULT '0',
  `ManaCostPerLevel` int unsigned NOT NULL DEFAULT '0',
  `ManaPerSecond` int unsigned NOT NULL DEFAULT '0',
  `ManaPerSecondPerLevel` int unsigned NOT NULL DEFAULT '0',
  `RangeIndex` int unsigned NOT NULL DEFAULT '0',
  `Speed` float NOT NULL DEFAULT '0',
  `ModalNextSpell` int unsigned NOT NULL DEFAULT '0',
  `CumulativeAura` int unsigned NOT NULL DEFAULT '0',
  `Totem_1` int unsigned NOT NULL DEFAULT '0',
  `Totem_2` int unsigned NOT NULL DEFAULT '0',
  `Reagent_1` int NOT NULL DEFAULT '0',
  `Reagent_2` int NOT NULL DEFAULT '0',
  `Reagent_3` int NOT NULL DEFAULT '0',
  `Reagent_4` int NOT NULL DEFAULT '0',
  `Reagent_5` int NOT NULL DEFAULT '0',
  `Reagent_6` int NOT NULL DEFAULT '0',
  `Reagent_7` int NOT NULL DEFAULT '0',
  `Reagent_8` int NOT NULL DEFAULT '0',
  `ReagentCount_1` int NOT NULL DEFAULT '0',
  `ReagentCount_2` int NOT NULL DEFAULT '0',
  `ReagentCount_3` int NOT NULL DEFAULT '0',
  `ReagentCount_4` int NOT NULL DEFAULT '0',
  `ReagentCount_5` int NOT NULL DEFAULT '0',
  `ReagentCount_6` int NOT NULL DEFAULT '0',
  `ReagentCount_7` int NOT NULL DEFAULT '0',
  `ReagentCount_8` int NOT NULL DEFAULT '0',
  `EquippedItemClass` int NOT NULL DEFAULT '0',
  `EquippedItemSubclass` int NOT NULL DEFAULT '0',
  `EquippedItemInvTypes` int NOT NULL DEFAULT '0',
  `Effect_1` int unsigned NOT NULL DEFAULT '0',
  `Effect_2` int unsigned NOT NULL DEFAULT '0',
  `Effect_3` int unsigned NOT NULL DEFAULT '0',
  `EffectDieSides_1` int NOT NULL DEFAULT '0',
  `EffectDieSides_2` int NOT NULL DEFAULT '0',
  `EffectDieSides_3` int NOT NULL DEFAULT '0',
  `EffectRealPointsPerLevel_1` float NOT NULL DEFAULT '0',
  `EffectRealPointsPerLevel_2` float NOT NULL DEFAULT '0',
  `EffectRealPointsPerLevel_3` float NOT NULL DEFAULT '0',
  `EffectBasePoints_1` int NOT NULL DEFAULT '0',
  `EffectBasePoints_2` int NOT NULL DEFAULT '0',
  `EffectBasePoints_3` int NOT NULL DEFAULT '0',
  `EffectMechanic_1` int unsigned NOT NULL DEFAULT '0',
  `EffectMechanic_2` int unsigned NOT NULL DEFAULT '0',
  `EffectMechanic_3` int unsigned NOT NULL DEFAULT '0',
  `ImplicitTargetA_1` int unsigned NOT NULL DEFAULT '0',
  `ImplicitTargetA_2` int unsigned NOT NULL DEFAULT '0',
  `ImplicitTargetA_3` int unsigned NOT NULL DEFAULT '0',
  `ImplicitTargetB_1` int unsigned NOT NULL DEFAULT '0',
  `ImplicitTargetB_2` int unsigned NOT NULL DEFAULT '0',
  `ImplicitTargetB_3` int unsigned NOT NULL DEFAULT '0',
  `EffectRadiusIndex_1` int unsigned NOT NULL DEFAULT '0',
  `EffectRadiusIndex_2` int unsigned NOT NULL DEFAULT '0',
  `EffectRadiusIndex_3` int unsigned NOT NULL DEFAULT '0',
  `EffectAura_1` int unsigned NOT NULL DEFAULT '0',
  `EffectAura_2` int unsigned NOT NULL DEFAULT '0',
  `EffectAura_3` int unsigned NOT NULL DEFAULT '0',
  `EffectAuraPeriod_1` int unsigned NOT NULL DEFAULT '0',
  `EffectAuraPeriod_2` int unsigned NOT NULL DEFAULT '0',
  `EffectAuraPeriod_3` int unsigned NOT NULL DEFAULT '0',
  `EffectMultipleValue_1` float NOT NULL DEFAULT '0',
  `EffectMultipleValue_2` float NOT NULL DEFAULT '0',
  `EffectMultipleValue_3` float NOT NULL DEFAULT '0',
  `EffectChainTargets_1` int unsigned NOT NULL DEFAULT '0',
  `EffectChainTargets_2` int unsigned NOT NULL DEFAULT '0',
  `EffectChainTargets_3` int unsigned NOT NULL DEFAULT '0',
  `EffectItemType_1` int unsigned NOT NULL DEFAULT '0',
  `EffectItemType_2` int unsigned NOT NULL DEFAULT '0',
  `EffectItemType_3` int unsigned NOT NULL DEFAULT '0',
  `EffectMiscValue_1` int NOT NULL DEFAULT '0',
  `EffectMiscValue_2` int NOT NULL DEFAULT '0',
  `EffectMiscValue_3` int NOT NULL DEFAULT '0',
  `EffectMiscValueB_1` int NOT NULL DEFAULT '0',
  `EffectMiscValueB_2` int NOT NULL DEFAULT '0',
  `EffectMiscValueB_3` int NOT NULL DEFAULT '0',
  `EffectTriggerSpell_1` int unsigned NOT NULL DEFAULT '0',
  `EffectTriggerSpell_2` int unsigned NOT NULL DEFAULT '0',
  `EffectTriggerSpell_3` int unsigned NOT NULL DEFAULT '0',
  `EffectPointsPerCombo_1` float NOT NULL DEFAULT '0',
  `EffectPointsPerCombo_2` float NOT NULL DEFAULT '0',
  `EffectPointsPerCombo_3` float NOT NULL DEFAULT '0',
  `EffectSpellClassMaskA_1` int unsigned NOT NULL DEFAULT '0',
  `EffectSpellClassMaskA_2` int unsigned NOT NULL DEFAULT '0',
  `EffectSpellClassMaskA_3` int unsigned NOT NULL DEFAULT '0',
  `EffectSpellClassMaskB_1` int unsigned NOT NULL DEFAULT '0',
  `EffectSpellClassMaskB_2` int unsigned NOT NULL DEFAULT '0',
  `EffectSpellClassMaskB_3` int unsigned NOT NULL DEFAULT '0',
  `EffectSpellClassMaskC_1` int unsigned NOT NULL DEFAULT '0',
  `EffectSpellClassMaskC_2` int unsigned NOT NULL DEFAULT '0',
  `EffectSpellClassMaskC_3` int unsigned NOT NULL DEFAULT '0',
  `SpellVisualID_1` int unsigned NOT NULL DEFAULT '0',
  `SpellVisualID_2` int unsigned NOT NULL DEFAULT '0',
  `SpellIconID` int unsigned NOT NULL DEFAULT '0',
  `ActiveIconID` int unsigned NOT NULL DEFAULT '0',
  `SpellPriority` int unsigned NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `NameSubtext_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `NameSubtext_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Description_Lang_enUS` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_enGB` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_koKR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_frFR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_deDE` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_enCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_zhCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_enTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_zhTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_esES` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_esMX` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_ruRU` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_ptPT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_ptBR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_itIT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_Unk` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `AuraDescription_Lang_enUS` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_enGB` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_koKR` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_frFR` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_deDE` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_enCN` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_zhCN` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_enTW` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_zhTW` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_esES` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_esMX` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_ruRU` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_ptPT` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_ptBR` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_itIT` varchar(550) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AuraDescription_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `ManaCostPct` int unsigned NOT NULL DEFAULT '0',
  `StartRecoveryCategory` int unsigned NOT NULL DEFAULT '0',
  `StartRecoveryTime` int unsigned NOT NULL DEFAULT '0',
  `MaxTargetLevel` int unsigned NOT NULL DEFAULT '0',
  `SpellClassSet` int unsigned NOT NULL DEFAULT '0',
  `SpellClassMask_1` int unsigned NOT NULL DEFAULT '0',
  `SpellClassMask_2` int unsigned NOT NULL DEFAULT '0',
  `SpellClassMask_3` int unsigned NOT NULL DEFAULT '0',
  `MaxTargets` int unsigned NOT NULL DEFAULT '0',
  `DefenseType` int unsigned NOT NULL DEFAULT '0',
  `PreventionType` int unsigned NOT NULL DEFAULT '0',
  `StanceBarOrder` int unsigned NOT NULL DEFAULT '0',
  `EffectChainAmplitude_1` float NOT NULL DEFAULT '0',
  `EffectChainAmplitude_2` float NOT NULL DEFAULT '0',
  `EffectChainAmplitude_3` float NOT NULL DEFAULT '0',
  `MinFactionID` int unsigned NOT NULL DEFAULT '0',
  `MinReputation` int unsigned NOT NULL DEFAULT '0',
  `RequiredAuraVision` int unsigned NOT NULL DEFAULT '0',
  `RequiredTotemCategoryID_1` int unsigned NOT NULL DEFAULT '0',
  `RequiredTotemCategoryID_2` int unsigned NOT NULL DEFAULT '0',
  `RequiredAreasID` int NOT NULL DEFAULT '0',
  `SchoolMask` int unsigned NOT NULL DEFAULT '0',
  `RuneCostID` int unsigned NOT NULL DEFAULT '0',
  `SpellMissileID` int unsigned NOT NULL DEFAULT '0',
  `PowerDisplayID` int NOT NULL DEFAULT '0',
  `EffectBonusMultiplier_1` float NOT NULL DEFAULT '0',
  `EffectBonusMultiplier_2` float NOT NULL DEFAULT '0',
  `EffectBonusMultiplier_3` float NOT NULL DEFAULT '0',
  `SpellDescriptionVariableID` int unsigned NOT NULL DEFAULT '0',
  `SpellDifficultyID` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_enchant_proc_data`
--

DROP TABLE IF EXISTS `spell_enchant_proc_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_enchant_proc_data` (
  `entry` int unsigned NOT NULL,
  `customChance` int unsigned NOT NULL DEFAULT '0',
  `PPMChance` float NOT NULL DEFAULT '0',
  `procEx` int unsigned NOT NULL DEFAULT '0',
  `attributeMask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`entry`),
  CONSTRAINT `spell_enchant_proc_data_chk_1` CHECK ((`PPMChance` >= 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell enchant proc data';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_group`
--

DROP TABLE IF EXISTS `spell_group`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_group` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `spell_id` int NOT NULL,
  PRIMARY KEY (`id`,`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_group_stack_rules`
--

DROP TABLE IF EXISTS `spell_group_stack_rules`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_group_stack_rules` (
  `group_id` int unsigned NOT NULL DEFAULT '0',
  `stack_rule` tinyint NOT NULL DEFAULT '0',
  `description` varchar(150) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`group_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_jump_distance`
--

DROP TABLE IF EXISTS `spell_jump_distance`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_jump_distance` (
  `ID` int unsigned NOT NULL COMMENT 'spell id',
  `JumpDistance` float NOT NULL DEFAULT '0' COMMENT 'max hop distance in yards',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Per-spell chain jump distance override';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_linked_spell`
--

DROP TABLE IF EXISTS `spell_linked_spell`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_linked_spell` (
  `spell_trigger` int NOT NULL,
  `spell_effect` int NOT NULL DEFAULT '0',
  `type` tinyint unsigned NOT NULL DEFAULT '0',
  `comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  UNIQUE KEY `trigger_effect_type` (`spell_trigger`,`spell_effect`,`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_loot_template`
--

DROP TABLE IF EXISTS `spell_loot_template`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_loot_template` (
  `Entry` int unsigned NOT NULL DEFAULT '0',
  `Item` int unsigned NOT NULL DEFAULT '0',
  `Reference` int NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '100',
  `QuestRequired` tinyint NOT NULL DEFAULT '0',
  `LootMode` smallint unsigned NOT NULL DEFAULT '1',
  `GroupId` tinyint unsigned NOT NULL DEFAULT '0',
  `MinCount` tinyint unsigned NOT NULL DEFAULT '1',
  `MaxCount` tinyint unsigned NOT NULL DEFAULT '1',
  `Comment` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`Entry`,`Item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Loot System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_mixology`
--

DROP TABLE IF EXISTS `spell_mixology`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_mixology` (
  `entry` int unsigned NOT NULL,
  `pctMod` float NOT NULL DEFAULT '30' COMMENT 'bonus multiplier',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_pet_auras`
--

DROP TABLE IF EXISTS `spell_pet_auras`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_pet_auras` (
  `spell` int unsigned NOT NULL COMMENT 'dummy spell id',
  `effectId` tinyint unsigned NOT NULL DEFAULT '0',
  `pet` int unsigned NOT NULL DEFAULT '0' COMMENT 'pet id; 0 = all',
  `aura` int unsigned NOT NULL COMMENT 'pet aura id',
  PRIMARY KEY (`spell`,`effectId`,`pet`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_proc`
--

DROP TABLE IF EXISTS `spell_proc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_proc` (
  `SpellId` int NOT NULL DEFAULT '0',
  `SchoolMask` tinyint unsigned NOT NULL DEFAULT '0',
  `SpellFamilyName` smallint unsigned NOT NULL DEFAULT '0',
  `SpellFamilyMask0` int unsigned NOT NULL DEFAULT '0',
  `SpellFamilyMask1` int unsigned NOT NULL DEFAULT '0',
  `SpellFamilyMask2` int unsigned NOT NULL DEFAULT '0',
  `ProcFlags` int unsigned NOT NULL DEFAULT '0',
  `SpellTypeMask` int unsigned NOT NULL DEFAULT '0',
  `SpellPhaseMask` int unsigned NOT NULL DEFAULT '0',
  `HitMask` int unsigned NOT NULL DEFAULT '0',
  `AttributesMask` int unsigned NOT NULL DEFAULT '0',
  `DisableEffectsMask` int unsigned NOT NULL DEFAULT '0',
  `ProcsPerMinute` float NOT NULL DEFAULT '0',
  `Chance` float NOT NULL DEFAULT '0',
  `Cooldown` int unsigned NOT NULL DEFAULT '0',
  `Charges` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`SpellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_proc_event`
--

DROP TABLE IF EXISTS `spell_proc_event`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_proc_event` (
  `entry` int NOT NULL DEFAULT '0',
  `SchoolMask` tinyint NOT NULL DEFAULT '0',
  `SpellFamilyName` smallint unsigned NOT NULL DEFAULT '0',
  `SpellFamilyMask0` int unsigned NOT NULL DEFAULT '0',
  `SpellFamilyMask1` int unsigned NOT NULL DEFAULT '0',
  `SpellFamilyMask2` int unsigned NOT NULL DEFAULT '0',
  `procFlags` int unsigned NOT NULL DEFAULT '0',
  `procEx` int unsigned NOT NULL DEFAULT '0',
  `procPhase` int unsigned NOT NULL DEFAULT '0',
  `ppmRate` float NOT NULL DEFAULT '0',
  `CustomChance` float NOT NULL DEFAULT '0',
  `Cooldown` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_ranks`
--

DROP TABLE IF EXISTS `spell_ranks`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_ranks` (
  `first_spell_id` int unsigned NOT NULL DEFAULT '0',
  `spell_id` int unsigned NOT NULL DEFAULT '0',
  `rank` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`first_spell_id`,`rank`),
  UNIQUE KEY `spell_id` (`spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell Rank Data';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_required`
--

DROP TABLE IF EXISTS `spell_required`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_required` (
  `spell_id` int NOT NULL DEFAULT '0',
  `req_spell` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`spell_id`,`req_spell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell Additinal Data';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_script_names`
--

DROP TABLE IF EXISTS `spell_script_names`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_script_names` (
  `spell_id` int NOT NULL,
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  UNIQUE KEY `spell_id` (`spell_id`,`ScriptName`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_scripts`
--

DROP TABLE IF EXISTS `spell_scripts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_scripts` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `effIndex` tinyint unsigned NOT NULL DEFAULT '0',
  `delay` int unsigned NOT NULL DEFAULT '0',
  `command` int unsigned NOT NULL DEFAULT '0',
  `datalong` int unsigned NOT NULL DEFAULT '0',
  `datalong2` int unsigned NOT NULL DEFAULT '0',
  `dataint` int NOT NULL DEFAULT '0',
  `x` float NOT NULL DEFAULT '0',
  `y` float NOT NULL DEFAULT '0',
  `z` float NOT NULL DEFAULT '0',
  `o` float NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_target_position`
--

DROP TABLE IF EXISTS `spell_target_position`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_target_position` (
  `ID` int unsigned NOT NULL DEFAULT '0' COMMENT 'Identifier',
  `EffectIndex` tinyint unsigned NOT NULL DEFAULT '0',
  `MapID` smallint unsigned NOT NULL DEFAULT '0',
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  `Orientation` float NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT NULL,
  PRIMARY KEY (`ID`,`EffectIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Spell System';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spell_threat`
--

DROP TABLE IF EXISTS `spell_threat`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spell_threat` (
  `entry` int unsigned NOT NULL,
  `flatMod` int DEFAULT NULL,
  `pctMod` float NOT NULL DEFAULT '1' COMMENT 'threat multiplier for damage/healing',
  `apPctMod` float NOT NULL DEFAULT '0' COMMENT 'additional threat bonus from attack power',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellcasttimes_dbc`
--

DROP TABLE IF EXISTS `spellcasttimes_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellcasttimes_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Base` int NOT NULL DEFAULT '0',
  `PerLevel` int NOT NULL DEFAULT '0',
  `Minimum` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellcategory_dbc`
--

DROP TABLE IF EXISTS `spellcategory_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellcategory_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spelldifficulty_dbc`
--

DROP TABLE IF EXISTS `spelldifficulty_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spelldifficulty_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `DifficultySpellID_1` int NOT NULL DEFAULT '0',
  `DifficultySpellID_2` int NOT NULL DEFAULT '0',
  `DifficultySpellID_3` int NOT NULL DEFAULT '0',
  `DifficultySpellID_4` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellduration_dbc`
--

DROP TABLE IF EXISTS `spellduration_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellduration_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Duration` int NOT NULL DEFAULT '0',
  `DurationPerLevel` int NOT NULL DEFAULT '0',
  `MaxDuration` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellfocusobject_dbc`
--

DROP TABLE IF EXISTS `spellfocusobject_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellfocusobject_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellitemenchantment_dbc`
--

DROP TABLE IF EXISTS `spellitemenchantment_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellitemenchantment_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Charges` int NOT NULL DEFAULT '0',
  `Effect_1` int NOT NULL DEFAULT '0',
  `Effect_2` int NOT NULL DEFAULT '0',
  `Effect_3` int NOT NULL DEFAULT '0',
  `EffectPointsMin_1` int NOT NULL DEFAULT '0',
  `EffectPointsMin_2` int NOT NULL DEFAULT '0',
  `EffectPointsMin_3` int NOT NULL DEFAULT '0',
  `EffectPointsMax_1` int NOT NULL DEFAULT '0',
  `EffectPointsMax_2` int NOT NULL DEFAULT '0',
  `EffectPointsMax_3` int NOT NULL DEFAULT '0',
  `EffectArg_1` int NOT NULL DEFAULT '0',
  `EffectArg_2` int NOT NULL DEFAULT '0',
  `EffectArg_3` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `ItemVisual` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `Src_ItemID` int NOT NULL DEFAULT '0',
  `Condition_Id` int NOT NULL DEFAULT '0',
  `RequiredSkillID` int NOT NULL DEFAULT '0',
  `RequiredSkillRank` int NOT NULL DEFAULT '0',
  `MinLevel` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellitemenchantmentcondition_dbc`
--

DROP TABLE IF EXISTS `spellitemenchantmentcondition_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellitemenchantmentcondition_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Lt_OperandType_1` tinyint unsigned NOT NULL DEFAULT '0',
  `Lt_OperandType_2` tinyint unsigned NOT NULL DEFAULT '0',
  `Lt_OperandType_3` tinyint unsigned NOT NULL DEFAULT '0',
  `Lt_OperandType_4` tinyint unsigned NOT NULL DEFAULT '0',
  `Lt_OperandType_5` tinyint unsigned NOT NULL DEFAULT '0',
  `Lt_Operand_1` int NOT NULL DEFAULT '0',
  `Lt_Operand_2` int NOT NULL DEFAULT '0',
  `Lt_Operand_3` int NOT NULL DEFAULT '0',
  `Lt_Operand_4` int NOT NULL DEFAULT '0',
  `Lt_Operand_5` int NOT NULL DEFAULT '0',
  `Operator_1` tinyint unsigned NOT NULL DEFAULT '0',
  `Operator_2` tinyint unsigned NOT NULL DEFAULT '0',
  `Operator_3` tinyint unsigned NOT NULL DEFAULT '0',
  `Operator_4` tinyint unsigned NOT NULL DEFAULT '0',
  `Operator_5` tinyint unsigned NOT NULL DEFAULT '0',
  `Rt_OperandType_1` tinyint unsigned NOT NULL DEFAULT '0',
  `Rt_OperandType_2` tinyint unsigned NOT NULL DEFAULT '0',
  `Rt_OperandType_3` tinyint unsigned NOT NULL DEFAULT '0',
  `Rt_OperandType_4` tinyint unsigned NOT NULL DEFAULT '0',
  `Rt_OperandType_5` tinyint unsigned NOT NULL DEFAULT '0',
  `Rt_Operand_1` int NOT NULL DEFAULT '0',
  `Rt_Operand_2` int NOT NULL DEFAULT '0',
  `Rt_Operand_3` int NOT NULL DEFAULT '0',
  `Rt_Operand_4` int NOT NULL DEFAULT '0',
  `Rt_Operand_5` int NOT NULL DEFAULT '0',
  `Logic_1` tinyint unsigned NOT NULL DEFAULT '0',
  `Logic_2` tinyint unsigned NOT NULL DEFAULT '0',
  `Logic_3` tinyint unsigned NOT NULL DEFAULT '0',
  `Logic_4` tinyint unsigned NOT NULL DEFAULT '0',
  `Logic_5` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellradius_dbc`
--

DROP TABLE IF EXISTS `spellradius_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellradius_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Radius` float NOT NULL DEFAULT '0',
  `RadiusPerLevel` float NOT NULL DEFAULT '0',
  `RadiusMax` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellrange_dbc`
--

DROP TABLE IF EXISTS `spellrange_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellrange_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `RangeMin_1` float NOT NULL DEFAULT '0',
  `RangeMin_2` float NOT NULL DEFAULT '0',
  `RangeMax_1` float NOT NULL DEFAULT '0',
  `RangeMax_2` float NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `DisplayName_Lang_enUS` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_enGB` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_koKR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_frFR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_deDE` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_enCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_zhCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_enTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_zhTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_esES` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_esMX` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_ruRU` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_ptPT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_ptBR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_itIT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_Unk` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayName_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `DisplayNameShort_Lang_enUS` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_enGB` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_koKR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_frFR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_deDE` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_enCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_zhCN` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_enTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_zhTW` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_esES` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_esMX` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_ruRU` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_ptPT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_ptBR` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_itIT` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_Unk` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `DisplayNameShort_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellrunecost_dbc`
--

DROP TABLE IF EXISTS `spellrunecost_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellrunecost_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Blood` int NOT NULL DEFAULT '0',
  `Unholy` int NOT NULL DEFAULT '0',
  `Frost` int NOT NULL DEFAULT '0',
  `RunicPower` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellshapeshiftform_dbc`
--

DROP TABLE IF EXISTS `spellshapeshiftform_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellshapeshiftform_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `BonusActionBar` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `CreatureType` int NOT NULL DEFAULT '0',
  `AttackIconID` int NOT NULL DEFAULT '0',
  `CombatRoundTime` int NOT NULL DEFAULT '0',
  `CreatureDisplayID_1` int NOT NULL DEFAULT '0',
  `CreatureDisplayID_2` int NOT NULL DEFAULT '0',
  `CreatureDisplayID_3` int NOT NULL DEFAULT '0',
  `CreatureDisplayID_4` int NOT NULL DEFAULT '0',
  `PresetSpellID_1` int NOT NULL DEFAULT '0',
  `PresetSpellID_2` int NOT NULL DEFAULT '0',
  `PresetSpellID_3` int NOT NULL DEFAULT '0',
  `PresetSpellID_4` int NOT NULL DEFAULT '0',
  `PresetSpellID_5` int NOT NULL DEFAULT '0',
  `PresetSpellID_6` int NOT NULL DEFAULT '0',
  `PresetSpellID_7` int NOT NULL DEFAULT '0',
  `PresetSpellID_8` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `spellvisual_dbc`
--

DROP TABLE IF EXISTS `spellvisual_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `spellvisual_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `PrecastKit` int NOT NULL DEFAULT '0',
  `CastKit` int NOT NULL DEFAULT '0',
  `ImpactKit` int NOT NULL DEFAULT '0',
  `StateKit` int NOT NULL DEFAULT '0',
  `StateDoneKit` int NOT NULL DEFAULT '0',
  `ChannelKit` int NOT NULL DEFAULT '0',
  `HasMissile` int NOT NULL DEFAULT '0',
  `MissileModel` int NOT NULL DEFAULT '0',
  `MissilePathType` int NOT NULL DEFAULT '0',
  `MissileDestinationAttachment` int NOT NULL DEFAULT '0',
  `MissileSound` int NOT NULL DEFAULT '0',
  `AnimEventSoundID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `CasterImpactKit` int NOT NULL DEFAULT '0',
  `TargetImpactKit` int NOT NULL DEFAULT '0',
  `MissileAttachment` int NOT NULL DEFAULT '0',
  `MissileFollowGroundHeight` int NOT NULL DEFAULT '0',
  `MissileFollowGroundDropSpeed` int NOT NULL DEFAULT '0',
  `MissileFollowGroundApproach` int NOT NULL DEFAULT '0',
  `MissileFollowGroundFlags` int NOT NULL DEFAULT '0',
  `MissileMotion` int NOT NULL DEFAULT '0',
  `MissileTargetingKit` int NOT NULL DEFAULT '0',
  `InstantAreaKit` int NOT NULL DEFAULT '0',
  `ImpactAreaKit` int NOT NULL DEFAULT '0',
  `PersistentAreaKit` int NOT NULL DEFAULT '0',
  `MissileCastOffsetX` float NOT NULL DEFAULT '0',
  `MissileCastOffsetY` float NOT NULL DEFAULT '0',
  `MissileCastOffsetZ` float NOT NULL DEFAULT '0',
  `MissileImpactOffsetX` float NOT NULL DEFAULT '0',
  `MissileImpactOffsetY` float NOT NULL DEFAULT '0',
  `MissileImpactOffsetZ` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `stableslotprices_dbc`
--

DROP TABLE IF EXISTS `stableslotprices_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `stableslotprices_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Cost` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `summonproperties_dbc`
--

DROP TABLE IF EXISTS `summonproperties_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `summonproperties_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Control` int NOT NULL DEFAULT '0',
  `Faction` int NOT NULL DEFAULT '0',
  `Title` int NOT NULL DEFAULT '0',
  `Slot` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `talent_dbc`
--

DROP TABLE IF EXISTS `talent_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `talent_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `TabID` int NOT NULL DEFAULT '0',
  `TierID` int NOT NULL DEFAULT '0',
  `ColumnIndex` int NOT NULL DEFAULT '0',
  `SpellRank_1` int NOT NULL DEFAULT '0',
  `SpellRank_2` int NOT NULL DEFAULT '0',
  `SpellRank_3` int NOT NULL DEFAULT '0',
  `SpellRank_4` int NOT NULL DEFAULT '0',
  `SpellRank_5` int NOT NULL DEFAULT '0',
  `SpellRank_6` int NOT NULL DEFAULT '0',
  `SpellRank_7` int NOT NULL DEFAULT '0',
  `SpellRank_8` int NOT NULL DEFAULT '0',
  `SpellRank_9` int NOT NULL DEFAULT '0',
  `PrereqTalent_1` int NOT NULL DEFAULT '0',
  `PrereqTalent_2` int NOT NULL DEFAULT '0',
  `PrereqTalent_3` int NOT NULL DEFAULT '0',
  `PrereqRank_1` int NOT NULL DEFAULT '0',
  `PrereqRank_2` int NOT NULL DEFAULT '0',
  `PrereqRank_3` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `RequiredSpellID` int NOT NULL DEFAULT '0',
  `CategoryMask_1` int NOT NULL DEFAULT '0',
  `CategoryMask_2` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `talenttab_dbc`
--

DROP TABLE IF EXISTS `talenttab_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `talenttab_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `SpellIconID` int NOT NULL DEFAULT '0',
  `RaceMask` int NOT NULL DEFAULT '0',
  `ClassMask` int NOT NULL DEFAULT '0',
  `PetTalentMask` int NOT NULL DEFAULT '0',
  `OrderIndex` int NOT NULL DEFAULT '0',
  `BackgroundFile` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `taxinodes_dbc`
--

DROP TABLE IF EXISTS `taxinodes_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `taxinodes_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `ContinentID` int NOT NULL DEFAULT '0',
  `X` float NOT NULL DEFAULT '0',
  `Y` float NOT NULL DEFAULT '0',
  `Z` float NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `MountCreatureID_1` int NOT NULL DEFAULT '0',
  `MountCreatureID_2` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `taxipath_dbc`
--

DROP TABLE IF EXISTS `taxipath_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `taxipath_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `FromTaxiNode` int NOT NULL DEFAULT '0',
  `ToTaxiNode` int NOT NULL DEFAULT '0',
  `Cost` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `taxipathnode_dbc`
--

DROP TABLE IF EXISTS `taxipathnode_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `taxipathnode_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `PathID` int NOT NULL DEFAULT '0',
  `NodeIndex` int NOT NULL DEFAULT '0',
  `ContinentID` int NOT NULL DEFAULT '0',
  `LocX` float NOT NULL DEFAULT '0',
  `LocY` float NOT NULL DEFAULT '0',
  `LocZ` float NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `Delay` int NOT NULL DEFAULT '0',
  `ArrivalEventID` int NOT NULL DEFAULT '0',
  `DepartureEventID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `teamcontributionpoints_dbc`
--

DROP TABLE IF EXISTS `teamcontributionpoints_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `teamcontributionpoints_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Data` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `totemcategory_dbc`
--

DROP TABLE IF EXISTS `totemcategory_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `totemcategory_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Name_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `Name_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  `TotemCategoryType` int NOT NULL DEFAULT '0',
  `TotemCategoryMask` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `trainer`
--

DROP TABLE IF EXISTS `trainer`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `trainer` (
  `Id` int unsigned NOT NULL DEFAULT '0',
  `Type` tinyint unsigned NOT NULL DEFAULT '2',
  `Requirement` mediumint unsigned NOT NULL DEFAULT '0',
  `Greeting` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci,
  `VerifiedBuild` int DEFAULT '0',
  PRIMARY KEY (`Id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `trainer_locale`
--

DROP TABLE IF EXISTS `trainer_locale`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `trainer_locale` (
  `Id` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL,
  `Greeting_lang` mediumtext CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci,
  `VerifiedBuild` int DEFAULT '0',
  PRIMARY KEY (`Id`,`locale`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `trainer_spell`
--

DROP TABLE IF EXISTS `trainer_spell`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `trainer_spell` (
  `TrainerId` int unsigned NOT NULL DEFAULT '0',
  `SpellId` int unsigned NOT NULL DEFAULT '0',
  `MoneyCost` int unsigned NOT NULL DEFAULT '0',
  `ReqSkillLine` int unsigned NOT NULL DEFAULT '0',
  `ReqSkillRank` int unsigned NOT NULL DEFAULT '0',
  `ReqAbility1` int unsigned NOT NULL DEFAULT '0',
  `ReqAbility2` int unsigned NOT NULL DEFAULT '0',
  `ReqAbility3` int unsigned NOT NULL DEFAULT '0',
  `ReqLevel` tinyint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int DEFAULT '0',
  PRIMARY KEY (`TrainerId`,`SpellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `transportanimation_dbc`
--

DROP TABLE IF EXISTS `transportanimation_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `transportanimation_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `TransportID` int NOT NULL DEFAULT '0',
  `TimeIndex` int NOT NULL DEFAULT '0',
  `PosX` float NOT NULL DEFAULT '0',
  `PosY` float NOT NULL DEFAULT '0',
  `PosZ` float NOT NULL DEFAULT '0',
  `SequenceID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `transportrotation_dbc`
--

DROP TABLE IF EXISTS `transportrotation_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `transportrotation_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `GameObjectsID` int NOT NULL DEFAULT '0',
  `TimeIndex` int NOT NULL DEFAULT '0',
  `RotX` float NOT NULL DEFAULT '0',
  `RotY` float NOT NULL DEFAULT '0',
  `RotZ` float NOT NULL DEFAULT '0',
  `RotW` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `transports`
--

DROP TABLE IF EXISTS `transports`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `transports` (
  `guid` int unsigned NOT NULL AUTO_INCREMENT,
  `entry` int unsigned NOT NULL DEFAULT '0',
  `name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ScriptName` char(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`guid`),
  UNIQUE KEY `idx_entry` (`entry`)
) ENGINE=InnoDB AUTO_INCREMENT=21 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Transports';
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
-- Table structure for table `vehicle_accessory`
--

DROP TABLE IF EXISTS `vehicle_accessory`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `vehicle_accessory` (
  `guid` int unsigned NOT NULL DEFAULT '0',
  `accessory_entry` int unsigned NOT NULL DEFAULT '0',
  `seat_id` tinyint NOT NULL DEFAULT '0',
  `minion` tinyint unsigned NOT NULL DEFAULT '0',
  `description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `summontype` tinyint unsigned NOT NULL DEFAULT '6' COMMENT 'see enum TempSummonType',
  `summontimer` int unsigned NOT NULL DEFAULT '30000' COMMENT 'timer, only relevant for certain summontypes',
  PRIMARY KEY (`guid`,`seat_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `vehicle_dbc`
--

DROP TABLE IF EXISTS `vehicle_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `vehicle_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `TurnSpeed` float NOT NULL DEFAULT '0',
  `PitchSpeed` float NOT NULL DEFAULT '0',
  `PitchMin` float NOT NULL DEFAULT '0',
  `PitchMax` float NOT NULL DEFAULT '0',
  `SeatID_1` int NOT NULL DEFAULT '0',
  `SeatID_2` int NOT NULL DEFAULT '0',
  `SeatID_3` int NOT NULL DEFAULT '0',
  `SeatID_4` int NOT NULL DEFAULT '0',
  `SeatID_5` int NOT NULL DEFAULT '0',
  `SeatID_6` int NOT NULL DEFAULT '0',
  `SeatID_7` int NOT NULL DEFAULT '0',
  `SeatID_8` int NOT NULL DEFAULT '0',
  `MouseLookOffsetPitch` float NOT NULL DEFAULT '0',
  `CameraFadeDistScalarMin` float NOT NULL DEFAULT '0',
  `CameraFadeDistScalarMax` float NOT NULL DEFAULT '0',
  `CameraPitchOffset` float NOT NULL DEFAULT '0',
  `FacingLimitRight` float NOT NULL DEFAULT '0',
  `FacingLimitLeft` float NOT NULL DEFAULT '0',
  `MsslTrgtTurnLingering` float NOT NULL DEFAULT '0',
  `MsslTrgtPitchLingering` float NOT NULL DEFAULT '0',
  `MsslTrgtMouseLingering` float NOT NULL DEFAULT '0',
  `MsslTrgtEndOpacity` float NOT NULL DEFAULT '0',
  `MsslTrgtArcSpeed` float NOT NULL DEFAULT '0',
  `MsslTrgtArcRepeat` float NOT NULL DEFAULT '0',
  `MsslTrgtArcWidth` float NOT NULL DEFAULT '0',
  `MsslTrgtImpactRadius_1` float NOT NULL DEFAULT '0',
  `MsslTrgtImpactRadius_2` float NOT NULL DEFAULT '0',
  `MsslTrgtArcTexture` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MsslTrgtImpactTexture` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MsslTrgtImpactModel_1` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `MsslTrgtImpactModel_2` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `CameraYawOffset` float NOT NULL DEFAULT '0',
  `UilocomotionType` int NOT NULL DEFAULT '0',
  `MsslTrgtImpactTexRadius` float NOT NULL DEFAULT '0',
  `VehicleUIIndicatorID` int NOT NULL DEFAULT '0',
  `PowerDisplayID_1` int NOT NULL DEFAULT '0',
  `PowerDisplayID_2` int NOT NULL DEFAULT '0',
  `PowerDisplayID_3` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `vehicle_seat_addon`
--

DROP TABLE IF EXISTS `vehicle_seat_addon`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `vehicle_seat_addon` (
  `SeatEntry` int unsigned NOT NULL COMMENT 'VehicleSeatEntry.dbc identifier',
  `SeatOrientation` float DEFAULT '0' COMMENT 'Seat Orientation override value',
  `ExitParamX` float DEFAULT '0',
  `ExitParamY` float DEFAULT '0',
  `ExitParamZ` float DEFAULT '0',
  `ExitParamO` float DEFAULT '0',
  `ExitParamValue` tinyint(1) DEFAULT '0',
  PRIMARY KEY (`SeatEntry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `vehicle_template_accessory`
--

DROP TABLE IF EXISTS `vehicle_template_accessory`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `vehicle_template_accessory` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `accessory_entry` int unsigned NOT NULL DEFAULT '0',
  `seat_id` tinyint NOT NULL DEFAULT '0',
  `minion` tinyint unsigned NOT NULL DEFAULT '0',
  `description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `summontype` tinyint unsigned NOT NULL DEFAULT '6' COMMENT 'see enum TempSummonType',
  `summontimer` int unsigned NOT NULL DEFAULT '30000' COMMENT 'timer, only relevant for certain summontypes',
  PRIMARY KEY (`entry`,`seat_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `vehicleseat_dbc`
--

DROP TABLE IF EXISTS `vehicleseat_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `vehicleseat_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `AttachmentID` int NOT NULL DEFAULT '0',
  `AttachmentOffsetX` float NOT NULL DEFAULT '0',
  `AttachmentOffsetY` float NOT NULL DEFAULT '0',
  `AttachmentOffsetZ` float NOT NULL DEFAULT '0',
  `EnterPreDelay` float NOT NULL DEFAULT '0',
  `EnterSpeed` float NOT NULL DEFAULT '0',
  `EnterGravity` float NOT NULL DEFAULT '0',
  `EnterMinDuration` float NOT NULL DEFAULT '0',
  `EnterMaxDuration` float NOT NULL DEFAULT '0',
  `EnterMinArcHeight` float NOT NULL DEFAULT '0',
  `EnterMaxArcHeight` float NOT NULL DEFAULT '0',
  `EnterAnimStart` int NOT NULL DEFAULT '0',
  `EnterAnimLoop` int NOT NULL DEFAULT '0',
  `RideAnimStart` int NOT NULL DEFAULT '0',
  `RideAnimLoop` int NOT NULL DEFAULT '0',
  `RideUpperAnimStart` int NOT NULL DEFAULT '0',
  `RideUpperAnimLoop` int NOT NULL DEFAULT '0',
  `ExitPreDelay` float NOT NULL DEFAULT '0',
  `ExitSpeed` float NOT NULL DEFAULT '0',
  `ExitGravity` float NOT NULL DEFAULT '0',
  `ExitMinDuration` float NOT NULL DEFAULT '0',
  `ExitMaxDuration` float NOT NULL DEFAULT '0',
  `ExitMinArcHeight` float NOT NULL DEFAULT '0',
  `ExitMaxArcHeight` float NOT NULL DEFAULT '0',
  `ExitAnimStart` int NOT NULL DEFAULT '0',
  `ExitAnimLoop` int NOT NULL DEFAULT '0',
  `ExitAnimEnd` int NOT NULL DEFAULT '0',
  `PassengerYaw` float NOT NULL DEFAULT '0',
  `PassengerPitch` float NOT NULL DEFAULT '0',
  `PassengerRoll` float NOT NULL DEFAULT '0',
  `PassengerAttachmentID` int NOT NULL DEFAULT '0',
  `VehicleEnterAnim` int NOT NULL DEFAULT '0',
  `VehicleExitAnim` int NOT NULL DEFAULT '0',
  `VehicleRideAnimLoop` int NOT NULL DEFAULT '0',
  `VehicleEnterAnimBone` int NOT NULL DEFAULT '0',
  `VehicleExitAnimBone` int NOT NULL DEFAULT '0',
  `VehicleRideAnimLoopBone` int NOT NULL DEFAULT '0',
  `VehicleEnterAnimDelay` float NOT NULL DEFAULT '0',
  `VehicleExitAnimDelay` float NOT NULL DEFAULT '0',
  `VehicleAbilityDisplay` int NOT NULL DEFAULT '0',
  `EnterUISoundID` int NOT NULL DEFAULT '0',
  `ExitUISoundID` int NOT NULL DEFAULT '0',
  `UiSkin` int NOT NULL DEFAULT '0',
  `FlagsB` int NOT NULL DEFAULT '0',
  `CameraEnteringDelay` float NOT NULL DEFAULT '0',
  `CameraEnteringDuration` float NOT NULL DEFAULT '0',
  `CameraExitingDelay` float NOT NULL DEFAULT '0',
  `CameraExitingDuration` float NOT NULL DEFAULT '0',
  `CameraOffsetX` float NOT NULL DEFAULT '0',
  `CameraOffsetY` float NOT NULL DEFAULT '0',
  `CameraOffsetZ` float NOT NULL DEFAULT '0',
  `CameraPosChaseRate` float NOT NULL DEFAULT '0',
  `CameraFacingChaseRate` float NOT NULL DEFAULT '0',
  `CameraEnteringZoom` float NOT NULL DEFAULT '0',
  `CameraSeatZoomMin` float NOT NULL DEFAULT '0',
  `CameraSeatZoomMax` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `version`
--

DROP TABLE IF EXISTS `version`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `version` (
  `core_version` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Core revision dumped at startup.',
  `core_revision` varchar(120) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `db_version` varchar(120) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'Version of world DB.',
  `cache_id` int DEFAULT '0',
  PRIMARY KEY (`core_version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Version Notes';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `warden_checks`
--

DROP TABLE IF EXISTS `warden_checks`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `warden_checks` (
  `id` smallint unsigned NOT NULL AUTO_INCREMENT,
  `type` tinyint unsigned DEFAULT NULL,
  `data` varchar(48) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `str` varchar(170) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `address` int unsigned DEFAULT NULL,
  `length` tinyint unsigned DEFAULT NULL,
  `result` varchar(24) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `comment` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=812 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `waypoint_data`
--

DROP TABLE IF EXISTS `waypoint_data`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `waypoint_data` (
  `id` int unsigned NOT NULL DEFAULT '0' COMMENT 'Creature GUID',
  `point` int unsigned NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float DEFAULT NULL,
  `velocity` float NOT NULL DEFAULT '0',
  `delay` int unsigned NOT NULL DEFAULT '0',
  `smoothTransition` tinyint NOT NULL DEFAULT '0',
  `move_type` int NOT NULL DEFAULT '0',
  `action` int NOT NULL DEFAULT '0',
  `action_chance` smallint NOT NULL DEFAULT '100',
  `wpguid` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`,`point`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `waypoint_data_addon`
--

DROP TABLE IF EXISTS `waypoint_data_addon`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `waypoint_data_addon` (
  `PathID` int unsigned NOT NULL,
  `PointID` int unsigned NOT NULL,
  `SplinePointIndex` int unsigned NOT NULL,
  `PositionX` float NOT NULL DEFAULT '0',
  `PositionY` float NOT NULL DEFAULT '0',
  `PositionZ` float NOT NULL DEFAULT '0',
  PRIMARY KEY (`PathID`,`PointID`,`SplinePointIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `waypoint_scripts`
--

DROP TABLE IF EXISTS `waypoint_scripts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `waypoint_scripts` (
  `id` int unsigned NOT NULL DEFAULT '0',
  `delay` int unsigned NOT NULL DEFAULT '0',
  `command` int unsigned NOT NULL DEFAULT '0',
  `datalong` int unsigned NOT NULL DEFAULT '0',
  `datalong2` int unsigned NOT NULL DEFAULT '0',
  `dataint` int unsigned NOT NULL DEFAULT '0',
  `x` float NOT NULL DEFAULT '0',
  `y` float NOT NULL DEFAULT '0',
  `z` float NOT NULL DEFAULT '0',
  `o` float NOT NULL DEFAULT '0',
  `guid` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `waypoints`
--

DROP TABLE IF EXISTS `waypoints`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `waypoints` (
  `entry` int unsigned NOT NULL DEFAULT '0',
  `pointid` int unsigned NOT NULL DEFAULT '0',
  `position_x` float NOT NULL DEFAULT '0',
  `position_y` float NOT NULL DEFAULT '0',
  `position_z` float NOT NULL DEFAULT '0',
  `orientation` float DEFAULT NULL,
  `delay` int unsigned NOT NULL DEFAULT '0',
  `point_comment` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  PRIMARY KEY (`entry`,`pointid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Creature waypoints';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `wmoareatable_dbc`
--

DROP TABLE IF EXISTS `wmoareatable_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `wmoareatable_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `WMOID` int NOT NULL DEFAULT '0',
  `NameSetID` int NOT NULL DEFAULT '0',
  `WMOGroupID` int NOT NULL DEFAULT '0',
  `SoundProviderPref` int NOT NULL DEFAULT '0',
  `SoundProviderPrefUnderwater` int NOT NULL DEFAULT '0',
  `AmbienceID` int NOT NULL DEFAULT '0',
  `ZoneMusic` int NOT NULL DEFAULT '0',
  `IntroSound` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `AreaTableID` int NOT NULL DEFAULT '0',
  `AreaName_Lang_enUS` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_enGB` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_koKR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_frFR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_deDE` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_enCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_zhCN` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_enTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_zhTW` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_esES` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_esMX` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_ruRU` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_ptPT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_ptBR` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_itIT` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_Unk` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `AreaName_Lang_Mask` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `worldmaparea_dbc`
--

DROP TABLE IF EXISTS `worldmaparea_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `worldmaparea_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `MapID` int NOT NULL DEFAULT '0',
  `AreaID` int NOT NULL DEFAULT '0',
  `AreaName` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `LocLeft` float NOT NULL DEFAULT '0',
  `LocRight` float NOT NULL DEFAULT '0',
  `LocTop` float NOT NULL DEFAULT '0',
  `LocBottom` float NOT NULL DEFAULT '0',
  `DisplayMapID` int NOT NULL DEFAULT '0',
  `DefaultDungeonFloor` int NOT NULL DEFAULT '0',
  `ParentWorldMapID` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `worldmapoverlay_dbc`
--

DROP TABLE IF EXISTS `worldmapoverlay_dbc`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `worldmapoverlay_dbc` (
  `ID` int NOT NULL DEFAULT '0',
  `MapAreaID` int NOT NULL DEFAULT '0',
  `AreaID_1` int NOT NULL DEFAULT '0',
  `AreaID_2` int NOT NULL DEFAULT '0',
  `AreaID_3` int NOT NULL DEFAULT '0',
  `AreaID_4` int NOT NULL DEFAULT '0',
  `MapPointX` int NOT NULL DEFAULT '0',
  `MapPointY` int NOT NULL DEFAULT '0',
  `TextureName` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL,
  `TextureWidth` int NOT NULL DEFAULT '0',
  `TextureHeight` int NOT NULL DEFAULT '0',
  `OffsetX` int NOT NULL DEFAULT '0',
  `OffsetY` int NOT NULL DEFAULT '0',
  `HitRectTop` int NOT NULL DEFAULT '0',
  `HitRectLeft` int NOT NULL DEFAULT '0',
  `HitRectBottom` int NOT NULL DEFAULT '0',
  `HitRectRight` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping events for database 'acore_world'
--

--
-- Dumping routines for database 'acore_world'
--
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2026-05-01 23:55:14
