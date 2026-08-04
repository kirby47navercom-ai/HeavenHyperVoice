-- 003_grant_insert.sql — 회원가입을 위한 INSERT 권한
--
-- 실행:
--   .\tools\apply-migrations.ps1
--
-- 002 에서는 LoginServer 가 계정을 읽고 last_login_at 만 갱신하면 됐다.
-- 회원가입이 생기면서 INSERT 가 필요해졌다. DELETE 와 DDL 은 여전히 주지 않는다.

GRANT INSERT ON hhv.accounts TO 'hhv_server'@'localhost';

FLUSH PRIVILEGES;

INSERT IGNORE INTO hhv.schema_migrations (version) VALUES ('003_grant_insert');

SHOW GRANTS FOR 'hhv_server'@'localhost';
