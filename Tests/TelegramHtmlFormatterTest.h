#ifndef TELEGRAMHTMLFORMATTERTEST_H
#define TELEGRAMHTMLFORMATTERTEST_H

#include "../infra/TelegramHtmlFormatter.h"

#include <gtest/gtest.h>

#include <string>

namespace mbb::tests
{

TEST(TelegramHtmlFormatterTest, HeadingsConvertedToBold)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("# Heading 1");
    EXPECT_NE(result.find("<b>Heading 1</b>"), std::string::npos);
    EXPECT_EQ(result.find("<h1>"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, ParagraphsConvertedToNewlines)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("First\n\nSecond");
    EXPECT_EQ(result.find("<p>"), std::string::npos);
    EXPECT_NE(result.find("First"), std::string::npos);
    EXPECT_NE(result.find("Second"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, UnorderedListsConvertedToBullets)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("- item1\n- item2");
    EXPECT_NE(result.find("• item1"), std::string::npos);
    EXPECT_NE(result.find("• item2"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, OrderedListsConvertedToNumbers)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("1. first\n2. second");
    EXPECT_NE(result.find("1. first"), std::string::npos);
    EXPECT_NE(result.find("2. second"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, EmphasisPassedThrough)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("*italic* **bold**");
    EXPECT_NE(result.find("<em>italic</em>"), std::string::npos);
    EXPECT_NE(result.find("<strong>bold</strong>"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, CodeBlocksPreserved)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("```\ncode\n```");
    EXPECT_NE(result.find("<code>"), std::string::npos);
    EXPECT_NE(result.find("code"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, CodeBlockClassAttributePreserved)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("```python\nprint('hi')\n```");
    EXPECT_NE(result.find("class=\"language-python\""), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, HorizontalRuleConverted)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("---");
    EXPECT_NE(result.find("———"), std::string::npos);
    EXPECT_EQ(result.find("<hr"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, ImagesRemoved)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("![alt](http://example.com/img.png)");
    EXPECT_EQ(result.find("<img"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, MultipleNewlinesCollapsed)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("a\n\n\n\n\nb");
    EXPECT_EQ(result.find("\n\n\n"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, BlockquotePassedThrough)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("> quote text");
    EXPECT_NE(result.find("<blockquote>"), std::string::npos);
    EXPECT_NE(result.find("quote text"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, StrikethroughPassedThrough)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("~~deleted~~");
    EXPECT_NE(result.find("<del>deleted</del>"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, RawHtmlEscaped)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("<div>foo</div>");
    EXPECT_EQ(result.find("<div>"), std::string::npos);
    EXPECT_NE(result.find("&lt;div&gt;"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, UnsupportedTagsStripped)
{
    // Feed md4c output that would contain unsupported tags through the sanitizer.
    // Tables produce <table>, <tr>, <td> — these should be converted to text.
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("| A | B |\n|---|---|\n| 1 | 2 |");
    EXPECT_EQ(result.find("<table"), std::string::npos);
    EXPECT_EQ(result.find("<tr"), std::string::npos);
    EXPECT_EQ(result.find("<td"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, LinksPreserved)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("[click here](https://example.com)");
    EXPECT_NE(result.find("<a href=\"https://example.com\">click here</a>"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, NestedFormattingPreserved)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("**bold *and italic***");
    EXPECT_NE(result.find("<strong>"), std::string::npos);
    EXPECT_NE(result.find("<em>"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, FallbackOnInvalidInput)
{
    // Empty input should not crash
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("");
    EXPECT_TRUE(result.empty());
}

TEST(TelegramHtmlFormatterTest, InlineCodePreserved)
{
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("Use `fmt::format` here");
    EXPECT_NE(result.find("<code>fmt::format</code>"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, NestedBlockquotesFlattened)
{
    // Telegram does not allow nested blockquotes — they must be flattened.
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("> Level 1\n>> Level 2\n>>> Level 3");

    // Only one opening and one closing blockquote tag
    size_t openCount = 0;
    size_t closeCount = 0;
    for (size_t pos = 0; (pos = result.find("<blockquote>", pos)) != std::string::npos; pos++)
    {
        openCount++;
    }
    for (size_t pos = 0; (pos = result.find("</blockquote>", pos)) != std::string::npos; pos++)
    {
        closeCount++;
    }
    EXPECT_EQ(openCount, 1u);
    EXPECT_EQ(closeCount, 1u);

    // All levels of text should still be present
    EXPECT_NE(result.find("Level 1"), std::string::npos);
    EXPECT_NE(result.find("Level 2"), std::string::npos);
    EXPECT_NE(result.find("Level 3"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, OnlyTelegramEntitiesInOutput)
{
    // Verify that &amp; &lt; &gt; &quot; are the only entities produced.
    // md4c-html.c only emits these four, which Telegram supports.
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("AT&T says `a < b && c > d` and \"quotes\"");
    EXPECT_NE(result.find("&amp;"), std::string::npos);
    EXPECT_NE(result.find("&lt;"), std::string::npos);
    EXPECT_NE(result.find("&gt;"), std::string::npos);
    EXPECT_NE(result.find("&quot;"), std::string::npos);

    // No other entity types (e.g. &apos; &nbsp; &#123;)
    EXPECT_EQ(result.find("&apos;"), std::string::npos);
    EXPECT_EQ(result.find("&nbsp;"), std::string::npos);
    EXPECT_EQ(result.find("&#"), std::string::npos);
}

TEST(TelegramHtmlFormatterTest, NoFormattingInsideCodeBlock)
{
    // Markdown formatting inside code should remain literal, not become HTML tags.
    // Telegram rejects bold/italic inside <pre>/<code>.
    auto result = TelegramHtmlFormatter::ConvertMarkdownToTelegramHtml("```\n**not bold** *not italic*\n```");
    EXPECT_NE(result.find("**not bold**"), std::string::npos);
    EXPECT_NE(result.find("*not italic*"), std::string::npos);
    EXPECT_EQ(result.find("<strong>"), std::string::npos);
    EXPECT_EQ(result.find("<em>"), std::string::npos);
}

} // namespace mbb::tests

#endif // TELEGRAMHTMLFORMATTERTEST_H
