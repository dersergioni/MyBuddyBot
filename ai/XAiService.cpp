#include "ai/XAiService.h"

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
std::string ExtractOutputTextFromResponse(const rapidjson::Value& response)
{
    std::string text;

    if (response.HasMember("output_text"))
    {
        const auto& outputText = response["output_text"];
        if (outputText.IsString())
        {
            return outputText.GetString();
        }
        if (outputText.IsArray())
        {
            for (const auto& item : outputText.GetArray())
            {
                if (item.IsString())
                {
                    text += item.GetString();
                }
            }
            if (!text.empty())
            {
                return text;
            }
        }
    }

    if (response.HasMember("output") && response["output"].IsArray())
    {
        for (const auto& outputItem : response["output"].GetArray())
        {
            if (!outputItem.IsObject())
            {
                continue;
            }

            if (outputItem.HasMember("content") && outputItem["content"].IsArray())
            {
                for (const auto& contentItem : outputItem["content"].GetArray())
                {
                    if (contentItem.IsObject() && contentItem.HasMember("text") && contentItem["text"].IsString())
                    {
                        text += contentItem["text"].GetString();
                    }
                }
            }
            else if (outputItem.HasMember("text") && outputItem["text"].IsString())
            {
                text += outputItem["text"].GetString();
            }
        }
    }

    return text;
}
} // namespace

XAiService::XAiService(std::shared_ptr<IMessageWorker> messageWorker) : IAiService(std::move(messageWorker))
{
    const auto& apiKey = Config::GetXAiToken();

    primaryModel_ = {.name = "grok-4-1-fast-reasoning",
                     .contextSize = 2 * 1024 * 1024,
                     .apiKey = apiKey,
                     .url = "https://api.x.ai/v1/responses"};

    secondaryModel_ = {.name = "grok-4-1-fast-non-reasoning",
                       .contextSize = 2 * 1024 * 1024,
                       .apiKey = apiKey,
                       .url = "https://api.x.ai/v1/responses"};

    imageModel_ = {.name = "grok-2-image-1212",
                   .contextSize = 0,
                   .apiKey = apiKey,
                   .url = "https://api.x.ai/v1/images/generations"};

    // xAI does not support audio
    audioModel_ = {};
}

const std::string& XAiService::GetModelName(ModelSelector selector) const
{
    if (selector == ModelSelector::Audio)
    {
        throw AiServiceException("xAI does not support audio");
    }
    return IAiService::GetModelName(selector);
}

// =============================================================================
// ExtractStreamContent (xAI/Grok Responses SSE format)
// =============================================================================
// SSE parser behavior (from IAiService::TextStreamCallback):
// - Only "data:" payloads are passed here (event lines are ignored).
// - "data: [DONE]" is skipped before JSON parsing.
//
// Streaming payloads we process:
// event: response.output_text.delta
// data: {"type": "response.output_text.delta", "delta": "Hello"}
// event: response.output_text.done
// data: {"type": "response.output_text.done"}
// Stop markers: type == "response.output_text.done" or "response.completed".

bool XAiService::ExtractStreamContent(const std::string& jsonChunk, StreamState& state) const
{
    // Parse a single SSE JSON payload from the Responses stream.
    rapidjson::Document json;
    json.Parse(jsonChunk.c_str());

    if (json.HasParseError())
    {
        // Ignore malformed chunks; the stream may include partial frames.
        return false;
    }

    if (!json.HasMember("type") || !json["type"].IsString())
    {
        // All Responses events include a string "type".
        return false;
    }

    const char* type = json["type"].GetString();

    if (strcmp(type, "response.output_text.delta") == 0)
    {
        // Incremental text tokens for the current response.
        if (json.HasMember("delta") && json["delta"].IsString())
        {
            std::string content = json["delta"].GetString();
            if (!content.empty())
            {
                state.responseText += content;
                state.workerId =
                    messageWorker_->AddMessagePortion(state.workerId, state.chatId, state.threadId, state.responseText);
            }
        }
        else if (json.HasMember("text") && json["text"].IsString())
        {
            // Fallback: some events may use "text" instead of "delta".
            std::string content = json["text"].GetString();
            if (!content.empty())
            {
                state.responseText += content;
                state.workerId =
                    messageWorker_->AddMessagePortion(state.workerId, state.chatId, state.threadId, state.responseText);
            }
        }
    }
    else if (strcmp(type, "response.output_text.done") == 0 || strcmp(type, "response.completed") == 0)
    {
        // Finalize text output and signal completion.
        if (state.responseText.empty() && json.HasMember("text") && json["text"].IsString())
        {
            // If text wasn't streamed, use the final text payload.
            state.responseText = json["text"].GetString();
        }
        else if (state.responseText.empty() && json.HasMember("response") && json["response"].IsObject())
        {
            // Some responses include the final output in a nested response object.
            state.responseText = ExtractOutputTextFromResponse(json["response"]);
        }
        state.workerId =
            messageWorker_->AddMessagePortion(state.workerId, state.chatId, state.threadId, state.responseText);
        return true;
    }

    return false;
}

