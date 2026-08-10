#include "OdbcStore.h"

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

// 개체값을 굴린다. 스탯당 0~31 이고 태어날 때 한 번 정해진다.
proto::StatSpread rollIndividualValues() {
    // 게임 밸런스용이라 암호학적 난수가 필요하지 않다. 스레드마다 하나 둔다.
    static thread_local std::mt19937 engine{std::random_device{}()};
    std::uniform_int_distribution<int> roll(0, proto::kMaxIndividualValue);

    proto::StatSpread spread;
    spread.hp = static_cast<std::uint16_t>(roll(engine));
    spread.atk = static_cast<std::uint16_t>(roll(engine));
    spread.def = static_cast<std::uint16_t>(roll(engine));
    spread.spAtk = static_cast<std::uint16_t>(roll(engine));
    spread.spDef = static_cast<std::uint16_t>(roll(engine));
    spread.speed = static_cast<std::uint16_t>(roll(engine));
    return spread;
}

// 캐릭터 조회 두 구문이 공유하는 컬럼 목록. 순서가 fetchCharacters 의
// 바인딩과 일치해야 한다.
constexpr const char* kCharacterColumns =
    "SELECT c.id, c.nickname, "
    "       p.species_id, p.nickname, p.level, "
    "       p.iv_hp, p.iv_atk, p.iv_def, p.iv_sp_atk, p.iv_sp_def, p.iv_speed, "
    "       p.ev_hp, p.ev_atk, p.ev_def, p.ev_sp_atk, p.ev_sp_def, p.ev_speed "
    "FROM characters c "
    "LEFT JOIN character_pokemon p ON p.character_id = c.id AND p.slot = 0 ";

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
    SQLHSTMT insertPokemon = SQL_NULL_HSTMT;
    SQLHSTMT touchPlayed = SQL_NULL_HSTMT;
    SQLHSTMT selectPosition = SQL_NULL_HSTMT;
    SQLHSTMT softDelete = SQL_NULL_HSTMT;
    SQLHSTMT deletePartner = SQL_NULL_HSTMT;
    SQLHSTMT updatePosition = SQL_NULL_HSTMT;

    // 모든 구문 핸들을 한 번에 돌기 위한 목록.
    std::vector<SQLHSTMT*> all() {
        return {&selectAccount,   &touchLogin,      &insertAccount,   &listCharacters,
                &findCharacter,   &countCharacters, &insertCharacter, &lastInsertId,
                &insertPokemon,   &touchPlayed,     &selectPosition,  &updatePosition,
                &softDelete,      &deletePartner};
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
                "or pass --db-driver / --db-connection-string.");
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
                "SELECT map_id, pos_x, pos_y FROM characters "
                "WHERE id = ? AND deleted_at IS NULL",
                "SQLPrepare(selectPosition)");

        // 쓰기 경로. 권한이 없는 배포도 있을 수 있으므로 실패해도 죽지 않는다.
        // 로그인과 캐릭터 조회는 SELECT/UPDATE 만으로 동작한다.
        bool writable = false;
        try {
            prepare(connection->insertAccount,
                    "INSERT INTO accounts (username, password_hash) VALUES (?, ?)",
                    "SQLPrepare(insertAccount)");
            prepare(connection->insertCharacter,
                    "INSERT INTO characters (account_id, nickname) VALUES (?, ?)",
                    "SQLPrepare(insertCharacter)");
            prepare(connection->lastInsertId, "SELECT LAST_INSERT_ID()",
                    "SQLPrepare(lastInsertId)");
            prepare(connection->insertPokemon,
                    "INSERT INTO character_pokemon "
                    "(character_id, slot, species_id, level, "
                    " iv_hp, iv_atk, iv_def, iv_sp_atk, iv_sp_def, iv_speed) "
                    "VALUES (?, 0, ?, ?, ?, ?, ?, ?, ?, ?)",
                    "SQLPrepare(insertPokemon)");
            prepare(connection->touchPlayed,
                    "UPDATE characters SET last_played_at = CURRENT_TIMESTAMP(3) WHERE id = ?",
                    "SQLPrepare(touchPlayed)");
            prepare(connection->updatePosition,
                    "UPDATE characters SET map_id = ?, pos_x = ?, pos_y = ? WHERE id = ?",
                    "SQLPrepare(updatePosition)");
            // 소유와 닉네임 일치를 WHERE 에 함께 넣는다. 조건이 하나라도 어긋나면
            // 0행이 갱신되므로 별도 검사 없이 거절된다.
            prepare(connection->softDelete,
                    "UPDATE characters SET deleted_at = CURRENT_TIMESTAMP(3) "
                    "WHERE id = ? AND account_id = ? AND nickname = ? AND deleted_at IS NULL",
                    "SQLPrepare(softDelete)");
            prepare(connection->deletePartner,
                    "DELETE p FROM character_pokemon p JOIN characters c ON c.id = p.character_id "
                    "WHERE p.character_id = ? AND p.slot = 0 "
                    "  AND c.account_id = ? AND c.deleted_at IS NULL",
                    "SQLPrepare(deletePartner)");
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

    Connection* connection = acquire();
    struct Release {
        OdbcStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

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
    Connection* connection = acquire();
    struct Release {
        OdbcStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

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

        found = SQLFetch(connection->selectAccount) == SQL_SUCCESS;
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

std::vector<Character> OdbcStore::fetchCharacters(Connection*, SQLHSTMT statement) {
    std::vector<Character> characters;

    std::uint64_t id = 0;
    SQLWCHAR nickname[kMaxTextChars] = {};

    std::uint16_t speciesId = 0;
    SQLWCHAR partnerNickname[kMaxTextChars] = {};
    std::uint32_t partnerLevel = 0;
    // 실 수치는 읽지 않는다. 저장돼 있지 않고 아래에서 계산한다.
    std::uint8_t iv[6] = {};
    std::uint8_t ev[6] = {};

    SQLLEN idLength = 0, nicknameLength = 0;
    SQLLEN speciesLength = 0, partnerNicknameLength = 0, partnerLevelLength = 0;
    SQLLEN spreadLength[12] = {};

    SQLBindCol(statement, 1, SQL_C_UBIGINT, &id, sizeof(id), &idLength);
    SQLBindCol(statement, 2, SQL_C_WCHAR, nickname, sizeof(nickname), &nicknameLength);
    SQLBindCol(statement, 3, SQL_C_USHORT, &speciesId, sizeof(speciesId), &speciesLength);
    SQLBindCol(statement, 4, SQL_C_WCHAR, partnerNickname, sizeof(partnerNickname),
               &partnerNicknameLength);
    SQLBindCol(statement, 5, SQL_C_ULONG, &partnerLevel, sizeof(partnerLevel), &partnerLevelLength);
    for (int i = 0; i < 6; ++i) {
        SQLBindCol(statement, static_cast<SQLUSMALLINT>(6 + i), SQL_C_UTINYINT, &iv[i],
                   sizeof(iv[i]), &spreadLength[i]);
        SQLBindCol(statement, static_cast<SQLUSMALLINT>(12 + i), SQL_C_UTINYINT, &ev[i],
                   sizeof(ev[i]), &spreadLength[6 + i]);
    }

    while (SQLFetch(statement) == SQL_SUCCESS) {
        Character character;
        character.id = id;
        character.nickname = narrow(nickname, nicknameLength);

        // LEFT JOIN 이라 파트너가 없으면 NULL 이 온다. 004 이전에 만들어진
        // 캐릭터가 그럴 수 있다.
        if (speciesLength != SQL_NULL_DATA) {
            character.hasPartner = true;
            character.partner.speciesId = speciesId;
            character.partner.nickname =
                partnerNicknameLength == SQL_NULL_DATA
                    ? std::string()
                    : narrow(partnerNickname, partnerNicknameLength);
            character.partner.level = partnerLevel;
            character.partner.ivs = {iv[0], iv[1], iv[2], iv[3], iv[4], iv[5]};
            character.partner.evs = {ev[0], ev[1], ev[2], ev[3], ev[4], ev[5]};

            // 실 수치는 여기서 만든다. 저장하면 레벨업이나 노력치 변화 때마다
            // 갱신해야 하고, 한 군데만 빠뜨려도 조용히 어긋난다.
            if (const proto::SpeciesBase* base = proto::findSpecies(speciesId)) {
                character.partner.stats = proto::computeStats(*base, partnerLevel,
                                                              character.partner.ivs,
                                                              character.partner.evs);
            } else {
                // 종족 표에서 사라진 id 다. 배열을 줄였을 때 생길 수 있다.
                spdlog::warn("character {} has unknown species {}", id, speciesId);
            }
        }
        characters.push_back(std::move(character));
    }

    SQLCloseCursor(statement);
    return characters;
}

std::vector<Character> OdbcStore::listByAccount(std::uint64_t accountId) {
    Connection* connection = acquire();
    struct Release {
        OdbcStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

    try {
        SQLLEN accountLength = 0;
        bindUInt64(connection->listCharacters, 1, accountId, accountLength);
        require(SQLExecute(connection->listCharacters), SQL_HANDLE_STMT,
                connection->listCharacters, "SQLExecute(listCharacters)");
        return fetchCharacters(connection, connection->listCharacters);
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->listCharacters);
        spdlog::error("character list failed for account {}: {}", accountId, e.what());
        return {};
    }
}

std::optional<Character> OdbcStore::find(std::uint64_t accountId, std::uint64_t characterId) {
    Connection* connection = acquire();
    struct Release {
        OdbcStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

    try {
        SQLLEN accountLength = 0;
        SQLLEN characterLength = 0;
        bindUInt64(connection->findCharacter, 1, accountId, accountLength);
        bindUInt64(connection->findCharacter, 2, characterId, characterLength);
        require(SQLExecute(connection->findCharacter), SQL_HANDLE_STMT, connection->findCharacter,
                "SQLExecute(findCharacter)");

        std::vector<Character> found = fetchCharacters(connection, connection->findCharacter);
        if (found.empty()) {
            return std::nullopt;
        }
        return std::move(found.front());
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->findCharacter);
        spdlog::error("character lookup failed: {}", e.what());
        return std::nullopt;
    }
}

CreateCharacterResult OdbcStore::create(std::uint64_t accountId, std::string_view nickname,
                                        std::uint16_t speciesId) {
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
    // 개체값은 태어날 때 한 번 굴리고 바뀌지 않는다. 노력치는 0 에서 시작한다.
    const proto::StatSpread ivs = species != nullptr ? rollIndividualValues()
                                                     : proto::StatSpread{};

    Connection* connection = acquire();
    struct Release {
        OdbcStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

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

    // 파트너를 같이 만들 때는 둘이 함께 생기거나 함께 실패해야 한다.
    // 캐릭터만 남으면 유일 제약 때문에 같은 닉네임으로 다시 만들 수도 없다.
    // 파트너 없이 만드는 경우에도 같은 경로를 쓴다 — INSERT 하나짜리 트랜잭션은
    // 해가 없고, 분기를 두 벌로 만드는 것보다 낫다.
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

    try {
        SQLLEN accountLength = 0;
        SQLLEN nicknameLength = 0;
        bindUInt64(connection->insertCharacter, 1, accountId, accountLength);
        bindText(connection->insertCharacter, 2, wideNickname, nicknameLength);

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
        const bool gotId = SQLFetch(connection->lastInsertId) == SQL_SUCCESS;
        SQLCloseCursor(connection->lastInsertId);
        if (!gotId || characterId == 0) {
            rollback();
            spdlog::error("could not read the new character id");
            return CreateCharacterResult::Error;
        }

        if (species == nullptr) {
            // 파트너 없이 시작한다. 나중에 잡으면 slot 0 에 들어간다.
            if (!succeeded(SQLEndTran(SQL_HANDLE_DBC, connection->dbc, SQL_COMMIT))) {
                spdlog::error("commit failed: {}",
                              diagnostics(SQL_HANDLE_DBC, connection->dbc));
                rollback();
                return CreateCharacterResult::Error;
            }
            return CreateCharacterResult::Created;
        }

        std::uint32_t level = proto::kStarterLevel;
        std::uint16_t speciesParam = speciesId;
        std::uint16_t values[6] = {ivs.hp, ivs.atk, ivs.def, ivs.spAtk, ivs.spDef, ivs.speed};
        SQLLEN lengths[9] = {};

        bindUInt64(connection->insertPokemon, 1, characterId, lengths[0]);
        bindUInt16(connection->insertPokemon, 2, speciesParam, lengths[1]);
        bindUInt32(connection->insertPokemon, 3, level, lengths[2]);
        for (int i = 0; i < 6; ++i) {
            bindUInt16(connection->insertPokemon, static_cast<SQLUSMALLINT>(4 + i), values[i],
                       lengths[3 + i]);
        }

        require(SQLExecute(connection->insertPokemon), SQL_HANDLE_STMT, connection->insertPokemon,
                "SQLExecute(insertPokemon)");
        SQLCloseCursor(connection->insertPokemon);
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

void OdbcStore::touchPlayed(std::uint64_t characterId) {
    if (!canWrite_) {
        return;
    }

    Connection* connection = acquire();
    struct Release {
        OdbcStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

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

std::optional<Position> OdbcStore::loadPosition(std::uint64_t characterId) {
    Connection* connection = acquire();
    struct Release {
        OdbcStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

    std::uint32_t mapId = 0;
    double x = 0.0, y = 0.0;
    SQLLEN mapLength = 0, xLength = 0, yLength = 0;

    try {
        SQLLEN idLength = 0;
        bindUInt64(connection->selectPosition, 1, characterId, idLength);
        require(SQLExecute(connection->selectPosition), SQL_HANDLE_STMT,
                connection->selectPosition, "SQLExecute(selectPosition)");

        SQLBindCol(connection->selectPosition, 1, SQL_C_ULONG, &mapId, sizeof(mapId), &mapLength);
        SQLBindCol(connection->selectPosition, 2, SQL_C_DOUBLE, &x, sizeof(x), &xLength);
        SQLBindCol(connection->selectPosition, 3, SQL_C_DOUBLE, &y, sizeof(y), &yLength);

        const bool found = SQLFetch(connection->selectPosition) == SQL_SUCCESS;
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
    return position;
}

void OdbcStore::savePosition(std::uint64_t characterId, const Position& position) {
    if (!canWrite_) {
        return;
    }

    Connection* connection = acquire();
    struct Release {
        OdbcStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

    std::uint32_t mapId = position.mapId;
    double x = position.x;
    double y = position.y;
    std::uint64_t id = characterId;
    SQLLEN lengths[4] = {};

    try {
        bindUInt32(connection->updatePosition, 1, mapId, lengths[0]);
        bindDouble(connection->updatePosition, 2, x, lengths[1]);
        bindDouble(connection->updatePosition, 3, y, lengths[2]);
        bindUInt64(connection->updatePosition, 4, id, lengths[3]);
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

    Connection* connection = acquire();
    struct Release {
        OdbcStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

    // 확인 문자열이 틀린 것과 캐릭터가 없는 것을 구분해서 알려주려면 먼저
    // 조회해야 한다. UPDATE 만으로는 둘 다 0행이라 구분이 안 된다.
    const auto character = find(accountId, characterId);
    if (!character.has_value()) {
        return DeleteResult::NotFound;
    }
    if (character->nickname != confirmNickname) {
        return DeleteResult::NameMismatch;
    }

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

    Connection* connection = acquire();
    struct Release {
        OdbcStore* store;
        Connection* connection;
        ~Release() { store->release(connection); }
    } releaseGuard{this, connection};

    std::uint64_t id = characterId;
    std::uint64_t owner = accountId;
    SQLLEN lengths[2] = {};

    try {
        // 소유 확인이 구문의 JOIN 에 들어 있다. 남의 캐릭터면 0행이다.
        bindUInt64(connection->deletePartner, 1, id, lengths[0]);
        bindUInt64(connection->deletePartner, 2, owner, lengths[1]);
        require(SQLExecute(connection->deletePartner), SQL_HANDLE_STMT, connection->deletePartner,
                "SQLExecute(deletePartner)");

        SQLLEN affected = 0;
        SQLRowCount(connection->deletePartner, &affected);
        SQLCloseCursor(connection->deletePartner);
        return affected > 0 ? DeleteResult::Deleted : DeleteResult::Nothing;
    } catch (const std::exception& e) {
        SQLCloseCursor(connection->deletePartner);
        spdlog::error("partner release failed for character {}: {}", characterId, e.what());
        return DeleteResult::Error;
    }
}

}  // namespace heaven::data
