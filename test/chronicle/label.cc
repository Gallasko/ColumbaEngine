#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/label.h"
#include "UI/paint.h"

#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
#include "2D/simple2dobject.h"
#include "2D/position.h"
#include "UI/prefab.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            const std::string ELL = "\xE2\x80\xA6";
            const std::string FOX = "The quick brown fox jumps over the lazy dog";

            bool endsWithEllipsis(const std::string& s)
            {
                return s.size() >= ELL.size() and s.compare(s.size() - ELL.size(), ELL.size(), ELL) == 0;
            }

            std::string paragraph(int words)
            {
                std::string p;
                for (int i = 0; i < words; ++i)
                    p += "lorem ";
                return p;
            }

            struct LabelFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                TextStyles styles;

                LabelFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<PrefabSystem>();
                    ecs.createSystem<PaintSystem>(&tokens);
                    styles = TextStyles::fromTokens(tokens);
                    styles.registerAll(ttf, "fonts");
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                CompRef<TTFText> ttfOf(EntityRef e) { return ecs.getEntity(e.id)->get<TTFText>(); }
                CompRef<UiAnchor> anchorOf(EntityRef e) { return ecs.getEntity(e.id)->get<UiAnchor>(); }
            };
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, grow_width_equals_measure)
        {
            MockLogger logger; LabelFixture s;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, {"body", "STR 14"});
            s.settle();
            EXPECT_NEAR(label.boxWidth(&s.ecs), s.ttf->measureText("chr-body", "STR 14").width, 0.01f);
            EXPECT_FLOAT_EQ(label.boxHeight(&s.ecs), 24.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, grow_height_is_token_line_height_not_font)
        {
            MockLogger logger; LabelFixture s;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, {"title", "North Forest"});
            s.settle();
            EXPECT_FLOAT_EQ(label.boxHeight(&s.ecs), 34.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, two_labels_share_line_height)
        {
            MockLogger logger; LabelFixture s;
            auto a = makeLabel(&s.ecs, s.tokens, s.styles, {"body", "x"});
            auto b = makeLabel(&s.ecs, s.tokens, s.styles, {"body", "Hg|"});
            s.settle();
            EXPECT_FLOAT_EQ(a.boxHeight(&s.ecs), b.boxHeight(&s.ecs));
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, right_aligned_edge_equals_anchor)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "figure"; spec.text = "412"; spec.align = Align::Right;
            spec.overflow = Overflow::Ellipsis; spec.width = 300.0f;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            s.settle();
            auto tp = s.pos(label.text);
            auto bp = s.pos(label.box);
            EXPECT_NEAR(tp->x + s.ttfOf(label.text)->textWidth, bp->x + 300.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, centre_aligned)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = "Training"; spec.align = Align::Centre;
            spec.overflow = Overflow::Ellipsis; spec.width = 300.0f;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            s.settle();
            auto tp = s.pos(label.text);
            auto bp = s.pos(label.box);
            EXPECT_NEAR(tp->x + s.ttfOf(label.text)->textWidth / 2.0f, bp->x + 150.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, ellipsis_fits)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = FOX; spec.overflow = Overflow::Ellipsis; spec.width = 120.0f;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            EXPECT_TRUE(endsWithEllipsis(label.fitted));
            const TextStyle& style = s.styles.get("body");
            EXPECT_LE(s.ttf->measureText("chr-body", label.fitted, 1, 0, 0, style.letterSpacingPx).width, 120.0f);
            EXPECT_GT(s.ttf->measureText("chr-body", FOX, 1, 0, 0, style.letterSpacingPx).width, 120.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, ellipsis_no_trailing_space)
        {
            MockLogger logger; LabelFixture s;
            const TextStyle& style = s.styles.get("body");
            // A width between "one two…" and "one two t…", so the fit lands on the word
            // boundary and must trim the trailing space before the ellipsis.
            const float lo = s.ttf->measureText("chr-body", "one two" + ELL, 1, 0, 0, style.letterSpacingPx).width;
            const float hi = s.ttf->measureText("chr-body", "one two t" + ELL, 1, 0, 0, style.letterSpacingPx).width;
            const float width = (lo + hi) / 2.0f;
            LabelSpec spec; spec.style = "body"; spec.text = "one two three"; spec.overflow = Overflow::Ellipsis; spec.width = width;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            EXPECT_EQ(label.fitted, "one two" + ELL);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, ellipsis_too_narrow_is_empty)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = FOX; spec.overflow = Overflow::Ellipsis; spec.width = 2.0f;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            EXPECT_EQ(label.fitted, "");
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, wrap_height_counts_lines)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = paragraph(60); spec.overflow = Overflow::Wrap; spec.width = 300.0f;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            s.settle();
            const int lines = countLines(*s.ttf, s.styles.get("body"), label.fitted, 300.0f);
            EXPECT_GE(lines, 3);
            EXPECT_FLOAT_EQ(label.boxHeight(&s.ecs), lines * 24.0f);
            EXPECT_TRUE(s.ttfOf(label.text)->wrap);
            EXPECT_NEAR(s.pos(label.text)->width, 300.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, wrap_max_lines_truncates)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = paragraph(60); spec.overflow = Overflow::Wrap;
            spec.width = 300.0f; spec.maxLines = 2;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            s.settle();
            EXPECT_FLOAT_EQ(label.boxHeight(&s.ecs), 48.0f);
            EXPECT_TRUE(endsWithEllipsis(label.fitted));
            EXPECT_EQ(countLines(*s.ttf, s.styles.get("body"), label.fitted, 300.0f), 2);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, set_text_regrows)
        {
            MockLogger logger; LabelFixture s;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, {"body", "a"});
            s.settle();
            const float before = label.boxWidth(&s.ecs);
            label.setText(&s.ecs, s.styles, "a much longer line");
            s.settle();
            EXPECT_GT(label.boxWidth(&s.ecs), before);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, set_width_refits)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = FOX; spec.overflow = Overflow::Ellipsis; spec.width = 300.0f;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            label.setWidth(&s.ecs, s.styles, 80.0f);
            EXPECT_TRUE(endsWithEllipsis(label.fitted));
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, colour_is_token_and_repaints)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = "x"; spec.colour = "status-gain";
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);

            auto verdigrisDay = s.tokens.colour("verdigris", Theme::Day);
            EXPECT_FLOAT_EQ(s.ttfOf(label.text)->colors.x, verdigrisDay.x);

            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.ecs.executeOnce();

            auto verdigrisCandle = s.tokens.colour("verdigris", Theme::Candle);
            EXPECT_FLOAT_EQ(s.ttfOf(label.text)->colors.x, verdigrisCandle.x);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, z_is_parent_plus_one)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = "x"; spec.z = 20;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            s.settle();
            EXPECT_FLOAT_EQ(s.pos(label.box)->z, 20.0f);
            EXPECT_FLOAT_EQ(s.pos(label.text)->z, 21.0f);

            s.pos(label.box)->setZ(30.0f);
            s.settle();
            EXPECT_FLOAT_EQ(s.pos(label.text)->z, 31.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, wrap_ignores_align_with_warning)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = paragraph(60); spec.overflow = Overflow::Wrap;
            spec.width = 300.0f; spec.align = Align::Right;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            s.settle();
            auto a = s.anchorOf(label.text);
            EXPECT_TRUE(a->hasLeftAnchor);
            EXPECT_TRUE(a->hasRightAnchor);
            EXPECT_FALSE(a->hasHorizontalCenter);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, letter_spacing_in_fit)
        {
            MockLogger logger; LabelFixture s;
            // A width that fits "REQUIRES" WITHOUT tracking but not WITH the label style's tracking.
            const float untracked = s.ttf->measureText("chr-label", "REQUIRES", 1, 0, 0, 0.0f).width;
            LabelSpec spec; spec.style = "label"; spec.text = "REQUIRES"; spec.overflow = Overflow::Ellipsis; spec.width = untracked;
            auto label = makeLabel(&s.ecs, s.tokens, s.styles, spec);
            EXPECT_TRUE(endsWithEllipsis(label.fitted));
        }
    }
}
