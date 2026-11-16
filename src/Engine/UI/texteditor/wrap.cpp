#include "wrap.h"

#include <cmath>

namespace pg
{
    namespace texteditor
    {
        namespace
        {
            bool isContinuation(unsigned char c) { return (c & 0xC0) == 0x80; }

            size_t nextCodePoint(const std::string& s, size_t at)
            {
                if (at >= s.size())
                    return s.size();
                size_t c = at + 1;
                while (c < s.size() and isContinuation(static_cast<unsigned char>(s[c])))
                    ++c;
                return c;
            }
        }

        WrapLayout::Pieces WrapLayout::breakLine(const std::string& text, float width, const MeasureFn& measure) const
        {
            Pieces pieces;
            if (text.empty())
            {
                pieces.emplace_back(0, 0);
                return pieces;
            }
            size_t start = 0;
            while (start < text.size())
            {
                size_t lastFit = start;
                size_t lastSpace = std::string::npos;
                size_t at = start;
                while (at < text.size())
                {
                    const size_t next = nextCodePoint(text, at);
                    if (measure(text.substr(start, next - start)) > width)
                        break;
                    lastFit = next;
                    if (text[next - 1] == ' ')
                        lastSpace = next;
                    at = next;
                }
                // Spaces past the width still end the line they follow
                while (lastFit < text.size() and text[lastFit] == ' ')
                {
                    ++lastFit;
                    lastSpace = lastFit;
                }
                size_t end = lastFit;
                if (lastFit < text.size() and lastSpace != std::string::npos and lastSpace > start)
                    end = lastSpace;
                if (end == start)
                    end = nextCodePoint(text, start);
                pieces.emplace_back(start, end);
                start = end;
            }
            return pieces;
        }

        void WrapLayout::rebuild(const std::vector<std::string>& lines, float width, const MeasureFn& measure)
        {
            if (width != cacheWidth_)
            {
                cache_.clear();
                cacheWidth_ = width;
            }
            // Keep the cache to the lines that exist, so it never grows without bound
            std::unordered_map<std::string, Pieces> kept;

            visual_.clear();
            firstVisual_.clear();
            for (size_t l = 0; l < lines.size(); ++l)
            {
                firstVisual_.push_back(visual_.size());
                auto it = cache_.find(lines[l]);
                Pieces pieces = it != cache_.end() ? it->second : breakLine(lines[l], width, measure);
                for (const auto& [start, end] : pieces)
                    visual_.push_back(VisualLine{l, start, end});
                kept[lines[l]] = std::move(pieces);
            }
            cache_ = std::move(kept);
        }

        size_t WrapLayout::visualOf(TextPos pos) const
        {
            if (firstVisual_.empty())
                return 0;
            const size_t line = pos.line < firstVisual_.size() ? pos.line : firstVisual_.size() - 1;
            size_t v = firstVisual_[line];
            while (v + 1 < visual_.size() and visual_[v + 1].line == line and visual_[v + 1].start <= pos.column)
                ++v;
            return v;
        }

        float WrapLayout::xOf(TextPos pos, const std::vector<std::string>& lines, const MeasureFn& measure) const
        {
            if (visual_.empty())
                return 0.0f;
            const VisualLine& v = visual_[visualOf(pos)];
            if (pos.column <= v.start)
                return 0.0f;
            return measure(lines[v.line].substr(v.start, pos.column - v.start));
        }

        TextPos WrapLayout::posAt(size_t visualIndex, float x, const std::vector<std::string>& lines, const MeasureFn& measure) const
        {
            if (visual_.empty())
                return {0, 0};
            if (visualIndex >= visual_.size())
                visualIndex = visual_.size() - 1;
            const VisualLine& v = visual_[visualIndex];
            const std::string& text = lines[v.line];
            // The end of a visual line that wraps is the start of the next one: not a place here
            const bool lastOfLine = visualIndex + 1 >= visual_.size() or visual_[visualIndex + 1].line != v.line;
            const size_t limit = lastOfLine ? v.end : (v.end > v.start ? v.end - 1 : v.end);

            size_t best = v.start;
            float bestDistance = std::fabs(x);
            size_t at = v.start;
            while (at < v.end)
            {
                const size_t next = nextCodePoint(text, at);
                if (next > limit)
                    break;
                const float d = std::fabs(measure(text.substr(v.start, next - v.start)) - x);
                if (d < bestDistance)
                {
                    bestDistance = d;
                    best = next;
                }
                at = next;
            }
            return {v.line, best};
        }
    }
}
