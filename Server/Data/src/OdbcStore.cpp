#include "OdbcStore.h"

#include <algorithm>
#include <spdlog/spdlog.h>

#include <random>
#include <stdexcept>
#include <utility>

#include "PasswordHash.h"

namespace heaven::data {

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

// 실패한 구문이 남긴 SQLSTATE. 23000 은 무결성 제약 위반이며 표준이라
// MSSQL 로 옮겨도 같다. 드라이버별 메시지 문자열을 파싱하는 것보다 안전하다.
std::string sqlStateOf(SQLHSTMT statement) {
    SQLWCHAR state[6] = {};
    SQLINTEGER native = 0;
    SQLWCHAR message[512] = {};
    SQLSMALLINT messageLength = 0;
    SQLGetDiagRecW(SQL_HANDLE_STMT, statement, 1, state, &native, message,
                   static_cast<SQLSMALLINT>(std::size(message)), &messageLength);
    return narrow(state, 5 * static_cast<SQLLEN>(sizeof(SQLWCHAR)));
}

void prepare(SQLHSTMT statement, const char* sql, const char* what) {
    const std::wstring wide = widen(sql);
    require(SQLPrepareW(statement,
                        const_cast<SQLWCHAR*>(reinterpret_cast<const SQLWCHAR*>(wide.c_str())),
                        SQL_NTS),
            SQL_HANDLE_STMT, statement, what);
}

// 파라미터 바인딩. 길이 변수는 SQLExecute 까지 살아 있어야 하므로 호출자가 소유한다.
void bindText(SQLHSTMT statement, SQLUSMALLINT index, std::wstring& value, SQLLEN& length) {
    length = SQL_NTS;
    require(SQLBindParameter(statement, index, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,
                             value.size(), 0, value.data(),
                             static_cast<SQLLEN>(value.size() * sizeof(SQLWCHAR)), &length),
            SQL_HANDLE_STMT, statement, "SQLBindParameter(text)");
}

void bindUInt64(SQLHSTMT statement, SQLUSMALLINT index, std::uint64_t& value, SQLLEN& length) {
    length = 0;
    require(SQLBindParameter(statement, index, SQL_PARAM_INPUT, SQL_C_UBIGINT, SQL_BIGINT, 0, 0,
                             &value, 0, &length),
            SQL_HANDLE_STMT, statement, "SQLBindParameter(uint64)");
}

void bindUInt32(SQLHSTMT statement, SQLUSMALLINT index, std::uint32_t& value, SQLLEN& length) {
    length = 0;
    require(SQLBindParameter(statement, index, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 0, 0,
                             &value, 0, &length),
            SQL_HANDLE_STMT, statement, "SQLBindParameter(uint32)");
}

void bindInt32(SQLHSTMT statement, SQLUSMALLINT index, std::int32_t& value, SQLLEN& length) {
    length = 0;
    require(SQLBindParameter(statement, index, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0,
                             &value, 0, &length),
            SQL_HANDLE_STMT, statement, "SQLBindParameter(int32)");
}

void bindDouble(SQLHSTMT statement, SQLUSMALLINT index, double& value, SQLLEN& length) {
    length = 0;
    require(SQLBindParameter(statement, index, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE, 0, 0,
                             &value, 0, &length),
            SQL_HANDLE_STMT, statement, "SQLBindParameter(double)");
}

void bindUInt16(SQLHSTMT statement, SQLUSMALLINT index, std::uint16_t& value, SQLLEN& length) {
    length = 0;
    require(SQLBindParameter(statement, index, SQL_PARAM_INPUT, SQL_C_USHORT, SQL_SMALLINT, 0, 0,
                             &value, 0, &length),
            SQL_HANDLE_STMT, statement, "SQLBindParameter(uint16)");
}

constexpr std::size_t kMaxTextChars = 256;

// 드라이버 이름에 박힌 버전 번호. "MySQL ODBC 9.4 Unicode Driver" -> 904.
// 사전순으로 비교하면 "9.4" 가 "26.7" 보다 뒤라 구버전을 고르게 된다.
int driverVersion(const std::string& name) {
    const std::size_t start = name.find_first_of("0123456789");
    if (start == std::string::npos) {
        return -1;
    }
    int major = 0;
    int minor = 0;
    std::size_t i = start;
    for (; i < name.size() && std::isdigit(static_cast<unsigned char>(name[i])); ++i) {
        major = major * 10 + (name[i] - '0');
    }
    if (i < name.size() && name[i] == '.') {
        for (++i; i < name.size() && std::isdigit(static_cast<unsigned char>(name[i])); ++i) {
            minor = minor * 10 + (name[i] - '0');
        }
    }
    return major * 100 + minor;
}

// 캐릭터 조회 두 구문이 공유하는 컬럼 목록. 순서가 fetchCharacters 의
// 바인딩과 일치해야 한다.
//
// character_pokemon 조인이 사라졌다. 데리고 다니는 종족은 characters.active_dex
// 한 컬럼이고, 능력치는 저장돼 있지 않아 읽어온 뒤 계산한다.
constexpr const char* kCharacterColumns =
    "SELECT c.id, c.nickname, c.level, c.active_dex, "
    // 외형. 5~22 번 컬럼이며 fetchCharacters 의 바인딩 순서와 맞아야 한다.
    "       c.appearance_gender, c.appearance_body, c.appearance_head, "
    "       c.appearance_hair, c.appearance_eye, c.appearance_equipment, "
    "       c.skin_r, c.skin_g, c.skin_b, "
    "       c.hair_r, c.hair_g, c.hair_b, "
    "       c.eye_r, c.eye_g, c.eye_b, "
    "       c.arm_volume, c.torso_volume, c.leg_volume, "
    // 해금 비트맵. 23 번 컬럼. 013 이전에 만들어진 캐릭터는 행이 없을 수 있어
    // LEFT JOIN 이다 — INNER 면 그런 캐릭터가 목록에서 통째로 사라진다.
    "       u.dex_bits "
    "FROM characters c "
    "LEFT JOIN character_unlocks u ON u.character_id = c.id ";

}  // namespace

