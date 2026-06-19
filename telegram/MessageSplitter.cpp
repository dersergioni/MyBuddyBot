#include "telegram/MessageSplitter.h"

#include "infra/TelegramHtmlFormatter.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace mbb
{

namespace
{

constexpr size_t kHeuristicThreshold = MessageSplitter::kMaxMessageLength * 3 / 4;

struct CodeFenceMatch
{
    bool matched = false;
    std::string_view language;
};

struct CodeBlockContext
{
    bool inside = false;
    std::string language;
};

struct SplitAdvance
{
    bool hasNextSubBlock = false;
    size_t nextScanPos = 0;
    bool nextInCodeBlock = false;
};

struct SplitResult
{
    std::string htmlMessage;
    size_t splitPoint = 0;
    CodeBlockContext codeBlockContext;
};

struct FormattedMessage
{
    std::string text;
    size_t sizeForLimit = 0;
};

constexpr size_t kReservedHeaderNumber = 999;

std::string BuildCodeFencePrefix(const std::string& language);
std::string BuildCodeFenceSuffix();
std::string BuildAnswerHeader(size_t number, size_t totalMessages, bool showTotalMessages);

bool IsValidCodeFenceLanguage(std::string_view language)
{
    return std::all_of(language.begin(), language.end(), [](unsigned char ch) {
        return std::isalnum(ch) || ch == '_' || ch == '-' || ch == '+' || ch == '.' || ch == '#';
    });
}

CodeFenceMatch MatchCodeFence(const std::string& text, size_t pos, bool insideCodeBlock);
size_t SkipNewlines(const std::string& text, size_t pos);

SplitAdvance ApplySplit(MessageBlock& block, std::vector<size_t>& likelySplitPoints);
SplitResult FindBestSplit(const std::string& text,
                          const TgMessageSubBlock& subBlock,
                          size_t totalMessages,
                          bool showTotalMessages,
                          const std::vector<size_t>& likelySplitPoints);
FormattedMessage BuildFormattedMessage(const std::string& text,
                                       const TgMessageSubBlock& subBlock,
                                       size_t end,
                                       size_t totalMessages,
                                       bool showTotalMessages);
CodeBlockContext FindCodeBlockContext(const std::string& text, size_t pos);

} // namespace

void MessageSplitter::SplitForStreaming(MessageBlock& block)
{
    std::vector<size_t> likelySplitPoints{};
    auto* lastSubBlock = &block.subBlocks.back();
    bool inCodeBlock = !lastSubBlock->markdownPrefix.empty();
    for (size_t i = lastSubBlock->range.first; i < block.rawFullText.size(); ++i)
    {
        if (i > lastSubBlock->range.first && i - lastSubBlock->range.first > kHeuristicThreshold)
        {
            auto splitAdvance = ApplySplit(block, likelySplitPoints);
            if (!splitAdvance.hasNextSubBlock)
            {
                break;
            }
            lastSubBlock = &block.subBlocks.back();
            i = splitAdvance.nextScanPos - 1;
            inCodeBlock = splitAdvance.nextInCodeBlock;
            continue;
        }

        if (const auto fenceMatch = MatchCodeFence(block.rawFullText, i, inCodeBlock); fenceMatch.matched)
        {
            inCodeBlock = !inCodeBlock;
            if (!inCodeBlock)
            {
                const size_t splitPoint = SkipNewlines(block.rawFullText, i + 3);
                likelySplitPoints.push_back(splitPoint);
                i = splitPoint - 1;
                continue;
            }
        }
        if (!inCodeBlock && block.rawFullText[i] == '\n')
        {
            const size_t splitPoint = SkipNewlines(block.rawFullText, i + 1);
            likelySplitPoints.push_back(splitPoint);
            i = splitPoint - 1;
        }
    }

    while (true)
    {
        lastSubBlock = &block.subBlocks.back();
        lastSubBlock->range.second = block.rawFullText.size();
        auto formattedMessage = BuildFormattedMessage(block.rawFullText, *lastSubBlock, lastSubBlock->range.second,
                                                      block.subBlocks.size(), block.isReadyToFinalize);

        if (formattedMessage.sizeForLimit <= kMaxMessageLength)
        {
            lastSubBlock->tgText = std::move(formattedMessage.text);
            break;
        }

        if (!ApplySplit(block, likelySplitPoints).hasNextSubBlock)
        {
            break;
        }
    }

    if (block.isReadyToFinalize)
    {
        const size_t totalMessages = block.subBlocks.size();
        for (auto& subBlock : block.subBlocks)
        {
            subBlock.tgText =
                BuildFormattedMessage(block.rawFullText, subBlock, subBlock.range.second, totalMessages, true).text;
        }
    }
}

void MessageSplitter::FinalizeSubBlocks(MessageBlock& block)
{
    for (auto& subBlock : block.subBlocks)
    {
        if (!subBlock.isFinalized)
        {
            subBlock.isFinalized = true;
        }
    }
}

#ifdef BUILD_TESTS
std::vector<std::string> MessageSplitter::FormatForTest(const std::string& input)
{
    MessageBlock block;
    block.isReadyToFinalize = true;
    block.rawFullText = input;
    block.subBlocks.emplace_back();

    SplitForStreaming(block);
    FinalizeSubBlocks(block);

    std::vector<std::string> result;
    result.reserve(block.subBlocks.size());
    for (const auto& sub : block.subBlocks)
    {
        result.push_back(sub.tgText);
    }
    return result;
}

size_t MessageSplitter::GetMaxMessageLengthForTest()
{
    return kMaxMessageLength;
}
#endif

namespace
{

SplitAdvance ApplySplit(MessageBlock& block, std::vector<size_t>& likelySplitPoints)
{
    auto& lastSubBlock = block.subBlocks.back();
    auto splitResult = FindBestSplit(block.rawFullText, lastSubBlock, block.subBlocks.size() + 1,
                                     block.isReadyToFinalize, likelySplitPoints);

    lastSubBlock.range.second = splitResult.splitPoint;
    lastSubBlock.markdownSuffix = splitResult.codeBlockContext.inside ? BuildCodeFenceSuffix() : std::string{};
    lastSubBlock.tgText = std::move(splitResult.htmlMessage);

    likelySplitPoints.erase(
        std::remove_if(likelySplitPoints.begin(), likelySplitPoints.end(),
                       [splitPoint = splitResult.splitPoint](size_t point) { return point <= splitPoint; }),
        likelySplitPoints.end());

    if (splitResult.splitPoint == block.rawFullText.size())
    {
        return {};
    }

    TgMessageSubBlock newSubBlock;
    newSubBlock.number = lastSubBlock.number + 1;
    newSubBlock.range.first = splitResult.splitPoint;
    newSubBlock.range.second = block.rawFullText.size();
    if (splitResult.codeBlockContext.inside)
    {
        newSubBlock.markdownPrefix = BuildCodeFencePrefix(splitResult.codeBlockContext.language);
    }
    block.subBlocks.push_back(std::move(newSubBlock));
    return {.hasNextSubBlock = true,
            .nextScanPos = splitResult.splitPoint,
            .nextInCodeBlock = splitResult.codeBlockContext.inside};
}

SplitResult FindBestSplit(const std::string& text,
                          const TgMessageSubBlock& subBlock,
                          size_t totalMessages,
                          bool showTotalMessages,
                          const std::vector<size_t>& likelySplitPoints)
{
    auto candidatePoint = likelySplitPoints.rbegin();

    while (candidatePoint != likelySplitPoints.rend() && *candidatePoint > subBlock.range.first)
    {
        auto formattedMessage =
            BuildFormattedMessage(text, subBlock, *candidatePoint, totalMessages, showTotalMessages);

        if (formattedMessage.sizeForLimit <= MessageSplitter::kMaxMessageLength)
        {
            return {std::move(formattedMessage.text), *candidatePoint, FindCodeBlockContext(text, *candidatePoint)};
        }
        ++candidatePoint;
    }

    const size_t remainingLength = text.size() - subBlock.range.first;
    const size_t maxCandidateLength = std::min(remainingLength, MessageSplitter::kMaxMessageLength);

    size_t bestPoint = subBlock.range.first + 1;
    std::string bestMessage;
    CodeBlockContext bestContext = FindCodeBlockContext(text, bestPoint);

    size_t low = 1;
    size_t high = maxCandidateLength;
    while (low <= high)
    {
        const size_t candidateLength = low + (high - low) / 2;
        const size_t candidatePointValue = subBlock.range.first + candidateLength;
        const auto candidateContext = FindCodeBlockContext(text, candidatePointValue);
        auto candidateSubBlock = subBlock;
        candidateSubBlock.markdownSuffix = candidateContext.inside ? BuildCodeFenceSuffix() : std::string{};

        auto formattedMessage =
            BuildFormattedMessage(text, candidateSubBlock, candidatePointValue, totalMessages, showTotalMessages);

        if (formattedMessage.sizeForLimit <= MessageSplitter::kMaxMessageLength)
        {
            bestPoint = candidatePointValue;
            bestMessage = std::move(formattedMessage.text);
            bestContext = candidateContext;
            low = candidateLength + 1;
        }
        else
        {
            high = candidateLength - 1;
        }
    }

    return {std::move(bestMessage), bestPoint, std::move(bestContext)};
}

FormattedMessage BuildFormattedMessage(const std::string& text,
                                       const TgMessageSubBlock& subBlock,
                                       size_t end,
                                       size_t totalMessages,
                                       bool showTotalMessages)
{
    std::string markdown;
    markdown.reserve((subBlock.markdownPrefix.size()) + (end - subBlock.range.first) +
                     (subBlock.markdownSuffix.size()));
    markdown += subBlock.markdownPrefix;
    markdown.append(text, subBlock.range.first, end - subBlock.range.first);
    markdown += subBlock.markdownSuffix;

    auto header = BuildAnswerHeader(subBlock.number, totalMessages, showTotalMessages);
    auto body = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml(markdown);
    auto message = header + body;

    size_t sizeForLimit = message.size();
    if (!showTotalMessages)
    {
        static const size_t kReservedHeaderLength =
            BuildAnswerHeader(kReservedHeaderNumber, kReservedHeaderNumber, true).size();
        const size_t reservedHeaderGrowth =
            header.size() >= kReservedHeaderLength ? 0 : kReservedHeaderLength - header.size();
        sizeForLimit += reservedHeaderGrowth;
    }

    return {std::move(message), sizeForLimit};
}

std::string BuildAnswerHeader(size_t number, size_t totalMessages, bool showTotalMessages)
{
    if (showTotalMessages)
    {
        return fmt::format("\n<b><i>Answer [{}/{}]</i></b>\n\n", number, totalMessages);
    }
    else
    {
        return fmt::format("\n<b><i>Answer [{}]</i></b>\n\n", number);
    }
}

CodeBlockContext FindCodeBlockContext(const std::string& text, size_t pos)
{
    CodeBlockContext ctx;

    for (size_t i = 0; i < text.size() && i < pos; ++i)
    {
        const auto fenceMatch = MatchCodeFence(text, i, ctx.inside);
        if (fenceMatch.matched)
        {
            if (!ctx.inside)
            {
                ctx.language = std::string(fenceMatch.language);
            }
            else
            {
                ctx.language.clear();
            }
            ctx.inside = !ctx.inside;
            i += 2;
        }
    }

    return ctx;
}

std::string BuildCodeFencePrefix(const std::string& language)
{
    if (language.empty())
    {
        return "```\n";
    }
    return fmt::format("```{}\n", language);
}

std::string BuildCodeFenceSuffix()
{
    return "\n```";
}

CodeFenceMatch MatchCodeFence(const std::string& text, size_t pos, bool insideCodeBlock)
{
    if (text.compare(pos, 3, "```") != 0)
    {
        return {};
    }

    const size_t suffixStart = pos + 3;
    size_t lineEnd = text.find('\n', suffixStart);
    if (lineEnd == std::string::npos)
    {
        lineEnd = text.size();
    }

    const std::string_view suffix{text.data() + suffixStart, lineEnd - suffixStart};
    if (insideCodeBlock)
    {
        const bool closesFence = suffix.empty() || (suffix.front() != '"' && suffix.front() != '\'');
        return {.matched = closesFence, .language = {}};
    }
    else
    {
        if (pos > 0 && text[pos - 1] != '\n')
        {
            return {};
        }
        if (!IsValidCodeFenceLanguage(suffix))
        {
            return {};
        }
        return {.matched = true, .language = suffix};
    }
}

size_t SkipNewlines(const std::string& text, size_t pos)
{
    while (pos < text.size() && text[pos] == '\n')
    {
        pos++;
    }
    return pos;
}

} // namespace
} // namespace mbb
