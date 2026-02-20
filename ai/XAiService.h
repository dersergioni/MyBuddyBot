#pragma once

#include "ai/IAiService.h"

namespace mbb
{

class IMessageWorker;

class XAiService : public IAiService
{
  public:
    explicit XAiService(std::shared_ptr<IMessageWorker> messageWorker);
    ~XAiService() override = default;

    // Non-copyable, non-movable
    XAiService(const XAiService&) = delete;
    XAiService& operator=(const XAiService&) = delete;

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

    [[nodiscard]] const std::string& GetModelName(ModelSelector selector) const override;

  protected:
    [[nodiscard]] bool ExtractStreamContent(const std::string& jsonChunk, StreamState& state) const override;
};

} // namespace mbb
