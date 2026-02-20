#include "telegram/MessageWorker.h"

#include "core/Logger.h"
#include "ext/md4c/md4c-html.h"
#include "telegram/TelegramApi.h"

#include <fmt/core.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ranges>
#include <regex>

namespace mbb
{

constexpr int kWorkerLoopIntervalMs = 3000;      // Worker processes blocks every 3 seconds
constexpr int kSendFailureBackoffMultiplier = 2; // Timeout multiplier on failed message send
constexpr int kEditFailureBackoffMultiplier = 3; // Timeout multiplier on failed message edit

const std::chrono::seconds MessageWorker::kUpdateTimeout = std::chrono::seconds(1);
const std::string MessageWorker::kPartMessagePrefix = "\n<b><i>Answer [%num%/%tot%]</i></b>\n\n";

MessageWorker::~MessageWorker()
{
    Stop();
}

void MessageWorker::Start(std::shared_ptr<TelegramApi> api)
{
    stopping = false;
    api_ = api;
    workerThread_ = std::thread(&MessageWorker::WorkerLoop, this);
}

void MessageWorker::Stop()
{
    stopping = true;
    cv_.notify_all();
    if (workerThread_.joinable())
    {
        workerThread_.join();
        Logger::Debug("MessageWorker stopped");
    }
}

void MessageWorker::WorkerLoop()
{
    while (!stopping)
    {
        std::unique_lock lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(kWorkerLoopIntervalMs),
                     [this] { return (!messageBlocks_.empty() && newPortion_) || stopping; });

        for (auto& block : messageBlocks_)
        {
            ProcessMessageBlock(block);
        }

        // Remove completed blocks
        auto it = std::ranges::remove_if(messageBlocks_, [](const MessageBlock& block) {
                      return std::ranges::all_of(block.subBlocks,
                                                 [](const TgMessageSubBlock& sub) { return sub.isReadyToDelete; });
                  }).begin();
        messageBlocks_.erase(it, messageBlocks_.end());
        newPortion_ = false;
    }
}

void MessageWorker::ProcessMessageBlock(MessageBlock& block)
{
    auto api = api_.lock();
    if (!api)
    {
        return;
    }

    BalanceSubBlocks(block);
    if (block.isReadyToFinalize)
    {
        FinalizeSubBlocks(block);
    }

    for (auto& subBlock : block.subBlocks)
    {
        if (subBlock.isReadyToDelete)
        {
            continue;
        }

        auto now = std::chrono::system_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - subBlock.lastSent) <
            kUpdateTimeout * subBlock.factorTimeout)
        {
            continue;
        }

        if (!subBlock.isFilled && Trim(subBlock.tgOutcomingText) == Trim(subBlock.tgLastSentText))
        {
            continue;
        }

        TgBot::Message::Ptr result;
        if (subBlock.message == nullptr)
        {
            // Send new message
            result = api->SendMessage(block.chatId, block.threadId, subBlock.tgOutcomingText, subBlock.parseMode);
            subBlock.message = result;

            if (!result)
            {
                subBlock.factorTimeout *= kSendFailureBackoffMultiplier;
                Logger::Debug(fmt::format("[MODE:'{}';BLOCK:{}]Can not send:\n[BEGIN]{}[END]\n\n", subBlock.parseMode,
                                          subBlock.number, subBlock.tgOutcomingText));
            }
        }
        else
        {
            // Edit existing message
            result = api->EditMessage(block.chatId, subBlock.message->messageId, subBlock.tgOutcomingText,
                                      subBlock.parseMode);

            if (!result)
            {
                subBlock.factorTimeout *= kEditFailureBackoffMultiplier;
                Logger::Debug(fmt::format("[MODE:'{}';BLOCK:{}]Can not edit:\n", subBlock.parseMode, subBlock.number));
                Logger::Debug(fmt::format("Original [BEGIN]\n{}\n[END]\n\n", subBlock.unformattedText));
                Logger::Debug(fmt::format("Edited [BEGIN]\n{}\n[END]\n\n", subBlock.tgOutcomingText));
            }
        }

        if (result)
        {
            subBlock.lastSent = now;
            subBlock.tgLastSentText = result->text;
        }

        if (subBlock.isFilled && (subBlock.finalizedAttempts-- <= 0 || result))
        {
            subBlock.isReadyToDelete = true;
        }
    }
}

