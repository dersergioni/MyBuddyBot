#include "core/Storage.h"

#include "core/Logger.h"

#include <fmt/format.h>

namespace mbb
{

namespace
{

constexpr const char* CMD_CLEAR_HISTORY = "clearhistory";

inline void CheckSqliteError(int rc, sqlite3* db, int expected, const char* context = nullptr)
{
    if (rc != expected)
    {
        throw std::runtime_error(
            fmt::format("SQLite error{}: {}", context ? fmt::format(" in {}", context) : "", sqlite3_errmsg(db)));
    }
}

class SqliteStmt
{
  public:
    SqliteStmt(sqlite3* db, const char* sql) : db_(db)
    {
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr);
        CheckSqliteError(rc, db, SQLITE_OK, "prepare");
    }

    ~SqliteStmt()
    {
        if (stmt_)
            sqlite3_finalize(stmt_);
    }

    SqliteStmt(const SqliteStmt&) = delete;
    SqliteStmt& operator=(const SqliteStmt&) = delete;

    SqliteStmt(SqliteStmt&& other) noexcept : stmt_(other.stmt_), db_(other.db_)
    {
        other.stmt_ = nullptr;
    }

    void exec(int expected = SQLITE_DONE)
    {
        int rc = sqlite3_step(stmt_);
        CheckSqliteError(rc, db_, expected, "exec");
    }

    bool next()
    {
        int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW)
            return true;
        if (rc == SQLITE_DONE)
            return false;
        throw std::runtime_error(fmt::format("SQLite error in next: {}", sqlite3_errmsg(db_)));
    }

    void bindInt64(int idx, int64_t value)
    {
        int rc = sqlite3_bind_int64(stmt_, idx, value);
        CheckSqliteError(rc, db_, SQLITE_OK, "bind_int64");
    }

    void bindInt(int idx, int32_t value)
    {
        int rc = sqlite3_bind_int(stmt_, idx, value);
        CheckSqliteError(rc, db_, SQLITE_OK, "bind_int");
    }

    void bindText(int idx, const std::string& value)
    {
        int rc = sqlite3_bind_text(stmt_, idx, value.c_str(), -1, SQLITE_TRANSIENT);
        CheckSqliteError(rc, db_, SQLITE_OK, "bind_text");
    }

    void bindText(int idx, const char* value)
    {
        int rc = sqlite3_bind_text(stmt_, idx, value, -1, SQLITE_TRANSIENT);
        CheckSqliteError(rc, db_, SQLITE_OK, "bind_text");
    }

    [[nodiscard]] int columnInt(int idx) const
    {
        return sqlite3_column_int(stmt_, idx);
    }

    [[nodiscard]] int64_t columnInt64(int idx) const
    {
        return sqlite3_column_int64(stmt_, idx);
    }

    [[nodiscard]] std::string columnText(int idx) const
    {
        const unsigned char* text = sqlite3_column_text(stmt_, idx);
        return text ? std::string(reinterpret_cast<const char*>(text)) : std::string();
    }

  private:
    sqlite3_stmt* stmt_ = nullptr;
    sqlite3* db_ = nullptr;
};

const char* RoleToString(Storage::Role role)
{
    switch (role)
    {
    case Storage::Role::User:
        return "user";
    case Storage::Role::AssistantOpenAI:
    case Storage::Role::AssistantXAI:
    case Storage::Role::AssistantGoogle:
        return "assistant";
    case Storage::Role::System:
        return "system";
    }
    return "unknown";
}

} // anonymous namespace

Storage::Storage(const std::filesystem::path& dbFullPath) : dbPath(dbFullPath), db(nullptr, sqlite3_close)
{
    Logger::Info("Creating app db");
    CreateDb();
    InitDb();
}

void Storage::CreateDb()
{
    sqlite3* sqlite_db;
    int rc = sqlite3_open(dbPath.string().c_str(), &sqlite_db);
    CheckSqliteError(rc, sqlite_db, SQLITE_OK, "open");

    db = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>(sqlite_db, sqlite3_close);
    Logger::Info(fmt::format("Opened {} database successfully", dbPath.string()));
}

