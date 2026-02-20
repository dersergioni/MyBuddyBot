#include "ai/IAiService.h"

#include "core/Logger.h"
#include "telegram/IMessageWorker.h"

#include <curl/curl.h>
#include <fmt/core.h>
#include <rapidjson/document.h>

#include <chrono>
#include <thread>

namespace mbb
{

IAiService::IAiService(std::shared_ptr<IMessageWorker> messageWorker) : messageWorker_(std::move(messageWorker))
{
}

// =============================================================================
// SimpleCallback
// =============================================================================

size_t IAiService::SimpleCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// =============================================================================
// TextStreamCallback (SSE parser)
// =============================================================================
// Server-Sent Events format (may include "event:" lines):
// data: {"choices":[{"delta":{"content":"Hello"}}]}              // OpenAI Chat Completions
// event: response.output_text.delta
// data: {"type":"response.output_text.delta","delta":"Hello"}    // xAI Responses
// data: {"candidates":[{"content":{"parts":[{"text":"Hello"}]}}]} // Google Gemini
// data: [DONE]
//
// SSE rules used here:
// - An event is a block of lines separated by a blank line.
// - We ignore all "event:" lines and only process "data:" lines.
// - Multiple "data:" lines in one event are concatenated with '\n'.
// - The special payload "[DONE]" is skipped (no JSON parsing).
// - Each JSON payload is passed to ExtractStreamContent() for provider-specific handling.

size_t IAiService::TextStreamCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    auto* state = static_cast<StreamState*>(userp);
    state->incomingChunk = std::string(static_cast<char*>(contents), size * nmemb);
    // Accumulate across callbacks because SSE events may arrive split.
    state->remainingChunk += state->incomingChunk;

    while (true)
    {
        // Each SSE event is separated by a blank line (\n\n or \r\n\r\n).
        size_t sepPos = state->remainingChunk.find("\n\n");
        size_t sepLen = 2;
        size_t altPos = state->remainingChunk.find("\r\n\r\n");
        if (altPos != std::string::npos && (sepPos == std::string::npos || altPos < sepPos))
        {
            sepPos = altPos;
            sepLen = 4;
        }

        if (sepPos == std::string::npos)
        {
            // Not enough data for a complete event yet.
            break;
        }

        // Extract one full SSE event block and remove it from the buffer.
        std::string eventBlock = state->remainingChunk.substr(0, sepPos);
        state->remainingChunk.erase(0, sepPos + sepLen);

        // Collect and concatenate all "data:" lines (SSE allows multiple).
        std::string dataPayload;
        size_t lineStart = 0;
        while (lineStart <= eventBlock.size())
        {
            size_t lineEnd = eventBlock.find('\n', lineStart);
            if (lineEnd == std::string::npos)
            {
                lineEnd = eventBlock.size();
            }

            std::string line = eventBlock.substr(lineStart, lineEnd - lineStart);
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            if (line.rfind("data:", 0) == 0)
            {
                // Strip "data:" and optional leading space.
                std::string chunk = line.substr(5);
                if (!chunk.empty() && chunk[0] == ' ')
                {
                    chunk.erase(0, 1);
                }
                if (!dataPayload.empty())
                {
                    dataPayload.push_back('\n');
                }
                dataPayload += chunk;
            }

            if (lineEnd == eventBlock.size())
            {
                break;
            }
            lineStart = lineEnd + 1;
        }

        // Ignore empty events and the stream terminator.
        if (dataPayload.empty() || dataPayload == "[DONE]")
        {
            continue;
        }

        try
        {
            // Parse JSON payload and delegate provider-specific extraction.
            rapidjson::Document json;
            json.Parse(dataPayload.c_str());

            if (json.HasParseError())
            {
                Logger::Debug(fmt::format("Invalid SSE JSON chunk: {}", dataPayload));
                continue;
            }

            if (json.HasMember("error"))
            {
                std::string errorMsg = "API stream error";
                if (json["error"].IsObject() && json["error"].HasMember("message") &&
                    json["error"]["message"].IsString())
                {
                    errorMsg = json["error"]["message"].GetString();
                }
                Logger::Error(fmt::format("Error in SSE stream: {}", errorMsg));
                throw AiServiceException(errorMsg);
            }

            if (state->service)
            {
                // ExtractStreamContent knows the provider-specific JSON schema.
                (void)state->service->ExtractStreamContent(dataPayload, *state);
            }
        }
        catch (const std::exception& e)
        {
            // Keep streaming even if a single chunk fails.
            Logger::Debug(fmt::format("Exception in {}: {} for {}", __func__, e.what(), dataPayload));
        }
    }

