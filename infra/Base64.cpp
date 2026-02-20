#include "Base64.h"

#include "ext/cpp-base64/base64.h"

namespace mbb
{

std::string Base64::Encode(const std::vector<char>& data)
{
    return base64_encode(reinterpret_cast<const unsigned char*>(data.data()), data.size());
}

std::string Base64::Encode(const std::string& data)
{
    return base64_encode(data);
}

std::string Base64::Encode(const unsigned char* data, size_t len)
{
    return base64_encode(data, len);
}

std::string Base64::Decode(const std::string& encoded)
{
    return base64_decode(encoded);
}

std::vector<char> Base64::DecodeToBytes(const std::string& encoded)
{
    auto decoded = base64_decode(encoded);
    return {decoded.begin(), decoded.end()};
}

} // namespace mbb
