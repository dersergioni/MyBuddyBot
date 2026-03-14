#pragma once

#include "telegram/MessageTypes.h"

#include <string>
#include <vector>

namespace mbb
{

class MessageSplitter
{
  public:
    static void SplitForStreaming(MessageBlock& block);
    static void FinalizeSubBlocks(MessageBlock& block);

    static constexpr size_t kMaxMessageLength = 4096;

#ifdef BUILD_TESTS
    static std::vector<std::string> FormatForTest(const std::string& input);
    static size_t GetMaxMessageLengthForTest();
#endif
};

} // namespace mbb
