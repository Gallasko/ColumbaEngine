#include "stdafx.h"

#include <gtest/gtest.h>

#include "UI/ornament.h"

#include "ECS/entitysystem.h"
#include "UI/iconsystem.h"
#include "Core/tokens.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
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
    }
}
