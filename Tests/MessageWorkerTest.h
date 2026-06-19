#ifndef MESSAGEWORKERTEST_H
#define MESSAGEWORKERTEST_H

#include "../telegram/MessageSplitter.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace mbb::tests
{

TEST(MessageWorkerTest, ReSplitsWhenHtmlExceedsLimit)
{
    const size_t maxLen = MessageSplitter::GetMaxMessageLengthForTest();

    // Build input of repeated **a** — heavy HTML expansion (~3.4x).
    // Each **a** = 5 chars raw → <strong>a</strong> = 17 chars HTML.
    std::string input;
    const std::string chunk = "**a** ";
    while (input.size() < maxLen - 64)
    {
        input += chunk;
    }

    auto blocks = MessageSplitter::FormatForTest(input);
    ASSERT_GE(blocks.size(), 2u);

    for (const auto& block : blocks)
    {
        EXPECT_LE(block.size(), maxLen);
        // All blocks should be HTML-formatted (no raw **a** fallback)
        EXPECT_EQ(block.find("**a**"), std::string::npos) << "Expected HTML formatting, got raw markdown";
        EXPECT_NE(block.find("<strong>a</strong>"), std::string::npos) << "Expected <strong> tags in HTML output";
    }
}

TEST(MessageWorkerTest, PreservesHtmlForCodeHeavyContent)
{
    // Simulate the real-world case: lots of inline code that expands due to entities.
    std::string input;
    for (int i = 0; i < 80; ++i)
    {
        input += "- `CoCreateInstance(CLSID_..., NULL, CLSCTX_ALL, IID_..., (void**)&p)`\n";
    }

    auto blocks = MessageSplitter::FormatForTest(input);
    ASSERT_FALSE(blocks.empty());

    for (const auto& block : blocks)
    {
        EXPECT_LE(block.size(), MessageSplitter::GetMaxMessageLengthForTest());
        // Should be HTML, not raw fallback
        EXPECT_NE(block.find("<code>"), std::string::npos) << "Expected <code> tags in HTML output";
    }
}

TEST(MessageWorkerTest, HardSplitLongCodeBlockKeepsHtmlAndFitsLimit)
{
    std::string input = "```cpp\n";
    for (int i = 0; i < 220; ++i)
    {
        input += std::string("std::cout << \"very long line with escaped chars <>& and index ") + std::to_string(i) +
                 "\" << std::endl;\n";
    }
    input += "```";

    auto blocks = MessageSplitter::FormatForTest(input);
    ASSERT_GE(blocks.size(), 2u);

    for (const auto& block : blocks)
    {
        EXPECT_LE(block.size(), MessageSplitter::GetMaxMessageLengthForTest());
        EXPECT_NE(block.find("<pre>"), std::string::npos) << "Expected fenced code block HTML";
    }
}

TEST(MessageWorkerTest, HardSplitDoesNotMutateRawText)
{
    std::string input = "```python\n";
    for (int i = 0; i < 260; ++i)
    {
        input += std::string("print('long generated code line ") + std::to_string(i) +
                 " with symbols <>& and markdown **markers**')\n";
    }
    input += "```";

    MessageBlock block;
    block.rawFullText = input;
    block.subBlocks.emplace_back();

    MessageSplitter::SplitForStreaming(block);

    EXPECT_EQ(block.rawFullText, input);
}

TEST(MessageWorkerTest, SplitsImmediatelyAfterClosedCodeBlockWhenTextContinues)
{
    const size_t threshold = MessageSplitter::GetMaxMessageLengthForTest() * 3 / 4;

    std::string input = "```txt\n";
    input += std::string(threshold - 600, 'a');
    input += "```";
    const size_t expectedSplitPoint = input.size();
    input += std::string(800, 'b');

    MessageBlock block;
    block.rawFullText = input;
    block.subBlocks.emplace_back();

    MessageSplitter::SplitForStreaming(block);

    ASSERT_GE(block.subBlocks.size(), 2u);
    EXPECT_EQ(block.subBlocks.front().range.second, expectedSplitPoint);
}

TEST(MessageWorkerTest, SplitsAfterAllTrailingNewlinesFollowingClosedCodeBlock)
{
    const size_t threshold = MessageSplitter::GetMaxMessageLengthForTest() * 3 / 4;

    std::string input = "```txt\n";
    input += std::string(threshold - 600, 'a');
    input += "```";
    input += "\n\n\n";
    const size_t expectedSplitPoint = input.size();
    input += std::string(800, 'b');

    MessageBlock block;
    block.rawFullText = input;
    block.subBlocks.emplace_back();

    MessageSplitter::SplitForStreaming(block);

    ASSERT_GE(block.subBlocks.size(), 2u);
    EXPECT_EQ(block.subBlocks.front().range.second, expectedSplitPoint);
}

TEST(MessageWorkerTest, SplitsAfterSingleNewlineOutsideCodeBlock)
{
    const size_t threshold = MessageSplitter::GetMaxMessageLengthForTest() * 3 / 4;

    std::string input(threshold - 400, 'a');
    input += '\n';
    const size_t expectedSplitPoint = input.size();
    input += std::string(800, 'b');

    MessageBlock block;
    block.rawFullText = input;
    block.subBlocks.emplace_back();

    MessageSplitter::SplitForStreaming(block);

    ASSERT_GE(block.subBlocks.size(), 2u);
    EXPECT_EQ(block.subBlocks.front().range.second, expectedSplitPoint);
}

TEST(MessageWorkerTest, SplitsAfterAllConsecutiveNewlinesOutsideCodeBlock)
{
    const size_t threshold = MessageSplitter::GetMaxMessageLengthForTest() * 3 / 4;

    std::string input(threshold - 400, 'a');
    input += "\n\n\n\n";
    const size_t expectedSplitPoint = input.size();
    input += std::string(800, 'b');

    MessageBlock block;
    block.rawFullText = input;
    block.subBlocks.emplace_back();

    MessageSplitter::SplitForStreaming(block);

    ASSERT_GE(block.subBlocks.size(), 2u);
    EXPECT_EQ(block.subBlocks.front().range.second, expectedSplitPoint);
}

TEST(MessageWorkerTest, ResumesInsideCodeBlockWhenSubBlockStartsWithMarkdownPrefix)
{
    const size_t threshold = MessageSplitter::GetMaxMessageLengthForTest() * 3 / 4;

    std::string carriedCodeBlockBody;
    while (carriedCodeBlockBody.size() < threshold - 700)
    {
        carriedCodeBlockBody += "print('carried code line')\n";
    }

    MessageBlock block;
    block.rawFullText = carriedCodeBlockBody + "```\n\n" + std::string(1000, 'b');
    block.subBlocks.emplace_back();
    block.subBlocks.back().markdownPrefix = "```python\n";

    const size_t expectedSplitPoint = carriedCodeBlockBody.size() + 5;

    MessageSplitter::SplitForStreaming(block);

    ASSERT_GE(block.subBlocks.size(), 2u);
    EXPECT_EQ(block.subBlocks.front().range.second, expectedSplitPoint);
}

TEST(MessageWorkerTest, RebuildsAnswerHeadersWithFinalSubBlockCount)
{
    const size_t maxLen = MessageSplitter::GetMaxMessageLengthForTest();

    std::string input;
    const std::string chunk = "**a** ";
    while (input.size() < maxLen * 2)
    {
        input += chunk;
    }

    auto blocks = MessageSplitter::FormatForTest(input);
    ASSERT_GE(blocks.size(), 3u);

    for (size_t i = 0; i < blocks.size(); ++i)
    {
        const std::string expectedHeader =
            "<b><i>Answer [" + std::to_string(i + 1) + "/" + std::to_string(blocks.size()) + "]</i></b>";
        EXPECT_NE(blocks[i].find(expectedHeader), std::string::npos);
    }
}

TEST(MessageWorkerTest, StreamingHeadersDoNotExposeTotalCount)
{
    const size_t maxLen = MessageSplitter::GetMaxMessageLengthForTest();

    std::string input;
    const std::string chunk = "**a** ";
    while (input.size() < maxLen * 2)
    {
        input += chunk;
    }

    MessageBlock block;
    block.rawFullText = input;
    block.subBlocks.emplace_back();

    MessageSplitter::SplitForStreaming(block);

    ASSERT_GE(block.subBlocks.size(), 3u);
    for (size_t i = 0; i < block.subBlocks.size(); ++i)
    {
        const std::string expectedHeader = "<b><i>Answer [" + std::to_string(i + 1) + "]</i></b>";
        const std::string unexpectedHeaderPrefix = "<b><i>Answer [" + std::to_string(i + 1) + "/";

        EXPECT_NE(block.subBlocks[i].tgText.find(expectedHeader), std::string::npos);
        EXPECT_EQ(block.subBlocks[i].tgText.find(unexpectedHeaderPrefix), std::string::npos);
    }
}

TEST(MessageWorkerTest, FinalizeRebuildsHeadersWithTotalCountForExistingBlocks)
{
    const size_t maxLen = MessageSplitter::GetMaxMessageLengthForTest();

    std::string input;
    const std::string chunk = "**a** ";
    while (input.size() < maxLen * 2)
    {
        input += chunk;
    }

    MessageBlock block;
    block.rawFullText = input;
    block.subBlocks.emplace_back();

    MessageSplitter::SplitForStreaming(block);
    block.isReadyToFinalize = true;
    MessageSplitter::SplitForStreaming(block);
    MessageSplitter::FinalizeSubBlocks(block);

    ASSERT_GE(block.subBlocks.size(), 3u);
    for (size_t i = 0; i < block.subBlocks.size(); ++i)
    {
        const std::string expectedHeader =
            "<b><i>Answer [" + std::to_string(i + 1) + "/" + std::to_string(block.subBlocks.size()) + "]</i></b>";
        EXPECT_NE(block.subBlocks[i].tgText.find(expectedHeader), std::string::npos);
    }
}

TEST(MessageWorkerTest, IgnoresLiteralBackticksInsideCodeLines)
{
    const size_t threshold = MessageSplitter::GetMaxMessageLengthForTest() * 3 / 4;

    std::string input = "```cpp\n";
    while (input.size() < threshold - 700)
    {
        input += "std::string fence = \"```\";\n";
    }
    input += "```\n\n";
    const size_t expectedSplitPoint = input.size();
    input += std::string(1000, 'b');

    MessageBlock block;
    block.rawFullText = input;
    block.subBlocks.emplace_back();

    MessageSplitter::SplitForStreaming(block);

    ASSERT_GE(block.subBlocks.size(), 2u);
    EXPECT_EQ(block.subBlocks.front().range.second, expectedSplitPoint);
}

} // namespace mbb::tests

#endif // MESSAGEWORKERTEST_H
