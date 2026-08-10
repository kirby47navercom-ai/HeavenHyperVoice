-- 010_facing_and_dead_columns.sql — 바라보는 방향 추가, 안 쓰는 컬럼 제거
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- facing: Position 에는 있었지만 컬럼이 없어서 DB 경로에서 조용히 0 이 됐다.
--         Redis 캐시로만 왕복하고 있었고, 캐시가 없으면 접속할 때마다 방향이 초기화됐다.
--
-- pos_z:  "컬럼 추가를 한 번 아끼려고 미리 둔다" 로 만들었지만 아무도 읽지 않는다.
--         009 가 characters.level 을 지운 이유와 같다 — 안 읽는 컬럼은 나중에
--         "이게 왜 있지" 가 된다. 높이를 넣을 때 그때 추가하면 된다.
--
-- token_version: 계정 정지/강제 로그아웃에 쓰려고 만들었으나 코드가 읽지도 쓰지도
--         않고, 티켓에도 해당 클레임이 없다. 실제로 쓸 때 다시 만든다.
--
-- 되돌릴 수 없다. apply-migrations.ps1 이 먼저 mysqldump 백업을 뜬다.

USE hhv;

-- 컬럼 추가/삭제는 재실행에 안전해야 하므로 존재 여부를 보고 만든다.
SET @add_facing := (
  SELECT IF(COUNT(*) = 0,
            'ALTER TABLE characters ADD COLUMN facing FLOAT NOT NULL DEFAULT 0 AFTER pos_y',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'characters' AND column_name = 'facing'
);
PREPARE stmt FROM @add_facing;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @drop_z := (
  SELECT IF(COUNT(*) > 0,
            'ALTER TABLE characters DROP COLUMN pos_z',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'characters' AND column_name = 'pos_z'
);
PREPARE stmt FROM @drop_z;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @drop_token_version := (
  SELECT IF(COUNT(*) > 0,
            'ALTER TABLE accounts DROP COLUMN token_version',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'accounts' AND column_name = 'token_version'
);
PREPARE stmt FROM @drop_token_version;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

INSERT IGNORE INTO schema_migrations (version) VALUES ('010_facing_and_dead_columns');

SELECT '010_facing_and_dead_columns applied' AS result;
