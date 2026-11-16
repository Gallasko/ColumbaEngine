#include "markdown.h"

namespace pg
{
    namespace texteditor
    {
        namespace
        {
            bool isWordByte(unsigned char c)
            {
                return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or c >= 0x80;
            }

            bool startsWith(const std::string& s, size_t at, const char* prefix)
            {
                for (size_t i = 0; prefix[i] != '\0'; ++i)
                    if (at + i >= s.size() or s[at + i] != prefix[i])
                        return false;
                return true;
            }

            size_t leadingSpaces(const std::string& s)
            {
                size_t i = 0;
                while (i < s.size() and s[i] == ' ')
                    ++i;
                return i;
            }

            bool isFence(const std::string& s)
            {
                return startsWith(s, leadingSpaces(s), "```");
            }

            bool isHeading(const std::string& s)
            {
                size_t i = 0;
                while (i < s.size() and s[i] == '#')
                    ++i;
                return i >= 1 and i <= 6 and i < s.size() and s[i] == ' ';
            }

            // The end of a list marker starting at `at`, or `at` when there is none
            size_t listMarkerEnd(const std::string& s, size_t at)
            {
                if (at + 1 < s.size() and (s[at] == '-' or s[at] == '*' or s[at] == '+') and s[at + 1] == ' ')
                    return at + 2;
                size_t i = at;
                while (i < s.size() and s[i] >= '0' and s[i] <= '9')
                    ++i;
                if (i > at and i + 1 < s.size() and s[i] == '.' and s[i + 1] == ' ')
                    return i + 2;
                return at;
            }

            // Fills the gaps between ordered, non-overlapping spans with Plain
            std::vector<Span> cover(const std::vector<Span>& found, size_t size)
            {
                std::vector<Span> out;
                size_t at = 0;
                for (const Span& s : found)
                {
                    if (s.start > at)
                        out.push_back({at, s.start, SpanKind::Plain});
                    out.push_back(s);
                    at = s.end;
                }
                if (at < size)
                    out.push_back({at, size, SpanKind::Plain});
                return out;
            }

            std::vector<Span> inlineSpans(const std::string& s)
            {
                std::vector<Span> found;
                size_t i = leadingSpaces(s);
                const size_t marker = listMarkerEnd(s, i);
                if (marker > i)
                {
                    found.push_back({i, marker, SpanKind::ListMarker});
                    i = marker;
                }

                while (i < s.size())
                {
                    if (startsWith(s, i, "<!--"))
                    {
                        size_t j = i + 4;
                        while (j < s.size() and s[j] == ' ')
                            ++j;
                        if (startsWith(s, j, "visual:") or startsWith(s, j, "number:"))
                        {
                            const size_t close = s.find("-->", j);
                            const size_t end = close == std::string::npos ? s.size() : close + 3;
                            found.push_back({i, end, SpanKind::WriterMarker});
                            i = end;
                            continue;
                        }
                    }
                    if (s[i] == '`')
                    {
                        const size_t close = s.find('`', i + 1);
                        if (close != std::string::npos)
                        {
                            found.push_back({i, close + 1, SpanKind::InlineCode});
                            i = close + 1;
                            continue;
                        }
                    }
                    if (startsWith(s, i, "**"))
                    {
                        const size_t close = s.find("**", i + 2);
                        if (close != std::string::npos and close > i + 2)
                        {
                            found.push_back({i, close + 2, SpanKind::Bold});
                            i = close + 2;
                            continue;
                        }
                    }
                    if (s[i] == '*' and i + 1 < s.size() and s[i + 1] != ' ')
                    {
                        size_t close = i + 1;
                        while (close < s.size() and not (s[close] == '*' and not startsWith(s, close, "**")))
                            ++close;
                        if (close < s.size() and close > i + 1)
                        {
                            found.push_back({i, close + 1, SpanKind::Italic});
                            i = close + 1;
                            continue;
                        }
                    }
                    if (s[i] == '_' and (i == 0 or not isWordByte(static_cast<unsigned char>(s[i - 1]))))
                    {
                        size_t close = i + 1;
                        while (close < s.size())
                        {
                            if (s[close] == '_' and (close + 1 >= s.size() or not isWordByte(static_cast<unsigned char>(s[close + 1]))))
                                break;
                            ++close;
                        }
                        if (close < s.size() and close > i + 1)
                        {
                            found.push_back({i, close + 1, SpanKind::Italic});
                            i = close + 1;
                            continue;
                        }
                    }
                    if (s[i] == '[')
                    {
                        const size_t mid = s.find("](", i + 1);
                        if (mid != std::string::npos)
                        {
                            const size_t close = s.find(')', mid + 2);
                            if (close != std::string::npos)
                            {
                                found.push_back({i, close + 1, SpanKind::Link});
                                i = close + 1;
                                continue;
                            }
                        }
                    }
                    ++i;
                }
                return cover(found, s.size());
            }
        }

        std::vector<std::vector<Span>> markdownSpans(const std::vector<std::string>& lines)
        {
            std::vector<std::vector<Span>> out;
            out.reserve(lines.size());
            bool inCode = false;
            for (const std::string& line : lines)
            {
                if (line.empty())
                {
                    out.emplace_back();
                    continue;
                }
                if (isFence(line))
                {
                    out.push_back({Span{0, line.size(), SpanKind::Fence}});
                    inCode = not inCode;
                    continue;
                }
                if (inCode)
                {
                    out.push_back({Span{0, line.size(), SpanKind::CodeBlock}});
                    continue;
                }
                if (isHeading(line))
                {
                    out.push_back({Span{0, line.size(), SpanKind::Heading}});
                    continue;
                }
                out.push_back(inlineSpans(line));
            }
            return out;
        }
    }
}
