-- 014_character_party.sql — 해금한 것 중에서 데리고 다닐 3마리를 고른다
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 013 이 "무엇을 해금했는가" 를 담았다. 그건 로스터 전체라 수백 마리가 될 수
-- 있다. 실제로 데리고 다니는 것은 그중 최대 3마리이고, 그 3마리 중 한 마리만
-- 화면에 꺼내 놓는다.
--
-- 왜 characters 에 dex1/dex2/dex3 컬럼 세 개가 아니라 별도 테이블인가:
-- 곧 파티 구성원마다 기술 세팅이 붙는다. 컬럼으로 펼치면 기술 네 칸 × 3마리로
-- 열두 컬럼이 되고, 파티가 4마리가 되는 날 스키마를 통째로 다시 짜야 한다.
-- 행으로 두면 그때는 컬럼 하나를 ALTER 로 붙이면 끝이다.
--
-- 꺼내 놓은 한 마리는 characters.active_dex 에 그대로 둔다 (013 에서 만들었다).
-- 파티 테이블에 is_active 플래그를 두면 "한 캐릭터에 활성 두 마리" 가 표현
-- 가능해지고, 그걸 막는 제약을 MySQL 에서 깔끔하게 쓸 수 없다. 스칼라 한 칸이면
-- 애초에 둘이 될 수 없다.
--
-- 데이터를 지우지 않는다. 되돌리려면 이 테이블만 드롭하면 된다.

USE hhv;

CREATE TABLE IF NOT EXISTS character_party (
  character_id BIGINT UNSIGNED   NOT NULL,

  -- 0, 1, 2. 화면에 늘어놓는 순서다.
  slot         TINYINT UNSIGNED  NOT NULL,

  -- 도감번호. 013 의 비트맵과 같은 열쇠를 쓴다 — 내부 배열 번호를 쓰면 종족
  -- 표에 한 줄 끼워 넣는 순간 저장된 파티가 통째로 다른 포켓몬이 된다.
  dex          SMALLINT UNSIGNED NOT NULL,

  PRIMARY KEY (character_id, slot),

  -- 같은 종족을 두 칸에 넣지 못하게 한다. 앱에서도 거르지만, 두 요청이 겹쳐
  -- 들어오면 앱 검사만으로는 막히지 않는다.
  UNIQUE KEY uq_party_member (character_id, dex),

  -- 칸은 세 개뿐이다. 앱이 잘못 보내도 여기서 걸린다.
  CONSTRAINT ck_party_slot CHECK (slot < 3),

  CONSTRAINT fk_party_character FOREIGN KEY (character_id)
    REFERENCES characters (id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 이미 꺼내 놓은 것이 있으면 그게 곧 파티 첫 칸이다. 없으면 아무것도 안 넣는다.
-- 재실행해도 INSERT IGNORE 라 중복이 생기지 않는다.
INSERT IGNORE INTO character_party (character_id, slot, dex)
SELECT id, 0, active_dex
FROM characters
WHERE active_dex <> 0 AND deleted_at IS NULL;

-- 권한. character_unlocks 와 달리 DELETE 가 필요하다 — 파티에서 빼는 것은
-- 행을 지우는 일이고, 해금과 달리 되돌릴 수 있는 조작이다.
GRANT SELECT, INSERT, UPDATE, DELETE ON hhv.character_party TO 'hhv_server'@'localhost';
FLUSH PRIVILEGES;

INSERT IGNORE INTO schema_migrations (version) VALUES ('014_character_party');

SELECT '014_character_party applied' AS result;
SELECT c.id, c.nickname, c.active_dex, GROUP_CONCAT(p.dex ORDER BY p.slot) AS party
FROM characters c
LEFT JOIN character_party p ON p.character_id = c.id
WHERE c.deleted_at IS NULL
GROUP BY c.id, c.nickname, c.active_dex;
