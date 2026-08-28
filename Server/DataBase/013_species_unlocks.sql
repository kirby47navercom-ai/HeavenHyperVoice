-- 013_species_unlocks.sql — 개체 저장을 버리고 종족 해금으로 바꾼다
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 지금까지는 캐릭터마다 포켓몬 **개체**를 저장했다 (character_pokemon: 종족,
-- 레벨, 개체값). 이제는 "이 캐릭터가 어떤 종족을 해금했는가" 만 저장하고,
-- 실제 능력치는 플레이어 레벨과 종족값으로 그때그때 계산한다.
--
-- 그래서 개체값(IV)과 포켓몬별 닉네임·레벨은 사라진다. 같은 종족이면 모든
-- 플레이어가 동일하다 — 개체를 키우는 게임이 아니라 로스터를 해금하는 게임이 된다.
--
-- 해금은 비트맵으로 담는다. **비트 위치가 도감번호다.**
-- 촘촘한 인덱스(구현된 종족 순번)를 쓰면 안 된다. 종족을 중간에 하나 추가하는
-- 순간 그 뒤 비트가 전부 밀려서, 저장된 해금이 통째로 다른 포켓몬을 가리킨다.
-- 클라이언트 카탈로그를 배열 순서로 찾다가 파이리가 피카츄로 바뀐 적이 있고,
-- 그래서 도감번호로 옮긴 것이다. 같은 실수를 여기서 반복하지 않는다.
--
-- BINARY(160) = 1280 비트. 현재 최대 도감번호는 1105(벼리짱)이라 여유가 있다.
-- 더 필요해지면 뒤에 바이트만 늘리면 되고 기존 비트는 그대로다.
--
-- characters 가 아니라 1:1 테이블로 뺀 이유는, 로비의 캐릭터 목록 조회에는
-- 해금 정보가 필요 없기 때문이다. 160 바이트를 매번 실어 나를 이유가 없다.
--
-- 되돌릴 수 없다: character_pokemon 을 드롭한다.
-- apply-migrations.ps1 이 먼저 mysqldump 백업을 뜬다.

USE hhv;

-- 1) 해금 비트맵.
CREATE TABLE IF NOT EXISTS character_unlocks (
  character_id BIGINT UNSIGNED NOT NULL,

  -- 비트 위치 = 도감번호. 도감번호는 1 부터라 0 번 비트는 쓰지 않는다.
  -- 바이트 n 의 비트 k 가 도감번호 (n * 8 + k) 다.
  dex_bits     BINARY(160)     NOT NULL,

  PRIMARY KEY (character_id),

  CONSTRAINT fk_unlock_character FOREIGN KEY (character_id)
    REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 2) 플레이어 레벨.
--
-- 009 가 characters.level 을 지웠다. "레벨을 갖는 것은 포켓몬이지 트레이너가
-- 아니다" 였고, 그때는 실제로 아무도 읽지 않는 컬럼이었다. 이제는 포켓몬
-- 능력치의 입력이 되므로 목적이 분명하다.
--
-- 기본값은 kStarterLevel 과 같은 5 다. 1 로 두면 스탯 공식이 종족값의 몇 %만
-- 내놓아서 갓 만든 캐릭터의 포켓몬이 지나치게 약해진다.
SET @add_level := (
  SELECT IF(COUNT(*) = 0,
            'ALTER TABLE characters ADD COLUMN level INT UNSIGNED NOT NULL DEFAULT 5',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'characters' AND column_name = 'level'
);
PREPARE stmt FROM @add_level;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 3) 현재 데리고 다니는 종족. 해금한 것 중에서 고른다.
--    0 이면 파트너 없이 다닌다.
SET @add_active := (
  SELECT IF(COUNT(*) = 0,
            'ALTER TABLE characters ADD COLUMN active_dex SMALLINT UNSIGNED NOT NULL DEFAULT 0',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'characters' AND column_name = 'active_dex'
);
PREPARE stmt FROM @add_active;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 4) 모든 캐릭터에 빈 비트맵을 만들어 둔다.
--    행이 없으면 입장할 때마다 있는지 확인해야 하므로, 여기서 채워 놓는다.
INSERT IGNORE INTO character_unlocks (character_id, dex_bits)
  SELECT id, UNHEX(REPEAT('00', 160)) FROM characters;