    return size * nmemb;
}

// =============================================================================
// GetModelName (default implementation)
// =============================================================================

const std::string& IAiService::GetModelName(ModelSelector selector) const
{
    switch (selector)
    {
    case ModelSelector::Primary:
        return primaryModel_.name;
    case ModelSelector::Secondary:
        return secondaryModel_.name;
    case ModelSelector::Image:
        return imageModel_.name;
    case ModelSelector::Audio:
        return audioModel_.name;
    }
    throw AiServiceException("Invalid model selector");
}

// =============================================================================
// PostJson helper (with retry + exponential backoff)
// =============================================================================

namespace
{
constexpr int kMaxRetries = 3;
constexpr int kInitialDelayMs = 500;
constexpr int kBackoffMultiplier = 2;

bool IsRetryableCurlError(CURLcode code)
{
    return code == CURLE_COULDNT_CONNECT || code == CURLE_OPERATION_TIMEDOUT || code == CURLE_GOT_NOTHING ||
           code == CURLE_RECV_ERROR;
}

constexpr long kHttpTooManyRequests = 429;
constexpr long kHttpInternalServerError = 500;
constexpr long kHttpBadGateway = 502;
constexpr long kHttpServiceUnavailable = 503;
constexpr long kHttpGatewayTimeout = 504;

bool IsRetryableHttpStatus(long httpCode)
{
    return httpCode == kHttpTooManyRequests || httpCode == kHttpInternalServerError || httpCode == kHttpBadGateway ||
           httpCode == kHttpServiceUnavailable || httpCode == kHttpGatewayTimeout;
}
} // anonymous namespace

std::string IAiService::PostJson(const std::string& url,
                                 const std::string& authHeader,
                                 const std::string& jsonBody) const
{
    int delayMs = kInitialDelayMs;

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt)
    {
        if (attempt > 0)
        {
            Logger::Info(fmt::format("Retry attempt {}/{} after {}ms delay", attempt, kMaxRetries, delayMs));
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            delayMs *= kBackoffMultiplier;
        }

        CURL* curl = curl_easy_init();
        if (!curl)
        {
            throw AiServiceException("cURL initialization failed");
        }

        std::string responseData;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, IAiService::SimpleCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, authHeader.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);

        long httpCode = 0;
        if (res == CURLE_OK)
        {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            if (IsRetryableCurlError(res) && attempt < kMaxRetries)
            {
                Logger::Info(fmt::format("Retryable cURL error: {}", curl_easy_strerror(res)));
                continue;
            }
            throw AiServiceException(fmt::format("cURL request failed: {}", curl_easy_strerror(res)));
        }

        if (IsRetryableHttpStatus(httpCode) && attempt < kMaxRetries)
        {
            Logger::Info(fmt::format("Retryable HTTP status: {}", httpCode));
            continue;
        }

        return responseData;
    }

    throw AiServiceException("Request failed after maximum retries");
}

// =============================================================================
// PostJsonStream helper (with retry + exponential backoff)
// =============================================================================

