-- ============================================================
-- IoT WOMEN'S SAFETY DEVICE - DATABASE SCHEMA
-- Database: womens_safety_db
-- MySQL Version: 5.7+
-- XAMPP Compatible
-- ============================================================

CREATE DATABASE IF NOT EXISTS `womens_safety_db`
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE `womens_safety_db`;

-- ============================================================
-- TABLE: users (Registered Users / Device Owners)
-- ============================================================
CREATE TABLE IF NOT EXISTS `users` (
  `id`              INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `full_name`       VARCHAR(100) NOT NULL,
  `email`           VARCHAR(150) NOT NULL UNIQUE,
  `phone`           VARCHAR(20) NOT NULL,
  `password_hash`   VARCHAR(255) NOT NULL,
  `profile_photo`   VARCHAR(255) DEFAULT 'default.png',
  `address`         TEXT DEFAULT NULL,
  `blood_group`     ENUM('A+','A-','B+','B-','AB+','AB-','O+','O-') DEFAULT 'O+',
  `medical_notes`   TEXT DEFAULT NULL,
  `role`            ENUM('admin','user','guardian') DEFAULT 'user',
  `is_active`       TINYINT(1) DEFAULT 1,
  `last_login`      DATETIME DEFAULT NULL,
  `created_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  `updated_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  INDEX `idx_email` (`email`),
  INDEX `idx_phone` (`phone`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: devices (ESP32 Wearable Devices)
-- ============================================================
CREATE TABLE IF NOT EXISTS `devices` (
  `id`              INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `device_id`       VARCHAR(50) NOT NULL UNIQUE,
  `device_name`     VARCHAR(100) DEFAULT 'Safety Wearable',
  `user_id`         INT(11) UNSIGNED NOT NULL,
  `firmware_version`VARCHAR(20) DEFAULT '2.0',
  `imei`            VARCHAR(20) DEFAULT NULL,
  `sim_number`      VARCHAR(20) DEFAULT NULL,
  `battery_level`   TINYINT(3) DEFAULT 100,
  `is_online`       TINYINT(1) DEFAULT 0,
  `last_seen`       DATETIME DEFAULT NULL,
  `wifi_ssid`       VARCHAR(100) DEFAULT NULL,
  `ip_address`      VARCHAR(45) DEFAULT NULL,
  `created_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  `updated_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  INDEX `idx_device_id` (`device_id`),
  FOREIGN KEY (`user_id`) REFERENCES `users`(`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: emergency_contacts
-- ============================================================
CREATE TABLE IF NOT EXISTS `emergency_contacts` (
  `id`              INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `user_id`         INT(11) UNSIGNED NOT NULL,
  `contact_name`    VARCHAR(100) NOT NULL,
  `phone`           VARCHAR(20) NOT NULL,
  `email`           VARCHAR(150) DEFAULT NULL,
  `relationship`    VARCHAR(50) DEFAULT 'Family',
  `priority`        TINYINT(1) DEFAULT 1,
  `notify_sms`      TINYINT(1) DEFAULT 1,
  `notify_email`    TINYINT(1) DEFAULT 1,
  `notify_call`     TINYINT(1) DEFAULT 0,
  `is_active`       TINYINT(1) DEFAULT 1,
  `created_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  INDEX `idx_user_id` (`user_id`),
  FOREIGN KEY (`user_id`) REFERENCES `users`(`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: sensor_data (Real-time sensor readings)
-- ============================================================
CREATE TABLE IF NOT EXISTS `sensor_data` (
  `id`              BIGINT(20) UNSIGNED NOT NULL AUTO_INCREMENT,
  `device_id`       VARCHAR(50) NOT NULL,
  `user_id`         INT(11) UNSIGNED NOT NULL,
  `heart_rate`      FLOAT DEFAULT 0,
  `spo2`            FLOAT DEFAULT 0,
  `accel_x`         FLOAT DEFAULT 0,
  `accel_y`         FLOAT DEFAULT 0,
  `accel_z`         FLOAT DEFAULT 0,
  `gyro_x`          FLOAT DEFAULT 0,
  `gyro_y`          FLOAT DEFAULT 0,
  `gyro_z`          FLOAT DEFAULT 0,
  `temperature`     FLOAT DEFAULT 0,
  `vibration_count` INT(5) DEFAULT 0,
  `sound_level`     FLOAT DEFAULT 0,
  `battery_level`   TINYINT(3) DEFAULT 0,
  `latitude`        DECIMAL(10,8) DEFAULT 0,
  `longitude`       DECIMAL(11,8) DEFAULT 0,
  `altitude`        FLOAT DEFAULT 0,
  `speed`           FLOAT DEFAULT 0,
  `gps_fixed`       TINYINT(1) DEFAULT 0,
  `wifi_connected`  TINYINT(1) DEFAULT 0,
  `gsm_connected`   TINYINT(1) DEFAULT 0,
  `emergency_active`TINYINT(1) DEFAULT 0,
  `alert_type`      TINYINT(3) DEFAULT 0,
  `recorded_at`     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  INDEX `idx_device_id` (`device_id`),
  INDEX `idx_recorded_at` (`recorded_at`),
  INDEX `idx_user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: emergency_alerts
-- ============================================================
CREATE TABLE IF NOT EXISTS `emergency_alerts` (
  `id`              INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `device_id`       VARCHAR(50) NOT NULL,
  `user_id`         INT(11) UNSIGNED NOT NULL,
  `alert_type`      TINYINT(3) NOT NULL DEFAULT 0,
  `alert_name`      VARCHAR(100) NOT NULL,
  `reason`          TEXT DEFAULT NULL,
  `latitude`        DECIMAL(10,8) DEFAULT NULL,
  `longitude`       DECIMAL(11,8) DEFAULT NULL,
  `location_address`TEXT DEFAULT NULL,
  `heart_rate`      FLOAT DEFAULT 0,
  `spo2`            FLOAT DEFAULT 0,
  `battery_level`   TINYINT(3) DEFAULT 0,
  `sms_sent`        TINYINT(1) DEFAULT 0,
  `email_sent`      TINYINT(1) DEFAULT 0,
  `call_made`       TINYINT(1) DEFAULT 0,
  `acknowledged`    TINYINT(1) DEFAULT 0,
  `acknowledged_by` INT(11) UNSIGNED DEFAULT NULL,
  `acknowledged_at` DATETIME DEFAULT NULL,
  `resolved`        TINYINT(1) DEFAULT 0,
  `resolved_at`     DATETIME DEFAULT NULL,
  `notes`           TEXT DEFAULT NULL,
  `severity`        ENUM('low','medium','high','critical') DEFAULT 'high',
  `created_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  INDEX `idx_device_id` (`device_id`),
  INDEX `idx_user_id` (`user_id`),
  INDEX `idx_created_at` (`created_at`),
  INDEX `idx_acknowledged` (`acknowledged`),
  FOREIGN KEY (`user_id`) REFERENCES `users`(`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: location_history
-- ============================================================
CREATE TABLE IF NOT EXISTS `location_history` (
  `id`              BIGINT(20) UNSIGNED NOT NULL AUTO_INCREMENT,
  `device_id`       VARCHAR(50) NOT NULL,
  `user_id`         INT(11) UNSIGNED NOT NULL,
  `latitude`        DECIMAL(10,8) NOT NULL,
  `longitude`       DECIMAL(11,8) NOT NULL,
  `altitude`        FLOAT DEFAULT 0,
  `speed`           FLOAT DEFAULT 0,
  `accuracy`        FLOAT DEFAULT 0,
  `address`         TEXT DEFAULT NULL,
  `gps_fixed`       TINYINT(1) DEFAULT 1,
  `recorded_at`     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  INDEX `idx_device_id` (`device_id`),
  INDEX `idx_recorded_at` (`recorded_at`),
  INDEX `idx_user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: geofence_zones
-- ============================================================
CREATE TABLE IF NOT EXISTS `geofence_zones` (
  `id`              INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `user_id`         INT(11) UNSIGNED NOT NULL,
  `zone_name`       VARCHAR(100) NOT NULL,
  `latitude`        DECIMAL(10,8) NOT NULL,
  `longitude`       DECIMAL(11,8) NOT NULL,
  `radius`          FLOAT DEFAULT 500,
  `zone_type`       ENUM('safe','danger','home','work','school') DEFAULT 'safe',
  `alert_on_enter`  TINYINT(1) DEFAULT 0,
  `alert_on_exit`   TINYINT(1) DEFAULT 1,
  `is_active`       TINYINT(1) DEFAULT 1,
  `created_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  FOREIGN KEY (`user_id`) REFERENCES `users`(`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: alert_notifications (SMS/Email logs)
-- ============================================================
CREATE TABLE IF NOT EXISTS `alert_notifications` (
  `id`              INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `alert_id`        INT(11) UNSIGNED NOT NULL,
  `contact_id`      INT(11) UNSIGNED DEFAULT NULL,
  `notification_type` ENUM('sms','email','call','push') DEFAULT 'sms',
  `recipient`       VARCHAR(150) NOT NULL,
  `message`         TEXT NOT NULL,
  `status`          ENUM('pending','sent','failed','delivered') DEFAULT 'pending',
  `sent_at`         DATETIME DEFAULT NULL,
  `error_message`   TEXT DEFAULT NULL,
  `created_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  INDEX `idx_alert_id` (`alert_id`),
  FOREIGN KEY (`alert_id`) REFERENCES `emergency_alerts`(`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: device_commands (Remote commands to device)
-- ============================================================
CREATE TABLE IF NOT EXISTS `device_commands` (
  `id`              INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `device_id`       VARCHAR(50) NOT NULL,
  `command`         VARCHAR(50) NOT NULL,
  `params`          JSON DEFAULT NULL,
  `issued_by`       INT(11) UNSIGNED DEFAULT NULL,
  `status`          ENUM('pending','sent','executed','failed') DEFAULT 'pending',
  `created_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  `executed_at`     DATETIME DEFAULT NULL,
  PRIMARY KEY (`id`),
  INDEX `idx_device_id` (`device_id`),
  INDEX `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: system_settings
-- ============================================================
CREATE TABLE IF NOT EXISTS `system_settings` (
  `id`              INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
  `setting_key`     VARCHAR(100) NOT NULL UNIQUE,
  `setting_value`   TEXT NOT NULL,
  `description`     TEXT DEFAULT NULL,
  `updated_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: activity_logs
-- ============================================================
CREATE TABLE IF NOT EXISTS `activity_logs` (
  `id`              BIGINT(20) UNSIGNED NOT NULL AUTO_INCREMENT,
  `user_id`         INT(11) UNSIGNED DEFAULT NULL,
  `action`          VARCHAR(100) NOT NULL,
  `description`     TEXT DEFAULT NULL,
  `ip_address`      VARCHAR(45) DEFAULT NULL,
  `user_agent`      TEXT DEFAULT NULL,
  `created_at`      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  INDEX `idx_user_id` (`user_id`),
  INDEX `idx_created_at` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- TABLE: heart_rate_history (Analytics)
-- ============================================================
CREATE TABLE IF NOT EXISTS `heart_rate_history` (
  `id`              BIGINT(20) UNSIGNED NOT NULL AUTO_INCREMENT,
  `device_id`       VARCHAR(50) NOT NULL,
  `user_id`         INT(11) UNSIGNED NOT NULL,
  `heart_rate`      FLOAT NOT NULL,
  `spo2`            FLOAT DEFAULT 0,
  `status`          ENUM('normal','medium','high','low','critical') DEFAULT 'normal',
  `recorded_at`     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  INDEX `idx_device_id` (`device_id`),
  INDEX `idx_recorded_at` (`recorded_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ============================================================
-- INSERT DEFAULT DATA
-- ============================================================

-- Default Admin User (password: Admin@1234)
INSERT INTO `users` (`full_name`, `email`, `phone`, `password_hash`, `role`) VALUES
('System Admin', 'admin@safetywear.com', '+919999999999',
 '$2y$10$9zBXeHUkl.ESK9LfZh/52OTZcvbv9CUlTYGpZxQX59MM3Kx01QEvi', 'admin');

-- Wife User (password: password)
INSERT INTO `users` (`full_name`, `email`, `phone`, `password_hash`, `role`, `blood_group`) VALUES
('Wife User', 'wife@example.com', '+91XXXXXXXXXX',
 '$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2.uheWG/igi', 'user', 'O+');

-- Wife ESP32 Device
INSERT INTO `devices` (`device_id`, `device_name`, `user_id`, `firmware_version`) VALUES
('WIFE_ESP32_001', 'Wife ESP32 Safety Device', 2, '3.0');

-- Emergency Contacts for Wife User
INSERT INTO `emergency_contacts` (`user_id`, `contact_name`, `phone`, `relationship`, `priority`) VALUES
(2, 'Husband', '+91XXXXXXXXXX', 'Husband', 1),
(2, 'Family Contact', '+91XXXXXXXXXX', 'Family', 2),
(2, 'Police Emergency', '100', 'Police', 3);

-- Default System Settings
INSERT INTO `system_settings` (`setting_key`, `setting_value`, `description`) VALUES
('site_name', 'Women Safety Dashboard', 'Application name'),
('alert_email', 'alerts@safetywear.com', 'Default alert email'),
('sms_gateway', 'twilio', 'SMS gateway provider'),
('gps_update_interval', '10', 'GPS update interval in seconds'),
('heart_rate_high', '130', 'High heart rate threshold'),
('heart_rate_low', '45', 'Low heart rate threshold'),
('fall_threshold', '2.5', 'Fall detection g-force threshold'),
('geofence_enabled', '1', 'Enable geofence alerts'),
('data_retention_days', '90', 'Days to retain sensor data'),
('timezone', 'Asia/Kolkata', 'System timezone');

-- Default Geofence (Home zone)
INSERT INTO `geofence_zones` (`user_id`, `zone_name`, `latitude`, `longitude`, `radius`, `zone_type`) VALUES
(2, 'Home', 12.971598, 77.594562, 300, 'home'),
(2, 'College', 12.985678, 77.601234, 500, 'school');

-- ============================================================
-- VIEWS FOR DASHBOARD
-- ============================================================

CREATE OR REPLACE VIEW `vw_active_alerts` AS
SELECT 
  ea.id, ea.device_id, ea.alert_name, ea.reason, ea.severity,
  ea.latitude, ea.longitude, ea.heart_rate, ea.battery_level,
  ea.created_at, ea.acknowledged, ea.resolved,
  u.full_name, u.phone, u.blood_group,
  d.device_name
FROM emergency_alerts ea
JOIN users u ON ea.user_id = u.id
JOIN devices d ON ea.device_id = d.device_id
WHERE ea.resolved = 0
ORDER BY ea.created_at DESC;

CREATE OR REPLACE VIEW `vw_device_status` AS
SELECT 
  d.device_id, d.device_name, d.battery_level, d.is_online, d.last_seen,
  u.full_name, u.phone, u.blood_group,
  sd.heart_rate, sd.spo2, sd.latitude, sd.longitude,
  sd.temperature, sd.vibration_count, sd.sound_level, sd.emergency_active,
  sd.recorded_at as last_data_time
FROM devices d
JOIN users u ON d.user_id = u.id
LEFT JOIN sensor_data sd ON d.device_id = sd.device_id
  AND sd.id = (SELECT MAX(id) FROM sensor_data WHERE device_id = d.device_id)
ORDER BY d.id;

CREATE OR REPLACE VIEW `vw_daily_stats` AS
SELECT 
  DATE(recorded_at) as stat_date,
  device_id,
  COUNT(*) as total_readings,
  AVG(heart_rate) as avg_heart_rate,
  MAX(heart_rate) as max_heart_rate,
  MIN(heart_rate) as min_heart_rate,
  AVG(spo2) as avg_spo2,
  AVG(battery_level) as avg_battery,
  COUNT(CASE WHEN emergency_active = 1 THEN 1 END) as emergency_count
FROM sensor_data
GROUP BY DATE(recorded_at), device_id;

-- ============================================================
-- STORED PROCEDURES
-- ============================================================

DELIMITER //

DROP PROCEDURE IF EXISTS `sp_get_latest_sensor_data` //
CREATE PROCEDURE `sp_get_latest_sensor_data`(IN p_device_id VARCHAR(50))
BEGIN
  SELECT * FROM sensor_data 
  WHERE device_id = p_device_id 
  ORDER BY recorded_at DESC 
  LIMIT 1;
END //

DROP PROCEDURE IF EXISTS `sp_get_location_track` //
CREATE PROCEDURE `sp_get_location_track`(
  IN p_device_id VARCHAR(50), 
  IN p_hours INT
)
BEGIN
  SELECT latitude, longitude, speed, altitude, recorded_at
  FROM location_history
  WHERE device_id = p_device_id
    AND recorded_at >= DATE_SUB(NOW(), INTERVAL p_hours HOUR)
  ORDER BY recorded_at ASC;
END //

DROP PROCEDURE IF EXISTS `sp_cleanup_old_data` //
CREATE PROCEDURE `sp_cleanup_old_data`(IN p_days INT)
BEGIN
  DELETE FROM sensor_data WHERE recorded_at < DATE_SUB(NOW(), INTERVAL p_days DAY);
  DELETE FROM location_history WHERE recorded_at < DATE_SUB(NOW(), INTERVAL p_days DAY);
  DELETE FROM activity_logs WHERE created_at < DATE_SUB(NOW(), INTERVAL p_days DAY);
END //

DELIMITER ;
