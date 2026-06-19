#pragma once

#include "modules/framework/IModule.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mbb
{

class UserState;

// =============================================================================
// ModuleHost — registry and router for feature modules
// =============================================================================

class ModuleHost
{
  public:
    struct TriggerMatch
    {
        std::string callbackPrefix;
        std::string displayName;
        std::string matchedKeyword;
    };

    explicit ModuleHost(UserState& userState);

    // Non-copyable
    ModuleHost(const ModuleHost&) = delete;
    ModuleHost& operator=(const ModuleHost&) = delete;

    // Register a module
    void RegisterModule(std::unique_ptr<IModule> module);

    // Find a module whose trigger keywords match the given text.
    [[nodiscard]] std::optional<TriggerMatch> MatchTrigger(const std::string& text) const;

    // Activate a module after an explicit confirmation step.
    [[nodiscard]] bool ActivateModule(const std::string& callbackPrefix,
                                      int64_t chatId,
                                      int32_t threadId,
                                      int64_t userId,
                                      const std::string& username,
                                      int32_t messageId);

    // Deactivate the active module for a particular user session.
    void DeactivateModule(int64_t chatId, int32_t threadId, int64_t userId);

    // Route a callback query to the appropriate module by prefix.
    // Returns true if a module handled it.
    [[nodiscard]] bool RouteCallback(int64_t chatId,
                                     int32_t threadId,
                                     int32_t messageId,
                                     int64_t userId,
                                     const std::string& callbackData,
                                     const std::string& callbackQueryId);

    // Route a text message to the active module.
    // Returns true if the module consumed it.
    [[nodiscard]] bool RouteTextInput(
        int64_t chatId, int32_t threadId, int64_t userId, const std::string& text, int32_t messageId);

  private:
    [[nodiscard]] IModule* FindModuleByPrefix(const std::string& prefix) const;
    [[nodiscard]] IModule* FindModuleByCallbackData(const std::string& callbackData) const;

    UserState& userState_;
    std::vector<std::unique_ptr<IModule>> modules_;
};

} // namespace mbb
