-- 007_character_delete.sql — 캐릭터 삭제
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 캐릭터는 행을 지우지 않고 deleted_at 을 채운다. 실수로 지운 캐릭터를 되살릴
-- 방법이 있어야 하고, accounts.status 가 이미 같은 방식을 쓰고 있다.
-- 실제로 행을 비우는 것은 나중에 운영 작업으로 한다 (지금은 그럴 양이 아니다).
--
-- 닉네임은 계속 점유된다. uq_nickname 이 삭제된 행에도 그대로 걸리기 때문인데,
-- 이건 의도한 것이다 — 방금 지워진 캐릭터의 이름을 남이 바로 가져가면 사칭이
-- 된다. 본인도 못 쓰게 되는 불편이 있지만 그쪽이 안전하다.
--
-- 파트너는 반대로 진짜 지운다. 방생은 되돌릴 것을 전제하지 않는 행위고,
-- 유령 행을 남기면 uq_character_slot 때문에 새 파트너를 slot 0 에 못 넣는다.

USE hhv;

SET @add_deleted_at := (
  SELECT IF(COUNT(*) = 0,
            'ALTER TABLE characters ADD COLUMN deleted_at DATETIME(3) NULL',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'characters' AND column_name = 'deleted_at'
);
PREPARE stmt FROM @add_deleted_at;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 목록 조회가 항상 deleted_at IS NULL 로 좁히므로 계정별 인덱스에 같이 태운다.
SET @add_index := (
  SELECT IF(COUNT(*) = 0,
            'CREATE INDEX idx_account_live ON characters (account_id, deleted_at)',
            'DO 0')
  FROM information_schema.statistics
  WHERE table_schema = 'hhv' AND table_name = 'characters'
    AND index_name = 'idx_account_live'
);
PREPARE stmt FROM @add_index;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 방생은 행을 지운다. characters 에는 DELETE 를 주지 않는다 — 그쪽은 소프트 삭제다.
GRANT DELETE ON hhv.character_pokemon TO 'hhv_server'@'localhost';
FLUSH PRIVILEGES;

INSERT IGNORE INTO schema_migrations (version) VALUES ('007_character_delete');

SELECT '007_character_delete applied' AS result;
