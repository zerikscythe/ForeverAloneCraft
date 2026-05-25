CREATE TABLE IF NOT EXISTS `living_world_task_point_link` (
  `from_point_key` varchar(64) NOT NULL,
  `to_point_key` varchar(64) NOT NULL,
  `link_kind` varchar(32) NOT NULL DEFAULT 'local_nav',
  `manual_verified` tinyint(1) NOT NULL DEFAULT 0,
  `success_count` int unsigned NOT NULL DEFAULT 0,
  `failure_count` int unsigned NOT NULL DEFAULT 0,
  `first_seen_at` datetime NULL DEFAULT NULL,
  `last_seen_at` datetime NULL DEFAULT NULL,
  `last_success_at` datetime NULL DEFAULT NULL,
  `last_failure_at` datetime NULL DEFAULT NULL,
  `source` varchar(32) NOT NULL DEFAULT 'debug_route_harness',
  `notes` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`from_point_key`, `to_point_key`, `link_kind`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
