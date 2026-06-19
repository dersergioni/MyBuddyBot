#include "modules/wishlist/bridge/WishlistBridge.h"

#include "modules/framework/IModule.h"
#include "modules/services/ModuleOps.h"
#include "modules/services/TelegramOps.h"
#include "wishlist_cxx/lib.h"

#include "core/Logger.h"
#include "core/Storage.h"

#include <fmt/format.h>

#include <stdexcept>

namespace wishlist
{

// =============================================================================
// WishlistAdapter — IModule that delegates to Rust WishlistModule
// =============================================================================

namespace
{

class WishlistAdapter : public mbb::IModule
{
  public:
    WishlistAdapter(std::shared_ptr<mbb::ModuleOps> ops, rust::Box<WishlistModule> module)
        : ops_(std::move(ops)), module_(std::move(module))
    {
    }

    [[nodiscard]] std::string GetDisplayName() const override
    {
        return "Wishlist";
    }

    [[nodiscard]] std::vector<std::string> GetTriggerKeywords() const override
    {
        auto keywords = get_trigger_keywords(*module_);
        std::vector<std::string> result;
        result.reserve(keywords.size());
        for (const auto& kw : keywords)
        {
            result.emplace_back(std::string(kw));
        }
        return result;
    }

    [[nodiscard]] std::string GetCallbackPrefix() const override
    {
        return std::string(get_callback_prefix(*module_));
    }

    void HandleTrigger(
        int64_t chatId, int32_t threadId, int64_t userId, const std::string& username, int32_t messageId) override
    {
        handle_trigger(*module_, *ops_, chatId, threadId, userId, username, messageId);
    }

    void HandleCallback(int64_t chatId,
                        int32_t threadId,
                        int32_t messageId,
                        int64_t userId,
                        const std::string& callbackData,
                        const std::string& callbackQueryId) override
    {
        handle_callback(*module_, *ops_, chatId, threadId, messageId, userId, callbackData, callbackQueryId);
    }

    [[nodiscard]] bool HandleTextInput(
        int64_t chatId, int32_t threadId, int64_t userId, const std::string& text, int32_t messageId) override
    {
        return handle_text_input(*module_, *ops_, chatId, threadId, userId, text, messageId);
    }

    void DeactivateSession(int64_t chatId, int32_t threadId, int64_t userId) override
    {
        deactivate_session(*module_, chatId, threadId, userId);
    }

    [[nodiscard]] bool IsActive(int64_t chatId, int32_t threadId, int64_t userId) const override
    {
        return is_session_active(*module_, chatId, threadId, userId);
    }

  private:
    std::shared_ptr<mbb::ModuleOps> ops_;
    rust::Box<WishlistModule> module_;
};

} // anonymous namespace

// =============================================================================
// Factory
// =============================================================================

std::unique_ptr<mbb::IModule> CreateWishlistModule(std::shared_ptr<mbb::TelegramOps> ops,
                                                   std::shared_ptr<mbb::Storage> storage,
                                                   const std::string& dbPath)
{
    auto moduleOps = std::make_shared<mbb::ModuleOps>(std::move(ops), std::move(storage));
    try
    {
        auto module = create_wishlist_module(dbPath);
        return std::make_unique<WishlistAdapter>(std::move(moduleOps), std::move(module));
    }
    catch (const rust::Error& e)
    {
        throw std::runtime_error(fmt::format("Failed to initialize Wishlist module: {}", e.what()));
    }
}

} // namespace wishlist
