#include "HttpClient.h"

#include <curl/curl.h>
#include <fmt/core.h>

#include <stdexcept>

namespace mbb
{

namespace
{
constexpr long kConnectTimeoutMs = 10000L;  // 10 seconds
constexpr long kRequestTimeoutMs = 60000L;  // 60 seconds
constexpr long kLowSpeedLimitBytes = 1024L; // 1 KB/s minimum transfer speed
constexpr long kLowSpeedTimeSec = 30L;      // abort if below limit for 30 seconds
} // namespace

HttpClient::HttpClient()
{
    // No persistent CURL handle; we create per-request handles for thread safety.
}

HttpClient::~HttpClient()
{
}

size_t HttpClient::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    const size_t totalSize = size * nmemb;
    auto* buffer = static_cast<std::vector<char>*>(userp);
    buffer->insert(buffer->end(), static_cast<char*>(contents), static_cast<char*>(contents) + totalSize);
    return totalSize;
}

std::vector<char> HttpClient::Get(const std::string& url)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("cURL initialization failed");
    }

    std::vector<char> buffer;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, kConnectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, kRequestTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, kLowSpeedLimitBytes);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, kLowSpeedTimeSec);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK)
    {
        throw std::runtime_error(fmt::format("HTTP GET failed: {}", curl_easy_strerror(res)));
    }

    return buffer;
}

std::string HttpClient::GetString(const std::string& url)
{
    auto buffer = Get(url);
    return {buffer.begin(), buffer.end()};
}

std::string HttpClient::PostJson(const std::string& url,
                                 const std::string& json,
                                 const std::vector<std::string>& headers)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("cURL initialization failed");
    }

    std::vector<char> buffer;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, kConnectTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, kRequestTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, kLowSpeedLimitBytes);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, kLowSpeedTimeSec);

    // Build headers list
    struct curl_slist* headerList = nullptr;
    headerList = curl_slist_append(headerList, "Content-Type: application/json");
    for (const auto& header : headers)
    {
        headerList = curl_slist_append(headerList, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        throw std::runtime_error(fmt::format("HTTP POST failed: {}", curl_easy_strerror(res)));
    }

    return {buffer.begin(), buffer.end()};
}

} // namespace mbb
