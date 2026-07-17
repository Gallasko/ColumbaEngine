#include "application.h"

#include "window.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/prefab.h"
#include "UI/ttftext.h"
#include "UI/textinput.h"
#include "UI/sizer.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";

    // ─── Color palette (dark editor theme) ────────────────────────────────────
    // (Vector4D has no constexpr constructor, so use const)
    const constant::Vector4D PANEL_BG     = {26,  31,  42,  250};
    const constant::Vector4D HEADER_BG    = {19,  24,  34,  255};
    const constant::Vector4D DIVIDER_COL  = {40,  48,  65,  255};
    const constant::Vector4D TITLE_COL    = {218, 224, 238, 255};
    const constant::Vector4D LABEL_COL    = {148, 156, 175, 255};
    const constant::Vector4D FIELD_BG     = {15,  19,  28,  255};
    const constant::Vector4D FIELD_TXT    = {225, 230, 245, 255};

    // ─── Layout constants ─────────────────────────────────────────────────────
    constexpr float PANEL_X        = 60.0f;
    constexpr float PANEL_Y        = 60.0f;
    constexpr float PANEL_W        = 300.0f;
    constexpr float PANEL_H        = 440.0f;
    constexpr float PANEL_RADIUS   = 12.0f;
    constexpr float PANEL_Z        = 10.0f;

    constexpr float HEADER_H       = 44.0f;

    constexpr float PADDING        = 14.0f;
    constexpr float LABEL_SCALE    = 0.30f;
    constexpr float TITLE_SCALE    = 0.38f;
    constexpr float INPUT_SCALE    = 0.34f;
    constexpr float FIELD_H        = 34.0f;   // renamed: INPUT_H conflicts with a system macro
    constexpr float FIELD_RADIUS   = 6.0f;
    constexpr float ROW_SPACING    = 10;
    constexpr float FIELD_PAD_LEFT = 10.0f;
    constexpr float FIELD_PAD_TOP  = 8.0f;

    // ─── Panel prefab ─────────────────────────────────────────────────────────
    struct PanelWithInputs : public System<InitSys>
    {
        virtual void init() override
        {
            const float contentW = PANEL_W - 2.0f * PADDING;

            // ── Root prefab entity ──────────────────────────────────────────
            auto panelEnt   = makeAnchoredPrefab(ecsRef, PANEL_X, PANEL_Y, PANEL_Z);
            auto panelPos   = panelEnt.get<PositionComponent>();
            auto panelAnch  = panelEnt.get<UiAnchor>();
            auto panel      = panelEnt.get<Prefab>();

            panelPos->setWidth(PANEL_W);
            panelPos->setHeight(PANEL_H);

            // ── Panel background (rounded rect) ────────────────────────────
            auto bg = makeRoundedRect2DShape(ecsRef, PANEL_RADIUS, PANEL_W, PANEL_H, PANEL_BG);
            bg.entity.get<UiAnchor>()->fillIn(*panelAnch);
            bg.entity.get<UiAnchor>()->setZConstrain(
                PosConstrain{panelEnt.id, AnchorType::Z, PosOpType::Add, 1.0f});
            panel->addToPrefab(bg, "BG");

            // ── Header strip ───────────────────────────────────────────────
            // Same corner radius so corners blend with the background, bottom
            // edge is flat (it's hidden behind the background below).
            auto header     = makeRoundedRect2DShape(ecsRef, PANEL_RADIUS, PANEL_W, HEADER_H, HEADER_BG);
            auto headerAnch = header.entity.get<UiAnchor>();
            headerAnch->setTopAnchor(PosAnchor{panelEnt.id, AnchorType::Top});
            headerAnch->setLeftAnchor(PosAnchor{panelEnt.id, AnchorType::Left});
            headerAnch->setWidthConstrain(PosConstrain{panelEnt.id, AnchorType::Width});
            headerAnch->setZConstrain(
                PosConstrain{panelEnt.id, AnchorType::Z, PosOpType::Add, 2.0f});
            header.get<PositionComponent>()->setHeight(HEADER_H);
            panel->addToPrefab(header, "Header");

            // ── Title text (vertically centred in header) ──────────────────
            auto titleEnt  = makeTTFText(ecsRef, 0.0f, 0.0f, 0.0f,
                                         "semibold", "Properties", TITLE_SCALE, TITLE_COL);
            auto titleAnch = titleEnt.get<UiAnchor>();
            titleAnch->setTopAnchor(PosAnchor{panelEnt.id, AnchorType::Top});
            titleAnch->setLeftAnchor(PosAnchor{panelEnt.id, AnchorType::Left});
            titleAnch->setTopMargin(HEADER_H * 0.5f - 8.0f);
            titleAnch->setLeftMargin(PADDING);
            titleAnch->setZConstrain(
                PosConstrain{panelEnt.id, AnchorType::Z, PosOpType::Add, 4.0f});
            panel->addToPrefab(titleEnt, "Title");

            // ── Thin divider between header and content ────────────────────
            auto divider     = makeUiSimple2DShape(ecsRef, Shape2D::Square, PANEL_W, 1.0f, DIVIDER_COL);
            auto dividerAnch = divider.get<UiAnchor>();
            dividerAnch->setTopAnchor(PosAnchor{panelEnt.id, AnchorType::Top});
            dividerAnch->setLeftAnchor(PosAnchor{panelEnt.id, AnchorType::Left});
            dividerAnch->setTopMargin(HEADER_H);
            dividerAnch->setWidthConstrain(PosConstrain{panelEnt.id, AnchorType::Width});
            dividerAnch->setZConstrain(
                PosConstrain{panelEnt.id, AnchorType::Z, PosOpType::Add, 3.0f});
            divider.get<PositionComponent>()->setHeight(1.0f);
            panel->addToPrefab(divider, "Divider");

            // ── Vertical layout for the fields ────────────────────────────
            auto contentEnt    = makeVerticalLayout(ecsRef, 0.0f, 0.0f, contentW,
                                                    PANEL_H - HEADER_H - 2.0f * PADDING, false);
            auto contentAnch   = contentEnt.get<UiAnchor>();
            contentAnch->setTopAnchor(PosAnchor{panelEnt.id, AnchorType::Top});
            contentAnch->setLeftAnchor(PosAnchor{panelEnt.id, AnchorType::Left});
            contentAnch->setTopMargin(HEADER_H + PADDING);
            contentAnch->setLeftMargin(PADDING);
            contentAnch->setZConstrain(
                PosConstrain{panelEnt.id, AnchorType::Z, PosOpType::Add, 2.0f});
            auto layout = contentEnt.get<VerticalLayout>();
            layout->spacing = ROW_SPACING;
            panel->addToPrefab(contentEnt, "Content");

            // ── Input rows ────────────────────────────────────────────────
            addInputRow(layout, panelEnt.id, "Name",       "",       contentW);
            addInputRow(layout, panelEnt.id, "Position X", "0.0",    contentW);
            addInputRow(layout, panelEnt.id, "Position Y", "0.0",    contentW);
            addInputRow(layout, panelEnt.id, "Width",      "100.0",  contentW);
            addInputRow(layout, panelEnt.id, "Height",     "50.0",   contentW);
        }

        // ── Builds one label + input-field pair and pushes both into layout ──
        void addInputRow(CompRef<VerticalLayout> layout,
                         _unique_id /*panelId*/,
                         const std::string& labelText,
                         const std::string& defaultValue,
                         float width)
        {
            const float fieldZ = 20.0f;

            // Label
            auto labelEnt  = makeTTFText(ecsRef, 0.0f, 0.0f, fieldZ,
                                         "regular", labelText, LABEL_SCALE, LABEL_COL);
            labelEnt.get<PositionComponent>()->setWidth(width);
            layout->addEntity(labelEnt);

            // ── Input field prefab (background + text input) ──────────────
            auto fieldEnt  = makeAnchoredPrefab(ecsRef, 0.0f, 0.0f, fieldZ);
            auto fieldPos  = fieldEnt.get<PositionComponent>();
            auto fieldAnch = fieldEnt.get<UiAnchor>();
            auto fieldPfb  = fieldEnt.get<Prefab>();
            fieldPos->setWidth(width);
            fieldPos->setHeight(FIELD_H);

            // Background rounded rect
            auto bgEnt  = makeRoundedRect2DShape(ecsRef, FIELD_RADIUS, width, FIELD_H, FIELD_BG);
            auto bgAnch = bgEnt.entity.get<UiAnchor>();
            bgAnch->fillIn(*fieldAnch);
            bgAnch->setZConstrain(PosConstrain{fieldEnt.id, AnchorType::Z, PosOpType::Add, 1.0f});
            fieldPfb->addToPrefab(bgEnt, "BG");

            // Text input (TTFText + TextInputComponent)
            auto inputEnt   = makeTTFTextInput(ecsRef, 0.0f, 0.0f,
                                               StandardEvent{}, "regular",
                                               defaultValue, INPUT_SCALE);
            auto inputAnch  = inputEnt.get<UiAnchor>();
            auto inputText  = inputEnt.get<TTFText>();
            auto inputComp  = inputEnt.get<TextInputComponent>();

            ecsRef->attach<MouseLeftClickComponent>(bgEnt.entity, makeCallable<OnFocus>(OnFocus{inputEnt.id}) );

            inputText->setColors(FIELD_TXT);
            inputComp->clearTextAfterEnter = false;
            inputComp->minWidth            = static_cast<size_t>(width - FIELD_PAD_LEFT * 2.0f);

            inputAnch->setTopAnchor(PosAnchor{fieldEnt.id, AnchorType::Top});
            inputAnch->setLeftAnchor(PosAnchor{fieldEnt.id, AnchorType::Left});
            inputAnch->setTopMargin(FIELD_PAD_TOP);
            inputAnch->setLeftMargin(FIELD_PAD_LEFT);
            inputAnch->setZConstrain(PosConstrain{fieldEnt.id, AnchorType::Z, PosOpType::Add, 2.0f});
            fieldPfb->addToPrefab(inputEnt, "Input");

            layout->addEntity(fieldEnt);
        }
    };
}

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        // Register the TTFText rendering system and fonts
        auto ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);
        ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Regular.ttf",  "regular");
        ttfSys->registerFont("res/font/Inter/static/Inter_28pt-SemiBold.ttf", "semibold");

        // Build the panel
        ecs.createSystem<PanelWithInputs>();
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
