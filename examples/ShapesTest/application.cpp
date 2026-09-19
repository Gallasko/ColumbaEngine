#include "application.h"

#include "window.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/ttftext.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";

    // ─── Chronicle palette ──────────────────────────────────────────────────────
    // (Vector4D has no constexpr constructor, so use const)
    const constant::Vector4D VELLUM      = {236, 224, 200, 255};
    const constant::Vector4D VELLUM_WORN = {222, 208, 178, 255};
    const constant::Vector4D INK         = {43,  34,  25,  255};
    const constant::Vector4D VERDIGRIS   = {31,  96,  85,  255};
    const constant::Vector4D LEADER      = {203, 184, 148, 255};
    const constant::Vector4D STROKE_INK  = {128, 106, 74,  255};
    const constant::Vector4D GOLD_EDGE   = {138, 106, 22,  255};

    constexpr float TEXT_SCALE = 0.4f;

    // Draws the four demonstration rows over a vellum background.
    struct ShapesShowcaseSystem : public System<InitSys>
    {
        ShapesShowcaseSystem(float width, float height) : screenWidth(width), screenHeight(height) {}

        virtual std::string getSystemName() const override { return "Shapes Showcase System"; }

        template <typename Shape>
        void place(const Shape& shape, float x, float y, float z)
        {
            auto pos = shape.template get<PositionComponent>();
            pos->setX(x);
            pos->setY(y);
            pos->setZ(z);
        }

        virtual void init() override
        {
            // Vellum background behind everything.
            auto background = makeSimple2DShape(ecsRef, Shape2D::Square, screenWidth, screenHeight, VELLUM);
            background.get<PositionComponent>()->setZ(0.0f);

            // ── Row 1: life clock — worn strip with a hatched "running" segment ──
            auto strip = makeRoundedRect2DShape(ecsRef, 2.0f, 300.0f, 14.0f, VELLUM_WORN);
            place(strip, 40.0f, 40.0f, 1.0f);

            auto running = makeHatchRect2DShape(ecsRef, 100.0f, 14.0f, VERDIGRIS);
            place(running, 120.0f, 40.0f, 2.0f);

            // ── Row 2: ledger row — label, dotted leader, right-aligned figure ──
            auto* ttf = ecsRef->getSystem<TTFTextSystem>();

            auto coin = makeTTFText(ecsRef, 40.0f, 90.0f, 2.0f, "regular", "Coin", TEXT_SCALE, INK);

            const float figureWidth = ttf ? ttf->measureText("regular", "412", TEXT_SCALE).width : 30.0f;
            auto figure = makeTTFText(ecsRef, 300.0f - figureWidth, 90.0f, 2.0f, "regular", "412", TEXT_SCALE, INK);

            auto leader = makeDottedLine2DShape(ecsRef, 10.0f, LEADER);
            leader.get<PositionComponent>()->setY(98.0f);
            leader.get<PositionComponent>()->setZ(1.0f);
            auto leaderAnchor = leader.get<UiAnchor>();
            leaderAnchor->setLeftAnchor(PosAnchor{coin.id, AnchorType::Right});
            leaderAnchor->setRightAnchor(PosAnchor{figure.id, AnchorType::Left});

            // ── Row 3: three framed plates ───────────────────────────────────────
            auto plain = makeStrokeRect2DShape(ecsRef, 160.0f, 90.0f, STROKE_INK);
            place(plain, 40.0f, 130.0f, 1.0f);

            auto doubled = makeStrokeRect2DShape(ecsRef, 160.0f, 90.0f, GOLD_EDGE, 1.0f, 1.0f, true);
            place(doubled, 220.0f, 130.0f, 1.0f);

            auto rounded = makeStrokeRect2DShape(ecsRef, 160.0f, 90.0f, GOLD_EDGE, 1.0f, 1.0f, true);
            place(rounded, 400.0f, 130.0f, 1.0f);
            rounded.get<StrokeRect2DObject>()->setCornerRadius(3.0f);

            // ── Row 4: hatch at three angles ─────────────────────────────────────
            const float angles[3] = {0.0f, 90.0f, 135.0f};
            for (int i = 0; i < 3; ++i)
            {
                auto hatch = makeHatchRect2DShape(ecsRef, 100.0f, 40.0f, VERDIGRIS);
                place(hatch, 40.0f + i * 140.0f, 250.0f, 1.0f);
                hatch.get<HatchRect2DObject>()->setAngle(angles[i]);
            }
        }

        float screenWidth;
        float screenHeight;
    };

    // A window sized to fit the four rows.
    EngineConfig makeConfig()
    {
        EngineConfig config;
        config.width = 700;
        config.height = 360;
        return config;
    }
}

GameApp::GameApp(const std::string &appName) : engine(appName, makeConfig())
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        // Row 2 uses text; register the TTF system and a font.
        auto ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);
        ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Regular.ttf", "regular");

        // Build the showcase scene.
        ecs.createSystem<ShapesShowcaseSystem>(static_cast<float>(engine.getConfig().width),
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
