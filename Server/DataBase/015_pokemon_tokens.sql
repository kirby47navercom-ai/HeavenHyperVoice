-- 015_pokemon_tokens.sql — 뽑기에 쓰는 '포켓몬 토큰' 보유량
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 타입별 뽑기 한 번에 토큰 하나를 쓴다. 나온 종족이 이미 해금돼 있으면 꽝이고,
-- 그때도 토큰은 소모된다 (반환하지 않는다).
--
-- characters 에 두는 이유는 해금이 캐릭터 단위이기 때문이다 (013). 계정에 두면
-- A 캐릭터로 모은 토큰으로 B 캐릭터 로스터를 채우는 흐름이 생기는데, 지금
-- 로스터가 캐릭터마다 따로인 것과 어긋난다.
--
-- 별도 테이블이 아니라 컬럼인 이유는 값이 스칼라 하나이기 때문이다. 파티(014)는
-- 곧 구성원마다 기술이 붙어서 행으로 뺐지만, 토큰은 늘어날 것이 없다.
--
-- 차감은 앱에서 UPDATE 한 번으로 한다:
--
--   UPDATE characters SET pokemon_tokens = pokemon_tokens - 1
--    WHERE id = ? AND account_id = ? AND pokemon_tokens > 0
--
-- 읽고-확인-쓰기로 하면 DB 스레드가 둘이라 같은 토큰으로 두 번 뽑힌다.
-- UNSIGNED 라 0 에서 한 번 더 빼면 래핑하는데, WHERE 의 > 0 이 그걸 막는다.
--
-- 데이터를 지우지 않는다. 되돌리려면 컬럼만 드롭하면 된다.

USE hhv;

-- 006 이 characters 에 UPDATE 를 이미 줬으므로 GRANT 추가는 없다.
SET @add_tokens := (
  SELECT IF(COUNT(*) = 0,
            'ALTER TABLE characters ADD COLUMN pokemon_tokens INT UNSIGNED NOT NULL DEFAULT 0',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'characters' AND column_name = 'pokemon_tokens'
);
PREPARE stmt FROM @add_tokens;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

INSERT IGNORE INTO schema_migrations (version) VALUES ('015_pokemon_tokens');

SELECT '015_pokemon_tokens applied' AS result;
SELECT id, nickname, pokemon_tokens FROM characters WHERE deleted_at IS NULL;