-- 5) 기존 개체를 해금으로 옮긴다.
--
-- character_pokemon.species_id 는 서버 내부 번호(kSpecies 배열 인덱스 + 1)이지
-- 도감번호가 아니다. 둘의 대응은 C++ 표에만 있으므로 여기 직접 적는다.
-- 이 마이그레이션이 도는 시점의 20 종이 전부다.
--
-- 내부번호 -> 도감번호
--   1 귀뚤뚜기 401    2 기라티나 487    3 꼬링크 403     4 꼬부기 7
--   5 꽁어름 749      6 디아루가 483    7 랄토스 280     8 모부기 387
--   9 벼리짱 1105    10 불꽃숭이 390   11 아르세우스 493 12 이브이 133
--  13 이상해씨 1     14 자망칼 624     15 터검니 610    16 파이리 4
--  17 파치리스 417   18 팽도리 393     19 펄기아 484    20 피카츄 25
--
-- 비트 세팅: 도감번호 d 는 바이트 FLOOR(d/8), 그 안의 비트 (d%8) 이다.
-- MySQL 에 비트맵 함수가 없으므로 해당 바이트만 꺼내 OR 한 뒤 도로 끼운다.
DROP PROCEDURE IF EXISTS hhv_unlock_dex;
DELIMITER //
CREATE PROCEDURE hhv_unlock_dex(IN p_character_id BIGINT UNSIGNED, IN p_dex INT)
BEGIN
  DECLARE v_byte INT;
  DECLARE v_bit  INT;
  SET v_byte = FLOOR(p_dex / 8);
  SET v_bit  = p_dex % 8;

  UPDATE character_unlocks
     SET dex_bits = CONCAT(
           SUBSTRING(dex_bits, 1, v_byte),
           CHAR(ASCII(SUBSTRING(dex_bits, v_byte + 1, 1)) | (1 << v_bit)),
           SUBSTRING(dex_bits, v_byte + 2))
   WHERE character_id = p_character_id;
END //
DELIMITER ;

-- 갖고 있던 포켓몬을 해금 처리하고, 그중 slot 0 을 현재 파트너로 둔다.
DROP PROCEDURE IF EXISTS hhv_migrate_pokemon;
DELIMITER //
CREATE PROCEDURE hhv_migrate_pokemon()
BEGIN
  DECLARE v_done      INT DEFAULT 0;
  DECLARE v_character BIGINT UNSIGNED;
  DECLARE v_species   INT;
  DECLARE v_slot      INT;
  DECLARE v_dex       INT;

  DECLARE cur CURSOR FOR
    SELECT character_id, species_id, slot FROM character_pokemon;
  DECLARE CONTINUE HANDLER FOR NOT FOUND SET v_done = 1;

  OPEN cur;
  read_loop: LOOP
    FETCH cur INTO v_character, v_species, v_slot;
    IF v_done = 1 THEN
      LEAVE read_loop;
    END IF;

    SET v_dex = CASE v_species
      WHEN  1 THEN  401 WHEN  2 THEN  487 WHEN  3 THEN  403 WHEN  4 THEN    7
      WHEN  5 THEN  749 WHEN  6 THEN  483 WHEN  7 THEN  280 WHEN  8 THEN  387
      WHEN  9 THEN 1105 WHEN 10 THEN  390 WHEN 11 THEN  493 WHEN 12 THEN  133
      WHEN 13 THEN    1 WHEN 14 THEN  624 WHEN 15 THEN  610 WHEN 16 THEN    4
      WHEN 17 THEN  417 WHEN 18 THEN  393 WHEN 19 THEN  484 WHEN 20 THEN   25
      ELSE 0 END;

    IF v_dex > 0 THEN
      CALL hhv_unlock_dex(v_character, v_dex);
      IF v_slot = 0 THEN
        UPDATE characters SET active_dex = v_dex WHERE id = v_character;
      END IF;
    END IF;
  END LOOP;
  CLOSE cur;
END //
DELIMITER ;

-- character_pokemon 이 아직 있을 때만 이행한다 (재실행 안전).
SET @migrate := (
  SELECT IF(COUNT(*) > 0, 'CALL hhv_migrate_pokemon()', 'DO 0')
  FROM information_schema.tables
  WHERE table_schema = 'hhv' AND table_name = 'character_pokemon'
);
PREPARE stmt FROM @migrate;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

DROP PROCEDURE IF EXISTS hhv_migrate_pokemon;
DROP PROCEDURE IF EXISTS hhv_unlock_dex;

-- 6) 이행이 끝났으니 개체 테이블을 지운다.
DROP TABLE IF EXISTS character_pokemon;

-- 7) 권한. characters 와 같은 수준으로 준다.
GRANT SELECT, INSERT, UPDATE ON hhv.character_unlocks TO 'hhv_server'@'localhost';
FLUSH PRIVILEGES;

INSERT IGNORE INTO schema_migrations (version) VALUES ('013_species_unlocks');

SELECT '013_species_unlocks applied' AS result;
SELECT COUNT(*) AS unlock_rows FROM character_unlocks;
SELECT id, nickname, level, active_dex FROM characters WHERE deleted_at IS NULL;
