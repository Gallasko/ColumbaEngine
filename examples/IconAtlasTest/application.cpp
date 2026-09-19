#include "application.h"

#include "window.h"
#include "2D/simple2dobject.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";

    // ─── Chronicle palette ─────────────────────────────────────────────────────
    // (Vector4D has no constexpr constructor, so use const)
    const constant::Vector4D VELLUM = {236, 224, 200, 255};
    const constant::Vector4D INK    = {43,  34,  25,  255};
    const constant::Vector4D TEAL   = {31,  96,  85,  255};
    const constant::Vector4D RED    = {158, 43,  37,  255};

    // ─── Layout constants ───────────────────────────────────────────────────────
    constexpr float MARGIN  = 20.0f;
    constexpr float PITCH24 = 28.0f;   // 24 px icon + 4 px gap
    constexpr float PITCH48 = 52.0f;   // 48 px icon + 4 px gap
    constexpr float ROW1_Y  = 40.0f;   // 24 px row
    constexpr float ROW2_Y  = 80.0f;   // 48 px row
    constexpr float ROW3_Y  = 150.0f;  // tinted check / cross row
    constexpr float LABEL_SCALE = 0.4f;

    // A wide, short window so both icon rows fit side by side.
    EngineConfig makeConfig()
    {
        EngineConfig config;
        config.width = 1480;
        config.height = 320;
        return config;
    }

    const std::vector<std::string> iconNames = {
        "strength", "dexterity", "intelligence", "vitality", "gold", "reputation", "time", "age",
        "training", "study", "work", "trade", "guild", "adventure", "swordsmanship", "magic",
        "town", "academy", "forge", "gate", "seal", "relic", "equipment", "quill", "skull", "check", "cross"};

    // Lays out every icon at 24 px and 48 px, then a tinted check/cross row with
    // matching text labels, over a vellum background.
    struct IconShowcaseSystem : public System<InitSys>
    {
        IconShowcaseSystem(float width, float height) : screenWidth(width), screenHeight(height) {}

        virtual std::string getSystemName() const override { return "Icon Showcase System"; }

        void placeIcon(const CompList<PositionComponent, UiAnchor, ViewportComponent, IconComponent>& icon, float x, float y, float z)
        {
            auto pos = icon.get<PositionComponent>();
            pos->setX(x);
            pos->setY(y);
            pos->setZ(z);
        }

        virtual void init() override
        {
            // Vellum background behind everything.
            auto background = makeSimple2DShape(ecsRef, Shape2D::Square, screenWidth, screenHeight, VELLUM);
            auto backgroundPos = background.get<PositionComponent>();
            backgroundPos->setX(0.0f);
            backgroundPos->setY(0.0f);
            backgroundPos->setZ(0.0f);

            // Two rows of every icon, tinted ink.
            for (size_t i = 0; i < iconNames.size(); ++i)
            {
                auto small = makeIcon(ecsRef, "chronicle", iconNames[i], 24.0f, INK);
                placeIcon(small, MARGIN + i * PITCH24, ROW1_Y, 1.0f);

                auto large = makeIcon(ecsRef, "chronicle", iconNames[i], 48.0f, INK);
                placeIcon(large, MARGIN + i * PITCH48, ROW2_Y, 1.0f);
            }

            // Tinted check (teal) and cross (red) beside labels in the same colours.
            auto check = makeIcon(ecsRef, "chronicle", "check", 48.0f, TEAL);
            placeIcon(check, MARGIN, ROW3_Y, 1.0f);
            makeTTFText(ecsRef, MARGIN + 56.0f, ROW3_Y + 12.0f, 1.0f, "regular", "check", LABEL_SCALE, TEAL);

            auto cross = makeIcon(ecsRef, "chronicle", "cross", 48.0f, RED);
            placeIcon(cross, MARGIN + 220.0f, ROW3_Y, 1.0f);
            makeTTFText(ecsRef, MARGIN + 276.0f, ROW3_Y + 12.0f, 1.0f, "regular", "cross", LABEL_SCALE, RED);
        }

        float screenWidth;
        float screenHeight;
    };
}

GameApp::GameApp(const std::string &appName) : engine(appName, makeConfig())
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        // Text labels use the TTF system; register it and a font.
        auto ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);
        ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Regular.ttf", "regular");

        // Rasterise and pack the Chronicle icon set at 24 and 48 px.
        auto* icons = ecs.getSystem<IconSystem>();
        std::vector<std::string> paths;
        for (const auto& name : iconNames)
            paths.push_back("res/icons/chronicle/" + name + ".svg");
        icons->registerIconSet("chronicle", paths, {24, 48});

        // Build the showcase scene.
        ecs.createSystem<IconShowcaseSystem>(static_cast<float>(engine.getConfig().width),
                                             static_cast<float>(engine.getConfig().height));
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
