#include "modules/framework/ModuleHost.h"

#include "bot/UserState.h"
#include "core/Logger.h"
#include "infra/StringUtils.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace mbb
{

ModuleHost::ModuleHost(UserState& userState) : userState_(userState)
{
}

void ModuleHost::RegisterModule(std::unique_ptr<IModule> module)
{
    Logger::Info(fmt::format("Module registered: prefix='{}', keywords=[{}]", module->GetCallbackPrefix(),
                             fmt::join(module->GetTriggerKeywords(), ", ")));
    modules_.push_back(std::move(module));
}

std::optional<ModuleHost::TriggerMatch> ModuleHost::MatchTrigger(const std::string& text) const
{
    const std::string lower = StringUtils::ToLower(text);

    for (const auto& mod : modules_)
    {
        for (const auto& keyword : mod->GetTriggerKeywords())
        {
            if (lower.find(StringUtils::ToLower(keyword)) != std::string::npos)
            {
                return TriggerMatch{mod->GetCallbackPrefix(), mod->GetDisplayName(), keyword};
            }
        }
    }
    return std::nullopt;
}

IModule* ModuleHost::FindModuleByPrefix(const std::string& prefix) const
{
    for (const auto& mod : modules_)
    {
        if (mod->GetCallbackPrefix() == prefix)
        {
            return mod.get();
        }
    }
    return nullptr;
}

IModule* ModuleHost::FindModuleByCallbackData(const std::string& callbackData) const
{
    for (const auto& mod : modules_)
    {
        if (callbackData.starts_with(mod->GetCallbackPrefix()))
        {
            return mod.get();
        }
    }
    return nullptr;
}

bool ModuleHost::ActivateModule(const std::string& callbackPrefix,
                                int64_t chatId,
                                int32_t threadId,
                                int64_t userId,
                                const std::string& username,
                                int32_t messageId)
{
    IModule* mod = FindModuleByPrefix(callbackPrefix);
    if (!mod)
    {
        return false;
    }

    Logger::Debug(fmt::format("Module activated: prefix='{}' chat_id={} thread_id={} user_id={}", callbackPrefix,
                              chatId, threadId, userId));
    userState_.SetActiveModule({chatId, threadId, userId}, callbackPrefix);
    mod->HandleTrigger(chatId, threadId, userId, username, messageId);
    return true;
}

void ModuleHost::DeactivateModule(int64_t chatId, int32_t threadId, int64_t userId)
{
    Logger::Debug(fmt::format("Module deactivated: chat_id={} thread_id={} user_id={}", chatId, threadId, userId));
    const std::string activePrefix = userState_.GetActiveModule({chatId, threadId, userId});
    if (!activePrefix.empty())
    {
        if (IModule* mod = FindModuleByPrefix(activePrefix))
        {
            mod->DeactivateSession(chatId, threadId, userId);
        }
    }
    userState_.ClearActiveModule({chatId, threadId, userId});
}

bool ModuleHost::RouteCallback(int64_t chatId,
                               int32_t threadId,
                               int32_t messageId,
                               int64_t userId,
                               const std::string& callbackData,
                               const std::string& callbackQueryId)
{
    IModule* mod = FindModuleByCallbackData(callbackData);
    if (!mod)
    {
        return false;
    }

    mod->HandleCallback(chatId, threadId, messageId, userId, callbackData, callbackQueryId);

    if (!mod->IsActive(chatId, threadId, userId))
    {
        DeactivateModule(chatId, threadId, userId);
    }

    return true;
}

bool ModuleHost::RouteTextInput(
    int64_t chatId, int32_t threadId, int64_t userId, const std::string& text, int32_t messageId)
{
    const std::string activePrefix = userState_.GetActiveModule({chatId, threadId, userId});
    if (activePrefix.empty())
    {
        return false;
    }

    IModule* mod = FindModuleByPrefix(activePrefix);
    if (!mod)
    {
        DeactivateModule(chatId, threadId, userId);
        return false;
    }

    if (!mod->HandleTextInput(chatId, threadId, userId, text, messageId))
    {
        if (!mod->IsActive(chatId, threadId, userId))
        {
            DeactivateModule(chatId, threadId, userId);
        }
        return false;
    }

    if (!mod->IsActive(chatId, threadId, userId))
    {
        DeactivateModule(chatId, threadId, userId);
    }

    return true;
}

} // namespace mbb
