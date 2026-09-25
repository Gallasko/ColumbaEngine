#include "stdafx.h"

#include <cmath>

#include <gtest/gtest.h>

#include "UI/ornament.h"
#include "UI/paint.h"
#include "Core/textstyle.h"

#include "ECS/entitysystem.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/prefab.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            struct OrnamentFixture
            {
                Tokens tokens = Tokens::load("chronicle/tokens.json");
                EntitySystem ecs;
                MasterRenderer renderer;
                TTFTextSystem* ttf = nullptr;
                IconSystem* icons = nullptr;
                PaintSystem* paint = nullptr;
                TextStyles styles;

                OrnamentFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ttf = ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    paint = ecs.createSystem<PaintSystem>(&tokens);
                    styles = TextStyles::fromTokens(tokens);
                    styles.registerAll(ttf, "fonts");

                    installOrnamentEntries();
                }

                void installOrnamentEntries()
                {
                    std::vector<IconEntry> e = {
                        {"knot", 22, {22, 22}, {0.0f, 0.0f}, {0.1f, 0.1f}},
                        {"flourish", 120, {120, 120}, {0.0f, 0.0f}, {0.1f, 0.1f}},
                        {"corner-tl", 28, {28, 28}, {0.0f, 0.0f}, {0.1f, 0.1f}},
                        {"corner-tr", 28, {28, 28}, {0.0f, 0.0f}, {0.1f, 0.1f}},
                        {"corner-bl", 28, {28, 28}, {0.0f, 0.0f}, {0.1f, 0.1f}},
                        {"corner-br", 28, {28, 28}, {0.0f, 0.0f}, {0.1f, 0.1f}},
                        {"versal-curls", 72, {72, 72}, {0.0f, 0.0f}, {0.1f, 0.1f}},
                    };
                    icons->registerEntriesForTest("chronicle-ornaments", e, 1024, 1024);
                    renderer.registerTexture("IconAtlas_chronicle-ornaments", OpenGLTexture{});
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                CompRef<Simple2DObject> s2d(EntityRef e) { return ecs.getEntity(e.id)->get<Simple2DObject>(); }
                CompRef<IconComponent> iconOf(EntityRef e) { return ecs.getEntity(e.id)->get<IconComponent>(); }
                CompRef<StrokeRect2DObject> strokeOf(EntityRef e) { return ecs.getEntity(e.id)->get<StrokeRect2DObject>(); }
                CompRef<TTFText> ttfOf(EntityRef e) { return ecs.getEntity(e.id)->get<TTFText>(); }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, register_ornaments_rasterises)
        {
            MockLogger logger;

            Tokens tokens = Tokens::load("chronicle/tokens.json");
            EntitySystem ecs;
            MasterRenderer renderer;
            ecs.createSystem<PositionComponentSystem>();
            auto* icons = ecs.createSystem<IconSystem>(&renderer);

            EXPECT_TRUE(registerOrnaments(&ecs, "icons/ornaments"));

            auto corner = makeIcon(&ecs, "chronicle-ornaments", "corner-tr", 28);
            auto flourish = makeIcon(&ecs, "chronicle-ornaments", "flourish", 120);
            ecs.executeOnce();

            EXPECT_NE(icons->getRenderCall(corner.id), nullptr);
            EXPECT_NE(icons->getRenderCall(flourish.id), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, divider_hair_geometry)
        {
            MockLogger logger;
            OrnamentFixture s;

            OrnamentSpec spec;
            spec.kind = OrnamentKind::Divider; spec.weight = DividerWeight::Hair;
            spec.knot = false; spec.width = 300.0f;
            Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            EXPECT_FLOAT_EQ(s.pos(orn.root)->width, 300.0f);
            EXPECT_FLOAT_EQ(s.pos(orn.root)->height, 1.0f);

            ASSERT_EQ(orn.parts.size(), 1u);
            EXPECT_NEAR(s.pos(orn.parts[0])->width, 300.0f, 0.01f);
            EXPECT_NEAR(s.pos(orn.parts[0])->height, 1.0f, 0.01f);
            EXPECT_FLOAT_EQ(s.s2d(orn.parts[0])->colors.x, s.tokens.colour("rule-hair").x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, divider_rule_is_two_px)
        {
            MockLogger logger;
            OrnamentFixture s;

            OrnamentSpec spec;
            spec.kind = OrnamentKind::Divider; spec.weight = DividerWeight::Rule; spec.width = 200.0f;
            Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            EXPECT_FLOAT_EQ(s.pos(orn.root)->height, 2.0f);
            EXPECT_FLOAT_EQ(s.s2d(orn.parts[0])->colors.x, s.tokens.colour("rule-ruled").x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, divider_stretches_to_anchors)
        {
            MockLogger logger;
            OrnamentFixture s;

            auto parent = makeUiSimple2DShape(&s.ecs, Shape2D::Square, 500.0f, 40.0f, s.tokens.colour("vellum"));

            OrnamentSpec spec;
            spec.kind = OrnamentKind::Divider; spec.weight = DividerWeight::Hair; spec.knot = false; spec.width = 0.0f;
            Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
            s.pos(orn.root)->setX(0.0f);
            auto ra = s.ecs.getEntity(orn.root.id)->get<UiAnchor>();
            ra->setLeftAnchor(PosAnchor{parent.entity.id, AnchorType::Left});
            ra->setRightAnchor(PosAnchor{parent.entity.id, AnchorType::Right});
            s.settle();

            EXPECT_NEAR(s.pos(orn.root)->width, 500.0f, 0.01f);
            EXPECT_NEAR(s.pos(orn.parts[0])->width, 500.0f, 0.01f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, knot_is_centred_and_masks)
        {
            MockLogger logger;
            OrnamentFixture s;

            OrnamentSpec spec;
            spec.kind = OrnamentKind::Divider; spec.weight = DividerWeight::Hair;
            spec.knot = true; spec.width = 300.0f; spec.ground = "folio";
            Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            ASSERT_EQ(orn.parts.size(), 3u);
            EntityRef patch = orn.parts[1];
            EntityRef knot = orn.parts[2];
            const float rootX = s.pos(orn.root)->x;
            const float rootZ = s.pos(orn.root)->z;

            EXPECT_NEAR(s.pos(patch)->width, 22.0f, 0.01f);
            EXPECT_NEAR(s.pos(patch)->height, 10.0f, 0.01f);
            EXPECT_NEAR(s.pos(patch)->x + s.pos(patch)->width / 2.0f, rootX + 150.0f, 0.5f);
            EXPECT_FLOAT_EQ(s.pos(patch)->z, rootZ + 1.0f);
            EXPECT_FLOAT_EQ(s.s2d(patch)->colors.x, s.tokens.colour("folio").x);

            EXPECT_NEAR(s.pos(knot)->width, 22.0f, 0.01f);
            EXPECT_NEAR(s.pos(knot)->x + s.pos(knot)->width / 2.0f, rootX + 150.0f, 0.5f);
            EXPECT_FLOAT_EQ(s.pos(knot)->z, rootZ + 2.0f);
            EXPECT_FLOAT_EQ(s.iconOf(knot)->colors.x, s.tokens.colour("rule-hair").x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, knot_ground_follows_theme)
        {
            MockLogger logger;
            OrnamentFixture s;

            OrnamentSpec spec;
            spec.kind = OrnamentKind::Divider; spec.knot = true; spec.width = 300.0f; spec.ground = "folio";
            Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            s.tokens.setTheme(Theme::Candle);
            s.ecs.sendEvent(ThemeChangedEvent{Theme::Candle});
            s.settle();

            EXPECT_FLOAT_EQ(s.s2d(orn.parts[1])->colors.x, s.tokens.colour("folio", Theme::Candle).x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, flourish_band)
        {
            MockLogger logger;
            OrnamentFixture s;

            Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, {OrnamentKind::Flourish});
            s.settle();

            EXPECT_FLOAT_EQ(s.pos(orn.root)->width, 120.0f);
            EXPECT_FLOAT_EQ(s.pos(orn.root)->height, 18.0f);

            EntityRef icon = orn.parts[0];
            EXPECT_NEAR(s.pos(icon)->width, 120.0f, 0.01f);
            EXPECT_NEAR(s.pos(icon)->height, 120.0f, 0.01f);
            const float iconCentre = s.pos(icon)->y + s.pos(icon)->height / 2.0f;
            const float rootCentre = s.pos(orn.root)->y + s.pos(orn.root)->height / 2.0f;
            EXPECT_NEAR(iconCentre, rootCentre, 0.5f);
            EXPECT_FLOAT_EQ(s.iconOf(icon)->colors.x, s.tokens.colour("rule-ruled").x);

            OrnamentSpec vs; vs.kind = OrnamentKind::Flourish; vs.colour = "vermilion";
            Ornament orn2 = makeOrnament(&s.ecs, s.tokens, s.styles, vs);
            s.settle();
            EXPECT_FLOAT_EQ(s.iconOf(orn2.parts[0])->colors.x, s.tokens.colour("vermilion").x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, corner_files)
        {
            MockLogger logger;
            OrnamentFixture s;

            const CornerPos poss[4] = {CornerPos::TL, CornerPos::TR, CornerPos::BL, CornerPos::BR};
            const char* names[4] = {"corner-tl", "corner-tr", "corner-bl", "corner-br"};
            for (int i = 0; i < 4; ++i)
            {
                OrnamentSpec spec; spec.kind = OrnamentKind::Corner; spec.corner = poss[i];
                Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
                s.settle();

                EntityRef icon = orn.parts[0];
                EXPECT_EQ(s.iconOf(icon)->iconName, names[i]);
                EXPECT_FLOAT_EQ(s.pos(icon)->width, 28.0f);
                EXPECT_FLOAT_EQ(s.pos(icon)->height, 28.0f);
                EXPECT_FLOAT_EQ(s.iconOf(icon)->colors.x, s.tokens.colour("gold-edge").x);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, versal_is_72)
        {
            MockLogger logger;
            OrnamentFixture s;

            for (const char* letter : {"A", "W", "i"})
            {
                OrnamentSpec spec; spec.kind = OrnamentKind::Versal; spec.letter = letter;
                Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
                s.settle();
                EXPECT_FLOAT_EQ(s.pos(orn.root)->width, 72.0f) << "letter " << letter;
                EXPECT_FLOAT_EQ(s.pos(orn.root)->height, 72.0f) << "letter " << letter;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, versal_frame_inset)
        {
            MockLogger logger;
            OrnamentFixture s;

            OrnamentSpec spec; spec.kind = OrnamentKind::Versal; spec.letter = "A";
            Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            EntityRef frame = orn.parts[0];
            EXPECT_NEAR(s.pos(frame)->width, 64.0f, 0.01f);
            EXPECT_NEAR(s.pos(frame)->height, 64.0f, 0.01f);
            EXPECT_NEAR(s.pos(frame)->x, s.pos(orn.root)->x + 4.0f, 0.01f);
            EXPECT_NEAR(s.pos(frame)->y, s.pos(orn.root)->y + 4.0f, 0.01f);
            EXPECT_FLOAT_EQ(s.strokeOf(frame)->strokeWidth, 3.0f);
            EXPECT_FALSE(s.strokeOf(frame)->doubled);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, versal_letter_centred)
        {
            MockLogger logger;
            OrnamentFixture s;

            OrnamentSpec spec; spec.kind = OrnamentKind::Versal; spec.letter = "\xC3\x86thel";   // "Æthel"
            Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            ASSERT_TRUE(orn.letter.has_value());
            EXPECT_NEAR(s.pos(orn.letter->entity)->x, s.pos(orn.root)->x, 0.5f);
            const float textCentre = s.pos(orn.letter->entity)->x + s.ttfOf(orn.letter->entity)->textWidth / 2.0f;
            EXPECT_NEAR(textCentre, s.pos(orn.root)->x + 36.0f, 0.5f);
            EXPECT_EQ(orn.letter->spec.style, "chapter");
            EXPECT_EQ(orn.letter->spec.text, "\xC3\x86");   // first code point only
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, versal_tone_paints_all)
        {
            MockLogger logger;
            OrnamentFixture s;

            OrnamentSpec spec; spec.kind = OrnamentKind::Versal; spec.letter = "A"; spec.tone = VersalTone::Gold;
            Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
            s.settle();

            const auto gold = s.tokens.colour("gold-edge");
            EXPECT_FLOAT_EQ(s.strokeOf(orn.parts[0])->colors.x, gold.x);
            EXPECT_FLOAT_EQ(s.iconOf(orn.parts[1])->colors.x, gold.x);
            EXPECT_FLOAT_EQ(s.ttfOf(orn.letter->entity)->colors.x, gold.x);

            orn.setColour(&s.ecs, "lapis");
            s.settle();   // the PaintComponent change event applies on the next frame
            const auto lapis = s.tokens.colour("lapis");
            EXPECT_FLOAT_EQ(s.strokeOf(orn.parts[0])->colors.x, lapis.x);
            EXPECT_FLOAT_EQ(s.iconOf(orn.parts[1])->colors.x, lapis.x);
            EXPECT_FLOAT_EQ(s.ttfOf(orn.letter->entity)->colors.x, lapis.x);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(ornament_test, z_are_ints)
        {
            MockLogger logger;
            OrnamentFixture s;

            OrnamentSpec specs[4];
            specs[0].kind = OrnamentKind::Divider; specs[0].width = 300.0f;   // knot on -> line, patch, knot
            specs[1].kind = OrnamentKind::Flourish;
            specs[2].kind = OrnamentKind::Corner;
            specs[3].kind = OrnamentKind::Versal; specs[3].letter = "A";

            for (const auto& spec : specs)
            {
                Ornament orn = makeOrnament(&s.ecs, s.tokens, s.styles, spec);
                s.settle();
                const float rootZ = s.pos(orn.root)->z;
                for (const auto& part : orn.parts)
                {
                    const float z = s.pos(part)->z;
                    EXPECT_FLOAT_EQ(z, std::floor(z));
                    const float d = z - rootZ;
                    EXPECT_TRUE(d == 1.0f or d == 2.0f) << "z-offset " << d;
                }
            }
        }
    }
}
