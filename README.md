# MyBuddyBot

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.30%2B-blue.svg)](https://cmake.org/)
[![Build and Test](https://github.com/dersergioni/MyBuddyBot/actions/workflows/build.yml/badge.svg)](https://github.com/dersergioni/MyBuddyBot/actions/workflows/build.yml)

A C++ Telegram bot with multi-provider AI integration (OpenAI, xAI/Grok, Google Gemini) for AI-powered conversations.

## Features

- AI-powered conversations via OpenAI, xAI/Grok, and Google Gemini
- Image generation from text descriptions
- Voice message support (speech-to-text and audio responses)
- Conversation history with context preservation
- Switch between AI providers on the fly
- Response viewer for AI responses with LaTeX formulas, syntax-highlighted code, and tables

## Prerequisites

- C++ Compiler with C++20 support (GCC 10+, Clang 12+, MSVC 2019+)
- [CMake](https://cmake.org/) 3.30+
- [Boost](https://www.boost.org/)
- [OpenSSL](https://www.openssl.org/)
- [libcurl](https://curl.se/libcurl/)
- [fmt](https://github.com/fmtlib/fmt)
- [RapidJSON](https://rapidjson.org/)
- [SQLite3](https://www.sqlite.org/)
- [Google Test](https://github.com/google/googletest)
- [tgbot-cpp](https://github.com/reo7sp/tgbot-cpp) (must be built separately)
- [ffmpeg](https://ffmpeg.org/) (required for voice message conversion)

## Quick Start with Docker

```bash
git clone https://github.com/dersergioni/MyBuddyBot.git
cd MyBuddyBot
cp example.env .env
# Edit .env — fill in TG_API_TOKEN and at least one AI provider token
docker compose build     # first build takes a few minutes
docker compose up -d
```

Check logs with `docker compose logs -f`.

### Running with Docker directly

If you prefer not to use Docker Compose, you can build and run the image manually:

```bash
docker build -t mybuddybot .
docker volume create bot-data
docker run -d --name mybuddybot --env-file .env -v bot-data:/data --restart unless-stopped mybuddybot
```

### Managing

```bash
docker compose down          # stop (data is preserved in ./data)
docker compose up -d         # start again
docker compose build         # rebuild after pulling new code
docker compose down -v       # stop and delete all data
```

### Backup

```bash
docker run --rm -v bot-data:/data -v $(pwd):/backup ubuntu \
  tar czf /backup/bot-backup.tar.gz -C /data .
```

## Getting Started (Manual Build)

### 1. Clone the Repository

```bash
git clone https://github.com/dersergioni/MyBuddyBot.git
cd MyBuddyBot
```

### 2. Install Dependencies

Use a package manager to install the required libraries:

```bash
# macOS (Homebrew)
brew install cmake ninja boost openssl curl fmt rapidjson sqlite3 googletest ffmpeg

# Ubuntu/Debian
sudo apt install cmake ninja-build libboost-all-dev libssl-dev libcurl4-openssl-dev libfmt-dev rapidjson-dev libsqlite3-dev libgtest-dev ffmpeg

# Or use vcpkg (cross-platform)
vcpkg install boost openssl curl fmt rapidjson sqlite3 gtest
```

### 3. Build tgbot-cpp

```bash
git clone https://github.com/reo7sp/tgbot-cpp.git
cd tgbot-cpp
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
cmake --install build --prefix /usr/local
```

### 4. Build the Project

```bash
# Configure
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DTGBOT_LIBRARY_LOCATION=/usr/local \
      -G Ninja -B release

# Build
cmake --build release
```

**Note:** `TGBOT_LIBRARY_LOCATION` is required and must point to the tgbot-cpp installation prefix.

### 5. Configure Environment Variables

See `example.env` for a ready-to-fill template.

| Variable | Required | Description |
|----------|----------|-------------|
| `TG_API_TOKEN` | Yes | Telegram Bot API token from [@BotFather](https://t.me/BotFather) |
| `OAI_API_TOKEN` | Optional* | OpenAI API key |
| `XAI_API_TOKEN` | Optional* | xAI API key |
| `GOOGLE_API_TOKEN` | Optional* | Google Gemini API key |
| `MYBUDDYBOT_DB_PATH` | Yes | Path to SQLite database file |
| `MYBUDDYBOT_NAME` | No | Bot display name used in `/start`, `/health`, and runtime logs (default: `MyBuddyBot`) |
| `MYBUDDYBOT_DEBUG_LEVEL_MODE` | No | Set to enable debug logging |
| `MYBUDDYBOT_DEFAULT_PROVIDER` | No | Default AI provider: `openai`, `xai`, or `google` |
| `MYBUDDYBOT_STATE_PATH` | No | Override path for state file |
| `MYBUDDYBOT_WORKER_THREADS` | No | Worker threads for async tasks (default: CPU count) |
| `MYBUDDYBOT_ALLOWLIST_IDS` | No | CSV list of allowed Telegram user IDs |
| `MYBUDDYBOT_ALLOWLIST_USERNAMES` | No | CSV list of allowed Telegram usernames |
| `MYBUDDYBOT_BLOCKLIST_IDS` | No | CSV list of blocked Telegram user IDs |
| `MYBUDDYBOT_BLOCKLIST_USERNAMES` | No | CSV list of blocked Telegram usernames |
| `MYBUDDYBOT_ADMIN_IDS` | No | CSV list of admin Telegram user IDs |
| `MYBUDDYBOT_VIEWER_URL` | No | Public URL of the response viewer, **must end with `/`** (e.g. `https://bot.example.com/viewer/`) |
| `MYBUDDYBOT_VIEWER_DIR` | No | Local directory where the viewer files are served from (e.g. `/var/www/MyBuddyBot/viewer`) |
| `MYBUDDYBOT_RUN_INTEGRATION_TESTS` | No | Set to `1` to enable integration tests |

**\*** At least one AI provider token must be set. Providers without tokens are disabled, and `/switch_provider` cycles only through enabled providers.
Access control precedence: `admin` bypasses all lists, then blocklist denies, then allowlist allows (if allowlist is configured).

Access list format:
- Use comma-separated values (`a,b,c`). Do not use semicolons.
- Spaces around values are ignored.
- Usernames are normalized (`@Alice`, `alice`, and `ALICE` are treated as the same username).
- Empty entries are ignored.

Examples with multiple users:
- `MYBUDDYBOT_ALLOWLIST_IDS="123456789,987654321"`
- `MYBUDDYBOT_ALLOWLIST_USERNAMES="trusted_user,@team_lead,OpsAdmin"`
- `MYBUDDYBOT_BLOCKLIST_IDS="111111111,222222222"`
- `MYBUDDYBOT_BLOCKLIST_USERNAMES="spam_bot,@annoying_user"`
- `MYBUDDYBOT_ADMIN_IDS="123456789,555555555"`

### 6. Run the Bot

```bash
export TG_API_TOKEN="your-telegram-token"
export OAI_API_TOKEN="your-openai-key"
export XAI_API_TOKEN="your-xai-key"
export GOOGLE_API_TOKEN="your-google-key"
export MYBUDDYBOT_DB_PATH="./mybuddybot.db"
export MYBUDDYBOT_NAME="MyBuddyBot"
export MYBUDDYBOT_DEFAULT_PROVIDER="openai"
export MYBUDDYBOT_STATE_PATH="./MyBuddyBotState.bin"
export MYBUDDYBOT_WORKER_THREADS="4"
export MYBUDDYBOT_DEBUG_LEVEL_MODE="1"
export MYBUDDYBOT_ALLOWLIST_IDS="123456789"
export MYBUDDYBOT_ALLOWLIST_USERNAMES="trusted_user"
export MYBUDDYBOT_BLOCKLIST_IDS=""
export MYBUDDYBOT_BLOCKLIST_USERNAMES=""
export MYBUDDYBOT_ADMIN_IDS="123456789"
export MYBUDDYBOT_RUN_INTEGRATION_TESTS=""

./release/MyBuddyBot
```

You can omit provider tokens you don't plan to use. At least one AI provider token must be set.

## Usage

### Bot Commands

| Command | Description |
|---------|-------------|
| `/start` | Start the bot |
| `/clear` | Clear conversation history |
| `/image` | Enter image generation mode |
| `/audio` | Toggle audio responses on/off |
| `/switch_provider` | Switch between AI providers (OpenAI, xAI, Gemini) |
| `/switch_model` | Toggle between primary and secondary model |
| `/system` | Set/show/clear per-chat system prompt |
| `/health` | Show health and runtime configuration |

### Interaction

1. Open Telegram and find your bot
2. Send `/start` to begin
3. Chat naturally — the bot will respond using AI
4. Use `/image` and describe what you want to generate
5. Send voice messages for speech-to-text processing

### Response Viewer (LaTeX Rendering)

When AI responses contain LaTeX formulas (`$$...$$`, `\(...\)`, `\[...\]`), the bot automatically sends a button that opens a page with properly rendered math, syntax-highlighted code, and formatted tables.

**How it works:** The bot saves the raw Markdown response as a JSON file to disk. Nginx serves it as static content. The viewer page fetches and renders it client-side using KaTeX, highlight.js, and marked.js.

#### Production Setup (VPS with nginx)

1. Copy the `viewer/` directory to your VPS web root and set permissions:

```bash
mkdir -p /var/www/MyBuddyBot
cp -r viewer /var/www/MyBuddyBot/
mkdir -p /var/www/MyBuddyBot/viewer/responses
chown <bot-user>:<bot-user> /var/www/MyBuddyBot/viewer/responses
```

Replace `<bot-user>` with the OS user that runs the bot process. Only `responses/` needs write access — the rest stays read-only.

2. Configure nginx (`/etc/nginx/sites-available/bot.example.com`):

```nginx
server {
    listen 80;
    server_name bot.example.com;

    location /viewer/ {
        alias /var/www/MyBuddyBot/viewer/;
        try_files $uri $uri/ =404;
    }
}
```

```bash
sudo ln -s /etc/nginx/sites-available/bot.example.com /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

3. Enable HTTPS with Let's Encrypt:

```bash
sudo certbot --nginx -d bot.example.com
```

Certbot will automatically update the nginx config to add SSL and redirect HTTP to HTTPS.

4. Set the environment variables and restart the bot:

```bash
export MYBUDDYBOT_VIEWER_URL="https://bot.example.com/viewer/"
export MYBUDDYBOT_VIEWER_DIR="/var/www/MyBuddyBot/viewer"
```

5. The bot will:
   - Send normal Telegram HTML responses as usual
   - Detect LaTeX in responses and additionally send an "Open formatted response" button
   - Save response JSON files to `{VIEWER_DIR}/responses/`
   - The button opens `{VIEWER_URL}?id={response_id}` in Telegram's built-in browser

#### Local Development (without nginx)

**Quick rendering check (browser only):**

To verify that LaTeX, code blocks, and tables render correctly — no nginx or tunnel needed:

```bash
# Create a sample response in the repo's viewer directory
mkdir -p viewer/responses
echo '{"content": "# Test\n\nFormula: $$E = mc^2$$\n\n```python\nprint(\"hello\")\n```"}' \
  > viewer/responses/test.json

# Serve locally
cd viewer && python3 -m http.server 8080

# Open in browser: http://localhost:8080/?id=test
```

**Full end-to-end testing (with ngrok):**

The bot needs an HTTPS URL to send as a button link. Use [ngrok](https://ngrok.com/) to expose the local server:

```bash
# 1. Serve the viewer directory
cd viewer && python3 -m http.server 8080 &

# 2. Start ngrok tunnel (install: brew install ngrok)
ngrok http 8080

# ngrok will show a public URL like: https://abc123.ngrok-free.app

# 3. Run the bot with the ngrok URL and local viewer dir
export MYBUDDYBOT_VIEWER_URL="https://abc123.ngrok-free.app/"
export MYBUDDYBOT_VIEWER_DIR="$(pwd)/viewer"
./debug/MyBuddyBot
```

Now send a message that triggers a LaTeX response from the AI — the bot will save the JSON to `viewer/responses/`, send a button, and tapping it opens the viewer via the ngrok tunnel.

> **Tip:** The ngrok URL changes on every restart (free tier). Update `MYBUDDYBOT_VIEWER_URL` accordingly, or use a paid ngrok plan for a stable subdomain.

**Note:** Both `MYBUDDYBOT_VIEWER_URL` and `MYBUDDYBOT_VIEWER_DIR` must be set for the feature to activate. If either is missing, the bot works normally without the viewer button. Plain text responses (no LaTeX) never trigger the button.

## Testing

```bash
# Run all tests
cd release && ctest --output-on-failure

# Or run directly
./release/MyBuddyBot --test
```

## Versioning and Releases

MyBuddyBot follows Semantic Versioning (`MAJOR.MINOR.PATCH`) and uses annotated git tags (`vX.Y.Z`) for releases.

- Release notes are tracked in [CHANGELOG.md](CHANGELOG.md)
- Release process is documented in [docs/RELEASING.md](docs/RELEASING.md)

## Security

**Important:** Never commit API tokens or secrets to version control. Store them in environment variables or use a secrets manager.

For security vulnerability reports, please see [SECURITY.md](SECURITY.md).

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for component diagrams, message flow, and threading model.

## Code of Conduct

This project follows the Contributor Covenant. See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

## Contact

For questions or suggestions, contact [dersergioni](mailto:contact@dersergioni.com).
