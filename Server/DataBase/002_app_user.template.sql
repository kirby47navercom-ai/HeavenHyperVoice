-- 002_app_user.template.sql — LoginServer 전용 MySQL 계정
--
-- 이 파일은 템플릿이다. 비밀번호가 들어간 사본은 커밋되지 않는다.
--
-- 사용법:
--   1. 002_app_user.sql 로 복사한다 (.gitignore 에 등록돼 있다)
--        Copy-Item Server\DataBase\002_app_user.template.sql Server\DataBase\002_app_user.sql
--   2. CHANGE_ME 를 실제 비밀번호로 바꾼다
--   3. 실행한다
--        & "C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe" -u root -p < Server\DataBase\002_app_user.sql
--
-- 001_accounts.sql 을 먼저 실행해야 한다.

CREATE USER IF NOT EXISTS 'hhv_server'@'localhost'
  IDENTIFIED BY 'CHANGE_ME';

-- 최소 권한만 준다. DELETE 도 DDL 도 주지 않으므로,
-- LoginServer 가 뚫려도 테이블을 지우거나 스키마를 바꿀 수 없다.
-- 회원가입 기능을 넣을 때 INSERT 를 추가한다.
GRANT SELECT, UPDATE ON hhv.accounts TO 'hhv_server'@'localhost';

FLUSH PRIVILEGES;

SHOW GRANTS FOR 'hhv_server'@'localhost';
