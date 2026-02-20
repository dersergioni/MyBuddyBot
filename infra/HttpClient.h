#pragma once

#include <functional>
#include <string>
#include <vector>

namespace mbb
{

class HttpClient
{
  public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Simple GET request, returns raw bytes
    [[nodiscard]] std::vector<char> Get(const std::string& url);

    // Simple GET request, returns string
    [[nodiscard]] std::string GetString(const std::string& url);

    // POST JSON request with headers
    [[nodiscard]] std::string PostJson(const std::string& url,
                                       const std::string& json,
                                       const std::vector<std::string>& headers = {});

  private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

} // namespace mbb
