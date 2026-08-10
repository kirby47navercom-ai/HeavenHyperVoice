-- 009_drop_character_level.sql — 캐릭터 레벨 제거
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 레벨을 갖는 것은 포켓몬이지 트레이너가 아니다. characters.level 은 만들어만
-- 두고 아무도 읽지 않았고 값도 전부 1 이었다. 아무도 안 읽는 컬럼이 남으면
-- 나중에 "이게 왜 있지" 가 되므로 지운다.
--
-- 포켓몬 레벨(character_pokemon.level)은 그대로다. 실 수치 공식의 입력이다.
--
-- 되돌릴 수 없다. apply-migrations.ps1 이 먼저 mysqldump 백업을 뜬다.

USE hhv;

SET @drop_level := (
  SELECT IF(COUNT(*) > 0,
            'ALTER TABLE characters DROP COLUMN level',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'characters' AND column_name = 'level'
);
PREPARE stmt FROM @drop_level;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

INSERT IGNORE INTO schema_migrations (version) VALUES ('009_drop_character_level');

SELECT '009_drop_character_level applied' AS result;
