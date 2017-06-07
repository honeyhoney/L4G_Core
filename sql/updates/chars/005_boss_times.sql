CREATE TABLE IF NOT EXISTS `boss_times` (
  `instance_id` int(11) NOT NULL DEFAULT '0',
  `creature_id` int(11) NOT NULL DEFAULT '0',
  `start_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `end_time` timestamp NULL DEFAULT NULL,
  `players` longtext,
  `deaths` int(11) DEFAULT '0',
  PRIMARY KEY (`instance_id`,`creature_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;