std::string findMySqlUnicodeDriver() {
    SQLHENV env = SQL_NULL_HENV;
    if (!succeeded(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env))) {
        return {};
    }
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);

    std::string best;
    int bestVersion = -1;
    SQLWCHAR name[256] = {};
    SQLWCHAR attributes[512] = {};
    SQLSMALLINT nameLength = 0;
    SQLSMALLINT attributesLength = 0;

    SQLRETURN rc = SQLDriversW(env, SQL_FETCH_FIRST, name,
                               static_cast<SQLSMALLINT>(std::size(name)), &nameLength, attributes,
                               static_cast<SQLSMALLINT>(std::size(attributes)),
                               &attributesLength);
    while (succeeded(rc)) {
        const std::string driver =
            narrow(name, nameLength * static_cast<SQLLEN>(sizeof(SQLWCHAR)));

        // ANSI 드라이버는 한글이 코드페이지를 거쳐 깨지므로 유니코드만 받는다.
        if (driver.find("MySQL") != std::string::npos &&
            driver.find("Unicode") != std::string::npos) {
            // 여러 개면 버전이 가장 높은 것을 쓴다.
            const int version = driverVersion(driver);
            if (version > bestVersion) {
                bestVersion = version;
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
struct OdbcStore::Connection {
    SQLHDBC dbc = SQL_NULL_HDBC;

    SQLHSTMT selectAccount = SQL_NULL_HSTMT;
    SQLHSTMT touchLogin = SQL_NULL_HSTMT;
    SQLHSTMT insertAccount = SQL_NULL_HSTMT;

    SQLHSTMT listCharacters = SQL_NULL_HSTMT;
    SQLHSTMT findCharacter = SQL_NULL_HSTMT;
    SQLHSTMT countCharacters = SQL_NULL_HSTMT;
    SQLHSTMT insertCharacter = SQL_NULL_HSTMT;
    SQLHSTMT lastInsertId = SQL_NULL_HSTMT;
    SQLHSTMT touchPlayed = SQL_NULL_HSTMT;
    SQLHSTMT selectPosition = SQL_NULL_HSTMT;
    SQLHSTMT findNickname = SQL_NULL_HSTMT;
    SQLHSTMT softDelete = SQL_NULL_HSTMT;
    SQLHSTMT updatePosition = SQL_NULL_HSTMT;

    // 해금 비트맵.
    SQLHSTMT insertUnlockRow = SQL_NULL_HSTMT;  // 캐릭터를 만들 때 빈 비트맵을 깐다
    SQLHSTMT setUnlockBit = SQL_NULL_HSTMT;     // 비트 하나를 켠다
    SQLHSTMT testUnlockBit = SQL_NULL_HSTMT;    // 해금됐는지 본다
    SQLHSTMT setActiveDex = SQL_NULL_HSTMT;     // 데리고 다닐 종족을 세운다
    SQLHSTMT selectParty = SQL_NULL_HSTMT;      // 파티 구성을 읽는다
    SQLHSTMT clearParty = SQL_NULL_HSTMT;       // 파티를 통째로 비운다
    SQLHSTMT insertPartyMember = SQL_NULL_HSTMT;

    // 모든 구문 핸들을 한 번에 돌기 위한 목록.
    std::vector<SQLHSTMT*> all() {
        return {&selectAccount,   &touchLogin,       &insertAccount,  &listCharacters,
                &findCharacter,   &countCharacters,  &insertCharacter, &lastInsertId,
                &touchPlayed,     &selectPosition,   &updatePosition, &softDelete,
                &findNickname,
                &insertUnlockRow, &setUnlockBit,     &testUnlockBit,  &setActiveDex,
                &selectParty,     &clearParty,       &insertPartyMember};
    }
};

OdbcStore::OdbcStore(const OdbcSettings& settings) {
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
                "no MySQL Unicode ODBC driver found. Install MySQL Connector/ODBC (64-bit) "
                "or pass --db-driver / --db-conn.");
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

        for (SQLHSTMT* statement : connection->all()) {
            require(SQLAllocHandle(SQL_HANDLE_STMT, connection->dbc, statement), SQL_HANDLE_DBC,
                    connection->dbc, "SQLAllocHandle(STMT)");
        }

        // 읽기 경로. 사용자 입력은 반드시 파라미터로 넘긴다 (SQL 인젝션 차단).
        prepare(connection->selectAccount,
                "SELECT id, password_hash, status FROM accounts WHERE username = ?",
                "SQLPrepare(selectAccount)");
        prepare(connection->touchLogin,
                "UPDATE accounts SET last_login_at = CURRENT_TIMESTAMP(3) WHERE id = ?",
                "SQLPrepare(touchLogin)");
        // deleted_at IS NULL 은 모든 캐릭터 조회에 붙는다. 하나라도 빠뜨리면
        // 지운 캐릭터가 로비에 다시 나타나거나 티켓을 받아 갈 수 있다.
        prepare(connection->listCharacters,
                (std::string(kCharacterColumns) +
                 "WHERE c.account_id = ? AND c.deleted_at IS NULL ORDER BY c.id").c_str(),
                "SQLPrepare(listCharacters)");
        prepare(connection->findCharacter,
                (std::string(kCharacterColumns) +
                 "WHERE c.account_id = ? AND c.id = ? AND c.deleted_at IS NULL").c_str(),
                "SQLPrepare(findCharacter)");
        prepare(connection->countCharacters,
                "SELECT COUNT(*) FROM characters WHERE account_id = ? AND deleted_at IS NULL",
                "SQLPrepare(countCharacters)");
        prepare(connection->selectPosition,
                "SELECT map_id, pos_x, pos_y, facing FROM characters "
                "WHERE id = ? AND deleted_at IS NULL",
                "SQLPrepare(selectPosition)");
        // 여기만 deleted_at 을 보지 않는다. 지운 캐릭터도 닉네임은 계속
        // 점유하므로(007_character_delete.sql), 걸러내면 못 쓸 이름을
        // 쓸 수 있다고 답하게 된다.
        prepare(connection->findNickname,
                "SELECT 1 FROM characters WHERE nickname = ? LIMIT 1",
                "SQLPrepare(findNickname)");

        // 쓰기 경로. 권한이 없는 배포도 있을 수 있으므로 실패해도 죽지 않는다.
        // 로그인과 캐릭터 조회는 SELECT/UPDATE 만으로 동작한다.
        bool writable = false;
        try {
            prepare(connection->insertAccount,
                    "INSERT INTO accounts (username, password_hash) VALUES (?, ?)",
                    "SQLPrepare(insertAccount)");
            prepare(connection->insertCharacter,
                    "INSERT INTO characters "
                    "(account_id, nickname, level, "
                    " appearance_gender, appearance_body, appearance_head, "
                    " appearance_hair, appearance_eye, appearance_equipment, "
                    " skin_r, skin_g, skin_b, hair_r, hair_g, hair_b, "
                    " eye_r, eye_g, eye_b, arm_volume, torso_volume, leg_volume) "
                    "VALUES (?, ?, ?,  ?, ?, ?, ?, ?, ?,  ?, ?, ?, ?, ?, ?, ?, ?, ?,  ?, ?, ?)",
                    "SQLPrepare(insertCharacter)");
            prepare(connection->lastInsertId, "SELECT LAST_INSERT_ID()",
                    "SQLPrepare(lastInsertId)");
            // 해금 비트맵 한 행을 캐릭터와 함께 깐다. 행이 없으면 나중에 매번
            // 존재를 확인해야 한다.
            prepare(connection->insertUnlockRow,
                    "INSERT INTO character_unlocks (character_id, dex_bits) "
                    "VALUES (?, UNHEX(REPEAT('00', 160)))",
                    "SQLPrepare(insertUnlockRow)");

            // 비트 하나를 켠다. 읽고-고쳐-쓰지 않고 UPDATE 한 번으로 끝내
            // 동시 해금이 서로를 덮지 않게 한다.
            //
            // 파라미터: 바이트 인덱스, 바이트 인덱스, 비트 마스크, 바이트 인덱스,
            //           캐릭터 id, 계정 id
            // 소유 확인은 JOIN 에 들어 있다 — 남의 캐릭터면 0행이다.
            prepare(connection->setUnlockBit,
                    "UPDATE character_unlocks u "
                    "JOIN characters c ON c.id = u.character_id "
                    "SET u.dex_bits = CONCAT("
                    "      SUBSTRING(u.dex_bits, 1, ?), "
                    "      CHAR(ASCII(SUBSTRING(u.dex_bits, ? + 1, 1)) | ?), "
                    "      SUBSTRING(u.dex_bits, ? + 2)) "
                    "WHERE u.character_id = ? AND c.account_id = ? AND c.deleted_at IS NULL",
                    "SQLPrepare(setUnlockBit)");

            // 해금 여부. 파라미터: 바이트 인덱스, 비트 마스크, 캐릭터 id, 계정 id
            prepare(connection->testUnlockBit,
                    "SELECT (ASCII(SUBSTRING(u.dex_bits, ? + 1, 1)) & ?) <> 0 "
                    "FROM character_unlocks u "
                    "JOIN characters c ON c.id = u.character_id "
                    "WHERE u.character_id = ? AND c.account_id = ? AND c.deleted_at IS NULL",
                    "SQLPrepare(testUnlockBit)");

            // 데리고 다닐 종족. 0 이면 도로 넣는다.
            prepare(connection->setActiveDex,
                    "UPDATE characters SET active_dex = ? "
                    "WHERE id = ? AND account_id = ? AND deleted_at IS NULL",
                    "SQLPrepare(setActiveDex)");
            prepare(connection->selectParty,
                    "SELECT dex FROM character_party WHERE character_id = ? ORDER BY slot",
                    "SQLPrepare(selectParty)");

            // 파티 갱신은 지우고 다시 넣는다. 슬롯별 UPSERT 로 하면 줄어든
            // 파티의 남은 칸을 따로 지워야 하고, 그걸 빠뜨리면 뺀 포켓몬이
            // 그대로 남는다.
            prepare(connection->clearParty,
                    "DELETE p FROM character_party p "
                    "JOIN characters c ON c.id = p.character_id "
                    "WHERE p.character_id = ? AND c.account_id = ? AND c.deleted_at IS NULL",
                    "SQLPrepare(clearParty)");
            prepare(connection->insertPartyMember,
                    "INSERT INTO character_party (character_id, slot, dex) VALUES (?, ?, ?)",
                    "SQLPrepare(insertPartyMember)");
            prepare(connection->touchPlayed,
                    "UPDATE characters SET last_played_at = CURRENT_TIMESTAMP(3) WHERE id = ?",
                    "SQLPrepare(touchPlayed)");
            prepare(connection->updatePosition,
                    "UPDATE characters SET map_id = ?, pos_x = ?, pos_y = ?, facing = ? "
                    "WHERE id = ?",
                    "SQLPrepare(updatePosition)");
            // 소유와 닉네임 일치를 WHERE 에 함께 넣는다. 조건이 하나라도 어긋나면
            // 0행이 갱신되므로 별도 검사 없이 거절된다.
            prepare(connection->softDelete,
                    "UPDATE characters SET deleted_at = CURRENT_TIMESTAMP(3) "
                    "WHERE id = ? AND account_id = ? AND nickname = ? AND deleted_at IS NULL",
                    "SQLPrepare(softDelete)");
            writable = true;
        } catch (const std::exception& e) {
            if (i == 0) {
                spdlog::warn("registration and character creation are unavailable: {}", e.what());
                spdlog::warn("run tools\\apply-migrations.ps1 to grant the missing privileges");
            }
        }
        if (i == 0) {
            canWrite_ = writable;
        }

        free_.push_back(connection.get());
        connections_.push_back(std::move(connection));
    }

    // 계정이 없을 때 돌릴 더미 해시를 미리 만들어 둔다.
    dummyHash_ = hashPassword("");
}

OdbcStore::~OdbcStore() {
    for (auto& connection : connections_) {
        for (SQLHSTMT* statement : connection->all()) {
            if (*statement != SQL_NULL_HSTMT) {
                SQLFreeHandle(SQL_HANDLE_STMT, *statement);
            }
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

OdbcStore::Connection* OdbcStore::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    available_.wait(lock, [this] { return !free_.empty(); });
    Connection* connection = free_.back();
    free_.pop_back();
    return connection;
}

void OdbcStore::release(Connection* connection) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        free_.push_back(connection);
    }
    available_.notify_one();
}

// ------------------------------------------------------------------ 계정

CreateAccountResult OdbcStore::createAccount(std::string_view username,
                                             const std::string& passwordHash) {
    if (!canWrite_) {
        return CreateAccountResult::NotSupported;
    }

    Lease connection(*this);

    std::wstring wideUsername = widen(username);
    std::wstring wideHash = widen(passwordHash);
    SQLLEN usernameLength = 0;
    SQLLEN hashLength = 0;

    SQLRETURN rc = SQL_SUCCESS;
    try {
        bindText(connection->insertAccount, 1, wideUsername, usernameLength);
        bindText(connection->insertAccount, 2, wideHash, hashLength);
        rc = SQLExecute(connection->insertAccount);
    } catch (const std::exception& e) {
        spdlog::error("account insert failed: {}", e.what());
        return CreateAccountResult::Error;
    }

    if (succeeded(rc)) {
        SQLCloseCursor(connection->insertAccount);
        return CreateAccountResult::Created;
    }

    const std::string state = sqlStateOf(connection->insertAccount);
    SQLCloseCursor(connection->insertAccount);

    // accounts 에 남은 유일 제약은 username 하나뿐이므로 원인이 분명하다.
    // 닉네임은 characters 로 갔다.
    if (state == "23000") {
        return CreateAccountResult::UsernameTaken;
    }
    spdlog::error("account insert failed: SQLSTATE {}", state);
    return CreateAccountResult::Error;
}

std::optional<Account> OdbcStore::authenticate(std::string_view username,
                                               std::string_view password) {
    Lease connection(*this);

    std::wstring wideUsername = widen(username);
    SQLLEN usernameLength = 0;

    std::uint64_t id = 0;
    SQLWCHAR passwordHash[kMaxTextChars] = {};
    SQLWCHAR status[64] = {};
    SQLLEN idLength = 0, hashLength = 0, statusLength = 0;

    bool found = false;
    try {
        bindText(connection->selectAccount, 1, wideUsername, usernameLength);
        require(SQLExecute(connection->selectAccount), SQL_HANDLE_STMT, connection->selectAccount,
                "SQLExecute(selectAccount)");

        SQLBindCol(connection->selectAccount, 1, SQL_C_UBIGINT, &id, sizeof(id), &idLength);
        SQLBindCol(connection->selectAccount, 2, SQL_C_WCHAR, passwordHash, sizeof(passwordHash),
                   &hashLength);
        SQLBindCol(connection->selectAccount, 3, SQL_C_WCHAR, status, sizeof(status),
                   &statusLength);

        found = succeeded(SQLFetch(connection->selectAccount));
        SQLCloseCursor(connection->selectAccount);
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->selectAccount);
        spdlog::error("account lookup failed: {}", e.what());
        return std::nullopt;
    }

    if (!found) {
        // 계정이 없어도 검증을 한 번 돌려 응답 시간을 맞춘다.
        // 이게 없으면 응답이 빨리 오는 것만으로 아이디가 없다는 걸 알 수 있다.
        verifyPassword(password, dummyHash_);
        return std::nullopt;
    }

    if (!verifyPassword(password, narrow(passwordHash, hashLength))) {
        return std::nullopt;
    }

    const std::string accountStatus = narrow(status, statusLength);
    if (accountStatus != "active") {
        spdlog::info("login refused for {}: account is {}", std::string(username), accountStatus);
        return std::nullopt;
    }

    // 마지막 로그인 시각 갱신. 실패해도 로그인은 성공시킨다.
    try {
        SQLLEN idParamLength = 0;
        bindUInt64(connection->touchLogin, 1, id, idParamLength);
        require(SQLExecute(connection->touchLogin), SQL_HANDLE_STMT, connection->touchLogin,
                "SQLExecute(touchLogin)");
        SQLCloseCursor(connection->touchLogin);
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->touchLogin);
        spdlog::warn("could not update last_login_at: {}", e.what());
    }

    Account account;
    account.id = id;
    return account;
}

