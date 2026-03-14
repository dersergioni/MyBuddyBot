#include "infra/TelegramHtmlFormatter.h"

#include "ext/md4c/md4c-html.h"
#include "infra/StringUtils.h"

#include <regex>
#include <string>

namespace mbb
{

// =============================================================================
// ConvertMarkdownToTelegramHtml
// =============================================================================
// Converts Markdown to Telegram-compatible HTML using md4c library.
//
// Telegram supports only a limited set of HTML tags, so we post-process
// the standard HTML output from md4c:
//
// | Element            | md4c output        | Telegram support | We convert to     |
// |--------------------|--------------------|------------------|-------------------|
// | Headings           | <h1>...<h6>        | No               | <b> + \n          |
// | Paragraphs         | <p>                | No               | remove, add \n\n  |
// | Ordered lists      | <ol><li>           | No               | 1. , 2. , ...     |
// | Unordered lists    | <ul><li>           | No               | bullet points     |
// | Horizontal rule    | <hr>               | No               | ———\n             |
// | Line break         | <br>               | No               | \n                |
// | Images             | <img>              | No               | remove            |
// | Tables             | <table><tr><td>    | No               | text with |       |
//
// Tags that pass through unchanged (Telegram supports them natively):
// <b>, <strong>, <i>, <em>, <u>, <ins>, <s>, <strike>, <del>,
// <code>, <pre>, <pre><code class="language-xxx">,
// <a href="...">, <blockquote>
// Note: <span class="tg-spoiler"> is also supported but md4c never produces it.
//
// Raw HTML in markdown input is escaped (MD_FLAG_NOHTMLBLOCKS/SPANS).
// Final sanitization strips any remaining unsupported tags as a safety net.
// Fallback: if md4c fails to parse, we escape &, <, > and return as-is.
// =============================================================================
std::string TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml(const std::string& input)
{
    std::string html;

    auto outputCallback = [](const MD_CHAR* text, MD_SIZE size, void* userdata) {
        auto* output = static_cast<std::string*>(userdata);
        output->append(text, size);
    };

    unsigned parserFlags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_NOHTMLBLOCKS | MD_FLAG_NOHTMLSPANS;
    unsigned rendererFlags = 0;

    int mdResult =
        md_html(input.c_str(), static_cast<MD_SIZE>(input.size()), outputCallback, &html, parserFlags, rendererFlags);

    if (mdResult != 0)
    {
        // If parsing fails, return input with basic HTML escaping
        std::string escaped = input;
        size_t pos = 0;
        while ((pos = escaped.find('&', pos)) != std::string::npos)
        {
            escaped.replace(pos, 1, "&amp;");
            pos += 5;
        }
        pos = 0;
        while ((pos = escaped.find('<', pos)) != std::string::npos)
        {
            escaped.replace(pos, 1, "&lt;");
            pos += 4;
        }
        pos = 0;
        while ((pos = escaped.find('>', pos)) != std::string::npos)
        {
            escaped.replace(pos, 1, "&gt;");
            pos += 4;
        }
        return escaped;
    }

#ifdef _WIN32
    constexpr auto REGEX_FLAGS = std::regex_constants::ECMAScript;
#else
    constexpr auto REGEX_FLAGS = std::regex_constants::ECMAScript | std::regex_constants::multiline;
#endif

    // Headings -> bold
    html = std::regex_replace(html, std::regex(R"(<h[1-6]>)", REGEX_FLAGS), "<b>");
    html = std::regex_replace(html, std::regex(R"(</h[1-6]>)", REGEX_FLAGS), "</b>\n");

    // Paragraphs
    html = std::regex_replace(html, std::regex(R"(<p>)", REGEX_FLAGS), "");
    html = std::regex_replace(html, std::regex(R"(</p>)", REGEX_FLAGS), "\n\n");

    // Ordered lists - replace <li> with numbered items
    {
        std::string result;
        std::regex olRegex(R"re(<ol\b[^>]*>)re", REGEX_FLAGS);
        std::regex olEndRegex(R"(</ol>)", REGEX_FLAGS);
        std::regex liRegex(R"(<li[^>]*>)", REGEX_FLAGS);

        std::smatch match;
        std::string remaining = html;

        while (std::regex_search(remaining, match, olRegex))
        {
            result += match.prefix();

            std::string afterOl = match.suffix();
            std::smatch endMatch;

            if (std::regex_search(afterOl, endMatch, olEndRegex))
            {
                std::string olContent = endMatch.prefix();

                // Extract start value from attribute, default to 1
                int counter = 1;
                std::smatch startMatch;
                std::string olTag = match.str();
                if (std::regex_search(olTag, startMatch, std::regex(R"(start=(?:"|')(\d+)(?:"|'))", REGEX_FLAGS)))
                {
                    counter = std::stoi(startMatch[1].str());
                }

                // Replace <li> with numbered items
                std::string numberedContent;
                std::smatch liMatch;
                while (std::regex_search(olContent, liMatch, liRegex))
                {
                    numberedContent += liMatch.prefix();
                    numberedContent += std::to_string(counter++) + ". ";
                    olContent = liMatch.suffix();
                }
                numberedContent += olContent;

                result += numberedContent;
                remaining = endMatch.suffix();
            }
            else
            {
                result += match.str();
                remaining = match.suffix();
            }
        }
        result += remaining;
        html = result;
    }

    // Unordered lists - replace <li> with bullets
    html = std::regex_replace(html, std::regex(R"(<li[^>]*>)", REGEX_FLAGS), "• ");
    html = std::regex_replace(html, std::regex(R"(</li>)", REGEX_FLAGS), "\n");
    // Replace list wrappers with newlines (not empty) to separate nested items
    html = std::regex_replace(html, std::regex(R"(</?ul[^>]*>\n?)", REGEX_FLAGS), "\n");
    html = std::regex_replace(html, std::regex(R"(</?ol[^>]*>\n?)", REGEX_FLAGS), "\n");

    // Horizontal rules and breaks — convert to text before sanitization
    html = std::regex_replace(html, std::regex(R"(<hr\s*/?>)", REGEX_FLAGS), "———\n");
    html = std::regex_replace(html, std::regex(R"(<br\s*/?>)", REGEX_FLAGS), "\n");

    // Tables — convert to pipe-separated text on single lines
    html = std::regex_replace(
        html, std::regex(R"(</?table[^>]*>|</?thead[^>]*>|</?tbody[^>]*>|</?tfoot[^>]*>)", REGEX_FLAGS), "");
    html = std::regex_replace(html, std::regex(R"(<tr[^>]*>)", REGEX_FLAGS), "");
    html = std::regex_replace(html, std::regex(R"(</tr>)", REGEX_FLAGS), "\n");
    html = std::regex_replace(html, std::regex(R"(<t[dh][^>]*>)", REGEX_FLAGS), "");
    // Consume optional trailing newline so cells stay on the same line
    html = std::regex_replace(html, std::regex(R"(</t[dh]>\n?)", REGEX_FLAGS), " | ");
    // Remove trailing " | " at end of each row
    html = std::regex_replace(html, std::regex(R"( \| \n)", REGEX_FLAGS), "\n");

    // Sanitize: strip any HTML tags not in Telegram's supported whitelist.
    // Whitelisted tags pass through; everything else is removed.
    // This is the safety net that prevents Telegram from rejecting messages.
    // Note: Telegram also supports <span class="tg-spoiler"> but md4c never produces it.
    static const std::regex sanitizeRegex(
        R"(</?(?!(?:b|strong|i|em|u|ins|s|strike|del|code|pre|a|blockquote)\b)[a-zA-Z][a-zA-Z0-9]*\b[^>]*>)",
        REGEX_FLAGS);
    html = std::regex_replace(html, sanitizeRegex, "");

    // Flatten nested blockquotes — Telegram doesn't allow nesting.
    // Keep only outermost <blockquote>...</blockquote>.
    {
        std::string result;
        result.reserve(html.size());
        int depth = 0;
        const std::string openTag = "<blockquote>";
        const std::string closeTag = "</blockquote>";

        for (size_t i = 0; i < html.size(); ++i)
        {
            if (html.compare(i, openTag.size(), openTag) == 0)
            {
                if (depth == 0)
                {
                    result += openTag;
                }
                depth++;
                i += openTag.size() - 1;
            }
            else if (html.compare(i, closeTag.size(), closeTag) == 0)
            {
                depth--;
                if (depth == 0)
                {
                    result += closeTag;
                }
                i += closeTag.size() - 1;
            }
            else
            {
                result += html[i];
            }
        }
        html = std::move(result);
    }

    // Clean up trailing whitespace inside blockquotes
    html = std::regex_replace(html, std::regex(R"(\n+</blockquote>)", REGEX_FLAGS), "\n</blockquote>");

    // Clean up multiple newlines
    html = std::regex_replace(html, std::regex(R"(\n{3,})", REGEX_FLAGS), "\n\n");

    return StringUtils::TrimCopy(std::move(html));
}

} // namespace mbb
