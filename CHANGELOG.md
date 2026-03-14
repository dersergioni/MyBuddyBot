# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

- No changes yet.

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