// ---------------------------------------------------------------- 캐릭터

std::vector<Character> OdbcStore::fetchCharacters(SQLHSTMT statement) {
    std::vector<Character> characters;

    std::uint64_t id = 0;
    SQLWCHAR nickname[kMaxTextChars] = {};
    std::uint32_t level = 0;
    std::uint16_t activeDex = 0;

    // 외형. 컬럼 순서는 kCharacterColumns 와 맞아야 한다.
    std::uint8_t appearanceGender = 0;
    std::int32_t appearanceIndex[5] = {};   // body, head, hair, eye, equipment
    double appearanceColor[9] = {};        // skin rgb, hair rgb, eye rgb
    double appearanceVolume[3] = {};       // arm, torso, leg

    // 1280 비트. 013 의 BINARY(160) 과 같은 크기여야 한다.
    unsigned char dexBits[proto::kUnlockBitmapBytes] = {};

    SQLLEN idLength = 0, nicknameLength = 0, levelLength = 0, activeDexLength = 0;
    SQLLEN appearanceLength[18] = {};
    SQLLEN dexBitsLength = 0;

    SQLBindCol(statement, 1, SQL_C_UBIGINT, &id, sizeof(id), &idLength);
    SQLBindCol(statement, 2, SQL_C_WCHAR, nickname, sizeof(nickname), &nicknameLength);
    SQLBindCol(statement, 3, SQL_C_ULONG, &level, sizeof(level), &levelLength);
    SQLBindCol(statement, 4, SQL_C_USHORT, &activeDex, sizeof(activeDex), &activeDexLength);

    SQLBindCol(statement, 5, SQL_C_UTINYINT, &appearanceGender, sizeof(appearanceGender),
               &appearanceLength[0]);
    for (int i = 0; i < 5; ++i) {
        SQLBindCol(statement, static_cast<SQLUSMALLINT>(6 + i), SQL_C_SLONG, &appearanceIndex[i],
                   sizeof(appearanceIndex[i]), &appearanceLength[1 + i]);
    }
    for (int i = 0; i < 9; ++i) {
        SQLBindCol(statement, static_cast<SQLUSMALLINT>(11 + i), SQL_C_DOUBLE, &appearanceColor[i],
                   sizeof(appearanceColor[i]), &appearanceLength[6 + i]);
    }
    for (int i = 0; i < 3; ++i) {
        SQLBindCol(statement, static_cast<SQLUSMALLINT>(20 + i), SQL_C_DOUBLE,
                   &appearanceVolume[i], sizeof(appearanceVolume[i]), &appearanceLength[15 + i]);
    }
    SQLBindCol(statement, 23, SQL_C_BINARY, dexBits, sizeof(dexBits), &dexBitsLength);

    while (succeeded(SQLFetch(statement))) {
        Character character;
        character.id = id;
        character.nickname = narrow(nickname, nicknameLength);
        character.level = level;

        character.appearance.gender = appearanceGender;
        character.appearance.body = appearanceIndex[0];
        character.appearance.head = appearanceIndex[1];
        character.appearance.hair = appearanceIndex[2];
        character.appearance.eye = appearanceIndex[3];
        character.appearance.equipment = appearanceIndex[4];
        character.appearance.skinR = static_cast<float>(appearanceColor[0]);
        character.appearance.skinG = static_cast<float>(appearanceColor[1]);
        character.appearance.skinB = static_cast<float>(appearanceColor[2]);
        character.appearance.hairR = static_cast<float>(appearanceColor[3]);
        character.appearance.hairG = static_cast<float>(appearanceColor[4]);
        character.appearance.hairB = static_cast<float>(appearanceColor[5]);
        character.appearance.eyeR = static_cast<float>(appearanceColor[6]);
        character.appearance.eyeG = static_cast<float>(appearanceColor[7]);
        character.appearance.eyeB = static_cast<float>(appearanceColor[8]);
        character.appearance.armVolume = static_cast<float>(appearanceVolume[0]);
        character.appearance.torsoVolume = static_cast<float>(appearanceVolume[1]);
        character.appearance.legVolume = static_cast<float>(appearanceVolume[2]);

        // 데리고 다니는 종족. 저장된 것은 도감번호뿐이고 능력치는 여기서 만든다.
        // 저장해 두면 레벨이 오를 때마다 갱신해야 하고, 한 군데만 빠뜨려도
        // 조용히 어긋난다.
        if (activeDex != 0) {
            if (const proto::SpeciesBase* base = proto::findSpeciesByDex(activeDex)) {
                character.hasPartner = true;
                character.partner.speciesId = base->id;
                character.partner.stats =
                    proto::computeStats(*base, character.level, {}, {});
            } else {
                // 표에서 사라진 도감번호다. 종족을 목록에서 뺐을 때 생길 수 있다.
                spdlog::warn("character {} carries unknown dex {}", id, activeDex);
            }
        }

        // 해금 목록. 1280 비트를 다 훑지 않고 종족 표 쪽에서 되묻는다 —
        // 구현된 종족이 20 개라 그쪽이 훨씬 짧고, 표에서 빠진 도감번호가
        // 목록에 섞여 나가지도 않는다.
        if (dexBitsLength > 0) {
            const std::size_t bytes =
                std::min<std::size_t>(static_cast<std::size_t>(dexBitsLength), sizeof(dexBits));
            for (const proto::SpeciesBase& base : proto::kSpecies) {
                const std::size_t index = base.dex / 8;
                if (index < bytes && (dexBits[index] & (1u << (base.dex % 8))) != 0) {
                    character.unlocked.push_back(base.dex);
                }
            }
        }

        characters.push_back(std::move(character));
    }

    SQLCloseCursor(statement);
    return characters;
}