void Storage::InitDb()
{
    const char* GPT_CHAT = "CREATE TABLE IF NOT EXISTS GPT_CHAT ("
                           "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "CHAT_ID INT NOT NULL,"
                           "THREAD_ID INT NOT NULL DEFAULT 0,"
                           "USERNAME TEXT NOT NULL,"
                           "ROLE TEXT NOT NULL,"
                           "MESSAGE TEXT NOT NULL,"
                           "CREATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP,"
                           "UPDATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP"
                           ");";
    CreateTable(GPT_CHAT);

    const char* GPT_SYSTEM_MSG = "CREATE TABLE IF NOT EXISTS GPT_SYSTEM_MSG ("
                                 "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                 "CHAT_ID INT NOT NULL,"
                                 "THREAD_ID INT NOT NULL DEFAULT 0,"
                                 "USERNAME TEXT NOT NULL,"
                                 "ROLE TEXT NOT NULL,"
                                 "MESSAGE TEXT NOT NULL,"
                                 "CREATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP,"
                                 "UPDATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP"
                                 ");";
    CreateTable(GPT_SYSTEM_MSG);

    const char* GPT_SYSTEM_CMD = "CREATE TABLE IF NOT EXISTS GPT_SYSTEM_CMD ("
                                 "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                 "CHAT_ID INT NOT NULL,"
                                 "THREAD_ID INT NOT NULL DEFAULT 0,"
                                 "USERNAME TEXT NOT NULL,"
                                 "ROLE TEXT NOT NULL,"
                                 "MESSAGE TEXT NOT NULL,"
                                 "CREATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP,"
                                 "UPDATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP"
                                 ");";
    CreateTable(GPT_SYSTEM_CMD);

    const char* OPEN_AI_TOKEN_STAT = "CREATE TABLE IF NOT EXISTS OPEN_AI_TOKEN_STAT ("
                                     "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                     "OUTCOME_PAYLOADS INT NOT NULL DEFAULT 0,"
                                     "INCOME_PAYLOADS INT NOT NULL DEFAULT 0,"
                                     "OUTCOME_TOKENS INT NOT NULL DEFAULT 0,"
                                     "INCOME_TOKENS INT NOT NULL DEFAULT 0,"
                                     "CREATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP,"
                                     "UPDATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP"
                                     ");";
    CreateTable(OPEN_AI_TOKEN_STAT);

    const char* GPT_CHAT_MSG_ID = "CREATE TABLE IF NOT EXISTS GPT_CHAT_MSG_ID ("
                                  "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                                  "CHAT_ID INT NOT NULL,"
                                  "THREAD_ID INT NOT NULL DEFAULT 0,"
                                  "MESSAGE_ID INT NOT NULL,"
                                  "CREATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP,"
                                  "UPDATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP"
                                  ");";
    CreateTable(GPT_CHAT_MSG_ID);

    const char* IDX_GPT_CHAT = "CREATE INDEX IF NOT EXISTS idx_gpt_chat_chat_thread_id "
                               "ON GPT_CHAT (CHAT_ID, THREAD_ID, ID);";
    CreateTable(IDX_GPT_CHAT);

    const char* IDX_GPT_CHAT_MSG_ID = "CREATE INDEX IF NOT EXISTS idx_gpt_chat_msg_id_chat_thread "
                                      "ON GPT_CHAT_MSG_ID (CHAT_ID, THREAD_ID, MESSAGE_ID);";
    CreateTable(IDX_GPT_CHAT_MSG_ID);

    const char* USERS = "CREATE TABLE IF NOT EXISTS USERS ("
                        "USER_ID INTEGER PRIMARY KEY,"
                        "USERNAME TEXT,"
                        "FIRST_NAME TEXT NOT NULL,"
                        "UPDATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP"
                        ");";
    CreateTable(USERS);

    const char* IDX_USERS_USERNAME = "CREATE INDEX IF NOT EXISTS idx_users_username "
                                     "ON USERS (USERNAME);";
    CreateTable(IDX_USERS_USERNAME);

    const char* CHATS = "CREATE TABLE IF NOT EXISTS CHATS ("
                        "CHAT_ID INTEGER NOT NULL,"
                        "THREAD_ID INTEGER NOT NULL DEFAULT 0,"
                        "UPDATED_AT DATETIME DEFAULT CURRENT_TIMESTAMP,"
                        "PRIMARY KEY (CHAT_ID, THREAD_ID)"
                        ");";
    CreateTable(CHATS);
}

