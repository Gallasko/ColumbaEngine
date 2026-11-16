#include "stdafx.h"

#include "gtest/gtest.h"

#include "UI/texteditor/textbuffer.h"
#include "UI/texteditor/wrap.h"

namespace pg
{
    namespace test
    {
        using texteditor::TextBuffer;
        using texteditor::TextPos;

        // ---------------------------------------------------------------- TextBuffer: edits

        TEST(TextBuffer, EmptyBufferHasOneEmptyLine)
        {
            TextBuffer b;
            EXPECT_EQ(b.lines().size(), 1u);
            EXPECT_EQ(b.text(), "");
            EXPECT_EQ(b.cursor(), (TextPos{0, 0}));
        }

        TEST(TextBuffer, CrLfBecomesLf)
        {
            TextBuffer b("a\r\nb");
            ASSERT_EQ(b.lines().size(), 2u);
            EXPECT_EQ(b.lines()[0], "a");
            EXPECT_EQ(b.lines()[1], "b");
            EXPECT_EQ(b.text(), "a\nb");
        }

        TEST(TextBuffer, InsertAndNewLine)
        {
            TextBuffer b;
            b.insert("ab");
            b.newLine();
            b.insert("c");
            EXPECT_EQ(b.text(), "ab\nc");
            EXPECT_EQ(b.cursor(), (TextPos{1, 1}));
        }

        TEST(TextBuffer, InsertMultiLineText)
        {
            TextBuffer b("ab");
            b.setCursor({0, 1}, false);
            b.insert("x\ny\nz");
            EXPECT_EQ(b.text(), "ax\ny\nzb");
            EXPECT_EQ(b.cursor(), (TextPos{2, 1}));
        }

        TEST(TextBuffer, BackspaceJoinsLines)
        {
            TextBuffer b("ab\ncd");
            b.setCursor({1, 0}, false);
            b.backspace(false);
            EXPECT_EQ(b.text(), "abcd");
            EXPECT_EQ(b.cursor(), (TextPos{0, 2}));
        }

        TEST(TextBuffer, BackspaceWord)
        {
            TextBuffer b("foo bar  ");
            b.docEnd(false);
            b.backspace(true);
            EXPECT_EQ(b.text(), "foo ");
        }

        TEST(TextBuffer, DeleteAtLineEndJoins)
        {
            TextBuffer b("ab\ncd");
            b.setCursor({0, 2}, false);
            b.del(false);
            EXPECT_EQ(b.text(), "abcd");
            EXPECT_EQ(b.cursor(), (TextPos{0, 2}));
        }

        TEST(TextBuffer, DeleteWord)
        {
            TextBuffer b("foo bar baz");
            b.setCursor({0, 4}, false);
            b.del(true);
            EXPECT_EQ(b.text(), "foo  baz");
        }

        TEST(TextBuffer, Utf8NeverSplit)
        {
            // é (2 bytes), — (3 bytes), 😀 (4 bytes)
            const std::string text = "\xC3\xA9\xE2\x80\x94\xF0\x9F\x98\x80";
            TextBuffer b(text);
            b.right(false, false);
            EXPECT_EQ(b.cursor().column, 2u);
            b.right(false, false);
            EXPECT_EQ(b.cursor().column, 5u);
            b.right(false, false);
            EXPECT_EQ(b.cursor().column, 9u);
            b.backspace(false);
            EXPECT_EQ(b.text(), "\xC3\xA9\xE2\x80\x94");
            b.setCursor({0, 1}, false);
            EXPECT_TRUE(b.cursor().column == 0u or b.cursor().column == 2u);
            b.setCursor({0, 3}, false);
            EXPECT_TRUE(b.cursor().column == 2u or b.cursor().column == 5u);
        }

