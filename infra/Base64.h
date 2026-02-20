#pragma once

#include <string>
#include <vector>

namespace mbb
{

class Base64
{
  public:
    [[nodiscard]] static std::string Encode(const std::vector<char>& data);
    [[nodiscard]] static std::string Encode(const std::string& data);
    [[nodiscard]] static std::string Encode(const unsigned char* data, size_t len);
    [[nodiscard]] static std::string Decode(const std::string& encoded);
    [[nodiscard]] static std::vector<char> DecodeToBytes(const std::string& encoded);
};

} // namespace mbb