void Storage::CreateTable(const std::string& sql)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(), sql.c_str());
    stmt.exec();

    Logger::Info("Table created successfully");
}

void Storage::InsertMsgId(int64_t chatId, int32_t threadId, int32_t msgId)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(), "INSERT INTO GPT_CHAT_MSG_ID (CHAT_ID, THREAD_ID, MESSAGE_ID) VALUES (?, ?, ?);");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);
    stmt.bindInt(3, msgId);
    stmt.exec();
}

void Storage::DeleteMsgId(int64_t chatId, int32_t threadId, int32_t msgId)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(), "DELETE FROM GPT_CHAT_MSG_ID WHERE CHAT_ID = ? AND THREAD_ID = ? AND MESSAGE_ID = ?;");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);
    stmt.bindInt(3, msgId);
    stmt.exec();
}

std::set<int32_t> Storage::SelectMsgIds(int64_t chatId, int32_t threadId)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(), "SELECT MESSAGE_ID FROM GPT_CHAT_MSG_ID WHERE CHAT_ID = ? AND THREAD_ID = ?;");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);

    std::set<int32_t> result;
    while (stmt.next())
    {
        result.insert(stmt.columnInt(0));
    }
    return result;
}

void Storage::InsertSystemCommand(int64_t chatId,
                                  int32_t threadId,
                                  const std::string& username,
                                  const std::string& command)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(),
                    "INSERT INTO GPT_SYSTEM_CMD (CHAT_ID, THREAD_ID, USERNAME, ROLE, MESSAGE) VALUES (?, ?, ?, ?, ?);");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);
    stmt.bindText(3, username);
    stmt.bindText(4, "system");
    stmt.bindText(5, command);
    stmt.exec();
}

Storage::ClearHistoryInfo Storage::GetClearHistoryInfo(int64_t chatId, int32_t threadId)
{
    // Note: dbMutex should already be locked by caller

    SqliteStmt stmt(db.get(), "SELECT ID, USERNAME, MESSAGE, CREATED_AT FROM GPT_SYSTEM_CMD "
                              "WHERE CHAT_ID = ? AND THREAD_ID = ? AND MESSAGE = ? "
                              "ORDER BY ID DESC LIMIT 1;");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);
    stmt.bindText(3, CMD_CLEAR_HISTORY);

    ClearHistoryInfo result{0, 0, "", "", ""};
    if (stmt.next())
    {
        result.chatId = chatId;
        result.threadId = threadId;
        result.username = stmt.columnText(1);
        result.message = stmt.columnText(2);
        result.timestamp = stmt.columnText(3);
    }
    return result;
}

void Storage::InsertSystemMessage(
    int64_t chatId, int32_t threadId, const std::string& username, Role role, const std::string& message)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(),
                    "INSERT INTO GPT_SYSTEM_MSG (CHAT_ID, THREAD_ID, USERNAME, ROLE, MESSAGE) VALUES (?, ?, ?, ?, ?);");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);
    stmt.bindText(3, username);
    stmt.bindText(4, RoleToString(role));
    stmt.bindText(5, message);
    stmt.exec();
}

void Storage::InsertChatRecord(
    int64_t chatId, int32_t threadId, const std::string& username, Role role, const std::string& message)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(),
                    "INSERT INTO GPT_CHAT (CHAT_ID, THREAD_ID, USERNAME, ROLE, MESSAGE) VALUES (?, ?, ?, ?, ?);");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);
    stmt.bindText(3, username);
    stmt.bindText(4, RoleToString(role));
    stmt.bindText(5, message);
    stmt.exec();
}

std::string Storage::GetSystemMessageUnlocked(int64_t chatId, int32_t threadId)
{
    // Note: dbMutex should already be locked by caller

    SqliteStmt stmt(db.get(), "SELECT MESSAGE FROM GPT_SYSTEM_MSG "
                              "WHERE CHAT_ID = ? AND THREAD_ID = ? AND ROLE='system' "
                              "ORDER BY ID DESC LIMIT 1;");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);

    if (stmt.next())
    {
        return stmt.columnText(0);
    }
    return "";
}