        TEST(TextBuffer, WordMoves)
        {
            // "foo, bar_baz é"
            TextBuffer b("foo, bar_baz \xC3\xA9");
            b.right(true, false);
            EXPECT_EQ(b.cursor().column, 3u);
            b.right(true, false);
            EXPECT_EQ(b.cursor().column, 12u);
            b.docEnd(false);
            b.left(true, false);
            EXPECT_EQ(b.cursor().column, 13u);
            b.left(true, false);
            EXPECT_EQ(b.cursor().column, 5u);
        }

        TEST(TextBuffer, UpDownKeepColumn)
        {
            TextBuffer b("abcdef\nab\nabcdef");
            b.setCursor({0, 5}, false);
            b.down(false);
            EXPECT_EQ(b.cursor(), (TextPos{1, 2}));
            b.down(false);
            EXPECT_EQ(b.cursor(), (TextPos{2, 5}));
            b.up(false);
            b.up(false);
            EXPECT_EQ(b.cursor(), (TextPos{0, 5}));
        }

        TEST(TextBuffer, LineStartEndAndDocMoves)
        {
            TextBuffer b("abc\ndef");
            b.setCursor({1, 1}, false);
            b.lineStart(false);
            EXPECT_EQ(b.cursor(), (TextPos{1, 0}));
            b.lineEnd(false);
            EXPECT_EQ(b.cursor(), (TextPos{1, 3}));
            b.docStart(false);
            EXPECT_EQ(b.cursor(), (TextPos{0, 0}));
            b.docEnd(false);
            EXPECT_EQ(b.cursor(), (TextPos{1, 3}));
        }

        TEST(TextBuffer, SelectionByShift)
        {
            TextBuffer b("abc");
            b.right(false, true);
            b.right(false, true);
            EXPECT_TRUE(b.hasSelection());
            EXPECT_EQ(b.selectedText(), "ab");
            b.insert("X");
            EXPECT_EQ(b.text(), "Xc");
            EXPECT_FALSE(b.hasSelection());
        }

        TEST(TextBuffer, MoveWithoutShiftClearsSelection)
        {
            TextBuffer b("abc");
            b.right(false, true);
            b.right(false, false);
            EXPECT_FALSE(b.hasSelection());
        }

        TEST(TextBuffer, SelectAllAndDelete)
        {
            TextBuffer b("ab\ncd");
            b.selectAll();
            EXPECT_EQ(b.selectedText(), "ab\ncd");
            b.deleteSelection();
            EXPECT_EQ(b.text(), "");
            EXPECT_EQ(b.lines().size(), 1u);
        }

        TEST(TextBuffer, SelectionAcrossLines)
        {
            TextBuffer b("ab\ncd");
            b.setCursor({0, 1}, false);
            b.setCursor({1, 1}, true);
            EXPECT_EQ(b.selectedText(), "b\nc");
            auto [from, to] = b.selection();
            EXPECT_EQ(from, (TextPos{0, 1}));
            EXPECT_EQ(to, (TextPos{1, 1}));
        }

        TEST(TextBuffer, BackspaceOnSelectionDeletesIt)
        {
            TextBuffer b("hello");
            b.setCursor({0, 1}, false);
            b.setCursor({0, 4}, true);
            b.backspace(false);
            EXPECT_EQ(b.text(), "ho");
            EXPECT_EQ(b.cursor(), (TextPos{0, 1}));
        }

        TEST(TextBuffer, RevisionGrowsOnEdits)
        {
            TextBuffer b("a");
            const size_t r = b.revision();
            b.insert("b");
            EXPECT_GT(b.revision(), r);
            const size_t r2 = b.revision();
            b.right(false, false);
            EXPECT_EQ(b.revision(), r2);
        }

        // ---------------------------------------------------------------- TextBuffer: undo

        namespace
        {
            void typeEach(TextBuffer& b, const std::string& text)
            {
                for (char c : text)
                    b.insert(std::string(1, c));
            }
        }