std::optional<uint32_t> MessageWorker::AddMessagePortion(const std::optional<uint32_t> id,
                                                         const int64_t chatId,
                                                         const int32_t threadId,
                                                         const std::string& responseText)
{
    std::optional<uint32_t> workerId = std::nullopt;

    if (id.has_value())
    {
        std::lock_guard lock(mutex_);
        for (auto& block : messageBlocks_)
        {
            if (block.id == id.value())
            {
                block.incomingText = responseText;
                workerId = block.id;
            }
        }
    }
    else
    {
        MessageBlock block;
        block.id = idSequence_++;
        block.chatId = chatId;
        block.threadId = threadId;
        block.isReadyToFinalize = false;
        block.incomingText = responseText;
        block.subBlocks = {};

        TgMessageSubBlock subBlock;
        block.subBlocks.push_back(subBlock);

        workerId = block.id;
        std::lock_guard lock(mutex_);
        messageBlocks_.push_back(std::move(block));
    }

    newPortion_ = true;
    cv_.notify_one();
    return workerId;
}

void MessageWorker::FinalizeMessage(std::optional<uint32_t> id)
{
    if (id.has_value())
    {
        std::lock_guard lock(mutex_);
        for (auto& block : messageBlocks_)
        {
            if (block.id == id.value())
            {
                block.isReadyToFinalize = true;
                newPortion_ = true;
                cv_.notify_one();
            }
        }
    }
}

void MessageWorker::BalanceSubBlocks(MessageBlock& block)
{
    TgMessageSubBlock* lastSubBlock = &block.subBlocks.back();

    bool inCodeBlock = false;
    size_t lastNonCodeStarting = 0;
    size_t lastDoubleNewLine = 0;
    size_t lastNewLine = 0;

    for (size_t i = lastSubBlock->range.first; i < block.incomingText.size(); i++)
    {
        const size_t subBlockNewEnd = lastNonCodeStarting != 0 ? lastNonCodeStarting
                                      : lastDoubleNewLine != 0 ? lastDoubleNewLine
                                      : lastNewLine            ? lastNewLine
                                                               : i;

        if (i - lastSubBlock->range.first > kSplitThreshold - kPartMessagePrefix.size())
        {
            lastSubBlock->tgOutcomingText =
                block.incomingText.substr(lastSubBlock->range.first, subBlockNewEnd - lastSubBlock->range.first);
            lastSubBlock->range.second = subBlockNewEnd;

            TgMessageSubBlock newSubBlock;
            newSubBlock.number = lastSubBlock->number + 1;
            newSubBlock.range.first = lastSubBlock->range.second;
            newSubBlock.range.second = block.incomingText.size();
            newSubBlock.tgOutcomingText = block.incomingText.substr(newSubBlock.range.first);
            block.subBlocks.push_back(std::move(newSubBlock));

            lastSubBlock = &block.subBlocks.back();
            lastNonCodeStarting = 0;
            lastDoubleNewLine = 0;
            lastNewLine = 0;
        }

        if (block.incomingText.substr(i, 3) == "```")
        {
            inCodeBlock = !inCodeBlock;
            if (!inCodeBlock)
            {
                size_t j = i;
                while (j < block.incomingText.size() && block.incomingText[j] != '\n')
                {
                    j++;
                }
                lastNonCodeStarting = j + 1;
            }
        }
        if (!inCodeBlock && block.incomingText.substr(i, 2) == "\n\n")
        {
            lastDoubleNewLine = i + 2;
        }
        if (!inCodeBlock && block.incomingText.substr(i, 1) == "\n")
        {
            lastNewLine = i + 1;
        }
    }

    if (lastSubBlock->isFilled)
    {
        return;
    }

    lastSubBlock->range.second = block.incomingText.size();
    lastSubBlock->tgOutcomingText = block.incomingText.substr(lastSubBlock->range.first);
}

void MessageWorker::FinalizeSubBlocks(MessageBlock& block)
{
    for (auto& subBlock : block.subBlocks)
    {
        if (subBlock.isFilled)
        {
            continue;
        }

        subBlock.isFilled = true;
        subBlock.unformattedText = subBlock.tgOutcomingText;

        std::string prefix = kPartMessagePrefix;
        prefix.replace(prefix.find("%num%"), strlen("%num%"), std::to_string(subBlock.number));
        prefix.replace(prefix.find("%tot%"), strlen("%tot%"), std::to_string(block.subBlocks.size()));

        auto formattedText = ConvertMarkdownToTelegramHtml(subBlock.tgOutcomingText);
        auto htmlCandidate = prefix + formattedText;

        if (htmlCandidate.size() <= kMaxMessageLength)
        {
            subBlock.parseMode = "HTML";
            subBlock.tgOutcomingText = std::move(htmlCandidate);
        }
        else
        {
            // Fallback to raw text (no HTML) if formatting exceeds Telegram limits.
            std::string plainPrefix =
                "\nAnswer [" + std::to_string(subBlock.number) + "/" + std::to_string(block.subBlocks.size()) + "]\n\n";
            subBlock.parseMode.clear();
            subBlock.tgOutcomingText = plainPrefix + subBlock.unformattedText;
            if (subBlock.tgOutcomingText.size() > kMaxMessageLength)
            {
                subBlock.tgOutcomingText.resize(kMaxMessageLength);
            }
        }
    }
}

