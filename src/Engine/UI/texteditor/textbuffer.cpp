#include "textbuffer.h"

#include <algorithm>

namespace pg
{
    namespace texteditor
    {
        namespace
        {
            bool isContinuation(unsigned char c) { return (c & 0xC0) == 0x80; }

            // Letters, digits, '_' and anything not ASCII (accented letters, symbols of words)
            bool isWordByte(unsigned char c)
            {
                return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or c == '_' or c >= 0x80;
            }

            std::vector<std::string> splitLines(const std::string& text)
            {
                std::vector<std::string> lines(1);
                for (char c : text)
                {
                    if (c == '\r')
                        continue;
                    if (c == '\n')
                        lines.emplace_back();
                    else
                        lines.back().push_back(c);
                }
                return lines;
            }
        }

        TextBuffer::TextBuffer(const std::string& text)
        {
            setText(text);
        }

        void TextBuffer::setText(const std::string& text)
        {
            lines_ = splitLines(text);
            cursor_ = {0, 0};
            anchor_.reset();
            wantedColumn_.reset();
            edited();
        }

        std::string TextBuffer::text() const
        {
            std::string out;
            for (size_t i = 0; i < lines_.size(); ++i)
            {
                if (i > 0)
                    out.push_back('\n');
                out += lines_[i];
            }
            return out;
        }

        std::pair<TextPos, TextPos> TextBuffer::selection() const
        {
            if (not anchor_)
                return {cursor_, cursor_};
            if (*anchor_ < cursor_)
                return {*anchor_, cursor_};
            return {cursor_, *anchor_};
        }

        std::string TextBuffer::selectedText() const
        {
            if (not hasSelection())
                return "";
            auto [from, to] = selection();
            if (from.line == to.line)
                return lines_[from.line].substr(from.column, to.column - from.column);

            std::string out = lines_[from.line].substr(from.column);
            for (size_t l = from.line + 1; l < to.line; ++l)
                out += "\n" + lines_[l];
            out += "\n" + lines_[to.line].substr(0, to.column);
            return out;
        }

        size_t TextBuffer::prevBoundary(size_t line, size_t column) const
        {
            const std::string& s = lines_[line];
            if (column == 0)
                return 0;
            size_t c = std::min(column, s.size()) - 1;
            while (c > 0 and isContinuation(static_cast<unsigned char>(s[c])))
                --c;
            return c;
        }

        size_t TextBuffer::nextBoundary(size_t line, size_t column) const
        {
            const std::string& s = lines_[line];
            if (column >= s.size())
                return s.size();
            size_t c = column + 1;
            while (c < s.size() and isContinuation(static_cast<unsigned char>(s[c])))
                ++c;
            return c;
        }

        size_t TextBuffer::clampColumn(size_t line, size_t column) const
        {
            const std::string& s = lines_[line];
            size_t c = std::min(column, s.size());
            while (c > 0 and c < s.size() and isContinuation(static_cast<unsigned char>(s[c])))
                --c;
            return c;
        }

        size_t TextBuffer::wordLeft(size_t line, size_t column) const
        {
            const std::string& s = lines_[line];
            size_t c = std::min(column, s.size());
            while (c > 0 and not isWordByte(static_cast<unsigned char>(s[c - 1])))
                --c;
            while (c > 0 and isWordByte(static_cast<unsigned char>(s[c - 1])))
                --c;
            return clampColumn(line, c);
        }

        size_t TextBuffer::wordRight(size_t line, size_t column) const
        {
            const std::string& s = lines_[line];
            size_t c = std::min(column, s.size());
            while (c < s.size() and not isWordByte(static_cast<unsigned char>(s[c])))
                ++c;
            while (c < s.size() and isWordByte(static_cast<unsigned char>(s[c])))
                ++c;
            return clampColumn(line, c);
        }

        void TextBuffer::edited()
        {
            ++revision_;
        }

        void TextBuffer::moveTo(TextPos pos, bool extend, bool keepWanted)
        {
            pos.line = std::min(pos.line, lines_.size() - 1);
            pos.column = clampColumn(pos.line, pos.column);
            if (extend)
            {
                if (not anchor_)
                    anchor_ = cursor_;
            }
            else
            {
                anchor_.reset();
            }
            cursor_ = pos;
            if (not keepWanted)
                wantedColumn_.reset();
        }

        void TextBuffer::setCursor(TextPos pos, bool extend)
        {
            moveTo(pos, extend);
        }

        void TextBuffer::removeRange(TextPos from, TextPos to)
        {
            if (from.line == to.line)
            {
                lines_[from.line].erase(from.column, to.column - from.column);
            }
            else
            {
                lines_[from.line] = lines_[from.line].substr(0, from.column) + lines_[to.line].substr(to.column);
                lines_.erase(lines_.begin() + static_cast<long>(from.line) + 1, lines_.begin() + static_cast<long>(to.line) + 1);
            }
            cursor_ = from;
            anchor_.reset();
            wantedColumn_.reset();
        }

