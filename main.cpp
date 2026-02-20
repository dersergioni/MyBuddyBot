#ifdef BUILD_TESTS
#include "Tests/SmokeTestSuite.h"

#include <gtest/gtest.h>

#include <string_view>
#endif

#include "bot/BotApp.h"

#ifdef BUILD_TESTS
namespace
{
bool ShouldRunTests(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg{argv[i]};
        if (arg == "--test" || arg.starts_with("--gtest_"))
        {
            return true;
        }
    }
    return false;
}
} // namespace
#endif

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef BUILD_TESTS
    // Run tests
    if (ShouldRunTests(argc, argv))
    {
        testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
#endif

    // Run app
    mbb::BotApp app;
    return app.Run();
}
