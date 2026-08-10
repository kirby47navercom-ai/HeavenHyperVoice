-- 006_grant_characters.sql — 캐릭터 테이블 권한
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- LoginServer 가 캐릭터 목록을 읽고 새 캐릭터와 파트너를 만든다.
-- UPDATE 는 last_played_at 갱신용이다. DELETE 와 DDL 은 여전히 주지 않는다.
--
-- 나중에 필드 서버가 생기면 계정은 나누는 게 맞다. 필드 서버는 characters 와
-- character_pokemon 만 있으면 되고 accounts.password_hash 를 읽을 이유가 없다.

GRANT SELECT, INSERT, UPDATE ON hhv.characters        TO 'hhv_server'@'localhost';
GRANT SELECT, INSERT, UPDATE ON hhv.character_pokemon TO 'hhv_server'@'localhost';

FLUSH PRIVILEGES;

INSERT IGNORE INTO hhv.schema_migrations (version) VALUES ('006_grant_characters');

SHOW GRANTS FOR 'hhv_server'@'localhost';
