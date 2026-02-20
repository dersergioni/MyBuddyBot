#include "ai/OpenAiService.h"

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

OpenAiService::OpenAiService(std::shared_ptr<IMessageWorker> messageWorker)
    : IAiService(std::move(messageWorker))
{
    const auto& apiKey = Config::GetOpenAiToken();

    primaryModel_ = {
        .name = "gpt-5.2", .contextSize = 400 * 1024, .apiKey = apiKey, .url = "https://api.openai.com/v1/responses"};

    secondaryModel_ = {.name = "o4-mini-deep-research",
                       .contextSize = 200 * 1024,
                       .apiKey = apiKey,
                       .url = "https://api.openai.com/v1/responses"};

    imageModel_ = {.name = "chatgpt-image-latest",
                   .contextSize = 0,
                   .apiKey = apiKey,
                   .url = "https://api.openai.com/v1/images/generations"};

    audioModel_ = {.name = "gpt-4o-audio-preview",
                   .contextSize = 128 * 1024,
                   .apiKey = apiKey,
                   .url = "https://api.openai.com/v1/chat/completions"};
}

// =============================================================================
// ExtractStreamContent (dual-format: Responses API + Chat Completions fallback)
// =============================================================================
// SSE parser behavior (from IAiService::TextStreamCallback):
// - Only "data:" payloads are passed here (event lines are ignored).
// - "data: [DONE]" is skipped before JSON parsing.
//
// Responses API (text models via /v1/responses):
// data: {"type": "response.output_text.delta", "delta": "Hello"}
// data: {"type": "response.output_text.done"}                  // stop marker
// data: {"type": "response.completed"}                         // stop marker
//
// Chat Completions (audio model via /v1/chat/completions):
// data: {"choices": [{"delta": {"content": "Hello"}, "finish_reason": null}]}
// data: {"choices": [{"delta": {}, "finish_reason": "stop"}]}  // stop marker

bool OpenAiService::ExtractStreamContent(const std::string& jsonChunk, StreamState& state) const
{
    rapidjson::Document json;
    json.Parse(jsonChunk.c_str());

    if (json.HasParseError())
    {
        return false;
    }

    // --- Responses API format (has "type" field) ---
    if (json.HasMember("type") && json["type"].IsString())
    {
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
                    state.workerId = messageWorker_->AddMessagePortion(state.workerId, state.chatId,
                                                                             state.threadId, state.responseText);
                }
            }
            else if (json.HasMember("text") && json["text"].IsString())
            {
                // Fallback: some events may use "text" instead of "delta".
                std::string content = json["text"].GetString();
                if (!content.empty())
                {
                    state.responseText += content;
                    state.workerId = messageWorker_->AddMessagePortion(state.workerId, state.chatId,
                                                                             state.threadId, state.responseText);
                }
            }
        }
        else if (strcmp(type, "response.failed") == 0 || strcmp(type, "response.incomplete") == 0)
        {
            std::string detail = jsonChunk.substr(0, 500);
            Logger::Error(fmt::format("OpenAI response event '{}': {}", type, detail));
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
            state.workerId = messageWorker_->AddMessagePortion(state.workerId, state.chatId, state.threadId,
                                                                     state.responseText);
            return true;
        }

        return false;
    }

    // --- Chat Completions format (audio fallback, has "choices" array) ---
    if (!json.HasMember("choices") || !json["choices"].IsArray() || json["choices"].Size() == 0)
    {
        return false;
    }

    const auto& choices = json["choices"];

    if (choices[0].HasMember("finish_reason") && choices[0]["finish_reason"].IsString())
    {
        if (strcmp(choices[0]["finish_reason"].GetString(), "stop") == 0)
        {
            state.workerId = messageWorker_->AddMessagePortion(state.workerId, state.chatId, state.threadId,
                                                                     state.responseText);
            return true;
        }
    }

    if (choices[0].HasMember("delta"))
    {
        const auto& delta = choices[0]["delta"];
        if (delta.HasMember("content") && delta["content"].IsString())
        {
            std::string content = delta["content"].GetString();
            if (!content.empty())
            {
                state.responseText += content;
                state.workerId = messageWorker_->AddMessagePortion(state.workerId, state.chatId, state.threadId,
                                                                         state.responseText);
            }
        }
    }

    return false;
}

