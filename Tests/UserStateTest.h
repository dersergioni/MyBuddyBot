#ifndef USERSTATETEST_H
#define USERSTATETEST_H

#include "../bot/UserState.h"
#include "../infra/FileUtils.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

namespace mbb::tests
{

class UserStateTest : public testing::Test
{
  protected:
    UserStateTest() = default;
    ~UserStateTest() override = default;

    void TearDown() override
    {
        // Clean up any temp files
        if (!tempStatePath_.empty() && std::filesystem::exists(tempStatePath_))
        {
            std::filesystem::remove(tempStatePath_);
        }
    }

    static constexpr int64_t kTestChatId = 123456789;
    static constexpr int32_t kTestThreadId = 0;
    ChatKey testKey_{kTestChatId, kTestThreadId};
    std::filesystem::path tempStatePath_;
};

// =============================================================================
// Dialog Mode Tests
// =============================================================================

TEST_F(UserStateTest, DefaultDialogModeIsNone)
{
    UserState state;
    EXPECT_EQ(DialogMode::None, state.GetDialogMode(testKey_));
}

TEST_F(UserStateTest, SetDialogModeToImageGeneration)
{
    UserState state;
    state.SetDialogMode(testKey_, DialogMode::ImageGeneration);
    EXPECT_EQ(DialogMode::ImageGeneration, state.GetDialogMode(testKey_));
}

TEST_F(UserStateTest, SetDialogModeBackToNone)
{
    UserState state;
    state.SetDialogMode(testKey_, DialogMode::ImageGeneration);
    state.SetDialogMode(testKey_, DialogMode::None);
    EXPECT_EQ(DialogMode::None, state.GetDialogMode(testKey_));
}

// =============================================================================
// Audio Response Tests
// =============================================================================

TEST_F(UserStateTest, DefaultAudioResponseIsFalse)
{
    UserState state;
    EXPECT_FALSE(state.GetAudioResponse(testKey_));
}

TEST_F(UserStateTest, SetAudioResponseTrue)
{
    UserState state;
    state.SetAudioResponse(testKey_, true);
    EXPECT_TRUE(state.GetAudioResponse(testKey_));
}

TEST_F(UserStateTest, ToggleAudioResponse)
{
    UserState state;
    EXPECT_FALSE(state.GetAudioResponse(testKey_));

    state.ToggleAudioResponse(testKey_);
    EXPECT_TRUE(state.GetAudioResponse(testKey_));

    state.ToggleAudioResponse(testKey_);
    EXPECT_FALSE(state.GetAudioResponse(testKey_));
}

// =============================================================================
// AI Provider Tests
// =============================================================================

TEST_F(UserStateTest, DefaultAiProviderIsOpenAI)
{
    const char* envDbPath = getenv("MYBUDDYBOT_DB_PATH");
    const char* envBotToken = getenv("TG_API_TOKEN");
    const char* envOpenAiToken = getenv("OAI_API_TOKEN");
    const char* envDefaultProvider = getenv("MYBUDDYBOT_DEFAULT_PROVIDER");

#if defined(_WIN32)
    _putenv_s("MYBUDDYBOT_DB_PATH", "C:\\DB\\MyBuddy.db");
    _putenv_s("TG_API_TOKEN", "1234567890:ABC");
    _putenv_s("OAI_API_TOKEN", "OPENAI123");
    _putenv_s("MYBUDDYBOT_DEFAULT_PROVIDER", "openai");
#else
    setenv("MYBUDDYBOT_DB_PATH", "/tmp/mybuddy.db", 1);
    setenv("TG_API_TOKEN", "1234567890:ABC", 1);
    setenv("OAI_API_TOKEN", "OPENAI123", 1);
    setenv("MYBUDDYBOT_DEFAULT_PROVIDER", "openai", 1);
#endif

    Config::SetTestMode(true);
    Config::Init();

    UserState state;
    EXPECT_EQ(AiProvider::OpenAI, state.GetAiProvider(testKey_));

#if defined(_WIN32)
    if (envDbPath != nullptr)
    {
        _putenv_s("MYBUDDYBOT_DB_PATH", envDbPath);
    }
    else
    {
        _putenv_s("MYBUDDYBOT_DB_PATH", "");
    }
    if (envBotToken != nullptr)
    {
        _putenv_s("TG_API_TOKEN", envBotToken);
    }
    else
    {
        _putenv_s("TG_API_TOKEN", "");
    }
    if (envOpenAiToken != nullptr)
    {
        _putenv_s("OAI_API_TOKEN", envOpenAiToken);
    }
    else
    {
        _putenv_s("OAI_API_TOKEN", "");
    }
    if (envDefaultProvider != nullptr)
    {
        _putenv_s("MYBUDDYBOT_DEFAULT_PROVIDER", envDefaultProvider);
    }
    else
    {
        _putenv_s("MYBUDDYBOT_DEFAULT_PROVIDER", "");
    }
#else
    if (envDbPath != nullptr)
    {
        setenv("MYBUDDYBOT_DB_PATH", envDbPath, 1);
    }
    else
    {
        unsetenv("MYBUDDYBOT_DB_PATH");
    }
    if (envBotToken != nullptr)
    {
        setenv("TG_API_TOKEN", envBotToken, 1);
    }
    else
    {
        unsetenv("TG_API_TOKEN");
    }
    if (envOpenAiToken != nullptr)
    {
        setenv("OAI_API_TOKEN", envOpenAiToken, 1);
    }
    else
    {
        unsetenv("OAI_API_TOKEN");
    }
    if (envDefaultProvider != nullptr)
    {
        setenv("MYBUDDYBOT_DEFAULT_PROVIDER", envDefaultProvider, 1);
    }
    else
    {
        unsetenv("MYBUDDYBOT_DEFAULT_PROVIDER");
    }
#endif
}

TEST_F(UserStateTest, DefaultAiProviderUsesConfig)
{
    const char* envDbPath = getenv("MYBUDDYBOT_DB_PATH");
    const char* envBotToken = getenv("TG_API_TOKEN");
    const char* envGoogleToken = getenv("GOOGLE_API_TOKEN");
    const char* envDefaultProvider = getenv("MYBUDDYBOT_DEFAULT_PROVIDER");
    const char* envOpenAiToken = getenv("OAI_API_TOKEN");

#if defined(_WIN32)
    _putenv_s("MYBUDDYBOT_DB_PATH", "C:\\DB\\MyBuddy.db");
    _putenv_s("TG_API_TOKEN", "1234567890:ABC");
    _putenv_s("GOOGLE_API_TOKEN", "GOOG123");
    _putenv_s("MYBUDDYBOT_DEFAULT_PROVIDER", "google");
#else
    setenv("MYBUDDYBOT_DB_PATH", "/tmp/mybuddy.db", 1);
    setenv("TG_API_TOKEN", "1234567890:ABC", 1);
    setenv("GOOGLE_API_TOKEN", "GOOG123", 1);
    setenv("MYBUDDYBOT_DEFAULT_PROVIDER", "google", 1);
#endif

    Config::SetTestMode(true);
    Config::Init();

    UserState state;
    EXPECT_EQ(AiProvider::Google, state.GetAiProvider(testKey_));

#if defined(_WIN32)
    _putenv_s("MYBUDDYBOT_DB_PATH", "C:\\DB\\MyBuddy.db");
    _putenv_s("TG_API_TOKEN", "1234567890:ABC");
    _putenv_s("OAI_API_TOKEN", "OPENAI123");
    _putenv_s("MYBUDDYBOT_DEFAULT_PROVIDER", "openai");
#else
    setenv("MYBUDDYBOT_DB_PATH", "/tmp/mybuddy.db", 1);
    setenv("TG_API_TOKEN", "1234567890:ABC", 1);
    setenv("OAI_API_TOKEN", "OPENAI123", 1);
    setenv("MYBUDDYBOT_DEFAULT_PROVIDER", "openai", 1);
#endif

    Config::Init();

#if defined(_WIN32)
    if (envDbPath != nullptr)
    {
        _putenv_s("MYBUDDYBOT_DB_PATH", envDbPath);
    }
    else
    {
        _putenv_s("MYBUDDYBOT_DB_PATH", "");
    }
    if (envBotToken != nullptr)
    {
        _putenv_s("TG_API_TOKEN", envBotToken);
    }
    else
    {
        _putenv_s("TG_API_TOKEN", "");
    }
    if (envGoogleToken != nullptr)
    {
        _putenv_s("GOOGLE_API_TOKEN", envGoogleToken);
    }
    else
    {
        _putenv_s("GOOGLE_API_TOKEN", "");
    }
    if (envDefaultProvider != nullptr)
    {
        _putenv_s("MYBUDDYBOT_DEFAULT_PROVIDER", envDefaultProvider);
    }
    else
    {
        _putenv_s("MYBUDDYBOT_DEFAULT_PROVIDER", "");
    }
    if (envOpenAiToken != nullptr)
    {
        _putenv_s("OAI_API_TOKEN", envOpenAiToken);
    }
    else
    {
        _putenv_s("OAI_API_TOKEN", "");
    }
#else
    if (envDbPath != nullptr)
    {
        setenv("MYBUDDYBOT_DB_PATH", envDbPath, 1);
    }
    else
    {
        unsetenv("MYBUDDYBOT_DB_PATH");
    }
    if (envBotToken != nullptr)
    {
        setenv("TG_API_TOKEN", envBotToken, 1);
    }
    else
    {
        unsetenv("TG_API_TOKEN");
    }
    if (envGoogleToken != nullptr)
    {
        setenv("GOOGLE_API_TOKEN", envGoogleToken, 1);
    }
    else
    {
        unsetenv("GOOGLE_API_TOKEN");
    }
    if (envDefaultProvider != nullptr)
    {
        setenv("MYBUDDYBOT_DEFAULT_PROVIDER", envDefaultProvider, 1);
    }
    else
    {
        unsetenv("MYBUDDYBOT_DEFAULT_PROVIDER");
    }
    if (envOpenAiToken != nullptr)
    {
        setenv("OAI_API_TOKEN", envOpenAiToken, 1);
    }
    else
    {
        unsetenv("OAI_API_TOKEN");
    }
#endif
}

TEST_F(UserStateTest, SetAiProviderToXAI)
{
    UserState state;
    state.SetAiProvider(testKey_, AiProvider::XAI);
    EXPECT_EQ(AiProvider::XAI, state.GetAiProvider(testKey_));
}

TEST_F(UserStateTest, SetAiProviderToGoogle)
{
    UserState state;
    state.SetAiProvider(testKey_, AiProvider::Google);
    EXPECT_EQ(AiProvider::Google, state.GetAiProvider(testKey_));
}

TEST_F(UserStateTest, ToggleAiProviderCyclesThroughAll)
{
    UserState state;

    // Default is OpenAI
    EXPECT_EQ(AiProvider::OpenAI, state.GetAiProvider(testKey_));

    // OpenAI -> XAI
    state.ToggleAiProvider(testKey_);
    EXPECT_EQ(AiProvider::XAI, state.GetAiProvider(testKey_));

    // XAI -> Google
    state.ToggleAiProvider(testKey_);
    EXPECT_EQ(AiProvider::Google, state.GetAiProvider(testKey_));

    // Google -> OpenAI (cycle back)
    state.ToggleAiProvider(testKey_);
    EXPECT_EQ(AiProvider::OpenAI, state.GetAiProvider(testKey_));
}

// =============================================================================
// Pending Images Tests
// =============================================================================

TEST_F(UserStateTest, NoPendingImagesByDefault)
{
    UserState state;
    auto images = state.TakePendingImages(testKey_);
    EXPECT_TRUE(images.empty());
}

TEST_F(UserStateTest, AddAndTakePendingImages)
{
    UserState state;
    state.AddPendingImage(testKey_, "base64_image_1");
    state.AddPendingImage(testKey_, "base64_image_2");

    auto images = state.TakePendingImages(testKey_);
    EXPECT_EQ(2u, images.size());
    EXPECT_EQ("base64_image_1", images[0]);
    EXPECT_EQ("base64_image_2", images[1]);

    // After taking, should be empty
    auto imagesAfter = state.TakePendingImages(testKey_);
    EXPECT_TRUE(imagesAfter.empty());
}

TEST_F(UserStateTest, ClearPendingImages)
{
    UserState state;
    state.AddPendingImage(testKey_, "base64_image_1");
    state.ClearPendingImages(testKey_);

    auto images = state.TakePendingImages(testKey_);
    EXPECT_TRUE(images.empty());
}

// =============================================================================
// Media Group Tests
// =============================================================================

TEST_F(UserStateTest, MediaGroupNotProcessedByDefault)
{
    UserState state;
    EXPECT_FALSE(state.IsFromLastProcessedMediaGroup(testKey_, "group_123"));
}

TEST_F(UserStateTest, SetAndCheckMediaGroup)
{
    UserState state;
    state.SetLastProcessedMediaGroupId(testKey_, "group_123");

    EXPECT_TRUE(state.IsFromLastProcessedMediaGroup(testKey_, "group_123"));
    EXPECT_FALSE(state.IsFromLastProcessedMediaGroup(testKey_, "group_456"));
}

TEST_F(UserStateTest, MediaGroupOverwritten)
{
    UserState state;
    state.SetLastProcessedMediaGroupId(testKey_, "group_123");
    state.SetLastProcessedMediaGroupId(testKey_, "group_456");

    EXPECT_FALSE(state.IsFromLastProcessedMediaGroup(testKey_, "group_123"));
    EXPECT_TRUE(state.IsFromLastProcessedMediaGroup(testKey_, "group_456"));
}

// =============================================================================
// Clear Tests
// =============================================================================

TEST_F(UserStateTest, ClearResetsDialogModeAndAudio)
{
    UserState state;
    state.SetDialogMode(testKey_, DialogMode::ImageGeneration);
    state.SetAudioResponse(testKey_, true);
    state.AddPendingImage(testKey_, "image");
    state.SetLastProcessedMediaGroupId(testKey_, "group");

    state.Clear(testKey_);

    EXPECT_EQ(DialogMode::None, state.GetDialogMode(testKey_));
    EXPECT_FALSE(state.GetAudioResponse(testKey_));
    EXPECT_TRUE(state.TakePendingImages(testKey_).empty());
    EXPECT_FALSE(state.IsFromLastProcessedMediaGroup(testKey_, "group"));
}

TEST_F(UserStateTest, ClearKeepsAiProvider)
{
    UserState state;
    state.SetAiProvider(testKey_, AiProvider::Google);

    state.Clear(testKey_);

    // AI provider should be preserved after clear
    EXPECT_EQ(AiProvider::Google, state.GetAiProvider(testKey_));
}

// =============================================================================
// Isolation Tests (different chats don't affect each other)
// =============================================================================

TEST_F(UserStateTest, DifferentChatsAreIsolated)
{
    UserState state;
    ChatKey chat1{100, 0};
    ChatKey chat2{200, 0};

    state.SetDialogMode(chat1, DialogMode::ImageGeneration);
    state.SetAiProvider(chat1, AiProvider::XAI);
    state.SetAudioResponse(chat1, true);

    // chat2 should have defaults
    EXPECT_EQ(DialogMode::None, state.GetDialogMode(chat2));
    EXPECT_EQ(AiProvider::OpenAI, state.GetAiProvider(chat2));
    EXPECT_FALSE(state.GetAudioResponse(chat2));
}

TEST_F(UserStateTest, DifferentThreadsAreIsolated)
{
    UserState state;
    ChatKey thread1{100, 1};
    ChatKey thread2{100, 2};

    state.SetDialogMode(thread1, DialogMode::ImageGeneration);

    EXPECT_EQ(DialogMode::ImageGeneration, state.GetDialogMode(thread1));
    EXPECT_EQ(DialogMode::None, state.GetDialogMode(thread2));
}

// =============================================================================
// Persistence Tests
// =============================================================================

TEST_F(UserStateTest, SaveAndLoadState)
{
    tempStatePath_ = FileUtils::GetTempFilePath(".state");

    // Create state and save
    {
        UserState state;
        state.SetAiProvider(testKey_, AiProvider::Google);
        state.SetAudioResponse(testKey_, true);
        state.SaveToFile(tempStatePath_);
    }

    // Load into new state
    {
        UserState state;
        state.LoadFromFile(tempStatePath_);

        EXPECT_EQ(AiProvider::Google, state.GetAiProvider(testKey_));
        EXPECT_TRUE(state.GetAudioResponse(testKey_));
    }
}

TEST_F(UserStateTest, LoadFromNonexistentFileStartsFresh)
{
    UserState state;
    state.LoadFromFile("/nonexistent/path/state.bin");

    // Should have defaults
    EXPECT_EQ(AiProvider::OpenAI, state.GetAiProvider(testKey_));
    EXPECT_FALSE(state.GetAudioResponse(testKey_));
}

} // namespace mbb::tests

#endif // USERSTATETEST_H