        void TextBuffer::deleteSelection()
        {
            if (not hasSelection())
            {
                anchor_.reset();
                return;
            }
            auto [from, to] = selection();
            removeRange(from, to);
            edited();
        }

        void TextBuffer::insert(const std::string& text)
        {
            if (hasSelection())
                deleteSelection();
            anchor_.reset();
            wantedColumn_.reset();

            std::vector<std::string> parts = splitLines(text);
            std::string& line = lines_[cursor_.line];
            const std::string tail = line.substr(cursor_.column);
            line = line.substr(0, cursor_.column) + parts[0];

            if (parts.size() == 1)
            {
                cursor_.column = line.size();
                line += tail;
            }
            else
            {
                std::vector<std::string> added(parts.begin() + 1, parts.end());
                const size_t lastColumn = added.back().size();
                added.back() += tail;
                lines_.insert(lines_.begin() + static_cast<long>(cursor_.line) + 1, added.begin(), added.end());
                cursor_ = {cursor_.line + added.size(), lastColumn};
            }
            edited();
        }

        void TextBuffer::newLine()
        {
            insert("\n");
        }

        void TextBuffer::backspace(bool word)
        {
            if (hasSelection())
            {
                deleteSelection();
                return;
            }
            anchor_.reset();
            if (cursor_.column == 0)
            {
                if (cursor_.line == 0)
                    return;
                const TextPos from{cursor_.line - 1, lines_[cursor_.line - 1].size()};
                removeRange(from, cursor_);
                edited();
                return;
            }
            const size_t to = word ? wordLeft(cursor_.line, cursor_.column) : prevBoundary(cursor_.line, cursor_.column);
            removeRange({cursor_.line, to}, cursor_);
            edited();
        }

        void TextBuffer::del(bool word)
        {
            if (hasSelection())
            {
                deleteSelection();
                return;
            }
            anchor_.reset();
            const std::string& line = lines_[cursor_.line];
            if (cursor_.column >= line.size())
            {
                if (cursor_.line + 1 >= lines_.size())
                    return;
                removeRange(cursor_, {cursor_.line + 1, 0});
                edited();
                return;
            }
            const size_t to = word ? wordRight(cursor_.line, cursor_.column) : nextBoundary(cursor_.line, cursor_.column);
            const TextPos at = cursor_;
            removeRange(at, {at.line, to});
            edited();
        }

        void TextBuffer::left(bool word, bool extend)
        {
            if (not extend and hasSelection())
            {
                moveTo(selection().first, false);
                return;
            }
            if (cursor_.column == 0)
            {
                if (cursor_.line > 0)
                    moveTo({cursor_.line - 1, lines_[cursor_.line - 1].size()}, extend);
                else
                    moveTo(cursor_, extend);
                return;
            }
            const size_t c = word ? wordLeft(cursor_.line, cursor_.column) : prevBoundary(cursor_.line, cursor_.column);
            moveTo({cursor_.line, c}, extend);
        }

        void TextBuffer::right(bool word, bool extend)
        {
            if (not extend and hasSelection())
            {
                moveTo(selection().second, false);
                return;
            }
            if (cursor_.column >= lines_[cursor_.line].size())
            {
                if (cursor_.line + 1 < lines_.size())
                    moveTo({cursor_.line + 1, 0}, extend);
                else
                    moveTo(cursor_, extend);
                return;
            }
            const size_t c = word ? wordRight(cursor_.line, cursor_.column) : nextBoundary(cursor_.line, cursor_.column);
            moveTo({cursor_.line, c}, extend);
        }

        void TextBuffer::up(bool extend)
        {
            const size_t wanted = wantedColumn_.value_or(cursor_.column);
            if (cursor_.line == 0)
            {
                moveTo({0, 0}, extend);
                return;
            }
            moveTo({cursor_.line - 1, wanted}, extend, true);
            wantedColumn_ = wanted;
        }

        void TextBuffer::down(bool extend)
        {
            const size_t wanted = wantedColumn_.value_or(cursor_.column);
            if (cursor_.line + 1 >= lines_.size())
            {
                moveTo({cursor_.line, lines_[cursor_.line].size()}, extend);
                return;
            }
            moveTo({cursor_.line + 1, wanted}, extend, true);
            wantedColumn_ = wanted;
        }

        void TextBuffer::lineStart(bool extend)
        {
            moveTo({cursor_.line, 0}, extend);
        }

        void TextBuffer::lineEnd(bool extend)
        {
            moveTo({cursor_.line, lines_[cursor_.line].size()}, extend);
        }

        void TextBuffer::docStart(bool extend)
        {
            moveTo({0, 0}, extend);
        }

        void TextBuffer::docEnd(bool extend)
        {
            moveTo({lines_.size() - 1, lines_.back().size()}, extend);
        }

        void TextBuffer::selectAll()
        {
            anchor_ = TextPos{0, 0};
            cursor_ = {lines_.size() - 1, lines_.back().size()};
            wantedColumn_.reset();
        }
    }
}
