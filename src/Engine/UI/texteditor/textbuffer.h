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

            // Undo and redo, by groups: a word typed with the spaces after it, a run of
            // backspaces, or any other single edit; a move, setCursor or breakUndoGroup ends the
            // group. False when there is nothing to undo or redo
            bool undo();
            bool redo();
            void breakUndoGroup() { groupOpen_ = false; }

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
            struct Snapshot
            {
                std::vector<std::string> lines;
                TextPos cursor;
                std::optional<TextPos> anchor;
            };
            struct UndoGroup
            {
                Snapshot before;
                Snapshot after;
            };
            enum class EditKind { Typing, Backspace, Other };

            Snapshot snapshot() const { return {lines_, cursor_, anchor_}; }
            void restore(const Snapshot& s);
            // Around every public edit: joins the edit to the open group or starts one
            void beginEdit(EditKind kind, bool startsWord);
            void endEdit(EditKind kind);

            void moveTo(TextPos pos, bool extend, bool keepWanted = false);
            void insertText(const std::string& text);
            void backspaceAt(bool word);
            void delAt(bool word);
            void removeRange(TextPos from, TextPos to);
            void edited();

            std::vector<std::string> lines_;
            TextPos cursor_;
            std::optional<TextPos> anchor_;
            // The column up and down try to keep, set by the first vertical move
            std::optional<size_t> wantedColumn_;
            size_t revision_ = 0;

            std::vector<UndoGroup> undo_;
            std::vector<UndoGroup> redo_;
            bool groupOpen_ = false;
            EditKind groupKind_ = EditKind::Other;
            bool lastTypedWordByte_ = false;
            int editDepth_ = 0;
        };
    }
}
