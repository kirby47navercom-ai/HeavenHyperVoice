-- 004_characters.sql — 캐릭터 테이블 분리
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 지금까지 닉네임은 accounts 에 있었다. 계정 하나에 캐릭터 하나였기 때문인데,
-- 닉네임은 원래 캐릭터 속성이지 계정 속성이 아니다. 캐릭터를 여러 개 만들 수
-- 있게 되면서 제자리를 찾아준다.
--
-- 나누는 진짜 이유는 성능이 아니라 소유권이다. LoginServer 는 accounts 만,
-- 필드/DB 서버는 characters 만 보면 된다. 같은 테이블이면 필드 서버가
-- password_hash 를 읽는 것을 권한으로 막을 방법이 없다.
--
-- 되돌릴 수 없다: accounts.nickname 을 드롭한다.
-- apply-migrations.ps1 이 먼저 mysqldump 백업을 뜬다.

USE hhv;

CREATE TABLE IF NOT EXISTS characters (
  id             BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  account_id     BIGINT UNSIGNED NOT NULL,
  nickname       VARCHAR(32)     NOT NULL,

  level          INT UNSIGNED    NOT NULL DEFAULT 1,

  -- 필드 서버가 쓸 자리. 지금은 아무도 읽지 않지만, 컬럼 추가 마이그레이션을
  -- 한 번 아끼려고 미리 둔다. 실시간 위치는 Redis 에 있고 여기는 마지막 저장분이다.
  map_id         INT UNSIGNED    NOT NULL DEFAULT 0,
  pos_x          FLOAT           NOT NULL DEFAULT 0,
  pos_y          FLOAT           NOT NULL DEFAULT 0,
  pos_z          FLOAT           NOT NULL DEFAULT 0,

  created_at     DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  last_played_at DATETIME(3)     NULL,

  PRIMARY KEY (id),

  -- accounts 에 있던 제약을 그대로 옮긴다. 콜레이션이 대소문자를 구분하지
  -- 않으므로 Alice 와 alice 가 충돌한다. 사칭 방지.
  UNIQUE KEY uq_nickname (nickname),
  KEY idx_account (account_id),

  CONSTRAINT fk_char_account FOREIGN KEY (account_id)
    REFERENCES accounts (id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 기존 계정의 닉네임을 캐릭터로 옮긴다. 이미 옮겼으면 아무것도 하지 않는다.
INSERT INTO characters (account_id, nickname)
SELECT a.id, a.nickname
FROM accounts a
WHERE EXISTS (SELECT 1 FROM information_schema.columns
              WHERE table_schema = 'hhv' AND table_name = 'accounts'
                AND column_name = 'nickname')
  AND NOT EXISTS (SELECT 1 FROM characters c WHERE c.account_id = a.id);

-- 컬럼을 드롭한다. 인덱스 uq_nickname 도 함께 사라진다.
-- 여러 번 실행해도 안전하도록 있을 때만 실행한다.
SET @drop_nickname := (
  SELECT IF(COUNT(*) > 0,
            'ALTER TABLE accounts DROP COLUMN nickname',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'accounts' AND column_name = 'nickname'
);
PREPARE stmt FROM @drop_nickname;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

INSERT IGNORE INTO schema_migrations (version) VALUES ('004_characters');

SELECT '004_characters applied' AS result;
SELECT COUNT(*) AS characters_migrated FROM characters;
