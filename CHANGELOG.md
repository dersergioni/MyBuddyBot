# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

- No changes yet.

## [2.0.0] - 2026-06-19

### Added
- Module system: the bot now supports pluggable feature modules. Modules are registered at startup, expose Telegram commands and callbacks, and interact with the host via a dedicated C++ bridge. Modules are enabled by default and can be disabled via `MYBUDDYBOT_MODULE_<NAME>=0`.
- Wishlist module: the first feature module, built on the new module system. Supports private and public wish lists, wish reservations, and family sharing with an invite/accept/decline/leave flow. Owners are notified when a family member reserves or un-reserves a wish. Backed by a dedicated SQLite database, path configurable via `MYBUDDYBOT_MODULE_WISHLIST_DB`.
- Admin `/broadcast` command — an admin can broadcast a message to every chat the bot is active in (private chats and group topics), behind a preview with Confirm/Cancel and a final delivery summary. Broadcasts are framed as an admin announcement.
- AI provider quota/billing alerts — when a provider's quota or credits are exhausted (OpenAI, xAI, Google Gemini), the user gets a clear message instead of an empty reply, and the bot owner(s) are notified.

### Changed
- **Breaking:** AI provider models are now defined in a required JSON config file (path via `MYBUDDYBOT_AI_CONFIG_PATH`) instead of being hardcoded. Each provider's `primary`/`secondary`/`image`/`audio` model (name, URL, context size) comes from the file; API keys resolve from the environment first, then the file; a provider is enabled only when it has a key and a complete `primary` model. Existing deployments must add an `ai-config.json` (copy `ai-config.example.json`) before upgrading.

### Fixed
- Streaming AI replies no longer fail silently: a stream that ends with no content or a provider error now surfaces a clear error to the user instead of an empty message.

## [1.2.0] - 2026-03-14

### Added
- Whitelist-based HTML sanitizer ensuring only Telegram-supported tags reach the API
- Code-block-aware message splitting with automatic fence continuation across message boundaries
- Per-block rate limiting for message edits (min 1.1s between updates, max 20 per 66s window)

### Changed
- Decomposed monolithic `MessageWorker` into `MessageSplitter` and `TelegramHtmlFormatter` components
- Message splitting now uses actual HTML size instead of raw text heuristics
- Nested blockquotes are flattened (Telegram forbids nesting)

### Fixed
- Messages with heavy formatting (bold tables, code blocks) no longer fall back to plain text
- Long code blocks split across messages preserve syntax highlighting

## [1.1.0] - 2026-03-10

### Added
- Response viewer for AI responses with LaTeX formulas, syntax-highlighted code, and formatted tables
- LaTeX detection in AI responses (formulas, environments, math commands) with fenced code block awareness
- Inline URL button to open rendered view in Telegram's built-in browser
- `MYBUDDYBOT_VIEWER_URL` and `MYBUDDYBOT_VIEWER_DIR` configuration options

## [1.0.0] - 2026-02-19

### Added
- First public release of MyBuddyBot.
