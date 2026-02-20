#include "ai/GoogleService.h"

#include "core/Config.h"
#include "core/Logger.h"
#include "telegram/IMessageWorker.h"

#include <fmt/core.h>
#include <fmt/std.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cstring>
#include <thread>

namespace mbb
{

namespace
{
// Native Gemini API uses "user" and "model" roles
const char* ToGeminiRole(const std::string& role)
{
    if (role == "assistant")
    {
        return "model";
    }
    return "user";
}
} // anonymous namespace

GoogleService::GoogleService(std::shared_ptr<IMessageWorker> messageWorker)
    : IAiService(std::move(messageWorker))
{
    const auto& apiKey = Config::GetGoogleToken();

    // Native Gemini API: URL contains model name, streaming uses ?alt=sse
    primaryModel_ = {.name = "gemini-3-flash-preview",
                     .contextSize = 1024 * 1024,
                     .apiKey = apiKey,
                     .url = "https://generativelanguage.googleapis.com/v1beta/models/"};

    secondaryModel_ = {.name = "gemini-3-pro-preview",
                       .contextSize = 1024 * 1024,
                       .apiKey = apiKey,
                       .url = "https://generativelanguage.googleapis.com/v1beta/models/"};

    imageModel_ = {.name = "gemini-3-pro-image-preview",
                   .contextSize = 0,
                   .apiKey = apiKey,
                   .url = "https://generativelanguage.googleapis.com/v1beta/models/"};

    // Native Gemini audio support (STT only via generateContent API)
    audioModel_ = {.name = "gemini-3-flash-preview",
                   .contextSize = 1024 * 1024,
                   .apiKey = apiKey,
                   .url = "https://generativelanguage.googleapis.com/v1beta/models/"};
}

// =============================================================================
// ExtractStreamContent (Native Gemini SSE format)
// =============================================================================
// SSE parser behavior (from IAiService::TextStreamCallback):
// - Only "data:" payloads are passed here (event lines are ignored).
// - "data: [DONE]" is skipped before JSON parsing.
//
// Streaming payloads we process:
// data: {"candidates": [{"content": {"parts": [{"text": "Hello"}]}, "finishReason": null}]}
// data: {"candidates": [{"content": {"parts": [{"text": ""}]}, "finishReason": "STOP"}]}
// Stop markers: finishReason == "STOP" or "MAX_TOKENS".

bool GoogleService::ExtractStreamContent(const std::string& jsonChunk, StreamState& state) const
{
    // Parse a single SSE JSON payload from Gemini streaming.
    rapidjson::Document json;
    json.Parse(jsonChunk.c_str());

    if (json.HasParseError())
    {
        // Ignore malformed chunks; streaming may split frames.
        return false;
    }

    // Native Gemini format: candidates[].content.parts[].text
    if (!json.HasMember("candidates") || !json["candidates"].IsArray() || json["candidates"].Size() == 0)
    {
        // Expected shape is candidates[0].content.parts[*].text
        return false;
    }

    const auto& candidate = json["candidates"][0];

    // Check for finish reason
    if (candidate.HasMember("finishReason") && candidate["finishReason"].IsString())
    {
        const char* reason = candidate["finishReason"].GetString();
        if (strcmp(reason, "STOP") == 0 || strcmp(reason, "MAX_TOKENS") == 0)
        {
            // Flush final text into the message worker.
            state.workerId = messageWorker_->AddMessagePortion(state.workerId, state.chatId, state.threadId,
                                                                     state.responseText);
            return true;
        }
    }

    // Get text content from parts
    if (candidate.HasMember("content") && candidate["content"].HasMember("parts"))
    {
        for (const auto& part : candidate["content"]["parts"].GetArray())
        {
            if (part.HasMember("text") && part["text"].IsString())
            {
                // Append streamed text tokens.
                std::string text = part["text"].GetString();
                if (!text.empty())
                {
                    state.responseText += text;
                    state.workerId = messageWorker_->AddMessagePortion(state.workerId, state.chatId,
                                                                             state.threadId, state.responseText);
                }
            }
        }
    }

    return false;
}

// =============================================================================
// POST https://generativelanguage.googleapis.com/v1beta/models/{model}:streamGenerateContent?alt=sse
// =============================================================================
// Request (text only, with Google Search grounding):
// {
//   "contents": [
//     {"role": "user", "parts": [{"text": "Hello"}]},
//     {"role": "model", "parts": [{"text": "Hi there!"}]},
//     {"role": "user", "parts": [{"text": "How are you?"}]}
//   ],
//   "systemInstruction": {"parts": [{"text": "You are a helpful assistant."}]},
//   "tools": [{"google_search": {}}]
// }
// -----------------------------------------------------------------------------
// Request (with vision):
// {
//   "contents": [
//     {"role": "user", "parts": [{"text": "Previous message"}]},
//     {"role": "user", "parts": [
//       {"inline_data": {"mime_type": "image/jpeg", "data": "<base64>"}},
//       {"inline_data": {"mime_type": "image/jpeg", "data": "<base64>"}},
//       {"text": "What's in these images?"}
//     ]}
//   ],
//   "tools": [{"google_search": {}}],
//   "generationConfig": {"mediaResolution": "MEDIA_RESOLUTION_LOW"}
// }
// -----------------------------------------------------------------------------
// Response (streaming SSE):
// data: {"candidates": [{"content": {"parts": [{"text": "Hello"}]}, "finishReason": null}]}
// data: {"candidates": [{"content": {"parts": [{"text": " world"}]}, "finishReason": null}]}
// data: {"candidates": [{"content": {"parts": [{"text": ""}]}, "finishReason": "STOP"}]}      // stop marker
// data: {"candidates": [{"content": {"parts": [{"text": ""}]}, "finishReason": "MAX_TOKENS"}]} // stop marker
// =============================================================================
AiResponse GoogleService::GetTextResponse(int64_t chatId,
                                          int32_t threadId,
                                          const std::vector<std::pair<std::string, std::string>>& history,
                                          const std::vector<std::string>& visionImagesBase64,
                                          ModelSelector modelSelector,
                                          bool returnAudio)
{
    if (returnAudio)
    {
        throw AiServiceException("Google Gemini does not support audio output via generateContent API");
    }

    const AiModel& currentModel = (modelSelector == ModelSelector::Primary) ? primaryModel_ : secondaryModel_;

    Logger::Debug(fmt::format("{}. Thread ID: {}", __func__, std::this_thread::get_id()));

    // Build JSON payload (Native Gemini format)
    rapidjson::Document payload;
    payload.SetObject();
    auto& allocator = payload.GetAllocator();

    // Extract system message from history (if present) for systemInstruction
    std::string systemText;
    for (const auto& entry : history)
    {
        if (entry.first == "system")
        {
            systemText = entry.second;
            break;
        }
    }

    // Build contents array with token limiting (skip system entries)
    rapidjson::Value contents(rapidjson::kArrayType);
    size_t totalTokenCount = 0;
    size_t textTokenCount = 0;

    auto cursor = history.rbegin();
    for (; cursor != history.rend(); ++cursor)
    {
        if (cursor->first == "system")
        {
            continue;
        }
        if (ShouldStopAddingHistory(totalTokenCount, cursor->second, &textTokenCount, currentModel))
        {
            if (cursor == history.rbegin())
            {
                throw AiServiceException(fmt::format("Message too large: {} tokens", totalTokenCount + textTokenCount));
            }
            --cursor;
            Logger::Info(fmt::format("Messages truncated at {} tokens", totalTokenCount + textTokenCount));
            break;
        }
        totalTokenCount += textTokenCount;
    }

    // Add messages (skip system entries and skip last one if we have images)
    for (auto it = cursor.base(); it != history.end(); ++it)
    {
        // Skip system messages — handled via systemInstruction
        if (it->first == "system")
        {
            continue;
        }

        // Skip last message if we have images to attach
        if (!visionImagesBase64.empty() && it == history.end() - 1)
        {
            continue;
        }

        rapidjson::Value content(rapidjson::kObjectType);

        // Role: "user" or "model"
        content.AddMember("role", rapidjson::StringRef(ToGeminiRole(it->first)), allocator);

        // Parts array with text
        rapidjson::Value parts(rapidjson::kArrayType);
        rapidjson::Value part(rapidjson::kObjectType);
        rapidjson::Value textValue;
        textValue.SetString(it->second.c_str(), static_cast<rapidjson::SizeType>(it->second.length()), allocator);
        part.AddMember("text", textValue, allocator);
        parts.PushBack(part, allocator);

        content.AddMember("parts", parts, allocator);
        contents.PushBack(content, allocator);
    }

    // Add images message if present (Gemini vision support)
    if (!visionImagesBase64.empty())
    {
        rapidjson::Value content(rapidjson::kObjectType);
        content.AddMember("role", "user", allocator);

        rapidjson::Value parts(rapidjson::kArrayType);

        // Add all images
        for (const auto& imageBase64 : visionImagesBase64)
        {
            rapidjson::Value imagePart(rapidjson::kObjectType);
            rapidjson::Value inlineData(rapidjson::kObjectType);
            inlineData.AddMember("mime_type", "image/jpeg", allocator);
            rapidjson::Value imageData;
            imageData.SetString(imageBase64.c_str(), static_cast<rapidjson::SizeType>(imageBase64.length()), allocator);
            inlineData.AddMember("data", imageData, allocator);
            imagePart.AddMember("inline_data", inlineData, allocator);
            parts.PushBack(imagePart, allocator);
        }

        // Text part (last message)
        if (!history.empty())
        {
            rapidjson::Value textPart(rapidjson::kObjectType);
            const std::string& textMsg = history.back().second;
            rapidjson::Value textValue;
            textValue.SetString(textMsg.c_str(), static_cast<rapidjson::SizeType>(textMsg.length()), allocator);
            textPart.AddMember("text", textValue, allocator);
            parts.PushBack(textPart, allocator);
        }

        content.AddMember("parts", parts, allocator);
        contents.PushBack(content, allocator);
    }

    payload.AddMember("contents", contents, allocator);

    // Add systemInstruction if a system message was found
    if (!systemText.empty())
    {
        rapidjson::Value systemInstruction(rapidjson::kObjectType);
        rapidjson::Value sysParts(rapidjson::kArrayType);
        rapidjson::Value sysPart(rapidjson::kObjectType);
        rapidjson::Value sysTextValue;
        sysTextValue.SetString(systemText.c_str(), static_cast<rapidjson::SizeType>(systemText.length()), allocator);
        sysPart.AddMember("text", sysTextValue, allocator);
        sysParts.PushBack(sysPart, allocator);
        systemInstruction.AddMember("parts", sysParts, allocator);
        payload.AddMember("systemInstruction", systemInstruction, allocator);
    }

    // Enable Google Search grounding for up-to-date information
    rapidjson::Value tools(rapidjson::kArrayType);
    rapidjson::Value googleSearchTool(rapidjson::kObjectType);
    rapidjson::Value googleSearch(rapidjson::kObjectType);
    googleSearchTool.AddMember("google_search", googleSearch, allocator);
    tools.PushBack(googleSearchTool, allocator);
    payload.AddMember("tools", tools, allocator);

    // Add media_resolution for vision requests to limit token usage
    if (!visionImagesBase64.empty())
    {
        rapidjson::Value generationConfig(rapidjson::kObjectType);
        generationConfig.AddMember("mediaResolution", "MEDIA_RESOLUTION_LOW", allocator);
        payload.AddMember("generationConfig", generationConfig, allocator);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    payload.Accept(writer);

    // Native Gemini streaming URL: /models/{model}:streamGenerateContent?alt=sse
    std::string url = currentModel.url + currentModel.name + ":streamGenerateContent?alt=sse";
    std::string authHeader = fmt::format("x-goog-api-key: {}", currentModel.apiKey);

    StreamState state = {chatId, threadId, "", std::nullopt, "", "", {}, this};
    PostJsonStream(url, authHeader, buffer.GetString(), state);

    messageWorker_->FinalizeMessage(state.workerId);

    return AiResponse{.text = state.responseText, .media = {}, .textStreamed = true};
}

// =============================================================================
// POST https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent
// =============================================================================
// Request (STT - voice to text):
// {
//   "contents": [{
//     "parts": [{"inlineData": {"mimeType": "audio/wav", "data": "<base64>"}}]
//   }]
// }
// -----------------------------------------------------------------------------
// Response (STT):
// {
//   "candidates": [{
//     "content": {"parts": [{"text": "Transcribed and answered text"}]}
//   }]
// }
// =============================================================================
AiResponse GoogleService::GetResponseFromVoice(int64_t chatId,
                                               int32_t threadId,
                                               const std::string& audioBase64,
                                               bool returnAudio)
{
    if (returnAudio)
    {
        throw AiServiceException("Google Gemini does not support audio output via generateContent API");
    }

    Logger::Debug(fmt::format("{}. Thread ID: {} (native Gemini)", __func__, std::this_thread::get_id()));

    // Build JSON payload (Native Gemini format)
    rapidjson::Document payload;
    payload.SetObject();
    auto& allocator = payload.GetAllocator();

    // Contents with audio input
    rapidjson::Value contents(rapidjson::kArrayType);
    rapidjson::Value content(rapidjson::kObjectType);

    rapidjson::Value parts(rapidjson::kArrayType);
    rapidjson::Value audioPart(rapidjson::kObjectType);
    rapidjson::Value inlineData(rapidjson::kObjectType);
    inlineData.AddMember("mimeType", "audio/wav", allocator);
    rapidjson::Value audioData;
    audioData.SetString(audioBase64.c_str(), static_cast<rapidjson::SizeType>(audioBase64.length()), allocator);
    inlineData.AddMember("data", audioData, allocator);
    audioPart.AddMember("inlineData", inlineData, allocator);
    parts.PushBack(audioPart, allocator);

    content.AddMember("parts", parts, allocator);
    contents.PushBack(content, allocator);
    payload.AddMember("contents", contents, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    payload.Accept(writer);

    // Native Gemini API: /models/{model}:generateContent
    std::string url = audioModel_.url + audioModel_.name + ":generateContent";
    std::string authHeader = fmt::format("x-goog-api-key: {}", audioModel_.apiKey);

    auto responseData = IAiService::PostJson(url, authHeader, buffer.GetString());

    auto responseJson = ParseJsonResponse(responseData);

    AiResponse response;

    // Native Gemini format: candidates[].content.parts[].text
    if (!responseJson.HasMember("candidates") || !responseJson["candidates"].IsArray() ||
        responseJson["candidates"].Size() == 0)
    {
        throw AiServiceException("Invalid response format: missing candidates");
    }

    const auto& candidate = responseJson["candidates"][0];
    if (!candidate.HasMember("content") || !candidate["content"].HasMember("parts"))
    {
        throw AiServiceException("Invalid response format: missing content parts");
    }

    for (const auto& part : candidate["content"]["parts"].GetArray())
    {
        if (part.HasMember("text") && part["text"].IsString())
        {
            response.text = part["text"].GetString();
            break;
        }
    }

    if (response.text.empty())
    {
        throw AiServiceException("No text in response");
    }

    // Send text to message worker (streaming simulation)
    StreamState state = {chatId, threadId, response.text, std::nullopt, "", "", {}, this};
    state.workerId = messageWorker_->AddMessagePortion(state.workerId, chatId, threadId, response.text);
    messageWorker_->FinalizeMessage(state.workerId);
    response.textStreamed = true;

    return response;
}

// =============================================================================
// POST https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent
// =============================================================================
// Request (text only):
// {
//   "contents": [{"parts": [{"text": "A white cat"}]}],
//   "generationConfig": {"responseModalities": ["TEXT", "IMAGE"]}
// }
// -----------------------------------------------------------------------------
// Request (with reference images):
// {
//   "contents": [{"parts": [
//     {"inlineData": {"mimeType": "image/jpeg", "data": "<base64>"}},
//     {"inlineData": {"mimeType": "image/jpeg", "data": "<base64>"}},
//     {"text": "Generate similar image but with blue sky"}
//   ]}],
//   "generationConfig": {"responseModalities": ["TEXT", "IMAGE"]}
// }
// -----------------------------------------------------------------------------
// Response:
// {
//   "candidates": [{
//     "content": {
//       "parts": [
//         {"text": "Here's the generated image"},
//         {"inlineData": {"mimeType": "image/png", "data": "<base64>"}}
//       ]
//     }
//   }]
// }
// =============================================================================
AiResponse GoogleService::GetImageResponse(int64_t /* chatId */,
                                           int32_t /* threadId */,
                                           const std::string& prompt,
                                           const std::vector<std::string>& referenceImagesBase64)
{
    // Build JSON payload (Native Gemini format)
    rapidjson::Document payload;
    payload.SetObject();
    auto& allocator = payload.GetAllocator();

    // Contents with optional reference images and prompt
    rapidjson::Value contents(rapidjson::kArrayType);
    rapidjson::Value content(rapidjson::kObjectType);

    rapidjson::Value parts(rapidjson::kArrayType);

    // Add all reference images
    for (const auto& imageBase64 : referenceImagesBase64)
    {
        rapidjson::Value imagePart(rapidjson::kObjectType);
        rapidjson::Value inlineData(rapidjson::kObjectType);
        inlineData.AddMember("mimeType", "image/jpeg", allocator);
        rapidjson::Value imageData;
        imageData.SetString(imageBase64.c_str(), static_cast<rapidjson::SizeType>(imageBase64.length()), allocator);
        inlineData.AddMember("data", imageData, allocator);
        imagePart.AddMember("inlineData", inlineData, allocator);
        parts.PushBack(imagePart, allocator);
    }

    // Add text prompt
    rapidjson::Value textPart(rapidjson::kObjectType);
    rapidjson::Value promptValue;
    promptValue.SetString(prompt.c_str(), static_cast<rapidjson::SizeType>(prompt.length()), allocator);
    textPart.AddMember("text", promptValue, allocator);
    parts.PushBack(textPart, allocator);

    content.AddMember("parts", parts, allocator);
    contents.PushBack(content, allocator);
    payload.AddMember("contents", contents, allocator);

    // Generation config for image output
    rapidjson::Value generationConfig(rapidjson::kObjectType);
    rapidjson::Value responseModalities(rapidjson::kArrayType);
    responseModalities.PushBack("TEXT", allocator);
    responseModalities.PushBack("IMAGE", allocator);
    generationConfig.AddMember("responseModalities", responseModalities, allocator);

    payload.AddMember("generationConfig", generationConfig, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    payload.Accept(writer);

    // Native Gemini API: /models/{model}:generateContent
    std::string url = imageModel_.url + imageModel_.name + ":generateContent";
    std::string authHeader = fmt::format("x-goog-api-key: {}", imageModel_.apiKey);

    auto responseData = IAiService::PostJson(url, authHeader, buffer.GetString());

    auto responseJson = ParseJsonResponse(responseData);

    AiResponse response;

    // Native Gemini format: candidates[].content.parts[].inlineData.data
    if (responseJson.HasMember("candidates") && responseJson["candidates"].IsArray())
    {
        for (const auto& candidate : responseJson["candidates"].GetArray())
        {
            if (!candidate.HasMember("content") || !candidate["content"].HasMember("parts"))
            {
                continue;
            }

            for (const auto& part : candidate["content"]["parts"].GetArray())
            {
                if (part.HasMember("inlineData") && part["inlineData"].HasMember("data"))
                {
                    response.media.push_back(MediaItem{.type = MediaItem::Type::ImageBase64,
                                                       .data = part["inlineData"]["data"].GetString(),
                                                       .mimeType = "image/png"});
                }
            }
        }
    }

    return response;
}

} // namespace mbb