// 캐릭터당 한 번씩 더 묻는다. 계정당 캐릭터가 셋뿐이라 조인으로 행을 부풀려
// 중복 제거하는 것보다 이쪽이 짧고 읽기 쉽다.
void OdbcStore::fillParties(Connection& connection, std::vector<Character>& characters) {
    for (Character& character : characters) {
        std::uint64_t id = character.id;
        SQLLEN idLength = 0;
        std::uint16_t dex = 0;
        SQLLEN dexLength = 0;

        try {
            bindUInt64(connection.selectParty, 1, id, idLength);
            require(SQLExecute(connection.selectParty), SQL_HANDLE_STMT, connection.selectParty,
                    "SQLExecute(selectParty)");
            SQLBindCol(connection.selectParty, 1, SQL_C_USHORT, &dex, sizeof(dex), &dexLength);
            while (succeeded(SQLFetch(connection.selectParty))) {
                character.party.push_back(dex);
            }
            SQLCloseCursor(connection.selectParty);
        } catch (const std::exception& e) {
            SQLCloseCursor(connection.selectParty);
            // 파티를 못 읽어도 캐릭터 목록 자체는 돌려준다. 로비가 통째로
            // 비는 것보다 파티 칸이 비는 편이 낫다.
            spdlog::error("party lookup failed for character {}: {}", character.id, e.what());
        }
    }
}

