#include "OdbcAccountStore.h"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>

#include "PasswordHash.h"

namespace heaven::login {

namespace {

std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                             static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(),
                          needed);
    return wide;
}

std::string narrow(const SQLWCHAR* data, SQLLEN lengthBytes) {
    if (data == nullptr || lengthBytes <= 0) {
        return {};
    }
    const int chars = static_cast<int>(lengthBytes / sizeof(SQLWCHAR));
    const int needed = ::WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(data),
                                             chars, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(needed), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(data), chars, out.data(),
                          needed, nullptr, nullptr);
    return out;
}

bool succeeded(SQLRETURN rc) {
    return rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO;
}

// 드라이버가 남긴 진단을 읽어온다. 실패 원인을 알려면 이게 필요하다.
std::string diagnostics(SQLSMALLINT handleType, SQLHANDLE handle) {
    std::string out;
    for (SQLSMALLINT record = 1;; ++record) {
        SQLWCHAR state[6] = {};
        SQLINTEGER native = 0;
        SQLWCHAR message[1024] = {};
        SQLSMALLINT messageLength = 0;

        const SQLRETURN rc = SQLGetDiagRecW(handleType, handle, record, state, &native, message,
                                            static_cast<SQLSMALLINT>(std::size(message)),
                                            &messageLength);
        if (!succeeded(rc)) {
            break;
        }
        if (!out.empty()) {
            out += "; ";
        }
        out += "[" + narrow(state, 5 * static_cast<SQLLEN>(sizeof(SQLWCHAR))) + "] " +
               narrow(message, messageLength * static_cast<SQLLEN>(sizeof(SQLWCHAR)));
    }
    return out.empty() ? "no diagnostic record" : out;
}

void require(SQLRETURN rc, SQLSMALLINT handleType, SQLHANDLE handle, const char* what) {
    if (!succeeded(rc)) {
        throw std::runtime_error(std::string(what) + " failed: " +
                                 diagnostics(handleType, handle));
    }
}

constexpr std::size_t kMaxTextChars = 256;

}  // namespace

std::string findMySqlUnicodeDriver() {
    SQLHENV env = SQL_NULL_HENV;
    if (!succeeded(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env))) {
        return {};
    }
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);

    std::string best;
    SQLWCHAR name[256] = {};
    SQLWCHAR attributes[512] = {};
    SQLSMALLINT nameLength = 0;
    SQLSMALLINT attributesLength = 0;

    SQLRETURN rc = SQLDriversW(env, SQL_FETCH_FIRST, name, static_cast<SQLSMALLINT>(std::size(name)),
                               &nameLength, attributes,
                               static_cast<SQLSMALLINT>(std::size(attributes)),
                               &attributesLength);
    while (succeeded(rc)) {
        const std::string driver =
            narrow(name, nameLength * static_cast<SQLLEN>(sizeof(SQLWCHAR)));

        // ANSI 드라이버는 한글이 코드페이지를 거쳐 깨지므로 유니코드만 받는다.
        if (driver.find("MySQL") != std::string::npos &&
            driver.find("Unicode") != std::string::npos) {
            // 여러 개면 이름순으로 가장 뒤(대체로 최신 버전)를 쓴다.
            if (driver > best) {
                best = driver;
            }
        }

        rc = SQLDriversW(env, SQL_FETCH_NEXT, name, static_cast<SQLSMALLINT>(std::size(name)),
                         &nameLength, attributes,
                         static_cast<SQLSMALLINT>(std::size(attributes)), &attributesLength);
    }

    SQLFreeHandle(SQL_HANDLE_ENV, env);
    return best;
}

// 커넥션 하나와 그에 묶인 준비된 구문.
struct OdbcAccountStore::Connection {
    SQLHDBC dbc = SQL_NULL_HDBC;
    SQLHSTMT select = SQL_NULL_HSTMT;
    SQLHSTMT touchLogin = SQL_NULL_HSTMT;
};

