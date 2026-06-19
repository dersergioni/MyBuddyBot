#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mbb
{

// =============================================================================
// IModule — interface that each Rust (or C++) module implements
// =============================================================================

class IModule
{
  public:
    virtual ~IModule() = default;

    // Human-readable module name for prompts/UI (e.g., "Wishlist")
    [[nodiscard]] virtual std::string GetDisplayName() const = 0;

    // Keywords that activate this module (e.g., "wishlist", "wish list")
    [[nodiscard]] virtual std::vector<std::string> GetTriggerKeywords() const = 0;

    // Prefix for callback data routing (e.g., "wl:")
    [[nodiscard]] virtual std::string GetCallbackPrefix() const = 0;

    // Called when a trigger keyword is detected in a text message
    virtual void HandleTrigger(
        int64_t chatId, int32_t threadId, int64_t userId, const std::string& username, int32_t messageId) = 0;

    // Called when an inline button with this module's prefix is pressed
    virtual void HandleCallback(int64_t chatId,
                                int32_t threadId,
                                int32_t messageId,
                                int64_t userId,
                                const std::string& callbackData,
                                const std::string& callbackQueryId) = 0;

    // Called when a text message arrives. Returns true if the module consumed it (e.g., awaiting input).
    [[nodiscard]] virtual bool HandleTextInput(
        int64_t chatId, int32_t threadId, int64_t userId, const std::string& text, int32_t messageId) = 0;

    // Called when the host explicitly deactivates the module for a user session.
    virtual void DeactivateSession(int64_t chatId, int32_t threadId, int64_t userId) = 0;

    // Whether the given chat/thread session is currently active in this module for the user.
    [[nodiscard]] virtual bool IsActive(int64_t chatId, int32_t threadId, int64_t userId) const = 0;
};

} // namespace mbb
