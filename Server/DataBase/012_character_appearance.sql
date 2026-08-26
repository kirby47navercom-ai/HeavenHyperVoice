-- 012_character_appearance.sql — 캐릭터 외형(커마)을 서버로 옮긴다
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 지금까지 외형은 클라이언트의 로컬 SaveGame(UUECharacterSlotSaveGame)에만 있었다.
-- 캐릭터 목록이 서버에서 오기 시작하면 로컬 슬롯과 짝을 맞출 방법이 없다 — 서버는
-- character_id 로 말하는데 로컬은 슬롯 번호 0~2 로 저장하기 때문이다. 다른 PC 에서
-- 접속하면 외형이 통째로 기본값이 된다.
--
-- 외형은 캐릭터당 정확히 하나이고 캐릭터를 읽을 때 항상 같이 읽는다. 그래서
-- 1:1 테이블로 나누지 않고 characters 에 컬럼으로 붙인다 (character_pokemon 과 달리
-- 조인이 늘지 않는다).
--
-- 컬럼 이름과 기본값은 클라이언트의 FUEHHVAppearance 와 1:1 로 맞춘다. 기본값을
-- 구조체 기본값과 같게 두어야 이 마이그레이션 이전에 만들어진 캐릭터가 깨진 모습이
-- 아니라 "기본 캐릭터" 로 보인다.
--
-- 주의: 부위 선택은 인덱스로 저장한다. UUEHHVCustomizationCatalog 의 배열 순서를
-- 바꾸거나 중간에 끼워 넣으면 이미 저장된 캐릭터의 머리·옷이 조용히 다른 것으로
-- 바뀐다. Protocol/PokemonSpecies.h 의 kSpecies 배열과 같은 제약이다.
-- 옵션을 늘릴 때는 반드시 배열 뒤에 붙일 것.
--
-- 색은 FLOAT 세 개로 나눠 담는다. 0xRRGGBB 로 압축하면 왕복 오차가 커마 UI 의
-- 선택 판정(FLinearColor::Equals, 허용 오차 1e-4)을 넘어서, 저장했다 불러오면
-- 고른 색이 선택 표시되지 않는다. 알파는 쓰지 않으므로 저장하지 않는다.
--
-- characters 는 테이블 단위로 SELECT/INSERT/UPDATE 권한이 있어(006) 새 컬럼에
-- 별도 GRANT 가 필요 없다.
--
-- 되돌릴 수 없다. apply-migrations.ps1 이 먼저 mysqldump 백업을 뜬다.

USE hhv;

-- 18 개를 한 번에 붙인다. 컬럼마다 information_schema 를 보는 대신 대표 컬럼
-- 하나로 판정한다 — 이 마이그레이션은 항상 전부 함께 추가하거나 전혀 추가하지
-- 않으므로 중간 상태가 없다.
SET @add_appearance := (
  SELECT IF(COUNT(*) = 0,
            'ALTER TABLE characters
               ADD COLUMN appearance_gender    TINYINT UNSIGNED NOT NULL DEFAULT 0,
               ADD COLUMN appearance_body      INT NOT NULL DEFAULT 1,
               ADD COLUMN appearance_head      INT NOT NULL DEFAULT 0,
               ADD COLUMN appearance_hair      INT NOT NULL DEFAULT 0,
               ADD COLUMN appearance_eye       INT NOT NULL DEFAULT 0,
               ADD COLUMN appearance_equipment INT NOT NULL DEFAULT 1,
               ADD COLUMN skin_r FLOAT NOT NULL DEFAULT 1.0,
               ADD COLUMN skin_g FLOAT NOT NULL DEFAULT 0.712,
               ADD COLUMN skin_b FLOAT NOT NULL DEFAULT 0.6458,
               ADD COLUMN hair_r FLOAT NOT NULL DEFAULT 0.1719,
               ADD COLUMN hair_g FLOAT NOT NULL DEFAULT 0.1111,
               ADD COLUMN hair_b FLOAT NOT NULL DEFAULT 0.0850,
               ADD COLUMN eye_r  FLOAT NOT NULL DEFAULT 0.070638,
               ADD COLUMN eye_g  FLOAT NOT NULL DEFAULT 0.484375,
               ADD COLUMN eye_b  FLOAT NOT NULL DEFAULT 0.243701,
               ADD COLUMN arm_volume   FLOAT NOT NULL DEFAULT 0,
               ADD COLUMN torso_volume FLOAT NOT NULL DEFAULT 0,
               ADD COLUMN leg_volume   FLOAT NOT NULL DEFAULT 0',
            'DO 0')
  FROM information_schema.columns
  WHERE table_schema = 'hhv' AND table_name = 'characters'
    AND column_name = 'appearance_gender'
);
PREPARE stmt FROM @add_appearance;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

INSERT IGNORE INTO schema_migrations (version) VALUES ('012_character_appearance');

SELECT '012_character_appearance applied' AS result;
SELECT COUNT(*) AS characters_with_appearance FROM characters;