        TEST(TextBuffer, TypingAWordIsOneUndo)
        {
            TextBuffer b;
            typeEach(b, "hello");
            EXPECT_TRUE(b.undo());
            EXPECT_EQ(b.text(), "");
            EXPECT_FALSE(b.undo());
        }

        TEST(TextBuffer, SpaceStartsANewGroup)
        {
            TextBuffer b;
            typeEach(b, "ab cd");
            EXPECT_TRUE(b.undo());
            EXPECT_EQ(b.text(), "ab ");
            EXPECT_TRUE(b.undo());
            EXPECT_EQ(b.text(), "");
        }

        TEST(TextBuffer, MoveBreaksTheGroup)
        {
            TextBuffer b;
            typeEach(b, "ab");
            b.left(false, false);
            typeEach(b, "c");
            EXPECT_EQ(b.text(), "acb");
            EXPECT_TRUE(b.undo());
            EXPECT_EQ(b.text(), "ab");
            EXPECT_EQ(b.cursor(), (TextPos{0, 1}));
        }

        TEST(TextBuffer, PasteManyLinesIsOneUndo)
        {
            TextBuffer b("start");
            b.docEnd(false);
            b.insert("1\n2\n3\n4");
            EXPECT_EQ(b.lines().size(), 4u);
            EXPECT_TRUE(b.undo());
            EXPECT_EQ(b.text(), "start");
            EXPECT_EQ(b.cursor(), (TextPos{0, 5}));
        }

        TEST(TextBuffer, ReplaceSelectionThenUndo)
        {
            TextBuffer b("hello");
            b.selectAll();
            b.insert("X");
            EXPECT_EQ(b.text(), "X");
            EXPECT_TRUE(b.undo());
            EXPECT_EQ(b.text(), "hello");
            EXPECT_EQ(b.selectedText(), "hello");
        }

        TEST(TextBuffer, CutThenUndo)
        {
            TextBuffer b("hello");
            b.setCursor({0, 1}, false);
            b.setCursor({0, 4}, true);
            b.deleteSelection();
            EXPECT_EQ(b.text(), "ho");
            EXPECT_TRUE(b.undo());
            EXPECT_EQ(b.text(), "hello");
        }

        TEST(TextBuffer, RedoAfterUndo)
        {
            TextBuffer b;
            typeEach(b, "ab");
            b.undo();
            EXPECT_TRUE(b.redo());
            EXPECT_EQ(b.text(), "ab");
            EXPECT_EQ(b.cursor(), (TextPos{0, 2}));
            b.undo();
            b.insert("z");
            EXPECT_FALSE(b.redo());
        }

        TEST(TextBuffer, BackspacesGroupTogether)
        {
            TextBuffer b("abcd");
            b.docEnd(false);
            b.backspace(false);
            b.backspace(false);
            EXPECT_EQ(b.text(), "ab");
            EXPECT_TRUE(b.undo());
            EXPECT_EQ(b.text(), "abcd");
        }

        TEST(TextBuffer, BreakUndoGroupSplits)
        {
            TextBuffer b;
            typeEach(b, "ab");
            b.breakUndoGroup();
            typeEach(b, "c");
            EXPECT_TRUE(b.undo());
            EXPECT_EQ(b.text(), "ab");
        }

        TEST(TextBuffer, SetTextClearsHistory)
        {
            TextBuffer b;
            typeEach(b, "ab");
            b.setText("new");
            EXPECT_FALSE(b.undo());
            EXPECT_EQ(b.text(), "new");
        }

        // ---------------------------------------------------------------- Wrapping

        namespace
        {
            // 10 px a byte, counting its calls
            struct FixedMeasure
            {
                size_t calls = 0;
                texteditor::MeasureFn fn()
                {
                    return [this](const std::string& s) { ++calls; return static_cast<float>(s.size()) * 10.0f; };
                }
            };

            std::vector<std::string> piecesOf(const texteditor::WrapLayout& w, const std::vector<std::string>& lines)
            {
                std::vector<std::string> out;
                for (const auto& v : w.visual())
                    out.push_back(lines[v.line].substr(v.start, v.end - v.start));
                return out;
            }
        }

