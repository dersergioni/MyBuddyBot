#include "infra/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace mbb
{

std::string StringUtils::TrimCopy(std::string value)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string StringUtils::ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string StringUtils::NormalizeUsername(std::string value)
{
    std::string normalized = TrimCopy(std::move(value));
    if (!normalized.empty() && normalized.front() == '@')
    {
        normalized.erase(normalized.begin());
    }
    return ToLower(std::move(normalized));
}

bool StringUtils::ContainsLatex(const std::string& text)
{
    bool inCodeBlock = false;

    for (size_t i = 0; i < text.size(); ++i)
    {
        // Check for fenced code block toggle
        if (i + 2 < text.size() && text[i] == '`' && text[i + 1] == '`' && text[i + 2] == '`')
        {
            if (!inCodeBlock)
            {
                // Opening fence — check if language tag is latex/tex
                size_t langStart = i + 3;
                size_t lineEnd = text.find('\n', langStart);
                if (lineEnd == std::string::npos)
                {
                    lineEnd = text.size();
                }
                const std::string lang = ToLower(TrimCopy(text.substr(langStart, lineEnd - langStart)));
                if (lang == "latex" || lang == "tex")
                {
                    return true;
                }
            }
            inCodeBlock = !inCodeBlock;
            i += 2;
            continue;
        }

        if (inCodeBlock)
        {
            continue;
        }

        // Check for $$ (display math)
        if (i + 1 < text.size() && text[i] == '$' && text[i + 1] == '$')
        {
            return true;
        }

        // Check for \( or \[
        if (i + 1 < text.size() && text[i] == '\\' && (text[i + 1] == '(' || text[i + 1] == '['))
        {
            return true;
        }

        // Check for \begin{ (LaTeX environments: equation, align, gather, etc.)
        if (text.compare(i, 7, "\\begin{") == 0)
        {
            return true;
        }

        // Check for common LaTeX math commands
        if (text[i] == '\\')
        {
            for (const char* cmd :
                 {"frac{",    "sqrt{",   "sum",    "int",     "prod",    "lim",   "infty",     "alpha",      "beta",
                  "gamma",    "delta",   "theta",  "lambda",  "sigma",   "omega", "partial",   "nabla",      "cdot",
                  "times",    "equiv",   "approx", "neq",     "leq",     "geq",   "leftarrow", "rightarrow", "mathbb{",
                  "mathcal{", "mathrm{", "text{",  "textbf{", "textit{", "LaTeX"})
            {
                if (text.compare(i + 1, std::strlen(cmd), cmd) == 0)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

} // namespace mbb
