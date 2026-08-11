ALTER TABLE `updates`
    MODIFY COLUMN `state` ENUM('RELEASED', 'CUSTOM', 'MODULE', 'ARCHIVED', 'PENDING')
    CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'RELEASED'
    COMMENT 'defines if an update is released or archived.';
