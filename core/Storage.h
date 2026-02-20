#pragma once

#include <filesystem>
#include <mutex>
#include <set>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace mbb
{

class Storage
{
  public:
    enum class Role
    {
        User,
        AssistantOpenAI,
        AssistantXAI,
        AssistantGoogle,
        System
    };

    Storage() = delete;

    explicit Storage(const std::filesystem::path& dbFullPath);

    ~Storage() = default;

    void InsertMsgId(int64_t chatId, int32_t threadId, int32_t msgId);

    void DeleteMsgId(int64_t chatId, int32_t threadId, int32_t msgId);

    [[nodiscard]] std::set<int32_t> SelectMsgIds(int64_t chatId, int32_t threadId);

    void InsertSystemCommand(int64_t chatId, int32_t threadId, const std::string& username, const std::string& command);

    void InsertChatRecord(
        int64_t chatId, int32_t threadId, const std::string& username, Role role, const std::string& message);

    void InsertSystemMessage(
        int64_t chatId, int32_t threadId, const std::string& username, Role role, const std::string& message);

    [[nodiscard]] std::string GetSystemMessage(int64_t chatId, int32_t threadId);

    void ClearSystemMessage(int64_t chatId, int32_t threadId);

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> SelectActualUserChatHistory(int64_t chatId,
                                                                                               int32_t threadId);

  private:
    struct ClearHistoryInfo
    {
        int64_t chatId;
        int32_t threadId;
        std::string username;
        std::string message;
        std::string timestamp;
    };

    std::filesystem::path dbPath;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db;
    mutable std::mutex dbMutex;

    void CreateDb();
    void InitDb();
    void CreateTable(const std::string& sql);
    std::string GetSystemMessageUnlocked(int64_t chatId, int32_t threadId);
    ClearHistoryInfo GetClearHistoryInfo(int64_t chatId, int32_t threadId);
};

} // namespace mbb
