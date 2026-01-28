-- Server state store schema (MySQL-compatible).
CREATE TABLE IF NOT EXISTS mi_state_blob (
  scope VARCHAR(64) NOT NULL,
  key_name VARCHAR(191) NOT NULL,
  version BIGINT NOT NULL DEFAULT 0,
  payload LONGBLOB NOT NULL,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
    ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (scope, key_name)
) ENGINE=InnoDB;