std::vector<Character> OdbcStore::listByAccount(std::uint64_t accountId) {
    Lease connection(*this);

    try {
        SQLLEN accountLength = 0;
        bindUInt64(connection->listCharacters, 1, accountId, accountLength);
        require(SQLExecute(connection->listCharacters), SQL_HANDLE_STMT,
                connection->listCharacters, "SQLExecute(listCharacters)");
        std::vector<Character> characters = fetchCharacters(connection->listCharacters);
        fillParties(*connection, characters);
        return characters;
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->listCharacters);
        spdlog::error("character list failed for account {}: {}", accountId, e.what());
        return {};
    }
}

std::optional<Character> OdbcStore::find(std::uint64_t accountId, std::uint64_t characterId) {
    Lease connection(*this);

    try {
        SQLLEN accountLength = 0;
        SQLLEN characterLength = 0;
        bindUInt64(connection->findCharacter, 1, accountId, accountLength);
        bindUInt64(connection->findCharacter, 2, characterId, characterLength);
        require(SQLExecute(connection->findCharacter), SQL_HANDLE_STMT, connection->findCharacter,
                "SQLExecute(findCharacter)");

        std::vector<Character> found = fetchCharacters(connection->findCharacter);
        if (found.empty()) {
            return std::nullopt;
        }
        fillParties(*connection, found);
        return std::move(found.front());
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->findCharacter);
        spdlog::error("character lookup failed: {}", e.what());
        return std::nullopt;
    }
}

