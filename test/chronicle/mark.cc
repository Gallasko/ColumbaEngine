#include "stdafx.h"

#include <cmath>

#include <gtest/gtest.h>

#include "UI/mark.h"
#include "UI/paint.h"
#include "Core/textstyle.h"

#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "2D/simple2dobject.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            // Engine systems + Chronicle paint + registered styles + the mark entries installed
            // without any SVG rasterisation (registerEntriesForTest), so mark/label geometry
            // tests run headless. The one test that exercises real files calls registerMarks.
            struct MarkFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                PaintSystem* paint = nullptr;
                TextStyles styles;

                MarkFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    paint = ecs.createSystem<PaintSystem>(&tokens);
                    styles = TextStyles::fromTokens(tokens);
                    styles.registerAll(ttf, "fonts");

                    installMarkEntries();
                }

                void installMarkEntries()
                {
                    std::vector<IconEntry> entries;
                    const int sizes[5] = {14, 16, 18, 24, 48};
                    for (const auto& name : markNames())
                        for (int s : sizes)
                            entries.push_back({name, s, {s, s}, {0.0f, 0.0f}, {0.1f, 0.1f}});
                    icons->registerEntriesForTest("chronicle", entries, 1024, 1024);

                    // registerEntriesForTest installs no texture; register a stub so render-call
                    // builds don't log "texture doesn't exist" (real registration needs GL).
                    renderer.registerTexture("IconAtlas_chronicle", OpenGLTexture{});
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                CompRef<IconComponent> iconOf(EntityRef e) { return ecs.getEntity(e.id)->get<IconComponent>(); }
                CompRef<TTFText> ttfOf(EntityRef e) { return ecs.getEntity(e.id)->get<TTFText>(); }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, twenty_seven_names)
        {
            MockLogger logger;

            EXPECT_EQ(markNames().size(), 27u);
            EXPECT_EQ(markNames().front(), "strength");
            EXPECT_EQ(markNames().back(), "cross");
            EXPECT_TRUE(isMarkName("time"));
            EXPECT_FALSE(isMarkName("clock"));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, size_for_style)
        {
            MockLogger logger;

            EXPECT_EQ(markSizeFor("body"), MarkSize::S16);
            EXPECT_EQ(markSizeFor("figure"), MarkSize::S18);
            EXPECT_EQ(markSizeFor("tick"), MarkSize::S14);
            EXPECT_EQ(markSizeFor("title"), MarkSize::S24);
            EXPECT_EQ(markSizeFor("versal"), MarkSize::S48);

            // Unknown style falls back to S16 with exactly one warning.
            EXPECT_EQ(markSizeFor("no-such-style"), MarkSize::S16);
            EXPECT_EQ(markSizeFor("no-such-style"), MarkSize::S16);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, register_marks_rasterises_real_files)
        {
            MockLogger logger;

            Tokens tokens = Tokens::load("chronicle/tokens.json");
            EntitySystem ecs;
            MasterRenderer renderer;
            ecs.createSystem<PositionComponentSystem>();
            auto* icons = ecs.createSystem<IconSystem>(&renderer);
            (void)icons;

            EXPECT_TRUE(registerMarks(&ecs, "icons"));

            auto small = makeIcon(&ecs, "chronicle", "time", 16);
            auto large = makeIcon(&ecs, "chronicle", "time", 48);
            ecs.executeOnce();

            EXPECT_NE(icons->getRenderCall(small.id), nullptr);
            EXPECT_NE(icons->getRenderCall(large.id), nullptr);

            const size_t materials = renderer.getNbMaterials();

            // Second call is a no-op: no re-registration, material count unchanged.
            EXPECT_TRUE(registerMarks(&ecs, "icons"));
            auto again = makeIcon(&ecs, "chronicle", "gold", 24);
            ecs.executeOnce();
            EXPECT_NE(icons->getRenderCall(again.id), nullptr);
            EXPECT_EQ(renderer.getNbMaterials(), materials);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, mark_is_square_at_size)
        {
            MockLogger logger;
            MarkFixture s;

            Mark mark = makeMark(&s.ecs, s.tokens, {"time", MarkSize::S24});

            EXPECT_FLOAT_EQ(s.pos(mark.entity)->width, 24.0f);
            EXPECT_FLOAT_EQ(s.pos(mark.entity)->height, 24.0f);
            EXPECT_EQ(s.iconOf(mark.entity)->iconSet, "chronicle");
            EXPECT_EQ(s.iconOf(mark.entity)->iconName, "time");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, unknown_name_draws_seal)
        {
            MockLogger logger;
            MarkFixture s;

            // Ignore any noise from fixture setup; count only what the unknown mark logs.
            logger.reset();

            Mark mark = makeMark(&s.ecs, s.tokens, {"clock"});
            EXPECT_EQ(s.iconOf(mark.entity)->iconName, "seal");
            EXPECT_EQ(logger.getNbError(), 1u);

            // Same unknown name again logs nothing more.
            Mark mark2 = makeMark(&s.ecs, s.tokens, {"clock"});
            EXPECT_EQ(s.iconOf(mark2.entity)->iconName, "seal");
            EXPECT_EQ(logger.getNbError(), 1u);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, mark_colour_repaints)
        {
            MockLogger logger;
            MarkFixture s;

            Mark mark = makeMark(&s.ecs, s.tokens, {"check", MarkSize::S16, "status-gain"});
            EXPECT_FLOAT_EQ(s.iconOf(mark.entity)->colors.x, s.tokens.colour("verdigris", Theme::Day).x);

            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.settle();

            EXPECT_FLOAT_EQ(s.iconOf(mark.entity)->colors.x, s.tokens.colour("verdigris", Theme::Candle).x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, set_size_keeps_square)
        {
            MockLogger logger;
            MarkFixture s;

            Mark mark = makeMark(&s.ecs, s.tokens, {"gold", MarkSize::S16});
            mark.setSize(&s.ecs, MarkSize::S48);

            EXPECT_FLOAT_EQ(s.pos(mark.entity)->width, 48.0f);
            EXPECT_FLOAT_EQ(s.pos(mark.entity)->height, 48.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, marked_label_geometry)
        {
            MockLogger logger;
            MarkFixture s;

            MarkedLabelSpec spec;
            spec.mark = "time";
            spec.label = {"figure", "+3 mo"};
            MarkedLabel ml = makeMarkedLabel(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            ASSERT_TRUE(ml.mark.has_value());
            EXPECT_FLOAT_EQ(s.pos(ml.mark->entity)->width, 18.0f);   // figure -> S18

            const float rootX = s.pos(ml.root)->x;
            const float rootY = s.pos(ml.root)->y;
            const float labelW = s.pos(ml.label.box)->width;

            EXPECT_NEAR(s.pos(ml.mark->entity)->x, rootX, 0.01f);
            EXPECT_NEAR(s.pos(ml.label.box)->x, rootX + 18.0f + 8.0f, 0.01f);
            EXPECT_NEAR(s.pos(ml.root)->width, 26.0f + labelW, 0.01f);
            EXPECT_NEAR(s.pos(ml.root)->height, 22.0f, 0.01f);       // figure line height

            const float markCentre = s.pos(ml.mark->entity)->y + s.pos(ml.mark->entity)->height / 2.0f;
            EXPECT_NEAR(markCentre, rootY + 11.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, no_mark_no_gap)
        {
            MockLogger logger;
            MarkFixture s;

            MarkedLabel a = makeMarkedLabel(&s.ecs, s.tokens, s.styles, {"", false, {"body", "x"}});
            s.settle();
            EXPECT_NEAR(s.pos(a.label.box)->x, s.pos(a.root)->x, 0.01f);
            EXPECT_NEAR(s.pos(a.root)->width, s.pos(a.label.box)->width, 0.01f);
            EXPECT_FALSE(a.mark.has_value());

            MarkedLabel b = makeMarkedLabel(&s.ecs, s.tokens, s.styles, {"", true, {"body", "x"}});
            s.settle();
            EXPECT_NEAR(s.pos(b.label.box)->x, s.pos(b.root)->x + 16.0f + 8.0f, 0.01f);  // body -> S16 reserved
            EXPECT_FALSE(b.mark.has_value());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, colour_is_shared)
        {
            MockLogger logger;
            MarkFixture s;

            MarkedLabel ml = makeMarkedLabel(&s.ecs, s.tokens, s.styles,
                {"check", false, {"body", "Strength 14", "status-gain"}});
            s.settle();

            const auto verdigris = s.tokens.colour("verdigris", Theme::Day);
            EXPECT_FLOAT_EQ(s.iconOf(ml.mark->entity)->colors.x, verdigris.x);
            EXPECT_FLOAT_EQ(s.ttfOf(ml.label.text)->colors.x, verdigris.x);

            ml.setColour(&s.ecs, "status-loss");
            const auto vermilion = s.tokens.colour("vermilion", Theme::Day);
            EXPECT_FLOAT_EQ(s.iconOf(ml.mark->entity)->colors.x, vermilion.x);
            EXPECT_FLOAT_EQ(s.ttfOf(ml.label.text)->colors.x, vermilion.x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, versal_pairs_48)
        {
            MockLogger logger;
            MarkFixture s;

            MarkedLabel ml = makeMarkedLabel(&s.ecs, s.tokens, s.styles, {"seal", false, {"versal", "A"}});
            s.settle();

            ASSERT_TRUE(ml.mark.has_value());
            EXPECT_FLOAT_EQ(s.pos(ml.mark->entity)->width, 48.0f);
            EXPECT_FLOAT_EQ(s.pos(ml.root)->height, 48.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(mark_test, z_bands)
        {
            MockLogger logger;
            MarkFixture s;

            MarkedLabelSpec spec;
            spec.mark = "gold";
            spec.label = {"body", "412 gold"};
            spec.z = 20;
            MarkedLabel ml = makeMarkedLabel(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            const float rootZ = s.pos(ml.root)->z;
            const float boxZ = s.pos(ml.label.box)->z;
            const float textZ = s.pos(ml.label.text)->z;
            const float markZ = s.pos(ml.mark->entity)->z;

            EXPECT_FLOAT_EQ(rootZ, 20.0f);
            EXPECT_FLOAT_EQ(boxZ, 20.0f);
            EXPECT_FLOAT_EQ(textZ, 21.0f);
            EXPECT_FLOAT_EQ(markZ, 21.0f);

            for (float z : {rootZ, boxZ, textZ, markZ})
                EXPECT_FLOAT_EQ(z, std::floor(z));
        }
    }
}