// =============================================================================
// POST https://api.x.ai/v1/responses
// =============================================================================
// Request (text only, with agentic web search):
// {
//   "model": "grok-4-1-fast-reasoning",
//   "stream": true,
//   "input": [
//     {"role": "user", "content": [{"type": "input_text", "text": "Hello"}]},
//     {"role": "assistant", "content": [{"type": "input_text", "text": "Hi there!"}]},
//     {"role": "system", "content": [{"type": "input_text", "text": "You are a helpful assistant."}]},
//     {"role": "user", "content": [{"type": "input_text", "text": "How are you?"}]}
//   ],
//   "tools": [{"type": "web_search"}]
// }
// -----------------------------------------------------------------------------
// Request (with vision):
// {
//   "model": "grok-4-1-fast-reasoning",
//   "stream": true,
//   "input": [
//     {"role": "user", "content": [{"type": "input_text", "text": "Previous message"}]},
//     {"role": "user", "content": [
//       {"type": "input_image", "image_url": "data:image/jpeg;base64,...", "detail": "low"},
//       {"type": "input_image", "image_url": "data:image/jpeg;base64,...", "detail": "low"},
//       {"type": "input_text", "text": "What's in these images?"}
//     ]}
//   ],
//   "tools": [{"type": "web_search"}]
// }
// -----------------------------------------------------------------------------
// Response (streaming SSE):
// event: response.output_text.delta
// data: {"type": "response.output_text.delta", "delta": "Hello"}
// event: response.output_text.delta
// data: {"type": "response.output_text.delta", "delta": " world"}
// event: response.output_text.done
// data: {"type": "response.output_text.done"}                  // stop marker
// event: response.completed
// data: {"type": "response.completed"}                         // stop marker
// data: [DONE]                                                  // skipped by SSE parser
// =============================================================================
AiResponse XAiService::GetTextResponse(int64_t chatId,
                                       int32_t threadId,
                                       const std::vector<std::pair<std::string, std::string>>& history,
                                       const std::vector<std::string>& visionImagesBase64,
                                       ModelSelector modelSelector,
                                       bool returnAudio)
{
    if (returnAudio)
    {
        throw AiServiceException("xAI does not support audio output");
    }

    const AiModel& currentModel = (modelSelector == ModelSelector::Primary) ? primaryModel_ : secondaryModel_;

    Logger::Debug(fmt::format("{}. Thread ID: {}", __func__, std::this_thread::get_id()));

    // Build JSON payload
    rapidjson::Document payload;
    payload.SetObject();
    auto& allocator = payload.GetAllocator();

    rapidjson::Value modelValue;
    modelValue.SetString(currentModel.name.c_str(), static_cast<rapidjson::SizeType>(currentModel.name.length()),
                         allocator);
    payload.AddMember("model", modelValue, allocator);
    payload.AddMember("stream", true, allocator);

    // Build input array with token limiting
    rapidjson::Value input(rapidjson::kArrayType);
    size_t totalTokenCount = 0;
    size_t textTokenCount = 0;

    auto cursor = history.rbegin();
    for (; cursor != history.rend(); ++cursor)
    {
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

    // Add text messages (skip last one if we have images - it will be added with images)
    for (auto it = cursor.base(); it != history.end(); ++it)
    {
        // Skip last message if we have images to attach
        if (!visionImagesBase64.empty() && it == history.end() - 1)
        {
            continue;
        }

        rapidjson::Value message(rapidjson::kObjectType);
        rapidjson::Value role;
        role.SetString(it->first.c_str(), static_cast<rapidjson::SizeType>(it->first.length()), allocator);
        message.AddMember("role", role, allocator);

        rapidjson::Value content(rapidjson::kArrayType);
        rapidjson::Value textContent(rapidjson::kObjectType);
        textContent.AddMember("type", "input_text", allocator);
        rapidjson::Value textValue;
        textValue.SetString(it->second.c_str(), static_cast<rapidjson::SizeType>(it->second.length()), allocator);
        textContent.AddMember("text", textValue, allocator);
        content.PushBack(textContent, allocator);
        message.AddMember("content", content, allocator);

        input.PushBack(message, allocator);
    }

    // Add images message if present (xAI vision support)
    if (!visionImagesBase64.empty())
    {
        rapidjson::Value message(rapidjson::kObjectType);
        message.AddMember("role", "user", allocator);

        rapidjson::Value content(rapidjson::kArrayType);

        // Add all images
        for (const auto& imageBase64 : visionImagesBase64)
        {
            rapidjson::Value imageContent(rapidjson::kObjectType);
            imageContent.AddMember("type", "input_image", allocator);

            std::string imageDataUrl = "data:image/jpeg;base64," + imageBase64;
            rapidjson::Value urlValue;
            urlValue.SetString(imageDataUrl.c_str(), static_cast<rapidjson::SizeType>(imageDataUrl.length()),
                               allocator);
            imageContent.AddMember("image_url", urlValue, allocator);
            imageContent.AddMember("detail", "low", allocator);
            content.PushBack(imageContent, allocator);
        }

        // Text content (last message)
        if (!history.empty())
        {
            rapidjson::Value textContent(rapidjson::kObjectType);
            textContent.AddMember("type", "input_text", allocator);
            const std::string& textMsg = history.back().second;
            rapidjson::Value textValue;
            textValue.SetString(textMsg.c_str(), static_cast<rapidjson::SizeType>(textMsg.length()), allocator);
            textContent.AddMember("text", textValue, allocator);
            content.PushBack(textContent, allocator);
        }

        message.AddMember("content", content, allocator);
        input.PushBack(message, allocator);
    }

    payload.AddMember("input", input, allocator);

    // Enable agentic web search
    rapidjson::Value tools(rapidjson::kArrayType);
    rapidjson::Value webSearchTool(rapidjson::kObjectType);
    webSearchTool.AddMember("type", "web_search", allocator);
    tools.PushBack(webSearchTool, allocator);
    payload.AddMember("tools", tools, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    payload.Accept(writer);

    std::string authHeader = fmt::format("Authorization: Bearer {}", currentModel.apiKey);
    StreamState state = {chatId, threadId, "", std::nullopt, "", "", {}, this};
    PostJsonStream(currentModel.url, authHeader, buffer.GetString(), state);

    messageWorker_->FinalizeMessage(state.workerId);

    return AiResponse{.text = state.responseText, .media = {}, .textStreamed = true};
}

AiResponse XAiService::GetResponseFromVoice(int64_t /* chatId */,
                                            int32_t /* threadId */,
                                            const std::string& /* audioBase64 */,
                                            bool /* returnAudio */)
{
    throw AiServiceException("xAI does not support voice messages");
}

// =============================================================================
// POST https://api.x.ai/v1/images/generations
// =============================================================================
// Request:
// {
//   "model": "grok-2-image-1212",
//   "prompt": "A white cat",
//   "n": 3
// }
// -----------------------------------------------------------------------------
// Response:
// {
//   "data": [
//     {"url": "https://...image1.png"},
//     {"url": "https://...image2.png"},
//     {"url": "https://...image3.png"}
//   ]
// }
// =============================================================================
AiResponse XAiService::GetImageResponse(int64_t /* chatId */,
                                        int32_t /* threadId */,
                                        const std::string& prompt,
                                        const std::vector<std::string>& referenceImagesBase64)
{
    if (!referenceImagesBase64.empty())
    {
        throw AiServiceException("Reference images are not supported by xAI");
    }

    // Build JSON payload
    rapidjson::Document payload;
    payload.SetObject();
    auto& allocator = payload.GetAllocator();

    rapidjson::Value modelValue;
    modelValue.SetString(imageModel_.name.c_str(), static_cast<rapidjson::SizeType>(imageModel_.name.length()),
                         allocator);
    payload.AddMember("model", modelValue, allocator);

    rapidjson::Value promptValue;
    promptValue.SetString(prompt.c_str(), static_cast<rapidjson::SizeType>(prompt.length()), allocator);
    payload.AddMember("prompt", promptValue, allocator);
    payload.AddMember("n", kImageGenerationCount, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    payload.Accept(writer);

    std::string authHeader = fmt::format("Authorization: Bearer {}", imageModel_.apiKey);
    auto responseData = IAiService::PostJson(imageModel_.url, authHeader, buffer.GetString());

    auto responseJson = ParseJsonResponse(responseData);

    AiResponse response;

    if (responseJson.HasMember("data") && responseJson["data"].IsArray())
    {
        for (const auto& item : responseJson["data"].GetArray())
        {
            if (item.HasMember("url") && item["url"].IsString())
            {
                response.media.push_back(
                    MediaItem{.type = MediaItem::Type::ImageUrl, .data = item["url"].GetString(), .mimeType = ""});
            }
        }
    }

    return response;
}

} // namespace mbb
