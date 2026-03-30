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

    // Panel prefab
    struct PanelWithInputs : public System<InitSys>
    {
        virtual void init() override
        {
            const float contentW = PANEL_W - 2.0f * PADDING;

            // Root prefab entity
            auto panelEnt   = makeAnchoredPrefab(ecsRef, PANEL_X, PANEL_Y, PANEL_Z);
            auto panel      = panelEnt.get<Prefab>();

            // Panel background (rounded rect)
            auto bg = makeRoundedRect2DShape(ecsRef, PANEL_RADIUS, PANEL_W, PANEL_H, PANEL_BG);
            auto bgAnch = bg.entity.get<UiAnchor>();

            panel->setMainEntity(bg.entity);

            // Header strip
            // Same corner radius so corners blend with the background, bottom
            // edge is flat (it's hidden behind the background below).
            auto header     = makeRoundedRect2DShape(ecsRef, PANEL_RADIUS, PANEL_W, HEADER_H, HEADER_BG);
            auto headerAnch = header.entity.get<UiAnchor>();
            headerAnch->setTopAnchor(bgAnch->top);
            headerAnch->setLeftAnchor(bgAnch->left);
            headerAnch->setWidthConstrain(PosConstrain{bg.id, AnchorType::Width});
            headerAnch->setZConstrain(PosConstrain{bg.id, AnchorType::Z, PosOpType::Add, 2.0f});
            panel->addToPrefab(header, "Header");

            // Title text (vertically centred in header)
            auto titleEnt  = makeTTFText(ecsRef, 0.0f, 0.0f, 0.0f,"semibold", "Properties", TITLE_SCALE, TITLE_COL);
            auto titleAnch = titleEnt.get<UiAnchor>();
            titleAnch->setVerticalCenter(headerAnch->verticalCenter);
            titleAnch->setLeftAnchor(headerAnch->left);
            titleAnch->setLeftMargin(PADDING);
            titleAnch->setZConstrain(PosConstrain{panelEnt.id, AnchorType::Z, PosOpType::Add, 4.0f});
            panel->addToPrefab(titleEnt, "Title");

            // Thin divider between header and content
            auto divider     = makeUiSimple2DShape(ecsRef, Shape2D::Square, PANEL_W, 1.0f, DIVIDER_COL);
            auto dividerAnch = divider.get<UiAnchor>();
            dividerAnch->setTopAnchor(headerAnch->bottom);
            dividerAnch->setLeftAnchor(bgAnch->left);
            dividerAnch->setRightAnchor(bgAnch->right);
            dividerAnch->setZConstrain(PosConstrain{panelEnt.id, AnchorType::Z, PosOpType::Add, 3.0f});
            panel->addToPrefab(divider, "Divider");

            // Vertical layout for the fields
            auto contentEnt    = makeVerticalLayout(ecsRef, 0.0f, 0.0f, 1.0f, 1.0f, false);
            auto contentAnch   = contentEnt.get<UiAnchor>();
            contentAnch->setTopAnchor(dividerAnch->bottom);
            contentAnch->setTopMargin(PADDING);
            contentAnch->setLeftAnchor(bgAnch->left);
            contentAnch->setRightAnchor(bgAnch->right);
            contentAnch->setLeftMargin(PADDING);
            contentAnch->setRightMargin(PADDING);
            contentAnch->setZConstrain(PosConstrain{panelEnt.id, AnchorType::Z, PosOpType::Add, 2.0f});
            auto layout = contentEnt.get<VerticalLayout>();
            layout->spacing = ROW_SPACING;
            // layout->fitToAxis = true;
            panel->addToPrefab(contentEnt, "Content");

            bgAnch->setBottomAnchor(contentAnch->bottom);

            // Input rows
            addInputRow(layout, "Name",       "",       contentW);
            addInputRow(layout, "Position X", "0.0",    contentW);
            addInputRow(layout, "Position Y", "0.0",    contentW);
            addInputRow(layout, "Position Z", "0.0",    contentW);
            addInputRow(layout, "Width",      "100.0",  contentW);
            addInputRow(layout, "Height",     "50.0",   contentW);
        }

        // ── Builds one label + input-field pair and pushes both into layout ──
        void addInputRow(CompRef<VerticalLayout> layout,
                         const std::string& labelText,
                         const std::string& defaultValue,
                         float width)
        {
            const float fieldZ = 20.0f;

            // Label
            auto labelEnt  = makeTTFText(ecsRef, 0.0f, 0.0f, fieldZ,
                                         "regular", labelText, LABEL_SCALE, LABEL_COL);
            layout->addEntity(labelEnt);

            // ── Input field prefab (background + text input) ──────────────
            auto fieldEnt  = makeAnchoredPrefab(ecsRef, 0.0f, 0.0f, fieldZ);
            auto fieldPfb  = fieldEnt.get<Prefab>();

            // Background rounded rect
            auto bgEnt  = makeRoundedRect2DShape(ecsRef, FIELD_RADIUS, width, FIELD_H, FIELD_BG);
            auto bgAnch = bgEnt.entity.get<UiAnchor>();
            fieldPfb->setMainEntity(bgEnt);

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

            inputAnch->setTopAnchor(bgAnch->top);
            inputAnch->setLeftAnchor(bgAnch->left);
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
        ecs.succeed<MasterRenderer, TTFTextSystem>();

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
