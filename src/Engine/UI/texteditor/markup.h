#pragma once

#include "markdown.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pg
{
    namespace texteditor
    {
        struct Rgba
        {
            uint8_t r = 255, g = 255, b = 255, a = 255;
        };

        /**
         * The bytes [start, end) of a line as TTFText markup: a colour tag \c{r,g,b,a} before each
         * span's part, colour from `colourOf`. TTFText reads "\n" and "\c{" as markup and has no
         * escape: each backslash of the text is written as a backslash followed by a colour tag
         * (the parser then keeps the backslash and eats the tag), so the text is drawn as written.
         */
        std::string lineMarkup(const std::string& text, size_t start, size_t end, const std::vector<Span>& spans, const std::function<Rgba(SpanKind)>& colourOf);
    }
}
