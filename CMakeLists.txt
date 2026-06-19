cmake_minimum_required(VERSION 3.30)

project(MyBuddyBot VERSION 2.0.0 LANGUAGES C CXX)

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

# The SQLite3 imported-target name changed across CMake versions: older
# FindSQLite3 defines only SQLite::SQLite3, while newer CMake defines
# SQLite3::SQLite3 and deprecates SQLite::SQLite3. Prefer the modern name when
# available, falling back to the legacy one so older toolchains still build.
if (TARGET SQLite3::SQLite3)
    set(MBB_SQLITE3_TARGET SQLite3::SQLite3)
else()
    set(MBB_SQLITE3_TARGET SQLite::SQLite3)
endif()

if (BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
endif()

# =============================================================================
# Rust modules via Corrosion
# =============================================================================
include(FetchContent)
FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG        v0.5.1
)
FetchContent_MakeAvailable(Corrosion)
corrosion_import_crate(MANIFEST_PATH modules/wishlist/telegram/Cargo.toml)
corrosion_add_cxxbridge(wishlist_cxx CRATE wishlist_telegram FILES lib.rs)
target_include_directories(wishlist_cxx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# On Windows the C++ side links vcpkg's SQLite3, while rusqlite/libsqlite3-sys
# would otherwise bundle a second copy and collide at link time (LNK2005).
# Point libsqlite3-sys at the same SQLite3 that find_package(SQLite3) located so
# the whole binary shares a single SQLite3. Paired with the Windows-only
# (non-bundled) rusqlite dependency in modules/wishlist/sqlite/Cargo.toml.
if (WIN32)
    get_filename_component(_sqlite3_lib_dir "${SQLite3_LIBRARY}" DIRECTORY)
    corrosion_set_env_vars(wishlist_telegram
        "SQLITE3_LIB_DIR=${_sqlite3_lib_dir}"
        "SQLITE3_INCLUDE_DIR=${SQLite3_INCLUDE_DIR}"
    )
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
    core/AiConfig.cpp
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
    infra/ResponseSaver.cpp
    infra/TaskQueue.cpp
    infra/TelegramHtmlFormatter.cpp

    # telegram
    telegram/TelegramApi.cpp
    telegram/MediaDownloader.cpp
    telegram/MessageSplitter.cpp
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

    # modules
    modules/services/TelegramOps.cpp
    modules/framework/ModuleHost.cpp
    modules/services/ModuleOps.cpp
    modules/wishlist/bridge/WishlistBridge.cpp

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
    ${MBB_SQLITE3_TARGET}
    fmt::fmt
    Boost::headers
    wishlist_cxx
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
