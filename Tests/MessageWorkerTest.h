#ifndef MESSAGEWORKERTEST_H
#define MESSAGEWORKERTEST_H

#include "../telegram/MessageWorker.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace mbb::tests
{

TEST(MessageWorkerTest, FinalizeFallsBackWhenHtmlTooLong)
{
    const size_t maxLen = MessageWorker::GetMaxMessageLengthForTest();

    std::string input;
    const std::string chunk = "**a**";
    while (input.size() < maxLen - 64)
    {
        input += chunk;
    }

    auto blocks = MessageWorker::FormatForTest(input);
    ASSERT_FALSE(blocks.empty());

    bool sawRawFallback = false;
    for (const auto& block : blocks)
    {
        EXPECT_LE(block.size(), maxLen);
        if (block.find("**a**") != std::string::npos)
        {
            sawRawFallback = true;
        }
    }

    EXPECT_TRUE(sawRawFallback);
}

} // namespace mbb::tests

#endif // MESSAGEWORKERTEST_H
