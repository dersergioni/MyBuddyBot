#ifndef BASE64TEST_H
#define BASE64TEST_H

#include "../infra/Base64.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

namespace mbb::tests
{

class Base64Test : public testing::Test
{
  protected:
    Base64Test() = default;
    ~Base64Test() override = default;
};

TEST_F(Base64Test, EncodeEmptyString)
{
    std::string input;
    std::string result = Base64::Encode(input);
    EXPECT_TRUE(result.empty());
}

TEST_F(Base64Test, EncodeEmptyVector)
{
    std::vector<char> input;
    std::string result = Base64::Encode(input);
    EXPECT_TRUE(result.empty());
}

TEST_F(Base64Test, DecodeEmptyString)
{
    std::string input;
    std::string result = Base64::Decode(input);
    EXPECT_TRUE(result.empty());
}

TEST_F(Base64Test, EncodeSimpleString)
{
    std::string input = "Hello, World!";
    std::string result = Base64::Encode(input);
    EXPECT_EQ("SGVsbG8sIFdvcmxkIQ==", result);
}

TEST_F(Base64Test, DecodeSimpleString)
{
    std::string input = "SGVsbG8sIFdvcmxkIQ==";
    std::string result = Base64::Decode(input);
    EXPECT_EQ("Hello, World!", result);
}

TEST_F(Base64Test, RoundtripString)
{
    std::string original = "The quick brown fox jumps over the lazy dog.";
    std::string encoded = Base64::Encode(original);
    std::string decoded = Base64::Decode(encoded);
    EXPECT_EQ(original, decoded);
}

TEST_F(Base64Test, RoundtripVector)
{
    std::vector<char> original = {'H', 'e', 'l', 'l', 'o'};
    std::string encoded = Base64::Encode(original);
    std::vector<char> decoded = Base64::DecodeToBytes(encoded);
    EXPECT_EQ(original, decoded);
}

TEST_F(Base64Test, EncodeBinaryData)
{
    // Binary data with null bytes and high-bit characters
    std::vector<char> binary = {'\x00', '\x01', '\x02', '\xff', '\xfe', '\xfd'};
    std::string encoded = Base64::Encode(binary);
    std::vector<char> decoded = Base64::DecodeToBytes(encoded);
    EXPECT_EQ(binary, decoded);
}

TEST_F(Base64Test, EncodeUnicodeString)
{
    std::string original = "Привет, мир! 你好世界 🌍";
    std::string encoded = Base64::Encode(original);
    std::string decoded = Base64::Decode(encoded);
    EXPECT_EQ(original, decoded);
}

TEST_F(Base64Test, EncodeVariousLengths)
{
    // Test padding: 0, 1, 2 padding chars
    std::string len3 = "abc"; // No padding
    std::string len2 = "ab";  // One padding char
    std::string len1 = "a";   // Two padding chars

    EXPECT_EQ("YWJj", Base64::Encode(len3));
    EXPECT_EQ("YWI=", Base64::Encode(len2));
    EXPECT_EQ("YQ==", Base64::Encode(len1));
}

TEST_F(Base64Test, RoundtripLargeData)
{
    // Create 1KB of pseudo-random data
    std::vector<char> largeData(1024);
    for (size_t i = 0; i < largeData.size(); ++i)
    {
        largeData[i] = static_cast<char>((i * 17 + 31) % 256);
    }

    std::string encoded = Base64::Encode(largeData);
    std::vector<char> decoded = Base64::DecodeToBytes(encoded);
    EXPECT_EQ(largeData, decoded);
}

TEST_F(Base64Test, EncodeWithRawPointer)
{
    const unsigned char data[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // "Hello"
    std::string result = Base64::Encode(data, sizeof(data));
    EXPECT_EQ("SGVsbG8=", result);
}

} // namespace mbb::tests

#endif // BASE64TEST_H
