#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/label.h"

#include "ECS/entitysystem.h"
#include "UI/themesystem.h"
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
            const std::string FOX = "The quick brown fox jumps over the lazy dog";

            std::string paragraph(int words)
            {
                std::string p;
                for (int i = 0; i < words; ++i)
                    p += "lorem ";
                return p;
            }

            struct LabelFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                ThemeSystem* theme = nullptr;

                constant::Vector4D color(const std::string& token, const std::string& id = "") { return theme->theme().color(token, id.empty() ? theme->currentTheme() : id); }

                LabelFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<PrefabSystem>();
                    theme = ecs.createSystem<ThemeSystem>();
                    theme->loadTheme("chronicle/tokens.json", "fonts");
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                CompRef<TTFText> ttfOf(EntityRef e) { return ecs.getEntity(e.id)->get<TTFText>(); }

                // The label's layout constraint set, as the engine sees it.
                TextLayoutParams paramsOf(const Label& l)
                {
                    TextLayoutParams p;
                    p.maxWidth = l.spec.overflow != Overflow::Grow ? l.spec.width : 0.0f;
                    p.spacing = l.lineSpacingPx;
                    p.letterSpacing = l.letterSpacingPx;
                    p.overflow = l.spec.overflow;
                    p.align = l.spec.align;
                    p.maxLines = l.spec.maxLines;
                    return p;
                }
            };
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, grow_width_equals_measure)
        {
            MockLogger logger; LabelFixture s;
            auto label = makeLabel(&s.ecs, {"body", "STR 14"});
            s.settle();
            EXPECT_NEAR(s.pos(label.entity)->width, s.ttf->measureText("body", "STR 14").width, 0.01f);
            EXPECT_FLOAT_EQ(s.pos(label.entity)->height, 24.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, grow_height_is_token_line_height_not_font)
        {
            MockLogger logger; LabelFixture s;
            auto label = makeLabel(&s.ecs, {"title", "North Forest"});
            s.settle();
            EXPECT_FLOAT_EQ(s.pos(label.entity)->height, 34.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, two_labels_share_line_height)
        {
            MockLogger logger; LabelFixture s;
            auto a = makeLabel(&s.ecs, {"body", "x"});
            auto b = makeLabel(&s.ecs, {"body", "Hg|"});
            s.settle();
            EXPECT_FLOAT_EQ(s.pos(a.entity)->height, s.pos(b.entity)->height);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, right_aligned_edge_equals_box_edge)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "figure"; spec.text = "412"; spec.align = Align::Right;
            spec.overflow = Overflow::Ellipsis; spec.width = 300.0f;
            auto right = makeLabel(&s.ecs, spec);
            spec.align = Align::Left;
            auto left = makeLabel(&s.ecs, spec);
            s.settle();

            // Alignment happens inside the glyph layout: the right-aligned glyphs sit
            // exactly the free box space to the right of the left-aligned twin's.
            const float w = s.ttfOf(right.entity)->textWidth;
            const auto& rightGlyphs = s.ttf->entityGlyphTemplates[right.entity.id];
            const auto& leftGlyphs = s.ttf->entityGlyphTemplates[left.entity.id];
            ASSERT_FALSE(rightGlyphs.empty());
            ASSERT_EQ(rightGlyphs.size(), leftGlyphs.size());
            for (size_t i = 0; i < rightGlyphs.size(); ++i)
                EXPECT_NEAR(rightGlyphs[i].relX - leftGlyphs[i].relX, std::round(300.0f - w), 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, ellipsis_fits)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = FOX; spec.overflow = Overflow::Ellipsis; spec.width = 120.0f;
            auto label = makeLabel(&s.ecs, spec);
            s.settle();

            const TextMetrics metrics = s.ttf->measureText(label.fontAlias, FOX, s.paramsOf(label));
            EXPECT_TRUE(metrics.elided);
            EXPECT_EQ(metrics.lineCount, 1);

            EXPECT_GT(s.ttfOf(label.entity)->textWidth, 0.0f);
            EXPECT_LE(s.ttfOf(label.entity)->textWidth, 120.0f);
            const TextStyle& style = s.theme->style("body");
            EXPECT_GT(s.ttf->measureText("body", FOX, 1, 0, 0, style.letterSpacingPx).width, 120.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, wrap_height_counts_lines)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = paragraph(60); spec.overflow = Overflow::Wrap; spec.width = 300.0f;
            auto label = makeLabel(&s.ecs, spec);
            s.settle();
            const int lines = s.ttf->measureText(label.fontAlias, label.spec.text, s.paramsOf(label)).lineCount;
            EXPECT_GE(lines, 3);
            EXPECT_FLOAT_EQ(s.pos(label.entity)->height, lines * 24.0f);
            EXPECT_TRUE(s.ttfOf(label.entity)->overflow == pg::TextOverflow::Wrap);
            EXPECT_NEAR(s.pos(label.entity)->width, 300.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, wrap_max_lines_truncates)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = paragraph(60); spec.overflow = Overflow::Wrap;
            spec.width = 300.0f; spec.maxLines = 2;
            auto label = makeLabel(&s.ecs, spec);
            s.settle();
            EXPECT_FLOAT_EQ(s.pos(label.entity)->height, 48.0f);
            const TextMetrics metrics = s.ttf->measureText(label.fontAlias, label.spec.text, s.paramsOf(label));
            EXPECT_TRUE(metrics.elided);
            EXPECT_EQ(metrics.lineCount, 2);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, set_text_regrows)
        {
            MockLogger logger; LabelFixture s;
            auto label = makeLabel(&s.ecs, {"body", "a"});
            s.settle();
            const float before = s.pos(label.entity)->width;
            label.setText(&s.ecs, "a much longer line");
            // The label re-measures synchronously; callers size layouts on it at once.
            EXPECT_GT(s.pos(label.entity)->width, before);
            s.settle();
            EXPECT_GT(s.pos(label.entity)->width, before);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, set_width_refits)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = FOX; spec.overflow = Overflow::Ellipsis; spec.width = 300.0f;
            auto label = makeLabel(&s.ecs, spec);
            s.settle();
            label.setWidth(&s.ecs, 80.0f);
            s.settle();
            EXPECT_TRUE(s.ttf->measureText(label.fontAlias, FOX, s.paramsOf(label)).elided);
            EXPECT_LE(s.ttfOf(label.entity)->textWidth, 80.0f);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, color_is_token_and_repaints)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = "x"; spec.color = "status-gain";
            auto label = makeLabel(&s.ecs, spec);

            auto verdigrisDay = s.color("verdigris", "day");
            EXPECT_FLOAT_EQ(s.ttfOf(label.entity)->colors.x, verdigrisDay.x);

            s.theme->setTheme("candle");
            s.ecs.executeOnce();

            auto verdigrisCandle = s.color("verdigris", "candle");
            EXPECT_FLOAT_EQ(s.ttfOf(label.entity)->colors.x, verdigrisCandle.x);
        }

        // ----------------------------------------------------------------------------------------
        TEST(label_test, z_matches_spec)
        {
            MockLogger logger; LabelFixture s;
            LabelSpec spec; spec.style = "body"; spec.text = "x"; spec.z = 20;
            auto label = makeLabel(&s.ecs, spec);
            s.settle();
            EXPECT_FLOAT_EQ(s.pos(label.entity)->z, 20.0f);
        }
    }
}