// =============================================================================
// ConvertMarkdownToTelegramHtml
// =============================================================================
// Converts Markdown to Telegram-compatible HTML using md4c library.
//
// Telegram supports only a limited set of HTML tags, so we post-process
// the standard HTML output from md4c:
//
// | Element            | md4c output        | Telegram support | We convert to     |
// |--------------------|--------------------|------------------|-------------------|
// | Headings           | <h1>...<h6>        | No               | <b> + \n          |
// | Paragraphs         | <p>                | No               | remove, add \n\n  |
// | Ordered lists      | <ol><li>           | No               | 1. , 2. , ...     |
// | Unordered lists    | <ul><li>           | No               | bullet points     |
// | Horizontal rule    | <hr>               | No               | ———\n             |
// | Line break         | <br>               | No               | \n                |
// | Images             | <img>              | No               | remove            |
// | Tables             | <table><tr><td>    | No               | text with |       |
// | Code with class    | <code class="..."> | No (attributes)  | <code>            |
// | Italic             | <em>               | Yes, but <i>     | <i>               |
// | Bold               | <strong>           | Yes, but <b>     | <b>               |
//
// Tags that remain unchanged (Telegram supports them):
// <b>, <i>, <code>, <pre>, <s>, <a href="...">
//
// Fallback: if md4c fails to parse, we escape &, <, > and return as-is.
// =============================================================================
std::string MessageWorker::ConvertMarkdownToTelegramHtml(const std::string& input)
{
    std::string html;

    auto outputCallback = [](const MD_CHAR* text, MD_SIZE size, void* userdata) {
        auto* output = static_cast<std::string*>(userdata);
        output->append(text, size);
    };

    unsigned parserFlags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH;
    unsigned rendererFlags = 0;

    int mdResult =
        md_html(input.c_str(), static_cast<MD_SIZE>(input.size()), outputCallback, &html, parserFlags, rendererFlags);

    if (mdResult != 0)
    {
        // If parsing fails, return input with basic HTML escaping
        std::string escaped = input;
        size_t pos = 0;
        while ((pos = escaped.find('&', pos)) != std::string::npos)
        {
            escaped.replace(pos, 1, "&amp;");
            pos += 5;
        }
        pos = 0;
        while ((pos = escaped.find('<', pos)) != std::string::npos)
        {
            escaped.replace(pos, 1, "&lt;");
            pos += 4;
        }
        pos = 0;
        while ((pos = escaped.find('>', pos)) != std::string::npos)
        {
            escaped.replace(pos, 1, "&gt;");
            pos += 4;
        }
        return escaped;
    }

#ifdef _WIN32
    constexpr auto REGEX_FLAGS = std::regex_constants::ECMAScript;
#else
    constexpr auto REGEX_FLAGS = std::regex_constants::ECMAScript | std::regex_constants::multiline;
#endif

    // Headings -> bold
    html = std::regex_replace(html, std::regex(R"(<h[1-6]>)", REGEX_FLAGS), "<b>");
    html = std::regex_replace(html, std::regex(R"(</h[1-6]>)", REGEX_FLAGS), "</b>\n");

    // Paragraphs
    html = std::regex_replace(html, std::regex(R"(<p>)", REGEX_FLAGS), "");
    html = std::regex_replace(html, std::regex(R"(</p>)", REGEX_FLAGS), "\n\n");

    // Ordered lists - replace <li> with numbered items
    {
        std::string result;
        std::regex olRegex(R"re(<ol\b[^>]*>)re", REGEX_FLAGS);
        std::regex olEndRegex(R"(</ol>)", REGEX_FLAGS);
        std::regex liRegex(R"(<li[^>]*>)", REGEX_FLAGS);

        std::smatch match;
        std::string remaining = html;

        while (std::regex_search(remaining, match, olRegex))
        {
            result += match.prefix();

            std::string afterOl = match.suffix();
            std::smatch endMatch;

            if (std::regex_search(afterOl, endMatch, olEndRegex))
            {
                std::string olContent = endMatch.prefix();

                // Extract start value from attribute, default to 1
                int counter = 1;
                std::smatch startMatch;
                std::string olTag = match.str();
                if (std::regex_search(olTag, startMatch, std::regex(R"(start=(?:"|')(\d+)(?:"|'))", REGEX_FLAGS)))
                {
                    counter = std::stoi(startMatch[1].str());
                }

                // Replace <li> with numbered items
                std::string numberedContent;
                std::smatch liMatch;
                while (std::regex_search(olContent, liMatch, liRegex))
                {
                    numberedContent += liMatch.prefix();
                    numberedContent += std::to_string(counter++) + ". ";
                    olContent = liMatch.suffix();
                }
                numberedContent += olContent;

                result += numberedContent;
                remaining = endMatch.suffix();
            }
            else
            {
                result += match.str();
                remaining = match.suffix();
            }
        }
        result += remaining;
        html = result;
    }

    // Unordered lists - replace <li> with bullets
    html = std::regex_replace(html, std::regex(R"(<li[^>]*>)", REGEX_FLAGS), "• ");
    html = std::regex_replace(html, std::regex(R"(</li>)", REGEX_FLAGS), "\n");
    html = std::regex_replace(html, std::regex(R"(</?ul[^>]*>\n?)", REGEX_FLAGS), "");
    html = std::regex_replace(html, std::regex(R"(</?ol[^>]*>\n?)", REGEX_FLAGS), "");

    // Horizontal rules and breaks
    html = std::regex_replace(html, std::regex(R"(<hr\s*/?>)", REGEX_FLAGS), "———\n");
    html = std::regex_replace(html, std::regex(R"(<br\s*/?>)", REGEX_FLAGS), "\n");

    // Remove images
    html = std::regex_replace(html, std::regex(R"(<img[^>]*>)", REGEX_FLAGS), "");

    // Tables
    html = std::regex_replace(
        html, std::regex(R"(</?table[^>]*>|</?thead[^>]*>|</?tbody[^>]*>|</?tfoot[^>]*>)", REGEX_FLAGS), "");
    html = std::regex_replace(html, std::regex(R"(<tr[^>]*>)", REGEX_FLAGS), "");
    html = std::regex_replace(html, std::regex(R"(</tr>)", REGEX_FLAGS), "\n");
    // Handle td/th with optional attributes.
    html = std::regex_replace(html, std::regex(R"(<t[dh][^>]*>)", REGEX_FLAGS), "");
    html = std::regex_replace(html, std::regex(R"(</t[dh]>)", REGEX_FLAGS), " | ");

    // Code attributes
    html = std::regex_replace(html, std::regex(R"(<code\b[^>]*>)", REGEX_FLAGS), "<code>");

    // Emphasis normalization
    html = std::regex_replace(html, std::regex(R"(<em>)", REGEX_FLAGS), "<i>");
    html = std::regex_replace(html, std::regex(R"(</em>)", REGEX_FLAGS), "</i>");
    html = std::regex_replace(html, std::regex(R"(<strong>)", REGEX_FLAGS), "<b>");
    html = std::regex_replace(html, std::regex(R"(</strong>)", REGEX_FLAGS), "</b>");

    // Clean up multiple newlines
    html = std::regex_replace(html, std::regex(R"(\n{3,})", REGEX_FLAGS), "\n\n");

    return Trim(html);
}

std::string MessageWorker::Trim(const std::string& input)
{
    const auto start = std::ranges::find_if_not(input, [](int ch) { return std::isspace(ch); });
    const auto end =
        std::ranges::find_if_not(input.rbegin(), input.rend(), [](int ch) { return std::isspace(ch); }).base();
    return (start < end) ? std::string(start, end) : std::string();
}

#ifdef BUILD_TESTS
std::vector<std::string> MessageWorker::FormatForTest(const std::string& input)
{
    MessageBlock block;
    block.id = 0;
    block.chatId = 0;
    block.threadId = 0;
    block.isReadyToFinalize = true;
    block.incomingText = input;
    block.subBlocks.push_back(TgMessageSubBlock{});

    BalanceSubBlocks(block);
    FinalizeSubBlocks(block);

    std::vector<std::string> result;
    result.reserve(block.subBlocks.size());
    for (const auto& sub : block.subBlocks)
    {
        result.push_back(sub.tgOutcomingText);
    }
    return result;
}

size_t MessageWorker::GetMaxMessageLengthForTest()
{
    return kMaxMessageLength;
}
#endif

} // namespace mbb