std::string Storage::GetSystemMessage(int64_t chatId, int32_t threadId)
{
    std::lock_guard<std::mutex> lock(dbMutex);
    return GetSystemMessageUnlocked(chatId, threadId);
}

void Storage::ClearSystemMessage(int64_t chatId, int32_t threadId)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(), "DELETE FROM GPT_SYSTEM_MSG WHERE CHAT_ID = ? AND THREAD_ID = ?;");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);
    stmt.exec();
}

std::vector<std::pair<std::string, std::string>> Storage::SelectActualUserChatHistory(int64_t chatId, int32_t threadId)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    auto clearHistoryInfo = GetClearHistoryInfo(chatId, threadId);

    SqliteStmt stmt(db.get(), "SELECT ROLE, MESSAGE FROM GPT_CHAT "
                              "WHERE CHAT_ID = ? AND THREAD_ID = ? AND CREATED_AT > ? "
                              "ORDER BY ID ASC;");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);
    stmt.bindText(3, clearHistoryInfo.timestamp);

    std::vector<std::pair<std::string, std::string>> result;
    while (stmt.next())
    {
        result.emplace_back(stmt.columnText(0), stmt.columnText(1));
    }

    auto systemMsg = GetSystemMessageUnlocked(chatId, threadId);
    if (!systemMsg.empty())
    {
        result.insert(result.begin(), std::make_pair("system", systemMsg));
    }

    return result;
}

// =============================================================================
// User Registry
// =============================================================================

void Storage::UpsertUser(int64_t userId, const std::string& username, const std::string& firstName)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(), "INSERT INTO USERS (USER_ID, USERNAME, FIRST_NAME, UPDATED_AT) "
                              "VALUES (?, ?, ?, CURRENT_TIMESTAMP) "
                              "ON CONFLICT(USER_ID) DO UPDATE SET "
                              "USERNAME = excluded.USERNAME, "
                              "FIRST_NAME = excluded.FIRST_NAME, "
                              "UPDATED_AT = CURRENT_TIMESTAMP;");
    stmt.bindInt64(1, userId);
    stmt.bindText(2, username);
    stmt.bindText(3, firstName);
    stmt.exec();
}

int64_t Storage::LookupUserByUsername(const std::string& username)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(), "SELECT USER_ID FROM USERS WHERE USERNAME = ? LIMIT 1;");
    stmt.bindText(1, username);

    if (stmt.next())
    {
        return stmt.columnInt64(0);
    }
    return 0;
}

std::string Storage::GetUserDisplayName(int64_t userId)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(), "SELECT USERNAME, FIRST_NAME FROM USERS WHERE USER_ID = ? LIMIT 1;");
    stmt.bindInt64(1, userId);

    if (stmt.next())
    {
        auto username = stmt.columnText(0);
        auto firstName = stmt.columnText(1);
        if (!username.empty())
        {
            return fmt::format("{} (@{})", firstName, username);
        }
        return firstName;
    }
    return fmt::format("User {}", userId);
}

// =============================================================================
// Chat Registry
// =============================================================================

void Storage::UpsertChat(int64_t chatId, int32_t threadId)
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(), "INSERT INTO CHATS (CHAT_ID, THREAD_ID, UPDATED_AT) "
                              "VALUES (?, ?, CURRENT_TIMESTAMP) "
                              "ON CONFLICT(CHAT_ID, THREAD_ID) DO UPDATE SET "
                              "UPDATED_AT = CURRENT_TIMESTAMP;");
    stmt.bindInt64(1, chatId);
    stmt.bindInt(2, threadId);
    stmt.exec();
}

std::vector<std::pair<int64_t, int32_t>> Storage::SelectAllChats()
{
    std::lock_guard<std::mutex> lock(dbMutex);

    SqliteStmt stmt(db.get(), "SELECT CHAT_ID, THREAD_ID FROM CHATS;");

    std::vector<std::pair<int64_t, int32_t>> result;
    while (stmt.next())
    {
        result.emplace_back(stmt.columnInt64(0), stmt.columnInt(1));
    }
    return result;
}

} // namespace mbb