CreateCharacterResult OdbcStore::create(std::uint64_t accountId, std::string_view nickname,
                                        std::uint16_t speciesId,
                                        const Appearance& appearance) {
    if (!canWrite_) {
        return CreateCharacterResult::NotSupported;
    }

    // 0 은 "파트너 없이 시작" 이다. 그 외에는 클라이언트가 보낸 종족을 그대로
    // 믿지 않고 표로 거른다.
    const proto::SpeciesBase* species =
        speciesId == 0 ? nullptr : proto::findSpecies(speciesId);
    if (speciesId != 0 && species == nullptr) {
        return CreateCharacterResult::UnknownSpecies;
    }

    Lease connection(*this);

    // 슬롯 확인.
    try {
        SQLLEN accountLength = 0;
        std::int64_t count = 0;
        SQLLEN countLength = 0;
        bindUInt64(connection->countCharacters, 1, accountId, accountLength);
        require(SQLExecute(connection->countCharacters), SQL_HANDLE_STMT,
                connection->countCharacters, "SQLExecute(countCharacters)");
        SQLBindCol(connection->countCharacters, 1, SQL_C_SBIGINT, &count, sizeof(count),
                   &countLength);
        SQLFetch(connection->countCharacters);
        SQLCloseCursor(connection->countCharacters);

        if (count >= static_cast<std::int64_t>(kMaxCharactersPerAccount)) {
            return CreateCharacterResult::SlotsFull;
        }
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->countCharacters);
        spdlog::error("character count failed: {}", e.what());
        return CreateCharacterResult::Error;
    }

    // 캐릭터, 해금 행, 스타터 해금, 파트너 지정이 함께 생기거나 함께 실패해야
    // 한다. 캐릭터만 남으면 닉네임이 점유된 채로 못 쓰게 되고, 해금 행이 없으면
    // 이후 모든 해금이 0행 갱신으로 조용히 실패한다.
    if (!succeeded(SQLSetConnectAttr(connection->dbc, SQL_ATTR_AUTOCOMMIT,
                                     reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0))) {
        spdlog::error("could not begin a transaction: {}",
                      diagnostics(SQL_HANDLE_DBC, connection->dbc));
        return CreateCharacterResult::Error;
    }
    struct RestoreAutocommit {
        SQLHDBC dbc;
        ~RestoreAutocommit() {
            SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT,
                              reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0);
        }
    } restoreGuard{connection->dbc};

    const auto rollback = [&] {
        SQLEndTran(SQL_HANDLE_DBC, connection->dbc, SQL_ROLLBACK);
    };

    std::wstring wideNickname = widen(nickname);
    std::uint64_t characterId = 0;

    // 바인딩은 SQLExecute 까지 주소가 살아 있어야 하므로 지역 변수로 펼친다.
    std::uint32_t startLevel = proto::kStarterLevel;
    std::uint8_t gender = appearance.gender;
    std::int32_t indices[5] = {appearance.body, appearance.head, appearance.hair,
                               appearance.eye, appearance.equipment};
    double colors[9] = {appearance.skinR, appearance.skinG, appearance.skinB,
                        appearance.hairR, appearance.hairG, appearance.hairB,
                        appearance.eyeR,  appearance.eyeG,  appearance.eyeB};
    double volumes[3] = {appearance.armVolume, appearance.torsoVolume, appearance.legVolume};
    SQLLEN lengths[21] = {};

    try {
        bindUInt64(connection->insertCharacter, 1, accountId, lengths[0]);
        bindText(connection->insertCharacter, 2, wideNickname, lengths[1]);

        // 시작 레벨을 명시한다. 컬럼 기본값에 기대면 kStarterLevel 과 스키마가
        // 따로 놀다가 한쪽만 바뀌었을 때 조용히 어긋난다.
        bindUInt32(connection->insertCharacter, 3, startLevel, lengths[2]);

        require(SQLBindParameter(connection->insertCharacter, 4, SQL_PARAM_INPUT,
                                 SQL_C_UTINYINT, SQL_TINYINT, 0, 0, &gender, 0, &lengths[3]),
                SQL_HANDLE_STMT, connection->insertCharacter, "SQLBindParameter(gender)");
        for (int i = 0; i < 5; ++i) {
            bindInt32(connection->insertCharacter, static_cast<SQLUSMALLINT>(5 + i), indices[i],
                      lengths[4 + i]);
        }
        for (int i = 0; i < 9; ++i) {
            bindDouble(connection->insertCharacter, static_cast<SQLUSMALLINT>(10 + i), colors[i],
                       lengths[9 + i]);
        }
        for (int i = 0; i < 3; ++i) {
            bindDouble(connection->insertCharacter, static_cast<SQLUSMALLINT>(19 + i), volumes[i],
                       lengths[18 + i]);
        }

        const SQLRETURN rc = SQLExecute(connection->insertCharacter);
        if (!succeeded(rc)) {
            const std::string state = sqlStateOf(connection->insertCharacter);
            SQLCloseCursor(connection->insertCharacter);
            rollback();
            if (state == "23000") {
                return CreateCharacterResult::NicknameTaken;
            }
            spdlog::error("character insert failed: SQLSTATE {}", state);
            return CreateCharacterResult::Error;
        }
        SQLCloseCursor(connection->insertCharacter);

        // LAST_INSERT_ID() 는 커넥션마다 따로다. 같은 커넥션에서 바로 읽으므로
        // 다른 스레드의 INSERT 와 섞이지 않는다.
        SQLLEN idLength = 0;
        require(SQLExecute(connection->lastInsertId), SQL_HANDLE_STMT, connection->lastInsertId,
                "SQLExecute(lastInsertId)");
        SQLBindCol(connection->lastInsertId, 1, SQL_C_UBIGINT, &characterId, sizeof(characterId),
                   &idLength);
        const bool gotId = succeeded(SQLFetch(connection->lastInsertId));
        SQLCloseCursor(connection->lastInsertId);
        if (!gotId || characterId == 0) {
            rollback();
            spdlog::error("could not read the new character id");
            return CreateCharacterResult::Error;
        }

        // 빈 비트맵을 깐다.
        SQLLEN unlockRowLength = 0;
        bindUInt64(connection->insertUnlockRow, 1, characterId, unlockRowLength);
        require(SQLExecute(connection->insertUnlockRow), SQL_HANDLE_STMT,
                connection->insertUnlockRow, "SQLExecute(insertUnlockRow)");
        SQLCloseCursor(connection->insertUnlockRow);

        if (species != nullptr) {
            if (!setUnlockBitLocked(*connection, accountId, characterId, species->dex)) {
                rollback();
                spdlog::error("could not unlock the starter for character {}", characterId);
                return CreateCharacterResult::Error;
            }

            std::uint16_t dex = species->dex;
            std::uint64_t owner = accountId;

            // 스타터는 파티 첫 칸에도 들어간다. 꺼내 놓은 것은 언제나 파티
            // 구성원이어야 한다는 규칙이 여기서부터 지켜진다.
            std::uint8_t slot = 0;
            SQLLEN partyLengths[3] = {};
            bindUInt64(connection->insertPartyMember, 1, characterId, partyLengths[0]);
            require(SQLBindParameter(connection->insertPartyMember, 2, SQL_PARAM_INPUT,
                                     SQL_C_UTINYINT, SQL_TINYINT, 0, 0, &slot, 0,
                                     &partyLengths[1]),
                    SQL_HANDLE_STMT, connection->insertPartyMember, "SQLBindParameter(slot)");
            bindUInt16(connection->insertPartyMember, 3, dex, partyLengths[2]);
            require(SQLExecute(connection->insertPartyMember), SQL_HANDLE_STMT,
                    connection->insertPartyMember, "SQLExecute(insertPartyMember)");
            SQLCloseCursor(connection->insertPartyMember);

            SQLLEN activeLengths[3] = {};
            bindUInt16(connection->setActiveDex, 1, dex, activeLengths[0]);
            bindUInt64(connection->setActiveDex, 2, characterId, activeLengths[1]);
            bindUInt64(connection->setActiveDex, 3, owner, activeLengths[2]);
            require(SQLExecute(connection->setActiveDex), SQL_HANDLE_STMT,
                    connection->setActiveDex, "SQLExecute(setActiveDex)");
            SQLCloseCursor(connection->setActiveDex);
        }
    } catch (const std::exception& e) {
        rollback();
        spdlog::error("character creation failed: {}", e.what());
        return CreateCharacterResult::Error;
    }

    if (!succeeded(SQLEndTran(SQL_HANDLE_DBC, connection->dbc, SQL_COMMIT))) {
        spdlog::error("commit failed: {}", diagnostics(SQL_HANDLE_DBC, connection->dbc));
        rollback();
        return CreateCharacterResult::Error;
    }
    return CreateCharacterResult::Created;
}

// 비트 하나를 켠다. 커넥션을 이미 빌린 쪽에서 부른다 — 여기서 다시 빌리면
// 풀 크기만큼의 동시 생성이 서로를 기다리는 교착이 된다.
bool OdbcStore::setUnlockBitLocked(Connection& connection, std::uint64_t accountId,
                                   std::uint64_t characterId, std::uint16_t dex) {
    if (dex == 0) {
        return false;
    }

    // 비트 위치 = 도감번호. 바이트 n 의 비트 k 가 도감번호 (n * 8 + k) 다.
    std::int32_t byteIndex = dex / 8;
    std::int32_t mask = 1 << (dex % 8);
    std::uint64_t character = characterId;
    std::uint64_t owner = accountId;
    SQLLEN lengths[6] = {};

    try {
        bindInt32(connection.setUnlockBit, 1, byteIndex, lengths[0]);
        bindInt32(connection.setUnlockBit, 2, byteIndex, lengths[1]);
        bindInt32(connection.setUnlockBit, 3, mask, lengths[2]);
        bindInt32(connection.setUnlockBit, 4, byteIndex, lengths[3]);
        bindUInt64(connection.setUnlockBit, 5, character, lengths[4]);
        bindUInt64(connection.setUnlockBit, 6, owner, lengths[5]);
        require(SQLExecute(connection.setUnlockBit), SQL_HANDLE_STMT, connection.setUnlockBit,
                "SQLExecute(setUnlockBit)");
        SQLCloseCursor(connection.setUnlockBit);
        return true;
    } catch (const std::exception& e) {
        SQLCloseCursor(connection.setUnlockBit);
        spdlog::error("unlock failed for character {} dex {}: {}", characterId, dex, e.what());
        return false;
    }
}

