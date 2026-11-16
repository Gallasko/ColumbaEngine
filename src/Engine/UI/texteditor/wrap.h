#pragma once

#include "textbuffer.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace pg
{
    namespace texteditor
    {
        // The pen x after drawing a string (TTFTextSystem::measureText(...).penEnd in the widget)
        using MeasureFn = std::function<float(const std::string&)>;

        // A line as drawn: the bytes [start, end) of buffer line `line`
        struct VisualLine
        {
            size_t line = 0;
            size_t start = 0;
            size_t end = 0;
        };

        /**
         * The buffer's lines wrapped at a width: break after the last space that fits; a word
         * longer than the width breaks at the last code point that fits (at least one code point a
         * visual line). Trailing spaces stay on the line they end. Lines already wrapped at this
         * width are not measured again.
         */
        class WrapLayout
        {
        public:
            void rebuild(const std::vector<std::string>& lines, float width, const MeasureFn& measure);

            const std::vector<VisualLine>& visual() const { return visual_; }

            // The visual line holding a position (a position at a wrap point is the start of the
            // next visual line)
            size_t visualOf(TextPos pos) const;
            // Its x within that visual line
            float xOf(TextPos pos, const std::vector<std::string>& lines, const MeasureFn& measure) const;
            // The boundary of a visual line nearest to x
            TextPos posAt(size_t visualIndex, float x, const std::vector<std::string>& lines, const MeasureFn& measure) const;

        private:
            using Pieces = std::vector<std::pair<size_t, size_t>>;
            Pieces breakLine(const std::string& text, float width, const MeasureFn& measure) const;

            std::vector<VisualLine> visual_;
            std::vector<size_t> firstVisual_;
            std::unordered_map<std::string, Pieces> cache_;
            float cacheWidth_ = -1.0f;
        };
    }
}
