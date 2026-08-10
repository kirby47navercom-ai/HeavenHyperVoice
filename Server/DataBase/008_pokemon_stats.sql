-- 008_pokemon_stats.sql — 개체값과 노력치
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 지금까지는 레벨 5 실 수치를 계산해서 저장했다. 실제 공식(종족값 + 개체값 +
-- 노력치 + 레벨)을 쓰기 시작하면 그게 성립하지 않는다. 레벨이 오르거나 노력치가
-- 붙을 때마다 여섯 컬럼을 다시 써야 하고, 갱신을 한 군데라도 빠뜨리면 조용히
-- 어긋난다.
--
-- 그래서 **입력만 저장하고 실 수치는 읽을 때 계산한다.**
--   저장:  species_id, level, iv_*, ev_*
--   계산:  Protocol/PokemonSpecies.h 의 computeStats
--
-- 되돌릴 수 없다: max_hp 등 여섯 컬럼을 드롭한다.
-- apply-migrations.ps1 이 먼저 mysqldump 백업을 뜬다.

USE hhv;

-- 개체값 0~31, 노력치 0~252. 둘 다 TINYINT UNSIGNED 로 충분하다.
SET @add_columns := (
  SELECT IF(COUNT(*) = 0,
            'ALTER TABLE character_pokemon
               ADD COLUMN iv_hp     TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN iv_atk    TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN iv_def    TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN iv_sp_atk TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN iv_sp_def TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN iv_speed  TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN ev_hp     TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN ev_atk    TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN ev_def    TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN ev_sp_atk TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN ev_sp_def TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN ev_speed  TINYINT UNSIGNED NOT NULL DEFAULT 0',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'character_pokemon' AND column_name = 'iv_hp'
);
PREPARE stmt FROM @add_columns;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 기존 개체에 개체값을 굴려준다. 한 번만 실행한다 — 두 번째 실행에서 다시
-- 굴리면 같은 포켓몬의 스탯이 이유 없이 바뀐다.
SET @roll_ivs := IF(
  (SELECT COUNT(*) FROM schema_migrations WHERE version = '008_pokemon_stats') > 0,
  'DO 0',
  'UPDATE character_pokemon SET
     iv_hp     = FLOOR(RAND() * 32),
     iv_atk    = FLOOR(RAND() * 32),
     iv_def    = FLOOR(RAND() * 32),
     iv_sp_atk = FLOOR(RAND() * 32),
     iv_sp_def = FLOOR(RAND() * 32),
     iv_speed  = FLOOR(RAND() * 32)');
PREPARE stmt FROM @roll_ivs;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 계산해서 저장하던 실 수치를 드롭한다. 이제 파생값이라 여기 있으면 안 된다.
SET @drop_stats := (
  SELECT IF(COUNT(*) > 0,
            'ALTER TABLE character_pokemon
               DROP COLUMN max_hp, DROP COLUMN atk, DROP COLUMN def,
               DROP COLUMN sp_atk, DROP COLUMN sp_def, DROP COLUMN speed',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'character_pokemon' AND column_name = 'max_hp'
);
PREPARE stmt FROM @drop_stats;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

INSERT IGNORE INTO schema_migrations (version) VALUES ('008_pokemon_stats');

SELECT '008_pokemon_stats applied' AS result;
SELECT c.nickname, p.species_id, p.level,
       p.iv_hp, p.iv_atk, p.iv_def, p.iv_sp_atk, p.iv_sp_def, p.iv_speed
FROM characters c JOIN character_pokemon p ON p.character_id = c.id AND p.slot = 0
WHERE c.deleted_at IS NULL;