PartyResult OdbcStore::setParty(std::uint64_t accountId, std::uint64_t characterId,
                                const std::vector<std::uint16_t>& dexNumbers,
                                std::uint16_t activeDex) {
    if (!canWrite_) {
        return PartyResult::NotSupported;
    }
    if (dexNumbers.size() > kMaxPartySize) {
        return PartyResult::TooMany;
    }

    // 클라이언트가 보낸 번호는 하나도 믿지 않는다. 표에 있는 도감번호인지,
    // 중복은 없는지, 꺼낼 한 마리가 파티 안에 있는지 전부 여기서 본다.
    for (std::size_t i = 0; i < dexNumbers.size(); ++i) {
        if (proto::findSpeciesByDex(dexNumbers[i]) == nullptr) {
            return PartyResult::NotUnlocked;
        }
        for (std::size_t j = i + 1; j < dexNumbers.size(); ++j) {
            if (dexNumbers[i] == dexNumbers[j]) {
                return PartyResult::Duplicate;
            }
        }
    }
    if (activeDex != 0 &&
        std::find(dexNumbers.begin(), dexNumbers.end(), activeDex) == dexNumbers.end()) {
        return PartyResult::NotInParty;
    }

    Lease connection(*this);

    std::uint64_t character = characterId;
    std::uint64_t owner = accountId;

    // 해금 확인. 여기서 거르지 않으면 클라이언트가 아무 도감번호나 보내
    // 해금하지 않은 포켓몬을 파티에 넣을 수 있다.
    //
    // 이 조회의 JOIN 이 캐릭터 소유까지 확인한다 — 남의 캐릭터면 0행이다.
    // 파티를 비우는 요청(dexNumbers 가 빈 경우)에는 확인할 것이 없어서
    // 아래 clearParty 의 행 수로 대신 본다.
    try {
        for (const std::uint16_t dex : dexNumbers) {
            std::int32_t byteIndex = dex / 8;
            std::int32_t mask = 1 << (dex % 8);
            std::int32_t unlocked = 0;
            SQLLEN testLengths[4] = {};
            SQLLEN unlockedLength = 0;

            bindInt32(connection->testUnlockBit, 1, byteIndex, testLengths[0]);
            bindInt32(connection->testUnlockBit, 2, mask, testLengths[1]);
            bindUInt64(connection->testUnlockBit, 3, character, testLengths[2]);
            bindUInt64(connection->testUnlockBit, 4, owner, testLengths[3]);
            require(SQLExecute(connection->testUnlockBit), SQL_HANDLE_STMT,
                    connection->testUnlockBit, "SQLExecute(testUnlockBit)");
            SQLBindCol(connection->testUnlockBit, 1, SQL_C_SLONG, &unlocked, sizeof(unlocked),
                       &unlockedLength);
            const bool found = succeeded(SQLFetch(connection->testUnlockBit));
            SQLCloseCursor(connection->testUnlockBit);

            if (!found) {
                return PartyResult::NotFound;
            }
            if (unlocked == 0) {
                return PartyResult::NotUnlocked;
            }
        }
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->testUnlockBit);
        spdlog::error("party unlock check failed for character {}: {}", characterId, e.what());
        return PartyResult::Error;
    }

    // 비우고 다시 채우는 사이에 끊기면 파티가 사라진 채로 남는다.
    if (!succeeded(SQLSetConnectAttr(connection->dbc, SQL_ATTR_AUTOCOMMIT,
                                     reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0))) {
        spdlog::error("could not begin a transaction: {}",
                      diagnostics(SQL_HANDLE_DBC, connection->dbc));
        return PartyResult::Error;
    }
    struct RestoreAutocommit {
        SQLHDBC dbc;
        ~RestoreAutocommit() {
            SQLSetConnectAttr(dbc, SQL_ATTR_AUTOCOMMIT,
                              reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0);
        }
    } restoreGuard{connection->dbc};

    const auto rollback = [&] {
        SQLEndTran(SQL_HANDLE_DBC, connection->dbc, SQL_ROLLBACK);
    };

    try {
        // 슬롯별 UPSERT 로 하지 않는 이유는, 파티가 줄어들면 남은 칸을 따로
        // 지워야 하고 그걸 빠뜨리면 뺀 포켓몬이 그대로 남기 때문이다.
        SQLLEN clearLengths[2] = {};
        bindUInt64(connection->clearParty, 1, character, clearLengths[0]);
        bindUInt64(connection->clearParty, 2, owner, clearLengths[1]);
        require(SQLExecute(connection->clearParty), SQL_HANDLE_STMT, connection->clearParty,
                "SQLExecute(clearParty)");
        SQLCloseCursor(connection->clearParty);

        for (std::size_t i = 0; i < dexNumbers.size(); ++i) {
            std::uint8_t slot = static_cast<std::uint8_t>(i);
            std::uint16_t dex = dexNumbers[i];
            SQLLEN lengths[3] = {};

            bindUInt64(connection->insertPartyMember, 1, character, lengths[0]);
            require(SQLBindParameter(connection->insertPartyMember, 2, SQL_PARAM_INPUT,
                                     SQL_C_UTINYINT, SQL_TINYINT, 0, 0, &slot, 0, &lengths[1]),
                    SQL_HANDLE_STMT, connection->insertPartyMember, "SQLBindParameter(slot)");
            bindUInt16(connection->insertPartyMember, 3, dex, lengths[2]);
            require(SQLExecute(connection->insertPartyMember), SQL_HANDLE_STMT,
                    connection->insertPartyMember, "SQLExecute(insertPartyMember)");
            SQLCloseCursor(connection->insertPartyMember);
        }

        std::uint16_t active = activeDex;
        SQLLEN activeLengths[3] = {};
        bindUInt16(connection->setActiveDex, 1, active, activeLengths[0]);
        bindUInt64(connection->setActiveDex, 2, character, activeLengths[1]);
        bindUInt64(connection->setActiveDex, 3, owner, activeLengths[2]);
        require(SQLExecute(connection->setActiveDex), SQL_HANDLE_STMT, connection->setActiveDex,
                "SQLExecute(setActiveDex)");
        SQLCloseCursor(connection->setActiveDex);
    } catch (const std::exception& e) {
        rollback();
        spdlog::error("party update failed for character {}: {}", characterId, e.what());
        return PartyResult::Error;
    }

    if (!succeeded(SQLEndTran(SQL_HANDLE_DBC, connection->dbc, SQL_COMMIT))) {
        spdlog::error("commit failed: {}", diagnostics(SQL_HANDLE_DBC, connection->dbc));
        rollback();
        return PartyResult::Error;
    }
    return PartyResult::Ok;
}

void OdbcStore::touchPlayed(std::uint64_t characterId) {
    if (!canWrite_) {
        return;
    }

    Lease connection(*this);

    try {
        SQLLEN idLength = 0;
        bindUInt64(connection->touchPlayed, 1, characterId, idLength);
        require(SQLExecute(connection->touchPlayed), SQL_HANDLE_STMT, connection->touchPlayed,
                "SQLExecute(touchPlayed)");
        SQLCloseCursor(connection->touchPlayed);
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->touchPlayed);
        spdlog::warn("could not update last_played_at: {}", e.what());
    }
}

