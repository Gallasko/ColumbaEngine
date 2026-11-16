#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pg
{
    namespace texteditor
    {
        // A place in the text: a line and a byte column in it. A column never falls inside a
        // UTF-8 code point
        struct TextPos
        {
            size_t line = 0;
            size_t column = 0;

            bool operator==(const TextPos& other) const { return line == other.line and column == other.column; }
            bool operator!=(const TextPos& other) const { return not (*this == other); }
            bool operator<(const TextPos& other) const { return line < other.line or (line == other.line and column < other.column); }
        };

        /**
         * The text of an editor: its lines, the cursor, the selection (from an anchor to the
         * cursor), the edits and the moves. Plain C++, no rendering: the widget measures and draws.
         *
         * Every edit replaces the selection first when there is one. A move with `extend` keeps or
         * starts the selection; without, it clears it.
         */
        class TextBuffer
        {
        public:
            explicit TextBuffer(const std::string& text = "");

            void setText(const std::string& text);
            std::string text() const;
            const std::vector<std::string>& lines() const { return lines_; }

            TextPos cursor() const { return cursor_; }
            std::optional<TextPos> anchor() const { return anchor_; }
            bool hasSelection() const { return anchor_.has_value() and *anchor_ != cursor_; }
            // The selection, ordered (from <= to); cursor..cursor when there is none
            std::pair<TextPos, TextPos> selection() const;
            std::string selectedText() const;

            // The cursor placed at `pos` (clamped to the text and to a code point boundary)
            void setCursor(TextPos pos, bool extend);

            // Edits
            void insert(const std::string& text);
            void newLine();
            void backspace(bool word);
            void del(bool word);
            void deleteSelection();

            // Moves
            void left(bool word, bool extend);
            void right(bool word, bool extend);
            void up(bool extend);
            void down(bool extend);
            void lineStart(bool extend);
            void lineEnd(bool extend);
            void docStart(bool extend);
            void docEnd(bool extend);
            void selectAll();

            // Grows on every edit (a move leaves it)
            size_t revision() const { return revision_; }

            // UTF-8 helpers, on a line of the buffer
            size_t prevBoundary(size_t line, size_t column) const;
            size_t nextBoundary(size_t line, size_t column) const;
            size_t clampColumn(size_t line, size_t column) const;
            // Where a word move would go from `column` (within the line)
            size_t wordLeft(size_t line, size_t column) const;
            size_t wordRight(size_t line, size_t column) const;

        private:
            void moveTo(TextPos pos, bool extend, bool keepWanted = false);
            void removeRange(TextPos from, TextPos to);
            void edited();

            std::vector<std::string> lines_;
            TextPos cursor_;
            std::optional<TextPos> anchor_;
            // The column up and down try to keep, set by the first vertical move
            std::optional<size_t> wantedColumn_;
            size_t revision_ = 0;
        };
    }
}
