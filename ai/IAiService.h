#pragma once

#include <rapidjson/document.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mbb
{

class IMessageWorker;

// =============================================================================
// Exceptions
// =============================================================================

class AiServiceException : public std::runtime_error
{
  public:
    explicit AiServiceException(const std::string& message) : std::runtime_error(message)
    {
    }
};

// =============================================================================
// Enums and Types
// =============================================================================

enum class ModelSelector
{
    Primary,
    Secondary,
    Image,
    Audio
};

struct AiModel
{
    std::string name;
    size_t contextSize = 0;
    std::string apiKey;
    std::string url;
};

// =============================================================================
// Unified AI Response
// =============================================================================

struct MediaItem
{
    enum class Type
    {
        ImageUrl,    // URL to download image from
        ImageBase64, // Base64-encoded image data
        AudioBase64  // Base64-encoded WAV audio data
    };

    Type type;
    std::string data;     // URL or base64 data
    std::string mimeType; // e.g., "image/png", "audio/wav"
};

struct AiResponse
{
    // Text content (transcript for voice, or LLM response)
    std::string text;

    // Media items (images, audio)
    std::vector<MediaItem> media;

    // If true, text was already streamed via MessageWorker
    // CommandHandlers should NOT send it again
    bool textStreamed = false;

    // Helper methods
    [[nodiscard]] bool HasAudio() const
    {
        for (const auto& item : media)
        {
            if (item.type == MediaItem::Type::AudioBase64)
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool HasImages() const
    {
        for (const auto& item : media)
        {
            if (item.type == MediaItem::Type::ImageUrl || item.type == MediaItem::Type::ImageBase64)
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::vector<MediaItem> GetAudioItems() const
    {
        std::vector<MediaItem> result;
        for (const auto& item : media)
        {
            if (item.type == MediaItem::Type::AudioBase64)
            {
                result.push_back(item);
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<MediaItem> GetImageItems() const
    {
        std::vector<MediaItem> result;
        for (const auto& item : media)
        {
            if (item.type == MediaItem::Type::ImageUrl || item.type == MediaItem::Type::ImageBase64)
            {
                result.push_back(item);
            }
        }
        return result;
    }
};

// =============================================================================
// IAiService Interface
// =============================================================================

class IAiService
{
  public:
    virtual ~IAiService() = default;

    // Non-copyable
    IAiService(const IAiService&) = delete;
    IAiService& operator=(const IAiService&) = delete;

    IAiService(IAiService&&) = delete;
    IAiService& operator=(IAiService&&) = delete;

    // -------------------------------------------------------------------------
    // Text Response
    // -------------------------------------------------------------------------
    [[nodiscard]] virtual AiResponse GetTextResponse(int64_t chatId,
                                                     int32_t threadId,
                                                     const std::vector<std::pair<std::string, std::string>>& history,
                                                     const std::vector<std::string>& visionImagesBase64 = {},
                                                     ModelSelector model = ModelSelector::Primary,
                                                     bool returnAudio = false) = 0;

    // -------------------------------------------------------------------------
    // Audio/Voice Response
    // -------------------------------------------------------------------------
    [[nodiscard]] virtual AiResponse GetResponseFromVoice(int64_t chatId,
                                                          int32_t threadId,
                                                          const std::string& audioBase64,
                                                          bool returnAudio = false) = 0;

    // -------------------------------------------------------------------------
    // Image Generation
    // -------------------------------------------------------------------------
    [[nodiscard]] virtual AiResponse GetImageResponse(int64_t chatId,
                                                      int32_t threadId,
                                                      const std::string& prompt,
                                                      const std::vector<std::string>& referenceImagesBase64 = {}) = 0;

    // -------------------------------------------------------------------------
    // Model Info
    // -------------------------------------------------------------------------
    [[nodiscard]] virtual const std::string& GetModelName(ModelSelector selector = ModelSelector::Primary) const;

  protected:
    explicit IAiService(std::shared_ptr<IMessageWorker> messageWorker);

    // Streaming state for implementations
    struct StreamState
    {
        int64_t chatId = 0;
        int32_t threadId = 0;
        std::string responseText;
        std::optional<uint32_t> workerId;
        std::string incomingChunk;
        std::string remainingChunk;
        std::vector<char> binaryData;
        const IAiService* service = nullptr; // For virtual dispatch from static callbacks
    };

    // -------------------------------------------------------------------------
    // Common CURL callbacks
    // -------------------------------------------------------------------------
    // Simple callback: appends data to a std::string*
    static size_t SimpleCallback(void* contents, size_t size, size_t nmemb, void* userp);

    // SSE streaming callback: parses SSE events and dispatches to ExtractStreamContent
    static size_t TextStreamCallback(void* contents, size_t size, size_t nmemb, void* userp);

    // -------------------------------------------------------------------------
    // Virtual method for stream content extraction
    // -------------------------------------------------------------------------
    // Each service overrides this to extract text from its specific JSON chunk format.
    // Returns true when the stream is finished (e.g., finish_reason == "stop").
    [[nodiscard]] virtual bool ExtractStreamContent(const std::string& jsonChunk, StreamState& state) const = 0;

    // -------------------------------------------------------------------------
    // Common helpers
    // -------------------------------------------------------------------------
    // CURL POST helpers
    [[nodiscard]] std::string PostJson(const std::string& url,
                                       const std::string& authHeader,
                                       const std::string& jsonBody) const;

    void PostJsonStream(const std::string& url,
                        const std::string& authHeader,
                        const std::string& jsonBody,
                        StreamState& state) const;

    // JSON response parsing with error checking.
    // Parses JSON, throws AiServiceException on parse error or if "error" member present.
    [[nodiscard]] static rapidjson::Document ParseJsonResponse(const std::string& responseData);

    // Helper for token counting (shared by implementations)
    [[nodiscard]] virtual bool ShouldStopAddingHistory(size_t totalTokenCount,
                                                       const std::string& text,
                                                       size_t* textTokenCount,
                                                       const AiModel& model) const;

    static constexpr int kImageGenerationCount = 3; // Number of images to generate per request

    std::shared_ptr<IMessageWorker> messageWorker_;
    AiModel primaryModel_;
    AiModel secondaryModel_;
    AiModel imageModel_;
    AiModel audioModel_;
};

} // namespace mbb
