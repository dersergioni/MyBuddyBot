#pragma once

#include "ai/IAiService.h"

namespace mbb
{

class IMessageWorker;

class GoogleService : public IAiService
{
  public:
    explicit GoogleService(std::shared_ptr<IMessageWorker> messageWorker);
    ~GoogleService() override = default;

    // Non-copyable, non-movable
    GoogleService(const GoogleService&) = delete;
    GoogleService& operator=(const GoogleService&) = delete;

    // -------------------------------------------------------------------------
    // IAiService implementation
    // -------------------------------------------------------------------------
    [[nodiscard]] AiResponse GetTextResponse(int64_t chatId,
                                             int32_t threadId,
                                             const std::vector<std::pair<std::string, std::string>>& history,
                                             const std::vector<std::string>& visionImagesBase64,
                                             ModelSelector model,
                                             bool returnAudio) override;

    [[nodiscard]] AiResponse GetResponseFromVoice(int64_t chatId,
                                                  int32_t threadId,
                                                  const std::string& audioBase64,
                                                  bool returnAudio) override;

    [[nodiscard]] AiResponse GetImageResponse(int64_t chatId,
                                              int32_t threadId,
                                              const std::string& prompt,
                                              const std::vector<std::string>& referenceImagesBase64) override;

  protected:
    [[nodiscard]] bool ExtractStreamContent(const std::string& jsonChunk, StreamState& state) const override;
};

} // namespace mbb