OdbcAccountStore::OdbcAccountStore(const OdbcSettings& settings) {
    require(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_), SQL_HANDLE_ENV, nullptr,
            "SQLAllocHandle(ENV)");
    require(SQLSetEnvAttr(env_, SQL_ATTR_ODBC_VERSION,
                          reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0),
            SQL_HANDLE_ENV, env_, "SQLSetEnvAttr(ODBC3)");

    std::string driver = settings.driver;
    if (driver.empty() && settings.connectionString.empty()) {
        driver = findMySqlUnicodeDriver();
        if (driver.empty()) {
            throw std::runtime_error(
                "no MySQL Unicode ODBC driver is installed.\n"
                "  Install MySQL Connector/ODBC (64-bit), then check the name with:\n"
                "    Get-OdbcDriver | Where-Object { $_.Name -match 'MySQL' }\n"
                "  Pass it explicitly with --db-driver if auto-detection picks the wrong one.");
        }
        spdlog::info("auto-detected ODBC driver: {}", driver);
    }

    std::string connectionString = settings.connectionString;
    if (connectionString.empty()) {
        connectionString = "Driver={" + driver + "};" +
                           "Server=" + settings.server + ";" +
                           "Port=" + std::to_string(settings.port) + ";" +
                           "Database=" + settings.database + ";" +
                           "User=" + settings.user + ";" +
                           "Password=" + settings.password + ";" +
                           // 드라이버가 UTF-8 로 이야기하게 한다.
                           "charset=utf8mb4;";
    }

    target_ = settings.user + "@" + settings.server + ":" + std::to_string(settings.port) + "/" +
              settings.database + " via " + (driver.empty() ? "custom connection string" : driver);

    const unsigned poolSize = settings.poolSize == 0 ? 1 : settings.poolSize;
    connections_.reserve(poolSize);
    free_.reserve(poolSize);

    const std::wstring wideConnection = widen(connectionString);

    for (unsigned i = 0; i < poolSize; ++i) {
        auto connection = std::make_unique<Connection>();

        require(SQLAllocHandle(SQL_HANDLE_DBC, env_, &connection->dbc), SQL_HANDLE_ENV, env_,
                "SQLAllocHandle(DBC)");

        SQLRETURN rc = SQLDriverConnectW(
            connection->dbc, nullptr,
            const_cast<SQLWCHAR*>(reinterpret_cast<const SQLWCHAR*>(wideConnection.c_str())),
            SQL_NTS, nullptr, 0, nullptr, SQL_DRIVER_NOPROMPT);
        require(rc, SQL_HANDLE_DBC, connection->dbc, "SQLDriverConnect");

        require(SQLAllocHandle(SQL_HANDLE_STMT, connection->dbc, &connection->select),
                SQL_HANDLE_DBC, connection->dbc, "SQLAllocHandle(STMT select)");
        require(SQLAllocHandle(SQL_HANDLE_STMT, connection->dbc, &connection->touchLogin),
                SQL_HANDLE_DBC, connection->dbc, "SQLAllocHandle(STMT touchLogin)");

        // 준비된 구문. 사용자 입력은 반드시 파라미터로 넘긴다 (SQL 인젝션 차단).
        const std::wstring selectSql =
            widen("SELECT id, nickname, password_hash, status "
                  "FROM accounts WHERE username = ?");
        require(SQLPrepareW(connection->select,
                            const_cast<SQLWCHAR*>(
                                reinterpret_cast<const SQLWCHAR*>(selectSql.c_str())),
                            SQL_NTS),
                SQL_HANDLE_STMT, connection->select, "SQLPrepare(select)");

        const std::wstring touchSql =
            widen("UPDATE accounts SET last_login_at = CURRENT_TIMESTAMP(3) WHERE id = ?");
        require(SQLPrepareW(connection->touchLogin,
                            const_cast<SQLWCHAR*>(
                                reinterpret_cast<const SQLWCHAR*>(touchSql.c_str())),
                            SQL_NTS),
                SQL_HANDLE_STMT, connection->touchLogin, "SQLPrepare(touchLogin)");

        free_.push_back(connection.get());
        connections_.push_back(std::move(connection));
    }

    // 계정이 없을 때 돌릴 더미 해시를 미리 만들어 둔다.
    dummyHash_ = hashPassword("");
}

