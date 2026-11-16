#include "stdafx.h"

#include "gtest/gtest.h"

#include "UI/texteditor/textbuffer.h"

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
    }
}
