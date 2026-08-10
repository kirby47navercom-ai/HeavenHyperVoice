-- 011_narrow_account_grant.sql — accounts 의 UPDATE 를 컬럼 단위로 좁힌다
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 002 는 "최소 권한만 준다" 면서 `GRANT SELECT, UPDATE ON hhv.accounts` 를 줬다.
-- 테이블 전체 UPDATE 라 password_hash 와 status 까지 덮을 수 있다. 로그인 서버가
-- 뚫리면 남의 계정 비밀번호를 갈아끼우거나 정지를 스스로 풀 수 있다는 뜻이다.
--
-- 서버가 실제로 쓰는 UPDATE 는 last_login_at 하나뿐이다 (OdbcStore::touchLogin).
-- SELECT 와 INSERT 는 그대로 둔다 — 로그인과 가입에 필요하다.
--
-- characters 쪽은 좁히지 않는다. 위치·마지막 플레이 시각·소프트 삭제로 여러 컬럼을
-- 실제로 쓰고, 자격증명이 들어 있지 않다.

REVOKE UPDATE ON hhv.accounts FROM 'hhv_server'@'localhost';

GRANT UPDATE (last_login_at) ON hhv.accounts TO 'hhv_server'@'localhost';

FLUSH PRIVILEGES;

INSERT IGNORE INTO hhv.schema_migrations (version) VALUES ('011_narrow_account_grant');

SHOW GRANTS FOR 'hhv_server'@'localhost';
