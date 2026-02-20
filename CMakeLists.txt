cmake_minimum_required(VERSION 3.30)

project(MyBuddyBot VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MYBUDDYBOT_GIT_COMMIT "unknown")
find_package(Git QUIET)
if (GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short=8 HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE MYBUDDYBOT_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if (MYBUDDYBOT_GIT_COMMIT STREQUAL "")
        set(MYBUDDYBOT_GIT_COMMIT "unknown")
    endif()

endif()

# =============================================================================
# Options
# =============================================================================
option(BUILD_TESTS "Build test hooks and link GTest into the main executable" ON)
option(ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)

# =============================================================================
# Static analysis
# =============================================================================
if (ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if (CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy was not found")
    endif()
endif()

# =============================================================================
# Dependencies
# =============================================================================
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(fmt REQUIRED)
find_package(RapidJSON REQUIRED)

if (BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
endif()

# TgBot library (must be pre-built)
if (NOT DEFINED TGBOT_LIBRARY_LOCATION)
    message(FATAL_ERROR
        "TGBOT_LIBRARY_LOCATION is not defined.\n"
        "Please set it to the TgBot installation prefix:\n"
        "  cmake -DTGBOT_LIBRARY_LOCATION=/path/to/tgbot ..."
    )
endif()

add_library(TgBot STATIC IMPORTED)
if (WIN32)
    set_target_properties(TgBot PROPERTIES
        IMPORTED_LOCATION_DEBUG "${TGBOT_LIBRARY_LOCATION}/debug/lib/TgBot.lib"
        IMPORTED_LOCATION_RELEASE "${TGBOT_LIBRARY_LOCATION}/release/lib/TgBot.lib"
    )
else()
    # Linux / macOS
    set_target_properties(TgBot PROPERTIES
        IMPORTED_LOCATION "${TGBOT_LIBRARY_LOCATION}/lib/libTgBot.a"
    )
endif()
target_include_directories(TgBot INTERFACE "${TGBOT_LIBRARY_LOCATION}/include")

# =============================================================================
# Sources
# =============================================================================
set(PROJECT_SOURCES
    main.cpp

    # core
    core/Config.cpp
    core/Logger.cpp
    core/Storage.cpp

    # infra
    infra/Base64.cpp
    infra/Env.cpp
    infra/HttpClient.cpp
    infra/FileUtils.cpp
    infra/StringUtils.cpp
    infra/AudioConverter.cpp
    infra/TaskQueue.cpp

    # telegram
    telegram/TelegramApi.cpp
    telegram/MediaDownloader.cpp
    telegram/MessageWorker.cpp

    # ai
    ai/IAiService.cpp
    ai/OpenAiService.cpp
    ai/XAiService.cpp
    ai/GoogleService.cpp

    # bot
    bot/UserState.cpp
    bot/CommandHandlers.cpp
    bot/BotApp.cpp

)

set(EXTERNAL_SOURCES
    ext/cpp-base64/base64.cpp
    ext/md4c/md4c.c
    ext/md4c/md4c-html.c
    ext/md4c/entity.c
)

# Suppress warnings for external sources
set_source_files_properties(${EXTERNAL_SOURCES} PROPERTIES
    COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>"
)

# =============================================================================
# Executable
# =============================================================================
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${EXTERNAL_SOURCES})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    HAVE_CURL
    MYBUDDYBOT_VERSION=\"${PROJECT_VERSION}\"
    MYBUDDYBOT_GIT_COMMIT=\"${MYBUDDYBOT_GIT_COMMIT}\"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    # External libraries need their directories for internal includes
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/cpp-base64
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/md4c
)

# Compiler warnings
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>: /W3 /D_CRT_SECURE_NO_WARNINGS>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>: -Wall -Wextra>
)

# Debug configuration: full debug info, no optimization
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>: /Zi /Od /RTC1>
    $<$<AND:$<CONFIG:Debug>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>: -g3 -O0 -fno-omit-frame-pointer>
)

# Release configuration: optimize
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>: /O2>
    $<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>: -O2>
)

# =============================================================================
# Linking
# =============================================================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    TgBot
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    SQLite::SQLite3
    fmt::fmt
    Boost::headers
)

# Platform-specific libraries
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 crypt32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
    )
endif()

# =============================================================================
# Tests
# =============================================================================
if (BUILD_TESTS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_TESTS)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        GTest::gtest_main
    )
    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}
        DISCOVERY_TIMEOUT 30
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# =============================================================================
# Install (optional)
# =============================================================================
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
