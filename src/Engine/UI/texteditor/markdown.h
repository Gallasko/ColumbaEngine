#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace pg
{
    namespace texteditor
    {
        enum class SpanKind { Plain, Heading, Bold, Italic, InlineCode, Fence, CodeBlock, Link, ListMarker, WriterMarker };

        // The bytes [start, end) of a line, of one kind
        struct Span
        {
            size_t start = 0;
            size_t end = 0;
            SpanKind kind = SpanKind::Plain;
        };

        /**
         * The Markdown kinds of each line, as spans covering the line in order with no gap (an
         * empty line has none). A line starting with ``` is a Fence and opens or closes code; the
         * lines inside are CodeBlock whole. Otherwise: "#".."######" + space is a Heading line;
         * a list marker ("- ", "* ", "+ ", "12. ") after leading spaces; `code`, **bold**, *italic*
         * and _italic_ (not inside a word), [text](url), and the writing tool's markers
         * <!-- visual: ... --> and <!-- number: ... --> (unclosed: to the line's end).
         */
        std::vector<std::vector<Span>> markdownSpans(const std::vector<std::string>& lines);
    }
}
