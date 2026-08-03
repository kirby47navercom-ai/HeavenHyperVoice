#pragma once

// ODBC 기반 계정 저장소.
//
// 드라이버만 바꾸면 MySQL / MSSQL 등으로 옮길 수 있게 ODBC 를 쓴다.
// 유니코드 API(SQLWCHAR, W 접미사 함수)를 쓰는 이유는 두 가지다.
//   - MSSQL 드라이버가 유니코드 우선이라 전환 시 이 코드가 그대로 간다
//   - 좁은 문자 API 는 시스템 ANSI 코드페이지(한국어 Windows 는 CP949)를 거쳐
//     UTF-8 닉네임이 깨진다
//
// 커넥션은 스레드마다 하나씩 필요하다. ODBC 핸들은 동시 사용이 안전하지 않다.

#include <windows.h>
// windows.h 를 먼저 넣어야 sql.h 가 필요한 타입을 찾는다.
#include <sql.h>
#include <sqlext.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "AccountStore.h"

namespace heaven::login {

// 설치된 ODBC 드라이버 중 유니코드 MySQL 드라이버를 찾는다.
// 버전이 이름에 들어가므로(예: "MySQL ODBC 26.7 Unicode Driver") 고정할 수 없다.
// 못 찾으면 빈 문자열.
std::string findMySqlUnicodeDriver();

struct OdbcSettings {
    // DSN 없이 붙는다. 드라이버만 설치돼 있으면 되고 레지스트리 등록이 필요 없다.
    // 비워두면 findMySqlUnicodeDriver() 로 자동 탐지한다.
    std::string driver;
    std::string server = "127.0.0.1";
    std::uint16_t port = 3306;
    std::string database = "hhv";
    std::string user = "hhv_server";
    std::string password;

    // 이걸 주면 위 항목들을 무시하고 그대로 쓴다. DSN 을 쓰거나
    // 드라이버별 옵션을 붙일 때 사용한다.
    std::string connectionString;

    unsigned poolSize = 4;
};

class OdbcAccountStore : public AccountStore {
public:
    // 커넥션을 미리 열어둔다. 실패하면 예외를 던진다.
    explicit OdbcAccountStore(const OdbcSettings& settings);
    ~OdbcAccountStore() override;

    OdbcAccountStore(const OdbcAccountStore&) = delete;
    OdbcAccountStore& operator=(const OdbcAccountStore&) = delete;

    std::optional<Account> authenticate(std::string_view username,
                                        std::string_view password) override;

    // 접속 정보를 그대로 쓴다. 비밀번호는 들어 있지 않다.
    const char* describe() const override { return target_.c_str(); }

private:
    struct Connection;

    // 풀에서 커넥션을 하나 빌린다. 없으면 반납될 때까지 기다린다.
    Connection* acquire();
    void release(Connection* connection);

    SQLHENV env_ = SQL_NULL_HENV;
    std::string target_;

    std::mutex mutex_;
    std::condition_variable available_;
    std::vector<std::unique_ptr<Connection>> connections_;
    std::vector<Connection*> free_;

    // 계정이 없을 때도 같은 시간을 쓰기 위한 더미 해시.
    // 이게 없으면 응답 시간만으로 아이디 존재 여부를 알 수 있다.
    std::string dummyHash_;
};

}  // namespace heaven::login
