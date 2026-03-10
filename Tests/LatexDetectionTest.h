#pragma once

#include "infra/StringUtils.h"

#include <gtest/gtest.h>

namespace mbb::tests
{

TEST(LatexDetectionTest, DetectsDoubleDollarSign)
{
    EXPECT_TRUE(StringUtils::ContainsLatex("The formula is $$x^2 + y^2 = r^2$$"));
}

TEST(LatexDetectionTest, DetectsBackslashParen)
{
    EXPECT_TRUE(StringUtils::ContainsLatex("Inline math \\(x^2\\) here"));
}

TEST(LatexDetectionTest, DetectsBackslashBracket)
{
    EXPECT_TRUE(StringUtils::ContainsLatex("Display math \\[x^2 + y^2\\] here"));
}

TEST(LatexDetectionTest, ReturnsFalseForPlainText)
{
    EXPECT_FALSE(StringUtils::ContainsLatex("Hello, this is just plain text."));
}

TEST(LatexDetectionTest, ReturnsFalseForSingleDollarAmounts)
{
    EXPECT_FALSE(StringUtils::ContainsLatex("The price is $5 and tax is $100."));
}

TEST(LatexDetectionTest, IgnoresLatexInsideFencedCodeBlock)
{
    std::string text = "Some text\n```\n$$x^2$$\n\\(y^2\\)\n\\[z^2\\]\n```\nMore text";
    EXPECT_FALSE(StringUtils::ContainsLatex(text));
}

TEST(LatexDetectionTest, DetectsLatexAfterCodeBlockEnds)
{
    std::string text = "```\ncode here\n```\nThen $$x^2 = 4$$";
    EXPECT_TRUE(StringUtils::ContainsLatex(text));
}

TEST(LatexDetectionTest, ReturnsFalseForEmptyString)
{
    EXPECT_FALSE(StringUtils::ContainsLatex(""));
}

TEST(LatexDetectionTest, DetectsDoubleDollarAtStart)
{
    EXPECT_TRUE(StringUtils::ContainsLatex("$$E = mc^2$$"));
}

TEST(LatexDetectionTest, DetectsBeginEquation)
{
    EXPECT_TRUE(StringUtils::ContainsLatex("Formula:\n\\begin{equation}\nx^2\n\\end{equation}"));
}

TEST(LatexDetectionTest, DetectsBeginAlign)
{
    EXPECT_TRUE(StringUtils::ContainsLatex("Steps:\n\\begin{align}\na &= b\n\\end{align}"));
}

TEST(LatexDetectionTest, DetectsFracCommand)
{
    EXPECT_TRUE(StringUtils::ContainsLatex("The result is $\\frac{1}{2}$"));
}

TEST(LatexDetectionTest, DetectsSqrtCommand)
{
    EXPECT_TRUE(StringUtils::ContainsLatex("Value is $\\sqrt{2}$"));
}

TEST(LatexDetectionTest, DetectsGreekLetters)
{
    EXPECT_TRUE(StringUtils::ContainsLatex("where $\\alpha$ is the angle"));
}

TEST(LatexDetectionTest, DetectsLaTeXCommand)
{
    EXPECT_TRUE(StringUtils::ContainsLatex("Install \\LaTeX on your system"));
}

TEST(LatexDetectionTest, IgnoresBeginInsideCodeBlock)
{
    std::string text = "```python\nimport math  # $price = $100\n```";
    EXPECT_FALSE(StringUtils::ContainsLatex(text));
}

TEST(LatexDetectionTest, DetectsLatexCodeBlockByLanguageTag)
{
    std::string text = "Here is the document:\n```latex\n\\begin{equation}\nx^2\n\\end{equation}\n```";
    EXPECT_TRUE(StringUtils::ContainsLatex(text));
}

TEST(LatexDetectionTest, DetectsTexCodeBlockByLanguageTag)
{
    std::string text = "Source:\n```tex\n\\documentclass{article}\n```";
    EXPECT_TRUE(StringUtils::ContainsLatex(text));
}

TEST(LatexDetectionTest, DetectsLatexCodeBlockCaseInsensitive)
{
    std::string text = "```LaTeX\n\\frac{1}{2}\n```";
    EXPECT_TRUE(StringUtils::ContainsLatex(text));
}

} // namespace mbb::tests