        TEST(Wrap, ShortLinesDoNotWrap)
        {
            FixedMeasure m;
            std::vector<std::string> lines{"abc", "de"};
            texteditor::WrapLayout w;
            w.rebuild(lines, 100, m.fn());
            EXPECT_EQ(w.visual().size(), 2u);
        }

        TEST(Wrap, WrapsAtLastSpace)
        {
            FixedMeasure m;
            std::vector<std::string> lines{"aaa bbb ccc"};
            texteditor::WrapLayout w;
            w.rebuild(lines, 65, m.fn());
            EXPECT_EQ(piecesOf(w, lines), (std::vector<std::string>{"aaa ", "bbb ", "ccc"}));
        }

        TEST(Wrap, LongWordBreaksAnywhere)
        {
            FixedMeasure m;
            std::vector<std::string> lines{"https://example.com/a/very/long/path"};
            texteditor::WrapLayout w;
            w.rebuild(lines, 100, m.fn());
            std::string joined;
            for (const auto& piece : piecesOf(w, lines))
            {
                EXPECT_LE(piece.size(), 10u);
                EXPECT_GE(piece.size(), 1u);
                joined += piece;
            }
            EXPECT_EQ(joined, lines[0]);
        }

        TEST(Wrap, Utf8WrapPoint)
        {
            FixedMeasure m;
            std::vector<std::string> lines{"\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9"};
            texteditor::WrapLayout w;
            w.rebuild(lines, 50, m.fn());
            for (const auto& v : w.visual())
            {
                EXPECT_EQ(v.start % 2, 0u);
                EXPECT_EQ(v.end % 2, 0u);
            }
            EXPECT_EQ(w.visual().size(), 3u);
        }

        TEST(Wrap, EmptyLineIsOneVisualLine)
        {
            FixedMeasure m;
            std::vector<std::string> lines{"a", "", "b"};
            texteditor::WrapLayout w;
            w.rebuild(lines, 100, m.fn());
            ASSERT_EQ(w.visual().size(), 3u);
            EXPECT_EQ(w.visual()[1].line, 1u);
            EXPECT_EQ(w.visual()[1].start, 0u);
            EXPECT_EQ(w.visual()[1].end, 0u);
        }

        TEST(Wrap, XAndPosRoundTrip)
        {
            FixedMeasure m;
            std::vector<std::string> lines{"aaa bbb ccc ddd", "", "x\xC3\xA9y zz"};
            texteditor::WrapLayout w;
            w.rebuild(lines, 65, m.fn());
            TextBuffer b("aaa bbb ccc ddd\n\nx\xC3\xA9y zz");
            for (size_t l = 0; l < lines.size(); ++l)
            {
                size_t c = 0;
                while (true)
                {
                    TextPos p{l, c};
                    const size_t vi = w.visualOf(p);
                    const float x = w.xOf(p, lines, m.fn());
                    EXPECT_EQ(w.posAt(vi, x, lines, m.fn()), p) << "line " << l << " column " << c;
                    if (c >= lines[l].size())
                        break;
                    c = b.nextBoundary(l, c);
                }
            }
        }

        TEST(Wrap, CacheSkipsUnchangedLines)
        {
            FixedMeasure m;
            std::vector<std::string> lines{"aaa bbb ccc", "ddd eee fff", "ggg"};
            texteditor::WrapLayout w;
            w.rebuild(lines, 65, m.fn());
            lines[1] = "changed text here";
            FixedMeasure second;
            w.rebuild(lines, 65, second.fn());
            FixedMeasure third;
            texteditor::WrapLayout fresh;
            std::vector<std::string> only{"changed text here"};
            fresh.rebuild(only, 65, third.fn());
            EXPECT_EQ(second.calls, third.calls);
        }
    }
}