void IAiService::PostJsonStream(const std::string& url,
                                const std::string& authHeader,
                                const std::string& jsonBody,
                                StreamState& state) const
{
    int delayMs = kInitialDelayMs;

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt)
    {
        if (attempt > 0)
        {
            Logger::Info(fmt::format("Stream retry attempt {}/{} after {}ms delay", attempt, kMaxRetries, delayMs));
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            delayMs *= kBackoffMultiplier;

            // Reset stream state for retry
            state.responseText.clear();
            state.workerId = std::nullopt;
            state.incomingChunk.clear();
            state.remainingChunk.clear();
        }

        CURL* curl = curl_easy_init();
        if (!curl)
        {
            throw AiServiceException("cURL initialization failed");
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, IAiService::TextStreamCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, authHeader.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);

        long httpCode = 0;
        if (res == CURLE_OK)
        {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            if (IsRetryableCurlError(res) && attempt < kMaxRetries)
            {
                Logger::Info(fmt::format("Retryable cURL error (stream): {}", curl_easy_strerror(res)));
                continue;
            }
            throw AiServiceException(fmt::format("cURL request failed: {}", curl_easy_strerror(res)));
        }

        if (IsRetryableHttpStatus(httpCode) && attempt < kMaxRetries)
        {
            Logger::Info(fmt::format("Retryable HTTP status (stream): {}", httpCode));
            continue;
        }

        if (httpCode >= 400)
        {
            // Non-retryable HTTP error — extract message from unparsed response body
            std::string errorDetail;
            if (!state.remainingChunk.empty())
            {
                rapidjson::Document errorDoc;
                errorDoc.Parse(state.remainingChunk.c_str());
                if (!errorDoc.HasParseError() && errorDoc.HasMember("error") && errorDoc["error"].IsObject() &&
                    errorDoc["error"].HasMember("message") && errorDoc["error"]["message"].IsString())
                {
                    errorDetail = errorDoc["error"]["message"].GetString();
                }
                else
                {
                    errorDetail = state.remainingChunk.substr(0, 500);
                }
            }
            throw AiServiceException(
                fmt::format("API error (HTTP {}): {}", httpCode, errorDetail.empty() ? "unknown error" : errorDetail));
        }

        Logger::Debug(
            fmt::format("PostJsonStream completed: HTTP {}, responseText length: {}, remainingChunk length: {}",
                        httpCode, state.responseText.size(), state.remainingChunk.size()));

        return;
    }

    throw AiServiceException("Stream request failed after maximum retries");
}

// =============================================================================
// ParseJsonResponse
// =============================================================================
// Parses API response JSON and checks for errors.
//
// Error format (common across OpenAI/xAI/Gemini):
// {
//   "error": {
//     "message": "Error description",
//     "type": "invalid_request_error",
//     "code": "invalid_api_key"
//   }
// }

rapidjson::Document IAiService::ParseJsonResponse(const std::string& responseData)
{
    // Parse full JSON responses (non-streaming endpoints).
    rapidjson::Document doc;
    doc.Parse(responseData.c_str());

    if (doc.HasParseError())
    {
        // Surface low-level JSON parse errors early.
        throw AiServiceException(fmt::format("JSON parse error: {}", static_cast<int>(doc.GetParseError())));
    }

    if (doc.HasMember("error"))
    {
        // Normalize provider error objects into a simple exception message.
        std::string errorMsg = "API returned error";
        if (doc["error"].IsObject() && doc["error"].HasMember("message"))
        {
            errorMsg = doc["error"]["message"].GetString();
        }
        throw AiServiceException(errorMsg);
    }

    return doc;
}

// =============================================================================
// ShouldStopAddingHistory
// =============================================================================

bool IAiService::ShouldStopAddingHistory(size_t totalTokenCount,
                                         const std::string& text,
                                         size_t* textTokenCount,
                                         const AiModel& model) const
{
    constexpr size_t kCharsPerToken = 4;        // Approximate: ~4 characters per token
    constexpr size_t kContextInputPercent = 75; // Use 75% of context for input, reserve 25% for response
    const size_t tokenCount = text.size() / kCharsPerToken;

    if (textTokenCount != nullptr)
    {
        *textTokenCount = tokenCount;
    }

    const size_t maxInputTokens = model.contextSize * kContextInputPercent / 100;
    return (totalTokenCount + tokenCount) > maxInputTokens;
}

} // namespace mbb
