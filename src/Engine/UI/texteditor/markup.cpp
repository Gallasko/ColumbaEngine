#include "markup.h"

#include <algorithm>

namespace pg
{
    namespace texteditor
    {
        namespace
        {
            std::string tag(Rgba c)
            {
                return "\\c{" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + "," + std::to_string(c.a) + "}";
            }
        }

        std::string lineMarkup(const std::string& text, size_t start, size_t end, const std::vector<Span>& spans, const std::function<Rgba(SpanKind)>& colourOf)
        {
            end = std::min(end, text.size());
            std::string out;
            // A line with no span (empty, or spans that miss the range) is drawn plain
            std::vector<Span> parts;
            for (const Span& s : spans)
            {
                const size_t from = std::max(s.start, start);
                const size_t to = std::min(s.end, end);
                if (from < to)
                    parts.push_back({from, to, s.kind});
            }
            if (parts.empty() and start < end)
                parts.push_back({start, end, SpanKind::Plain});

            for (const Span& p : parts)
            {
                const std::string colour = tag(colourOf(p.kind));
                out += colour;
                for (size_t i = p.start; i < p.end; ++i)
                {
                    out.push_back(text[i]);
                    if (text[i] == '\\')
                        out += colour;
                }
            }
            return out;
        }
    }
}