// =============================================================================
// POST https://api.openai.com/v1/responses  (text, vision)
// POST https://api.openai.com/v1/chat/completions  (audio fallback)
// =============================================================================
// Request (text only, with agentic web search):
// {
//   "model": "gpt-5.2",
//   "stream": true,
//   "input": [
//     {"role": "user", "content": [{"type": "input_text", "text": "Hello"}]},
//     {"role": "assistant", "content": [{"type": "output_text", "text": "Hi there!"}]},
//     {"role": "system", "content": [{"type": "input_text", "text": "You are a helpful assistant."}]},
//     {"role": "user", "content": [{"type": "input_text", "text": "How are you?"}]}
//   ],
//   "tools": [{"type": "web_search"}]
// }
// -----------------------------------------------------------------------------
// Request (with vision):
// {
//   "model": "gpt-5.2",
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
// data: [DONE]                                                 // skipped by SSE parser
// =============================================================================
AiResponse OpenAiService::GetTextResponse(int64_t chatId,
                                          int32_t threadId,
                                          const std::vector<std::pair<std::string, std::string>>& history,
                                          const std::vector<std::string>& visionImagesBase64,
                                          ModelSelector modelSelector,
                                          bool returnAudio)
{
    // For audio output, use audioModel_ (gpt-4o-audio-preview) which stays on Chat Completions
    const AiModel& currentModel = returnAudio                                 ? audioModel_
                                  : (modelSelector == ModelSelector::Primary) ? primaryModel_
                                                                              : secondaryModel_;

    Logger::Debug(fmt::format("{}. Thread ID: {}", __func__, std::this_thread::get_id()));

    // Build JSON payload
    rapidjson::Document payload;
    payload.SetObject();
    auto& allocator = payload.GetAllocator();

    rapidjson::Value modelValue;
    modelValue.SetString(currentModel.name.c_str(), static_cast<rapidjson::SizeType>(currentModel.name.length()),
                         allocator);
    payload.AddMember("model", modelValue, allocator);

    if (returnAudio)
    {
        // ---- Audio path: Chat Completions format (/v1/chat/completions) ----
        payload.AddMember("stream", false, allocator);

        rapidjson::Value modalities(rapidjson::kArrayType);
        modalities.PushBack("text", allocator);
        modalities.PushBack("audio", allocator);
        payload.AddMember("modalities", modalities, allocator);

        rapidjson::Value audioConfig(rapidjson::kObjectType);
        audioConfig.AddMember("format", "wav", allocator);
        audioConfig.AddMember("voice", "ballad", allocator);
        payload.AddMember("audio", audioConfig, allocator);

        // Build messages array with token limiting
        rapidjson::Value messages(rapidjson::kArrayType);
        size_t totalTokenCount = 0;
        size_t textTokenCount = 0;

        auto cursor = history.rbegin();
        for (; cursor != history.rend(); ++cursor)
        {
            if (ShouldStopAddingHistory(totalTokenCount, cursor->second, &textTokenCount, currentModel))
            {
                if (cursor == history.rbegin())
                {
                    throw AiServiceException(
                        fmt::format("Message too large: {} tokens", totalTokenCount + textTokenCount));
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
            if (!visionImagesBase64.empty() && it == history.end() - 1)
            {
                continue;
            }

            rapidjson::Value message(rapidjson::kObjectType);
            rapidjson::Value role;
            role.SetString(it->first.c_str(), static_cast<rapidjson::SizeType>(it->first.length()), allocator);
            message.AddMember("role", role, allocator);

            rapidjson::Value content;
            content.SetString(it->second.c_str(), static_cast<rapidjson::SizeType>(it->second.length()), allocator);
            message.AddMember("content", content, allocator);

            messages.PushBack(message, allocator);
        }

        // Add images message if present (Chat Completions vision format)
        if (!visionImagesBase64.empty())
        {
            rapidjson::Value message(rapidjson::kObjectType);
            message.AddMember("role", "user", allocator);

            rapidjson::Value content(rapidjson::kArrayType);

            // Add all images
            for (const auto& imageBase64 : visionImagesBase64)
            {
                rapidjson::Value imageContent(rapidjson::kObjectType);
                imageContent.AddMember("type", "image_url", allocator);

                rapidjson::Value imageUrl(rapidjson::kObjectType);
                std::string imageDataUrl = "data:image/jpeg;base64," + imageBase64;
                rapidjson::Value urlValue;
                urlValue.SetString(imageDataUrl.c_str(), static_cast<rapidjson::SizeType>(imageDataUrl.length()),
                                   allocator);
                imageUrl.AddMember("url", urlValue, allocator);
                imageUrl.AddMember("detail", "low", allocator);
                imageContent.AddMember("image_url", imageUrl, allocator);
                content.PushBack(imageContent, allocator);
            }

            // Text content (last message)
            if (!history.empty())
            {
                rapidjson::Value textContent(rapidjson::kObjectType);
                textContent.AddMember("type", "text", allocator);
                const std::string& textMsg = history.back().second;
                rapidjson::Value textValue;
                textValue.SetString(textMsg.c_str(), static_cast<rapidjson::SizeType>(textMsg.length()), allocator);
                textContent.AddMember("text", textValue, allocator);
                content.PushBack(textContent, allocator);
            }

            message.AddMember("content", content, allocator);
            messages.PushBack(message, allocator);
        }

        payload.AddMember("messages", messages, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        payload.Accept(writer);

        std::string authHeader = fmt::format("Authorization: Bearer {}", currentModel.apiKey);

        Logger::Debug(fmt::format("OpenAI text-to-audio request to model: {}", currentModel.name));

        auto responseData = IAiService::PostJson(currentModel.url, authHeader, buffer.GetString());

        auto responseJson = ParseJsonResponse(responseData);

        if (!responseJson.HasMember("choices") || !responseJson["choices"].IsArray() ||
            responseJson["choices"].Size() == 0)
        {
            Logger::Debug(fmt::format("OpenAI text-to-audio: missing choices. Full response: {}", responseData));
            throw AiServiceException("Invalid response format: missing choices");
        }

        const auto& message0 = responseJson["choices"][0]["message"];
        if (!message0.HasMember("audio") || !message0["audio"].HasMember("data"))
        {
            Logger::Debug(fmt::format("OpenAI text-to-audio: missing audio data. Full response: {}", responseData));
            throw AiServiceException("Invalid response format: missing audio data");
        }

        std::string audioBase64 = message0["audio"]["data"].GetString();

        AiResponse response;
        response.media.push_back({MediaItem::Type::AudioBase64, audioBase64, "audio/wav"});

        if (message0["audio"].HasMember("transcript"))
        {
            response.text = message0["audio"]["transcript"].GetString();
        }
        return response;
    }

    // ---- Text path: Responses API format (/v1/responses) ----
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
        const char* contentType = (it->first == "assistant") ? "output_text" : "input_text";
        textContent.AddMember("type", rapidjson::Value(contentType, allocator), allocator);
        rapidjson::Value textValue;
        textValue.SetString(it->second.c_str(), static_cast<rapidjson::SizeType>(it->second.length()), allocator);
        textContent.AddMember("text", textValue, allocator);
        content.PushBack(textContent, allocator);
        message.AddMember("content", content, allocator);

        input.PushBack(message, allocator);
    }

    // Add images message if present (OpenAI vision support)
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

    if (state.responseText.empty())
    {
        Logger::Error(fmt::format("OpenAI streaming completed but responseText is empty (model: {}, workerId: {})",
                                  currentModel.name,
                                  state.workerId.has_value() ? std::to_string(*state.workerId) : "none"));
    }
    else
    {
        Logger::Debug(fmt::format("OpenAI streaming completed: {} chars (model: {}, workerId: {})",
                                  state.responseText.size(), currentModel.name,
                                  state.workerId.has_value() ? std::to_string(*state.workerId) : "none"));
    }

    messageWorker_->FinalizeMessage(state.workerId);

    return AiResponse{.text = state.responseText, .media = {}, .textStreamed = true};
}

// =============================================================================
// POST https://api.openai.com/v1/chat/completions
// =============================================================================
// Request (STT - voice to text, streaming):
// {
//   "model": "gpt-4o-audio-preview",
//   "stream": true,
//   "messages": [{
//     "role": "user",
//     "content": [{"type": "input_audio", "input_audio": {"data": "<base64>", "format": "wav"}}]
//   }]
// }
// -----------------------------------------------------------------------------
// Request (TTS - voice to voice):
// {
//   "model": "gpt-4o-audio-preview",
//   "modalities": ["text", "audio"],
//   "audio": {"format": "wav", "voice": "ballad"},
//   "messages": [{
//     "role": "user",
//     "content": [{"type": "input_audio", "input_audio": {"data": "<base64>", "format": "wav"}}]
//   }]
// }
// -----------------------------------------------------------------------------
// Response (STT streaming SSE):
// data: {"choices": [{"delta": {"content": "Transcribed text"}, "finish_reason": null}]}
// data: {"choices": [{"delta": {}, "finish_reason": "stop"}]}   // stop marker
// data: [DONE]                                                  // skipped by SSE parser
// -----------------------------------------------------------------------------
// Response (TTS non-streaming):
// {
//   "choices": [{
//     "message": {
//       "audio": {"data": "<base64_wav>", "transcript": "Response text"}
//     }
//   }]
// }
// =============================================================================
AiResponse OpenAiService::GetResponseFromVoice(int64_t chatId,
                                               int32_t threadId,
                                               const std::string& audioBase64,
                                               bool returnAudio)
{
    Logger::Debug(fmt::format("{}. Thread ID: {}", __func__, std::this_thread::get_id()));

    // Build JSON payload
    rapidjson::Document payload;
    payload.SetObject();
    auto& allocator = payload.GetAllocator();

    rapidjson::Value modelValue;
    modelValue.SetString(audioModel_.name.c_str(), static_cast<rapidjson::SizeType>(audioModel_.name.length()),
                         allocator);
    payload.AddMember("model", modelValue, allocator);

    if (!returnAudio)
    {
        payload.AddMember("stream", true, allocator);
    }
    else
    {
        rapidjson::Value modalities(rapidjson::kArrayType);
        modalities.PushBack("text", allocator);
        modalities.PushBack("audio", allocator);
        payload.AddMember("modalities", modalities, allocator);

        rapidjson::Value audioFormat(rapidjson::kObjectType);
        audioFormat.AddMember("format", "wav", allocator);
        audioFormat.AddMember("voice", "ballad", allocator);
        payload.AddMember("audio", audioFormat, allocator);
    }

    // Build message with audio input
    rapidjson::Value messages(rapidjson::kArrayType);
    rapidjson::Value message(rapidjson::kObjectType);
    message.AddMember("role", "user", allocator);

    rapidjson::Value content(rapidjson::kArrayType);
    rapidjson::Value audioContent(rapidjson::kObjectType);
    audioContent.AddMember("type", "input_audio", allocator);

    rapidjson::Value inputAudio(rapidjson::kObjectType);
    rapidjson::Value audioData;
    audioData.SetString(audioBase64.c_str(), static_cast<rapidjson::SizeType>(audioBase64.length()), allocator);
    inputAudio.AddMember("data", audioData, allocator);
    inputAudio.AddMember("format", "wav", allocator);
    audioContent.AddMember("input_audio", inputAudio, allocator);

    content.PushBack(audioContent, allocator);
    message.AddMember("content", content, allocator);
    messages.PushBack(message, allocator);
    payload.AddMember("messages", messages, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    payload.Accept(writer);

    std::string authHeader = fmt::format("Authorization: Bearer {}", audioModel_.apiKey);

    AiResponse response;

    if (returnAudio)
    {
        auto responseData = IAiService::PostJson(audioModel_.url, authHeader, buffer.GetString());

        auto responseJson = ParseJsonResponse(responseData);

        if (!responseJson.HasMember("choices") || !responseJson["choices"].IsArray() ||
            responseJson["choices"].Size() == 0)
        {
            throw AiServiceException("Invalid response format: missing choices");
        }

        const auto& message0 = responseJson["choices"][0]["message"];
        if (!message0.HasMember("audio") || !message0["audio"].HasMember("data"))
        {
            throw AiServiceException("Invalid response format: missing audio data");
        }

        std::string audioDataStr = message0["audio"]["data"].GetString();
        response.media.push_back({MediaItem::Type::AudioBase64, audioDataStr, "audio/wav"});

        // Include transcript if available
        if (message0["audio"].HasMember("transcript"))
        {
            response.text = message0["audio"]["transcript"].GetString();
        }
    }
    else
    {
        StreamState state = {chatId, threadId, "", std::nullopt, "", "", {}, this};
        PostJsonStream(audioModel_.url, authHeader, buffer.GetString(), state);

        messageWorker_->FinalizeMessage(state.workerId);
        response.text = state.responseText;
        response.textStreamed = true;
    }

    return response;
}

// =============================================================================
// POST https://api.openai.com/v1/images/generations
// =============================================================================
// Request:
// {
//   "prompt": "A white cat",
//   "n": 3,
//   "size": "1024x1024"
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
AiResponse OpenAiService::GetImageResponse(int64_t /* chatId */,
                                           int32_t /* threadId */,
                                           const std::string& prompt,
                                           const std::vector<std::string>& referenceImagesBase64)
{
    if (!referenceImagesBase64.empty())
    {
        throw AiServiceException("Reference images are not supported by OpenAI");
    }

    // Build JSON payload
    rapidjson::Document payload;
    payload.SetObject();
    auto& allocator = payload.GetAllocator();

    rapidjson::Value promptValue;
    promptValue.SetString(prompt.c_str(), static_cast<rapidjson::SizeType>(prompt.length()), allocator);
    payload.AddMember("prompt", promptValue, allocator);
    payload.AddMember("n", kImageGenerationCount, allocator);
    payload.AddMember("size", "1024x1024", allocator);

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
                response.media.push_back({MediaItem::Type::ImageUrl, item["url"].GetString(), "image/png"});
            }
        }
    }

    return response;
}

} // namespace mbb
