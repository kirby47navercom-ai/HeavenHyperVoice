-- 001_accounts.sql — 계정 스키마
--
-- 실행:
--   & "C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe" -u root -p < Server\DataBase\001_accounts.sql
--
-- 이 파일에는 비밀번호가 들어가지 않는다. 앱 계정 생성은 002 를 볼 것.
-- 여러 번 실행해도 안전하다.

CREATE DATABASE IF NOT EXISTS hhv
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_0900_ai_ci;

USE hhv;

-- 어떤 마이그레이션이 적용됐는지 기록한다.
CREATE TABLE IF NOT EXISTS schema_migrations (
  version    VARCHAR(64) NOT NULL,
  applied_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  PRIMARY KEY (version)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS accounts (
  id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  username      VARCHAR(32)     NOT NULL,
  nickname      VARCHAR(32)     NOT NULL,

  -- argon2id 인코딩 문자열. 솔트와 파라미터가 이 안에 들어 있으므로
  -- 별도 솔트 컬럼이 필요 없고, 나중에 파라미터를 올려도 기존 행이 그대로 검증된다.
  --   $argon2id$v=19$m=19456,t=2,p=1$<salt>$<hash>
  password_hash VARCHAR(255)    NOT NULL,

  -- 계정을 정지하거나 강제 로그아웃할 때 올린다. 티켓에 실어 보내면
  -- 검증하는 서버가 DB 를 보지 않고도 구 티켓을 걸러낼 수 있다.
  token_version INT UNSIGNED    NOT NULL DEFAULT 0,

  status        ENUM('active','suspended','deleted') NOT NULL DEFAULT 'active',
  created_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  last_login_at DATETIME(3)     NULL,

  PRIMARY KEY (id),

  -- 콜레이션이 대소문자를 구분하지 않으므로 Alice 와 alice 가 충돌한다. 사칭 방지.
  UNIQUE KEY uq_username (username),
  UNIQUE KEY uq_nickname (nickname)
) ENGINE=InnoDB;

INSERT IGNORE INTO schema_migrations (version) VALUES ('001_accounts');

SELECT '001_accounts applied' AS result;
