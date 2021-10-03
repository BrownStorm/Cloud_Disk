# 创建数据库
DROP database IF EXISTS `cloud_disk`;
CREATE DATABASE `cloud_disk`;

# 使用数据库
use `cloud_disk`;


DROP TABLE IF EXISTS `file_info`;
CREATE TABLE `file_info` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `md5` varchar(256) NOT NULL,
  `file_id` varchar(256) NOT NULL,
  `url` varchar(512) NOT NULL,
  `size` bigint(20) DEFAULT '0',
  `type` varchar(32) DEFAULT '',
  `count` int(11) DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=70 DEFAULT CHARSET=utf8;


DROP TABLE IF EXISTS `share_file_list`;
CREATE TABLE `share_file_list` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `user` varchar(32) NOT NULL,
  `md5` varchar(256) NOT NULL,
  `file_name` varchar(128) DEFAULT NULL,
  `pv` int(11) DEFAULT '1',
  `create_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=16 DEFAULT CHARSET=utf8;


DROP TABLE IF EXISTS `user_file_count`;
CREATE TABLE `user_file_count` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `user` varchar(128) NOT NULL,
  `count` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `user_UNIQUE` (`user`)
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8;


DROP TABLE IF EXISTS `user_file_list`;
CREATE TABLE `user_file_list` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `user` varchar(32) NOT NULL,
  `md5` varchar(256) NOT NULL,
  `create_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `file_name` varchar(128) DEFAULT NULL,
  `shared_status` int(11) DEFAULT NULL,
  `pv` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=30 DEFAULT CHARSET=utf8;


DROP TABLE IF EXISTS `user_info`;
CREATE TABLE `user_info` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `user_name` varchar(32) NOT NULL DEFAULT '',
  `nick_name` varchar(32) CHARACTER SET utf8mb4 NOT NULL DEFAULT '',
  `password` varchar(32) NOT NULL DEFAULT '',
  `phone` varchar(16) NOT NULL DEFAULT '',
  `email` varchar(64) DEFAULT '',
  `create_time` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_nick_name` (`nick_name`),
  UNIQUE KEY `uq_user_name` (`user_name`)
) ENGINE=InnoDB AUTO_INCREMENT=14 DEFAULT CHARSET=utf8;