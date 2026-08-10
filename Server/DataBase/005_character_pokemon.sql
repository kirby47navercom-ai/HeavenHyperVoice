-- 005_character_pokemon.sql — 종속 포켓몬
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 캐릭터당 한 마리지만 characters 에 컬럼으로 붙이지 않는다. 여러 마리가 되는
-- 순간 살아있는 데이터를 옮기는 마이그레이션이 필요해지는데, 그게 방금 004 에서
-- nickname 으로 한 작업이다. 별도 테이블은 조인 하나가 늘 뿐이고 그 조인은
-- 캐릭터 목록 조회 때 한 번이다.
--
-- slot 0 이 따라다니는 개체다. 포켓몬 게임의 "선두가 동행" 규칙과 같아서,
-- 여러 마리로 늘릴 때 is_active 같은 컬럼을 새로 만들 필요가 없다.
--
-- 종족 기본 스탯은 여기 있지 않다. 바뀌지 않는 게임 데이터라 코드
-- (Protocol/PokemonSpecies.h) 에 두고, 개체 생성 시 계산해 확정 저장한다.
-- 그래야 나중에 밸런스를 조정해도 기존 개체가 흔들리지 않는다.
-- 아래 백필의 숫자는 그 헤더와 같은 공식이어야 한다.

USE hhv;

CREATE TABLE IF NOT EXISTS character_pokemon (
  id           BIGINT UNSIGNED   NOT NULL AUTO_INCREMENT,
  character_id BIGINT UNSIGNED   NOT NULL,
  slot         TINYINT UNSIGNED  NOT NULL DEFAULT 0,

  species_id   SMALLINT UNSIGNED NOT NULL,
  nickname     VARCHAR(32)       NULL,
  level        INT UNSIGNED      NOT NULL DEFAULT 5,

  -- 개체마다 값이 다를 수 있으므로 종족값이 아니라 계산된 실제 스탯을 저장한다.
  -- 현재 체력은 여기 없다. 인스턴스 안에서만 의미가 있어서 Redis 로 간다.
  max_hp       SMALLINT UNSIGNED NOT NULL,
  atk          SMALLINT UNSIGNED NOT NULL,
  def          SMALLINT UNSIGNED NOT NULL,
  sp_atk       SMALLINT UNSIGNED NOT NULL,
  sp_def       SMALLINT UNSIGNED NOT NULL,
  speed        SMALLINT UNSIGNED NOT NULL,

  created_at   DATETIME(3)       NOT NULL DEFAULT CURRENT_TIMESTAMP(3),

  PRIMARY KEY (id),

  -- "캐릭터당 한 마리" 를 코드가 아니라 DB 가 보증한다. 여러 마리가 되면
  -- 코드에서 slot 1.. 을 쓰기 시작하면 되고 스키마는 그대로다.
  UNIQUE KEY uq_character_slot (character_id, slot),

  CONSTRAINT fk_pokemon_character FOREIGN KEY (character_id)
    REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 004 에서 넘어온 기존 캐릭터에 파트너를 넣어준다. 스타터를 물어볼 방법이
-- 없으므로 character_id 로 10 종을 돌려가며 배정한다. 결정적이라 여러 번
-- 실행해도 같은 결과다.
--
-- 레벨 5, 개체값/노력치 0 기준 표준 공식:
--   HP   = FLOOR(2 * 종족값 * 5 / 100) + 5 + 10
--   나머지 = FLOOR(2 * 종족값 * 5 / 100) + 5
INSERT INTO character_pokemon
  (character_id, slot, species_id, level, max_hp, atk, def, sp_atk, sp_def, speed)
SELECT c.id, 0, s.species_id, 5,
       FLOOR(2 * s.hp     * 5 / 100) + 5 + 10,
       FLOOR(2 * s.atk    * 5 / 100) + 5,
       FLOOR(2 * s.def    * 5 / 100) + 5,
       FLOOR(2 * s.sp_atk * 5 / 100) + 5,
       FLOOR(2 * s.sp_def * 5 / 100) + 5,
       FLOOR(2 * s.speed  * 5 / 100) + 5
FROM characters c
JOIN (
            SELECT  1 AS species_id, 45 AS hp, 49 AS atk, 49 AS def, 65 AS sp_atk, 65 AS sp_def, 45 AS speed
  UNION ALL SELECT  2, 39, 52, 43, 60, 50, 65
  UNION ALL SELECT  3, 44, 48, 65, 50, 64, 43
  UNION ALL SELECT  4, 35, 55, 40, 50, 50, 90
  UNION ALL SELECT  5, 55, 55, 50, 45, 65, 55
  UNION ALL SELECT  6, 40, 45, 40, 35, 35, 56
  UNION ALL SELECT  7, 30, 56, 35, 25, 35, 72
  UNION ALL SELECT  8, 50, 75, 85, 20, 30, 40
  UNION ALL SELECT  9, 40, 45, 35, 40, 40, 90
  UNION ALL SELECT 10, 50, 52, 48, 65, 50, 55
) s ON s.species_id = ((c.id - 1) % 10) + 1
WHERE NOT EXISTS (
  SELECT 1 FROM character_pokemon p WHERE p.character_id = c.id AND p.slot = 0
);

INSERT IGNORE INTO schema_migrations (version) VALUES ('005_character_pokemon');

SELECT '005_character_pokemon applied' AS result;
SELECT c.nickname, p.species_id, p.level, p.max_hp, p.atk, p.def, p.sp_atk, p.sp_def, p.speed
FROM characters c JOIN character_pokemon p ON p.character_id = c.id AND p.slot = 0;