bool OdbcStore::isNicknameTaken(std::string_view nickname) {
    Lease connection(*this);

    std::wstring wide = widen(nickname);
    SQLLEN length = 0;

    try {
        bindText(connection->findNickname, 1, wide, length);
        require(SQLExecute(connection->findNickname), SQL_HANDLE_STMT, connection->findNickname,
                "SQLExecute(findNickname)");
        const bool found = succeeded(SQLFetch(connection->findNickname));
        SQLCloseCursor(connection->findNickname);
        return found;
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->findNickname);
        // 못 읽었으면 "비어 있다" 고 답하지 않는다. 통과시켜 봐야 커마를 다
        // 끝낸 뒤 생성에서 막히고, 그게 이 검사가 없애려던 그 왕복이다.
        spdlog::error("nickname lookup failed: {}", e.what());
        return true;
    }
}

std::optional<Position> OdbcStore::loadPosition(std::uint64_t characterId) {
    Lease connection(*this);

    std::uint32_t mapId = 0;
    double x = 0.0, y = 0.0, facing = 0.0;
    SQLLEN mapLength = 0, xLength = 0, yLength = 0, facingLength = 0;

    try {
        SQLLEN idLength = 0;
        bindUInt64(connection->selectPosition, 1, characterId, idLength);
        require(SQLExecute(connection->selectPosition), SQL_HANDLE_STMT,
                connection->selectPosition, "SQLExecute(selectPosition)");

        SQLBindCol(connection->selectPosition, 1, SQL_C_ULONG, &mapId, sizeof(mapId), &mapLength);
        SQLBindCol(connection->selectPosition, 2, SQL_C_DOUBLE, &x, sizeof(x), &xLength);
        SQLBindCol(connection->selectPosition, 3, SQL_C_DOUBLE, &y, sizeof(y), &yLength);
        SQLBindCol(connection->selectPosition, 4, SQL_C_DOUBLE, &facing, sizeof(facing),
                   &facingLength);

        const bool found = succeeded(SQLFetch(connection->selectPosition));
        SQLCloseCursor(connection->selectPosition);
        if (!found) {
            return std::nullopt;
        }
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->selectPosition);
        spdlog::error("position load failed for character {}: {}", characterId, e.what());
        return std::nullopt;
    }

    Position position;
    position.mapId = mapId;
    position.x = static_cast<float>(x);
    position.y = static_cast<float>(y);
    position.facing = static_cast<float>(facing);
    return position;
}

void OdbcStore::savePosition(std::uint64_t characterId, const Position& position) {
    if (!canWrite_) {
        return;
    }

    Lease connection(*this);

    std::uint32_t mapId = position.mapId;
    double x = position.x;
    double y = position.y;
    double facing = position.facing;
    std::uint64_t id = characterId;
    SQLLEN lengths[5] = {};

    try {
        bindUInt32(connection->updatePosition, 1, mapId, lengths[0]);
        bindDouble(connection->updatePosition, 2, x, lengths[1]);
        bindDouble(connection->updatePosition, 3, y, lengths[2]);
        bindDouble(connection->updatePosition, 4, facing, lengths[3]);
        bindUInt64(connection->updatePosition, 5, id, lengths[4]);
        require(SQLExecute(connection->updatePosition), SQL_HANDLE_STMT,
                connection->updatePosition, "SQLExecute(updatePosition)");
        SQLCloseCursor(connection->updatePosition);
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->updatePosition);
        spdlog::warn("position save failed for character {}: {}", characterId, e.what());
    }
}

DeleteResult OdbcStore::remove(std::uint64_t accountId, std::uint64_t characterId,
                               std::string_view confirmNickname) {
    if (!canWrite_) {
        return DeleteResult::NotSupported;
    }

    // 확인 문자열이 틀린 것과 캐릭터가 없는 것을 구분해서 알려주려면 먼저
    // 조회해야 한다. UPDATE 만으로는 둘 다 0행이라 구분이 안 된다.
    //
    // 커넥션을 빌리기 **전에** 조회한다. find() 도 풀에서 하나 빌리므로,
    // 쥔 채로 부르면 풀 크기만큼의 동시 삭제가 서로를 영원히 기다린다.
    const auto character = find(accountId, characterId);
    if (!character.has_value()) {
        return DeleteResult::NotFound;
    }
    if (character->nickname != confirmNickname) {
        return DeleteResult::NameMismatch;
    }

    Lease connection(*this);

    std::uint64_t id = characterId;
    std::uint64_t owner = accountId;
    std::wstring wideNickname = widen(confirmNickname);
    SQLLEN lengths[3] = {};

    try {
        bindUInt64(connection->softDelete, 1, id, lengths[0]);
        bindUInt64(connection->softDelete, 2, owner, lengths[1]);
        bindText(connection->softDelete, 3, wideNickname, lengths[2]);
        require(SQLExecute(connection->softDelete), SQL_HANDLE_STMT, connection->softDelete,
                "SQLExecute(softDelete)");

        SQLLEN affected = 0;
        SQLRowCount(connection->softDelete, &affected);
        SQLCloseCursor(connection->softDelete);
        return affected > 0 ? DeleteResult::Deleted : DeleteResult::NotFound;
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->softDelete);
        spdlog::error("character delete failed for {}: {}", characterId, e.what());
        return DeleteResult::Error;
    }
}

DeleteResult OdbcStore::releasePartner(std::uint64_t accountId, std::uint64_t characterId) {
    if (!canWrite_) {
        return DeleteResult::NotSupported;
    }

    Lease connection(*this);

    // 0 = 아무도 데리고 다니지 않는다. 해금 비트는 건드리지 않는다 — 한 번 해금한
    // 종족을 다시 잠그는 경로는 없다.
    std::uint16_t none = 0;
    std::uint64_t id = characterId;
    std::uint64_t owner = accountId;
    SQLLEN lengths[3] = {};

    try {
        // 소유 확인이 구문의 WHERE 에 들어 있다. 남의 캐릭터면 0행이다.
        bindUInt16(connection->setActiveDex, 1, none, lengths[0]);
        bindUInt64(connection->setActiveDex, 2, id, lengths[1]);
        bindUInt64(connection->setActiveDex, 3, owner, lengths[2]);
        require(SQLExecute(connection->setActiveDex), SQL_HANDLE_STMT, connection->setActiveDex,
                "SQLExecute(setActiveDex)");

        // 값이 이미 0 이면 MySQL 이 바뀐 행을 세지 않으므로 0 이 나온다. 없는
        // 캐릭터와 구분되지 않지만 둘 다 "내려놓을 것이 없다" 라 같은 답이다.
        SQLLEN affected = 0;
        SQLRowCount(connection->setActiveDex, &affected);
        SQLCloseCursor(connection->setActiveDex);
        return affected > 0 ? DeleteResult::Deleted : DeleteResult::Nothing;
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->setActiveDex);
        spdlog::error("partner release failed for character {}: {}", characterId, e.what());
        return DeleteResult::Error;
    }
}

}  // namespace heaven::data