OdbcAccountStore::~OdbcAccountStore() {
    for (auto& connection : connections_) {
        if (connection->select != SQL_NULL_HSTMT) {
            SQLFreeHandle(SQL_HANDLE_STMT, connection->select);
        }
        if (connection->touchLogin != SQL_NULL_HSTMT) {
            SQLFreeHandle(SQL_HANDLE_STMT, connection->touchLogin);
        }
        if (connection->dbc != SQL_NULL_HDBC) {
            SQLDisconnect(connection->dbc);
            SQLFreeHandle(SQL_HANDLE_DBC, connection->dbc);
        }
    }
    if (env_ != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, env_);
    }
}

OdbcAccountStore::Connection* OdbcAccountStore::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    available_.wait(lock, [this] { return !free_.empty(); });
    Connection* connection = free_.back();
    free_.pop_back();
    return connection;
}

void OdbcAccountStore::release(Connection* connection) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        free_.push_back(connection);
    }
    available_.notify_one();
}

std::optional<Account> OdbcAccountStore::authenticate(std::string_view username,
                                                      std::string_view password) {
    Connection* connection = acquire();
    struct Release {
        OdbcAccountStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

    std::wstring wideUsername = widen(username);
    SQLLEN usernameLength = SQL_NTS;

    std::uint64_t id = 0;
    SQLWCHAR nickname[kMaxTextChars] = {};
    SQLWCHAR passwordHash[kMaxTextChars] = {};
    SQLWCHAR status[64] = {};
    SQLLEN idLength = 0, nicknameLength = 0, hashLength = 0, statusLength = 0;

    bool found = false;
    try {
        require(SQLBindParameter(connection->select, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
                                 SQL_WVARCHAR, wideUsername.size(), 0, wideUsername.data(),
                                 static_cast<SQLLEN>(wideUsername.size() * sizeof(SQLWCHAR)),
                                 &usernameLength),
                SQL_HANDLE_STMT, connection->select, "SQLBindParameter(username)");

        const SQLRETURN rc = SQLExecute(connection->select);
        require(rc, SQL_HANDLE_STMT, connection->select, "SQLExecute(select)");

        SQLBindCol(connection->select, 1, SQL_C_UBIGINT, &id, sizeof(id), &idLength);
        SQLBindCol(connection->select, 2, SQL_C_WCHAR, nickname, sizeof(nickname),
                   &nicknameLength);
        SQLBindCol(connection->select, 3, SQL_C_WCHAR, passwordHash, sizeof(passwordHash),
                   &hashLength);
        SQLBindCol(connection->select, 4, SQL_C_WCHAR, status, sizeof(status), &statusLength);

        found = SQLFetch(connection->select) == SQL_SUCCESS;
        SQLCloseCursor(connection->select);
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->select);
        spdlog::error("account lookup failed: {}", e.what());
        return std::nullopt;
    }

    if (!found) {
        // 계정이 없어도 검증을 한 번 돌려 응답 시간을 맞춘다.
        // 이게 없으면 응답이 빨리 오는 것만으로 아이디가 없다는 걸 알 수 있다.
        verifyPassword(password, dummyHash_);
        return std::nullopt;
    }

    const std::string storedHash = narrow(passwordHash, hashLength);
    if (!verifyPassword(password, storedHash)) {
        return std::nullopt;
    }

    const std::string accountStatus = narrow(status, statusLength);
    if (accountStatus != "active") {
        spdlog::info("login refused for {}: account is {}", std::string(username),
                     accountStatus);
        return std::nullopt;
    }

    // 마지막 로그인 시각 갱신. 실패해도 로그인은 성공시킨다.
    try {
        SQLLEN idParamLength = 0;
        require(SQLBindParameter(connection->touchLogin, 1, SQL_PARAM_INPUT, SQL_C_UBIGINT,
                                 SQL_BIGINT, 0, 0, &id, 0, &idParamLength),
                SQL_HANDLE_STMT, connection->touchLogin, "SQLBindParameter(id)");
        require(SQLExecute(connection->touchLogin), SQL_HANDLE_STMT, connection->touchLogin,
                "SQLExecute(touchLogin)");
        SQLCloseCursor(connection->touchLogin);
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->touchLogin);
        spdlog::warn("could not update last_login_at: {}", e.what());
    }

    Account account;
    account.id = id;
    account.nickname = narrow(nickname, nicknameLength);
    return account;
}

}  // namespace heaven::login
