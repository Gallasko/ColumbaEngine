#include "stdafx.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "UI/placetile.h"
#include "UI/label.h"
#include "UI/mark.h"
#include "Core/motion.h"

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"   // ResizeEvent
#include "UI/themesystem.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/prefab.h"
#include "Input/inputcomponent.h"
#include "2D/simple2dobject.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            struct PlaceFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                IconSystem* icons = nullptr;
                ThemeSystem* theme = nullptr;

                PlaceFixture()
                {
                    Motion::setReduced(true);
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.createSystem<PrefabSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, PrefabSystem>();
                    ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    icons = ecs.createSystem<IconSystem>(&renderer);
                    theme = ecs.createSystem<ThemeSystem>();
                    theme->loadTheme("chronicle/tokens.json", "fonts");
                    installIconEntries();
                    ecs.sendEvent(ResizeEvent{1320.0f, 860.0f});
                }

                void installIconEntries()
                {
                    std::vector<IconEntry> marks;
                    const int sizes[5] = {14, 16, 18, 24, 48};

                    for (const auto& name : markNames())
                    {
                        for (int s : sizes)
                            marks.push_back({name, s, {s, s}, {0.0f, 0.0f}, {0.1f, 0.1f}});
                    }

                    icons->registerEntriesForTest("chronicle", marks, 1024, 1024);
                    renderer.registerTexture("IconAtlas_chronicle", OpenGLTexture{});
                }

                void settle() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }

                std::string element(EntityRef e) { return ecs.getEntity(e.id)->get<ThemeComponent>()->element; }

                float left(EntityRef e, EntityRef root) { return pos(e)->x - pos(root)->x; }

                float top(EntityRef e, EntityRef root) { return pos(e)->y - pos(root)->y; }
            };

            PlaceTileSpec market(int level = 1)
            {
                PlaceTileSpec spec;
                spec.id = "market";
                spec.name = "The Market";
                spec.glyph = "trade";
                spec.level = level;
                spec.line = "BUY RATIONS BRINGS 8 RATIONS, NOT 6";

                return spec;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A place's tile: as wide as it is told, as tall as every other, its mark and name at the
        // top, its seals under them, its line at the foot, all inside it.
        TEST(placetile_test, geometry)
        {
            MockLogger logger;
            PlaceFixture f;

            PlaceTile tile = makePlaceTile(&f.ecs, market());
            tile.root->get<PositionComponent>()->setX(100.0f);
            tile.root->get<PositionComponent>()->setY(100.0f);
            f.settle();

            const float height = placeTileHeight(&f.ecs);

            EXPECT_FLOAT_EQ(f.pos(tile.root)->width, 148.0f);
            EXPECT_FLOAT_EQ(f.pos(tile.root)->height, height);
            EXPECT_FLOAT_EQ(tile.height(&f.ecs), height);

            // The ground is the whole tile, under everything else of it
            EXPECT_FLOAT_EQ(f.pos(tile.ground)->width, 148.0f);
            EXPECT_FLOAT_EQ(f.pos(tile.ground)->height, height);
            EXPECT_NEAR(f.left(tile.ground, tile.root), 0.0f, 0.01f);
            EXPECT_NEAR(f.top(tile.ground, tile.root), 0.0f, 0.01f);
            EXPECT_LT(f.pos(tile.ground)->z, f.pos(tile.mark.entity)->z);
            EXPECT_LT(f.pos(tile.ground)->z, f.pos(tile.name.entity)->z);

            // The mark at the top left, the name beside it
            EXPECT_NEAR(f.left(tile.mark.entity, tile.root), 8.0f, 0.01f);
            EXPECT_NEAR(f.top(tile.mark.entity, tile.root), 8.0f, 0.01f);
            EXPECT_FLOAT_EQ(f.pos(tile.mark.entity)->width, 24.0f);
            EXPECT_NEAR(f.left(tile.name.entity, tile.root), 40.0f, 0.01f);
            EXPECT_EQ(tile.name.spec.text, "The Market");
            EXPECT_EQ(tile.name.spec.maxLines, 2);

            // The seals under them, in a row; the line under the seals
            ASSERT_EQ(tile.seals.size(), 3u);

            for (size_t i = 0; i < tile.seals.size(); ++i)
            {
                EXPECT_NEAR(f.left(tile.seals[i].entity, tile.root), 8.0f + static_cast<float>(i) * 18.0f, 0.01f) << i;
                EXPECT_GE(f.top(tile.seals[i].entity, tile.root), f.top(tile.mark.entity, tile.root) + 24.0f) << i;
                EXPECT_NEAR(f.top(tile.seals[i].entity, tile.root), f.top(tile.seals[0].entity, tile.root), 0.01f) << i;
            }

            EXPECT_GE(f.top(tile.line.entity, tile.root), f.top(tile.seals[0].entity, tile.root) + 14.0f);
            EXPECT_NEAR(f.left(tile.line.entity, tile.root), 8.0f, 0.01f);
            EXPECT_LE(f.top(tile.line.entity, tile.root) + f.pos(tile.line.entity)->height, height + 0.01f);
            EXPECT_EQ(tile.line.spec.text, "BUY RATIONS BRINGS 8 RATIONS, NOT 6");

            // Wider: the ground and the texts follow
            tile.setWidth(&f.ecs, 200.0f);
            f.settle();

            EXPECT_FLOAT_EQ(f.pos(tile.root)->width, 200.0f);
            EXPECT_FLOAT_EQ(f.pos(tile.ground)->width, 200.0f);
            EXPECT_FLOAT_EQ(tile.line.spec.width, 184.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A seal a level: gold for the ones earned, a hair for the others; and they follow the level.
        TEST(placetile_test, seals_by_level)
        {
            MockLogger logger;
            PlaceFixture f;

            PlaceTile tile = makePlaceTile(&f.ecs, market(1));
            f.settle();

            ASSERT_EQ(tile.seals.size(), 3u);
            EXPECT_EQ(f.element(tile.seals[0].entity), "place.seal.earned");
            EXPECT_EQ(f.element(tile.seals[1].entity), "place.seal.empty");
            EXPECT_EQ(f.element(tile.seals[2].entity), "place.seal.empty");

            tile.setLevel(&f.ecs, 3, 3);

            for (const auto& seal : tile.seals)
                EXPECT_EQ(f.element(seal.entity), "place.seal.earned");

            EXPECT_EQ(tile.spec.level, 3);

            // Never more than it has, never less than none
            tile.setLevel(&f.ecs, 7, 3);
            EXPECT_EQ(tile.spec.level, 3);

            tile.setLevel(&f.ecs, -1, 3);
            EXPECT_EQ(tile.spec.level, 0);
            EXPECT_EQ(f.element(tile.seals[0].entity), "place.seal.empty");

            // A place of two levels has two seals
            tile.setLevel(&f.ecs, 1, 2);
            f.settle();

            ASSERT_EQ(tile.seals.size(), 2u);
            EXPECT_EQ(f.element(tile.seals[0].entity), "place.seal.earned");
            EXPECT_EQ(f.element(tile.seals[1].entity), "place.seal.empty");

            // And its line is rewritten
            tile.setLine(&f.ecs, "NOT YET BUILT");
            EXPECT_EQ(tile.line.spec.text, "NOT YET BUILT");
            EXPECT_EQ(tile.spec.line, "NOT YET BUILT");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A tile takes the click, and the one chosen is lit: its ground and its edge say so.
        TEST(placetile_test, a_click_and_the_selection)
        {
            MockLogger logger;
            PlaceFixture f;

            PlaceTile tile = makePlaceTile(&f.ecs, market());
            f.settle();

            EXPECT_TRUE(f.ecs.getEntity(tile.ground.id)->has<MouseLeftClickComponent>());

            EXPECT_EQ(f.element(tile.ground), "place.ground");
            EXPECT_EQ(f.element(tile.edge), "place.edge");
            EXPECT_FALSE(tile.spec.selected);

            tile.setSelected(&f.ecs, true);

            EXPECT_EQ(f.element(tile.ground), "place.ground.selected");
            EXPECT_EQ(f.element(tile.edge), "place.edge.selected");
            EXPECT_TRUE(tile.spec.selected);

            tile.setSelected(&f.ecs, false);

            EXPECT_EQ(f.element(tile.ground), "place.ground");

            // Built chosen
            PlaceTileSpec spec = market();
            spec.selected = true;

            PlaceTile chosen = makePlaceTile(&f.ecs, spec);

            EXPECT_EQ(f.element(chosen.ground), "place.ground.selected");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The grid lays its tiles as an activity list does: as many to a line as fit, 8 apart,
        // sharing the width; one of them at most is chosen.
        TEST(placetile_test, the_grid_lays_lines_and_selects)
        {
            MockLogger logger;
            PlaceFixture f;

            PlaceGridSpec spec;
            spec.width = 572.0f;
            spec.tileWidth = 148.0f;

            for (const char* id : {"mill", "market", "chapel", "yard", "gate", "smithy", "inn"})
            {
                PlaceTileSpec place;
                place.id = id;
                place.name = id;
                spec.places.push_back(place);
            }

            PlaceGrid grid = makePlaceGrid(&f.ecs, spec);
            grid.root->get<PositionComponent>()->setX(40.0f);
            grid.root->get<PositionComponent>()->setY(60.0f);
            f.settle();

            const float height = placeTileHeight(&f.ecs);

            // Three to a line at 572: 185.33 each, 8 between two; seven tiles, three lines
            ASSERT_EQ(grid.tiles.size(), 7u);
            EXPECT_EQ(grid.columns(), 3);
            EXPECT_NEAR(grid.tileSize(), (572.0f - 16.0f) / 3.0f, 0.01f);
            EXPECT_NEAR(f.pos(grid.root)->height, 3.0f * height + 16.0f, 0.01f);

            for (size_t i = 0; i < grid.tiles.size(); ++i)
            {
                const float column = static_cast<float>(i % 3);
                const float row = static_cast<float>(i / 3);

                EXPECT_NEAR(f.pos(grid.tiles[i].root)->width, grid.tileSize(), 0.01f) << i;
                EXPECT_NEAR(f.left(grid.tiles[i].root, grid.root), column * (grid.tileSize() + 8.0f), 0.01f) << i;
                EXPECT_NEAR(f.top(grid.tiles[i].root, grid.root), row * (height + 8.0f), 0.01f) << i;
            }

            // One chosen at a time; an unknown one is none
            grid.select(&f.ecs, "chapel");

            EXPECT_EQ(grid.selected(), "chapel");
            EXPECT_TRUE(grid.tile("chapel")->spec.selected);
            EXPECT_FALSE(grid.tile("mill")->spec.selected);

            grid.select(&f.ecs, "yard");

            EXPECT_FALSE(grid.tile("chapel")->spec.selected);
            EXPECT_TRUE(grid.tile("yard")->spec.selected);

            grid.select(&f.ecs, "nowhere");
            EXPECT_EQ(grid.selected(), "");
            EXPECT_FALSE(grid.tile("yard")->spec.selected);
            EXPECT_EQ(grid.tile("nowhere"), nullptr);

            // Narrower than two tiles: one to a line, as wide as the grid
            grid.setWidth(&f.ecs, 200.0f);
            f.settle();

            EXPECT_EQ(grid.columns(), 1);
            EXPECT_NEAR(grid.tileSize(), 200.0f, 0.01f);
            EXPECT_NEAR(f.pos(grid.tiles[1].root)->width, 200.0f, 0.01f);
            EXPECT_NEAR(f.left(grid.tiles[1].root, grid.root), 0.0f, 0.01f);
            EXPECT_NEAR(f.top(grid.tiles[1].root, grid.root), height + 8.0f, 0.01f);

            // Filled again, the one chosen stays chosen when its place is still there
            grid.select(&f.ecs, "inn");

            std::vector<PlaceTileSpec> fewer;

            for (const char* id : {"inn", "mill"})
            {
                PlaceTileSpec place;
                place.id = id;
                place.name = id;
                fewer.push_back(place);
            }

            grid.setPlaces(&f.ecs, fewer);

            ASSERT_EQ(grid.tiles.size(), 2u);
            EXPECT_EQ(grid.selected(), "inn");
            EXPECT_TRUE(grid.tile("inn")->spec.selected);

            grid.setPlaces(&f.ecs, {});

            EXPECT_TRUE(grid.tiles.empty());
            EXPECT_EQ(grid.selected(), "");
            EXPECT_FLOAT_EQ(f.pos(grid.root)->height, 0.0f);
        }
    }
}
