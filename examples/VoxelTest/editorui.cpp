#include "stdafx.h"

#include "editorui.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        constexpr const char* DOM = "EditorUI";
        constexpr size_t UI_VIEWPORT = 1;

        // Helper: create a Simple2DObject square at (x, y, z) with given size and color,
        // assigned to UI_VIEWPORT.
        EntityRef makeUIQuad(EntitySystem* ecs, float x, float y, float z,
                             float w, float h, const constant::Vector4D& color)
        {
            auto comp = makeUiSimple2DShape(ecs, Shape2D::Square, w, h, color);
            comp.get<PositionComponent>()->setX(x);
            comp.get<PositionComponent>()->setY(y);
            comp.get<PositionComponent>()->setZ(z);
            comp.get<Simple2DObject>()->setViewport(UI_VIEWPORT);
            return comp.entity;
        }

        EntityRef makeUIText(EntitySystem* ecs, float x, float y, float z,
                             const std::string& text, float scale,
                             const constant::Vector4D& color)
        {
            auto comp = makeTTFText(ecs, x, y, z, EDITOR_FONT, text, scale, color);
            comp.get<TTFText>()->setViewport(UI_VIEWPORT);
            return comp.entity;
        }
    }

    // =========================================================================
    // Construction
    // =========================================================================

    EditorUISystem::EditorUISystem(MasterRenderer* masterRenderer,
                                   EditorSystem* editor)
        : editor(editor)
        , masterRenderer(masterRenderer)
    {}

    // =========================================================================
    // Init
    // =========================================================================

    void EditorUISystem::init()
    {
        // Fetch actual screen dimensions from the renderer
        const auto& rTable = masterRenderer->getParameter();
        screenW = static_cast<float>(rTable.at("ScreenWidth").get<int>());
        screenH = static_cast<float>(rTable.at("ScreenHeight").get<int>());

        // Create the 2D camera for viewport 1
        auto camEntity = ecsRef->createEntity();
        auto cam2d = ecsRef->_attach<BaseCamera2D>(camEntity);
        cam2d->setWidth(screenW);
        cam2d->setHeight(screenH);
        cam2d->setNearPlane(-1.0f);
        cam2d->setFarPlane(1.0f);
        masterRenderer->queueRegisterCamera(camEntity.id);
        uiCamera = cam2d;

        createToolbar();
        createLayerPanel();
        createGizmoCube();
        createActionBar();
        createPaletteModal();
    }

    // =========================================================================
    // Toolbar creation (left side)
    // =========================================================================

    void EditorUISystem::createToolbar()
    {
        using ES = EditorSystem;
        const float tx = static_cast<float>(ES::TOOLBAR_X);
        const float ty = static_cast<float>(ES::TOOLBAR_Y);
        const float btn = static_cast<float>(ES::TOOLBAR_BTN);
        const float gap = static_cast<float>(ES::TOOLBAR_GAP);

        // 4 buttons: Place, Erase, ColorPick, Color swatch
        const float totalH = btn * 4.0f + gap * 3.0f + 8.0f; // 8 = padding
        const float totalW = btn + 8.0f;

        // Backdrop
        toolbarBackdrop = makeUIQuad(ecsRef, tx - 4.0f, ty - 4.0f, 0.0f,
                                     totalW, totalH,
                                     {20.0f, 20.0f, 20.0f, 180.0f});

        // Highlight (behind active tool)
        toolHighlight = makeUIQuad(ecsRef, tx, ty, 1.0f,
                                   btn, btn,
                                   {255.0f, 255.0f, 255.0f, 80.0f});

        // Place button (slot 0)
        const float placeY = ty;
        toolPlaceBtn = makeUIQuad(ecsRef, tx, placeY, 2.0f,
                                  btn, btn,
                                  {60.0f, 60.0f, 120.0f, 220.0f});
        toolPlaceLabel = makeUIText(ecsRef, tx + 14.0f, placeY + 12.0f, 3.0f,
                                    "P", 1.0f,
                                    {220.0f, 220.0f, 220.0f, 255.0f});

        // Erase button (slot 1)
        const float eraseY = ty + btn + gap;
        toolEraseBtn = makeUIQuad(ecsRef, tx, eraseY, 2.0f,
                                  btn, btn,
                                  {120.0f, 60.0f, 60.0f, 220.0f});
        toolEraseLabel = makeUIText(ecsRef, tx + 14.0f, eraseY + 12.0f, 3.0f,
                                    "E", 1.0f,
                                    {220.0f, 220.0f, 220.0f, 255.0f});

        // ColorPick button (slot 2)
        const float pickY = ty + (btn + gap) * 2.0f;
        toolColorPickBtn = makeUIQuad(ecsRef, tx, pickY, 2.0f,
                                      btn, btn,
                                      {60.0f, 120.0f, 80.0f, 220.0f});
        toolColorPickLabel = makeUIText(ecsRef, tx + 14.0f, pickY + 12.0f, 3.0f,
                                        "C", 1.0f,
                                        {220.0f, 220.0f, 220.0f, 255.0f});

        // Color swatch button (slot 3 — shows active color, opens palette modal)
        const float colorY = ty + (btn + gap) * 3.0f;
        const glm::vec4& col = editor->canvas->palette[static_cast<size_t>(editor->activeColor)];
        toolColorBtn = makeUIQuad(ecsRef, tx, colorY, 2.0f,
                                  btn, btn,
                                  {col.r, col.g, col.b, 255.0f});
    }

    // =========================================================================
    // Layer panel creation (right side, below gizmo)
    // =========================================================================

    void EditorUISystem::createLayerPanel()
    {
        using ES = EditorSystem;
        const float pw = static_cast<float>(ES::LAYER_PANEL_W);
        const float bh = static_cast<float>(ES::LAYER_ADD_BTN_H);

        // Position: right side, below gizmo cube
        const float gizmoBottomY = static_cast<float>(ES::GIZMO_MARGIN)
                                 + 4.0f * static_cast<float>(ES::GIZMO_BTN + ES::GIZMO_GAP)
                                 + 8.0f; // extra gap
        const float px = screenW - static_cast<float>(ES::LAYER_PANEL_MARGIN_RIGHT) - pw;
        const float py = gizmoBottomY;

        // Backdrop (will be resized in update)
        layerBackdrop = makeUIQuad(ecsRef, px - 2.0f, py - 2.0f, 0.0f,
                                   pw + 4.0f, bh + 8.0f,
                                   {20.0f, 20.0f, 20.0f, 180.0f});

        // Vertical layout for layer rows
        auto layout = makeVerticalLayout(ecsRef, px, py, pw, 800.0f, false);
        layout.get<VerticalLayout>()->spacing = 2;
        layerLayout = layout.entity;

        // Add button
        layerAddBtn = makeUIQuad(ecsRef, px, py, 1.f,
                                 pw, bh - 2.0f,
                                 {40.0f, 80.0f, 40.0f, 200.0f});

        // "+" crosshair bars
        layerAddCrossH = makeUIQuad(ecsRef,
                                    px + pw * 0.5f - 8.0f, py + bh * 0.5f - 2.0f, 2.f,
                                    16.0f, 4.0f,
                                    {220.0f, 220.0f, 220.0f, 255.0f});
        layerAddCrossV = makeUIQuad(ecsRef,
                                    px + pw * 0.5f - 2.0f, py + bh * 0.5f - 8.0f, 2.f,
                                    4.0f, 16.0f,
                                    {220.0f, 220.0f, 220.0f, 255.0f});
    }

    // =========================================================================
    // Gizmo cube creation (top-right)
    // =========================================================================

    void EditorUISystem::createGizmoCube()
    {
        using ES = EditorSystem;
        const float btn  = static_cast<float>(ES::GIZMO_BTN);
        const float gap  = static_cast<float>(ES::GIZMO_GAP);
        const float step = btn + gap;
        const float mg   = static_cast<float>(ES::GIZMO_MARGIN);

        // Grid origin: top-right corner, 3 columns wide
        const float gx0 = screenW - mg - 3.0f * step + gap;
        const float gy0 = mg;

        // Face definitions: name, column, row, color
        struct FaceDef { const char* name; int col; int row; constant::Vector4D color; };
        const FaceDef defs[GIZMO_FACE_COUNT] = {
            { "Top",    1, 0, { 80.0f, 180.0f,  80.0f, 220.0f } },  // green
            { "Left",   0, 1, { 180.0f,  80.0f,  80.0f, 220.0f } },  // red
            { "Front",  1, 1, {  80.0f,  80.0f, 180.0f, 220.0f } },  // blue
            { "Right",  2, 1, { 180.0f, 120.0f,  80.0f, 220.0f } },  // orange
            { "Bot",    1, 2, {  80.0f, 120.0f,  80.0f, 220.0f } },  // dark green
            { "Back",   1, 3, {  80.0f,  80.0f, 120.0f, 220.0f } },  // dark blue
        };

        for (int i = 0; i < GIZMO_FACE_COUNT; ++i)
        {
            const auto& d = defs[i];
            const float fx = gx0 + static_cast<float>(d.col) * step;
            const float fy = gy0 + static_cast<float>(d.row) * step;

            gizmoFaces[i] = makeUIQuad(ecsRef, fx, fy, 5.0f, btn, btn, d.color);

            // Center the label roughly in the button
            gizmoLabels[i] = makeUIText(ecsRef, fx + 4.0f, fy + 12.0f, 6.0f,
                                         d.name, 0.8f,
                                         {255.0f, 255.0f, 255.0f, 255.0f});
        }
    }

    // =========================================================================
    // Action bar creation (top-centre)
    // =========================================================================

    void EditorUISystem::createActionBar()
    {
        using ES = EditorSystem;
        const float btnW   = static_cast<float>(ES::ACTION_BAR_BTN_W);
        const float btnH   = static_cast<float>(ES::ACTION_BAR_BTN_H);
        const float gap    = static_cast<float>(ES::ACTION_BAR_GAP);
        const float pad    = static_cast<float>(ES::ACTION_BAR_PAD);
        const float margin = static_cast<float>(ES::ACTION_BAR_MARGIN);

        const float contentW = btnW * 2.0f + gap;
        const float totalW   = contentW + pad * 2.0f;
        const float totalH   = btnH + pad * 2.0f;

        const float x0 = (screenW - totalW) * 0.5f;
        const float y0 = margin;

        actionBarBackdrop = makeUIQuad(ecsRef, x0, y0, 0.0f,
                                       totalW, totalH,
                                       {20.0f, 20.0f, 20.0f, 180.0f});

        const float openX = x0 + pad;
        const float btnY  = y0 + pad;

        actionBarOpenBtn = makeUIQuad(ecsRef, openX, btnY, 1.0f,
                                      btnW, btnH,
                                      {60.0f, 60.0f, 120.0f, 220.0f});

        actionBarOpenLabel = makeUIText(ecsRef, openX + 12.0f, btnY + 6.0f, 2.0f,
                                        "Open", 1.0f,
                                        {220.0f, 220.0f, 220.0f, 255.0f});

        const float saveX = openX + btnW + gap;

        actionBarSaveBtn = makeUIQuad(ecsRef, saveX, btnY, 1.0f,
                                      btnW, btnH,
                                      {60.0f, 120.0f, 60.0f, 220.0f});

        actionBarSaveLabel = makeUIText(ecsRef, saveX + 12.0f, btnY + 6.0f, 2.0f,
                                        "Save", 1.0f,
                                        {220.0f, 220.0f, 220.0f, 255.0f});
    }

    // =========================================================================
    // Palette modal creation
    // =========================================================================

    void EditorUISystem::createPaletteModal()
    {
        // Full-screen semi-transparent overlay
        modalOverlay = makeUIQuad(ecsRef, 0.0f, 0.0f, 50.0f,
                                  screenW, screenH,
                                  {0.0f, 0.0f, 0.0f, 120.0f});

        using ES = EditorSystem;
        const int paletteSize = static_cast<int>(editor->canvas->palette.size());
        const float sw   = static_cast<float>(ES::MODAL_SWATCH);
        const float sp   = static_cast<float>(ES::MODAL_SWATCH_PAD);
        const float cols = static_cast<float>(ES::MODAL_COLS);
        const float pad  = static_cast<float>(ES::MODAL_PAD);

        // Calculate modal dimensions
        const int totalItems = paletteSize + 1; // +1 for "+" button
        const int rows = (totalItems + ES::MODAL_COLS - 1) / ES::MODAL_COLS;
        const float gridW = cols * (sw + sp) - sp;
        const float gridH = static_cast<float>(rows) * (sw + sp) - sp;

        const float modalW = gridW + pad * 2.0f;
        // Reserve space for title + grid + color creator
        const float creatorH = static_cast<float>(ES::RGB_BAR_H) * 3.0f
                             + static_cast<float>(ES::RGB_BAR_GAP) * 4.0f
                             + 30.0f  // preview + buttons row
                             + 10.0f; // bottom margin
        const float titleH = 28.0f;
        const float modalH = titleH + gridH + pad * 2.0f + creatorH + pad;

        const float mx = (screenW - modalW) * 0.5f;
        const float my = (screenH - modalH) * 0.5f;

        // Modal panel background
        modalPanel = makeUIQuad(ecsRef, mx, my, 51.0f,
                                modalW, modalH,
                                {30.0f, 30.0f, 30.0f, 240.0f});

        // Title
        modalTitle = makeUIText(ecsRef, mx + pad, my + 6.0f, 52.0f,
                                "Palette", 1.0f,
                                {220.0f, 220.0f, 220.0f, 255.0f});

        // Highlight for selected swatch
        modalHighlight = makeUIQuad(ecsRef, 0.0f, 0.0f, 53.0f,
                                    sw + 4.0f, sw + 4.0f,
                                    {255.0f, 255.0f, 255.0f, 255.0f});

        // Build swatch grid
        const float gridX = mx + pad;
        const float gridY = my + titleH + pad;

        for (int i = 0; i < paletteSize; ++i)
        {
            const int col = i % ES::MODAL_COLS;
            const int row = i / ES::MODAL_COLS;
            const float sx = gridX + static_cast<float>(col) * (sw + sp);
            const float sy = gridY + static_cast<float>(row) * (sw + sp);

            const glm::vec4& c = editor->canvas->palette[static_cast<size_t>(i)];
            auto swatch = makeUIQuad(ecsRef, sx, sy, 54.0f,
                                     sw, sw,
                                     {c.r, c.g, c.b, 255.0f});
            modalSwatches.push_back(swatch);
        }

        // "+" add button at end of grid
        {
            const int col = paletteSize % ES::MODAL_COLS;
            const int row = paletteSize / ES::MODAL_COLS;
            const float sx = gridX + static_cast<float>(col) * (sw + sp);
            const float sy = gridY + static_cast<float>(row) * (sw + sp);

            modalAddBtn = makeUIQuad(ecsRef, sx, sy, 54.0f,
                                     sw, sw,
                                     {40.0f, 80.0f, 40.0f, 220.0f});
            // "+" crosshair bars (same pattern as layer panel add button)
            modalAddCrossH = makeUIQuad(ecsRef,
                                        sx + sw * 0.5f - 8.0f, sy + sw * 0.5f - 2.0f, 55.0f,
                                        16.0f, 4.0f,
                                        {220.0f, 220.0f, 220.0f, 255.0f});
            modalAddCrossV = makeUIQuad(ecsRef,
                                        sx + sw * 0.5f - 2.0f, sy + sw * 0.5f - 8.0f, 55.0f,
                                        4.0f, 16.0f,
                                        {220.0f, 220.0f, 220.0f, 255.0f});
        }

        // Position highlight on active swatch
        if (editor->activeColor >= 0 && editor->activeColor < static_cast<int>(modalSwatches.size()))
        {
            auto* hlPos = ecsRef->getComponent<PositionComponent>(modalHighlight.id);
            auto* swPos = ecsRef->getComponent<PositionComponent>(modalSwatches[static_cast<size_t>(editor->activeColor)].id);
            if (hlPos && swPos)
            {
                hlPos->setX(swPos->x - 2.0f);
                hlPos->setY(swPos->y - 2.0f);
            }
        }

        // --- Color Creator (below the grid) ---
        const float creatorY = gridY + static_cast<float>(rows) * (sw + sp) + pad;
        const float barW = static_cast<float>(ES::RGB_BAR_W);
        const float barH = static_cast<float>(ES::RGB_BAR_H);
        const float barGap = static_cast<float>(ES::RGB_BAR_GAP);
        const float barX = mx + pad + 40.0f; // leave room for "R:" label

        // R bar
        creatorLabelR = makeUIText(ecsRef, mx + pad, creatorY + 2.0f, 55.0f,
                                   "R:", 1.0f,
                                   {255.0f, 100.0f, 100.0f, 255.0f});
        creatorBarR = makeUIQuad(ecsRef, barX, creatorY, 54.0f,
                                 barW, barH,
                                 {60.0f, 60.0f, 60.0f, 200.0f});
        creatorFillR = makeUIQuad(ecsRef, barX, creatorY, 55.0f,
                                  barW * (128.0f / 255.0f), barH,
                                  {255.0f, 0.0f, 0.0f, 200.0f});

        // G bar
        const float gBarY = creatorY + barH + barGap;
        creatorLabelG = makeUIText(ecsRef, mx + pad, gBarY + 2.0f, 55.0f,
                                   "G:", 1.0f,
                                   {100.0f, 255.0f, 100.0f, 255.0f});
        creatorBarG = makeUIQuad(ecsRef, barX, gBarY, 54.0f,
                                 barW, barH,
                                 {60.0f, 60.0f, 60.0f, 200.0f});
        creatorFillG = makeUIQuad(ecsRef, barX, gBarY, 55.0f,
                                  barW * (128.0f / 255.0f), barH,
                                  {0.0f, 255.0f, 0.0f, 200.0f});

        // B bar
        const float bBarY = gBarY + barH + barGap;
        creatorLabelB = makeUIText(ecsRef, mx + pad, bBarY + 2.0f, 55.0f,
                                   "B:", 1.0f,
                                   {100.0f, 100.0f, 255.0f, 255.0f});
        creatorBarB = makeUIQuad(ecsRef, barX, bBarY, 54.0f,
                                 barW, barH,
                                 {60.0f, 60.0f, 60.0f, 200.0f});
        creatorFillB = makeUIQuad(ecsRef, barX, bBarY, 55.0f,
                                  barW * (128.0f / 255.0f), barH,
                                  {0.0f, 0.0f, 255.0f, 200.0f});

        // Preview swatch
        const float previewY = bBarY + barH + barGap;
        creatorPreview = makeUIQuad(ecsRef, barX, previewY, 55.0f,
                                    sw, sw,
                                    {128.0f, 128.0f, 128.0f, 255.0f});

        // Add button
        const float addBtnX = barX + sw + barGap;
        creatorAddBtn = makeUIQuad(ecsRef, addBtnX, previewY, 54.0f,
                                   50.0f, sw,
                                   {40.0f, 120.0f, 40.0f, 220.0f});
        creatorAddLabel = makeUIText(ecsRef, addBtnX + 8.0f, previewY + 6.0f, 55.0f,
                                     "Add", 0.8f,
                                     {220.0f, 220.0f, 220.0f, 255.0f});

        // Cancel button
        const float cancelBtnX = addBtnX + 50.0f + barGap;
        creatorCancelBtn = makeUIQuad(ecsRef, cancelBtnX, previewY, 54.0f,
                                      60.0f, sw,
                                      {120.0f, 40.0f, 40.0f, 220.0f});
        creatorCancelLabel = makeUIText(ecsRef, cancelBtnX + 4.0f, previewY + 6.0f, 55.0f,
                                        "Cancel", 0.8f,
                                        {220.0f, 220.0f, 220.0f, 255.0f});

        // Start everything hidden
        hidePaletteModal();
        hideColorCreator();
    }

    // =========================================================================
    // Show/hide palette modal
    // =========================================================================

    void EditorUISystem::showPaletteModal()
    {
        auto setVis = [this](const EntityRef& e, bool v)
        {
            if (e.empty()) return;
            auto* pos = ecsRef->getComponent<PositionComponent>(e.id);
            if (pos) pos->setVisibility(v);
        };

        setVis(modalOverlay, true);
        setVis(modalPanel, true);
        setVis(modalTitle, true);
        setVis(modalHighlight, true);
        setVis(modalAddBtn, true);
        setVis(modalAddCrossH, true);
        setVis(modalAddCrossV, true);

        // Only show swatches up to current palette size (rest are hidden/excess)
        const int paletteSize = static_cast<int>(editor->canvas->palette.size());
        for (int i = 0; i < static_cast<int>(modalSwatches.size()); ++i)
            setVis(modalSwatches[static_cast<size_t>(i)], i < paletteSize);
    }

    void EditorUISystem::hidePaletteModal()
    {
        auto setVis = [this](const EntityRef& e, bool v)
        {
            if (e.empty()) return;
            auto* pos = ecsRef->getComponent<PositionComponent>(e.id);
            if (pos) pos->setVisibility(v);
        };

        setVis(modalOverlay, false);
        setVis(modalPanel, false);
        setVis(modalTitle, false);
        setVis(modalHighlight, false);
        setVis(modalAddBtn, false);
        setVis(modalAddCrossH, false);
        setVis(modalAddCrossV, false);

        for (auto& s : modalSwatches) setVis(s, false);

        // Also hide color creator
        hideColorCreator();
    }

    // =========================================================================
    // Show/hide color creator (inside modal)
    // =========================================================================

    void EditorUISystem::showColorCreator()
    {
        auto setVis = [this](const EntityRef& e, bool v)
        {
            if (e.empty()) return;
            auto* pos = ecsRef->getComponent<PositionComponent>(e.id);
            if (pos) pos->setVisibility(v);
        };

        setVis(creatorBarR, true);
        setVis(creatorBarG, true);
        setVis(creatorBarB, true);
        setVis(creatorFillR, true);
        setVis(creatorFillG, true);
        setVis(creatorFillB, true);
        setVis(creatorLabelR, true);
        setVis(creatorLabelG, true);
        setVis(creatorLabelB, true);
        setVis(creatorPreview, true);
        setVis(creatorAddBtn, true);
        setVis(creatorAddLabel, true);
        setVis(creatorCancelBtn, true);
        setVis(creatorCancelLabel, true);
    }

    void EditorUISystem::hideColorCreator()
    {
        auto setVis = [this](const EntityRef& e, bool v)
        {
            if (e.empty()) return;
            auto* pos = ecsRef->getComponent<PositionComponent>(e.id);
            if (pos) pos->setVisibility(v);
        };

        setVis(creatorBarR, false);
        setVis(creatorBarG, false);
        setVis(creatorBarB, false);
        setVis(creatorFillR, false);
        setVis(creatorFillG, false);
        setVis(creatorFillB, false);
        setVis(creatorLabelR, false);
        setVis(creatorLabelG, false);
        setVis(creatorLabelB, false);
        setVis(creatorPreview, false);
        setVis(creatorAddBtn, false);
        setVis(creatorAddLabel, false);
        setVis(creatorCancelBtn, false);
        setVis(creatorCancelLabel, false);
    }

    // =========================================================================
    // Update color creator fill bars and preview
    // =========================================================================

    void EditorUISystem::updateColorCreatorFill()
    {
        using ES = EditorSystem;
        const float barW = static_cast<float>(ES::RGB_BAR_W);

        auto updateFill = [this, barW](const EntityRef& fill, int value)
        {
            if (fill.empty()) return;
            auto* pos = ecsRef->getComponent<PositionComponent>(fill.id);
            if (pos) pos->setWidth(barW * (static_cast<float>(value) / 255.0f));
        };

        updateFill(creatorFillR, editor->creatorR);
        updateFill(creatorFillG, editor->creatorG);
        updateFill(creatorFillB, editor->creatorB);

        // Update preview swatch color
        if (!creatorPreview.empty())
        {
            auto* obj = ecsRef->getComponent<Simple2DObject>(creatorPreview.id);
            if (obj)
                obj->setColors({static_cast<float>(editor->creatorR),
                                static_cast<float>(editor->creatorG),
                                static_cast<float>(editor->creatorB),
                                255.0f});
        }

        // Update label text
        auto updateLabel = [this](const EntityRef& label, const std::string& prefix, int value)
        {
            if (label.empty()) return;
            auto* ttf = ecsRef->getComponent<TTFText>(label.id);
            if (ttf) ttf->setText(prefix + std::to_string(value));
        };

        updateLabel(creatorLabelR, "R:", editor->creatorR);
        updateLabel(creatorLabelG, "G:", editor->creatorG);
        updateLabel(creatorLabelB, "B:", editor->creatorB);
    }

    // =========================================================================
    // Rebuild modal swatches (when palette changes)
    // =========================================================================

    void EditorUISystem::rebuildModalSwatches()
    {
        using ES = EditorSystem;
        const int paletteSize = static_cast<int>(editor->canvas->palette.size());
        const float sw = static_cast<float>(ES::MODAL_SWATCH);
        const float sp = static_cast<float>(ES::MODAL_SWATCH_PAD);
        const float pad = static_cast<float>(ES::MODAL_PAD);

        // Recalculate modal position
        const float cols = static_cast<float>(ES::MODAL_COLS);
        const int totalItems = paletteSize + 1;
        const int rows = (totalItems + ES::MODAL_COLS - 1) / ES::MODAL_COLS;
        const float gridW = cols * (sw + sp) - sp;
        const float gridH = static_cast<float>(rows) * (sw + sp) - sp;

        const float modalW = gridW + pad * 2.0f;
        const float creatorH = static_cast<float>(ES::RGB_BAR_H) * 3.0f
                             + static_cast<float>(ES::RGB_BAR_GAP) * 4.0f
                             + 30.0f + 10.0f;
        const float titleH = 28.0f;
        const float modalH = titleH + gridH + pad * 2.0f + creatorH + pad;

        const float mx = (screenW - modalW) * 0.5f;
        const float my = (screenH - modalH) * 0.5f;

        // Update modal panel size and position
        if (auto* pos = ecsRef->getComponent<PositionComponent>(modalPanel.id))
        {
            pos->setX(mx);
            pos->setY(my);
            pos->setWidth(modalW);
            pos->setHeight(modalH);
        }

        // Update title position
        if (auto* pos = ecsRef->getComponent<PositionComponent>(modalTitle.id))
        {
            pos->setX(mx + pad);
            pos->setY(my + 6.0f);
        }

        const float gridX = mx + pad;
        const float gridY = my + titleH + pad;

        // Incremental update: reuse existing swatches, add new ones, hide excess
        const int oldCount = static_cast<int>(modalSwatches.size());

        // Update existing swatches (reposition + recolor)
        for (int i = 0; i < std::min(oldCount, paletteSize); ++i)
        {
            const int col = i % ES::MODAL_COLS;
            const int row = i / ES::MODAL_COLS;
            const float sx = gridX + static_cast<float>(col) * (sw + sp);
            const float sy = gridY + static_cast<float>(row) * (sw + sp);

            const glm::vec4& c = editor->canvas->palette[static_cast<size_t>(i)];

            if (auto* pos = ecsRef->getComponent<PositionComponent>(modalSwatches[static_cast<size_t>(i)].id))
            {
                pos->setX(sx);
                pos->setY(sy);
                pos->setVisibility(true);
            }
            if (auto* obj = ecsRef->getComponent<Simple2DObject>(modalSwatches[static_cast<size_t>(i)].id))
                obj->setColors({c.r, c.g, c.b, 255.0f});
        }

        // Hide excess swatches (if palette shrunk)
        for (int i = paletteSize; i < oldCount; ++i)
        {
            if (auto* pos = ecsRef->getComponent<PositionComponent>(modalSwatches[static_cast<size_t>(i)].id))
                pos->setVisibility(false);
        }

        // Create new swatches (if palette grew)
        for (int i = oldCount; i < paletteSize; ++i)
        {
            const int col = i % ES::MODAL_COLS;
            const int row = i / ES::MODAL_COLS;
            const float sx = gridX + static_cast<float>(col) * (sw + sp);
            const float sy = gridY + static_cast<float>(row) * (sw + sp);

            const glm::vec4& c = editor->canvas->palette[static_cast<size_t>(i)];
            auto swatch = makeUIQuad(ecsRef, sx, sy, 54.0f,
                                     sw, sw,
                                     {c.r, c.g, c.b, 255.0f});
            modalSwatches.push_back(swatch);
        }

        // Reposition "+" button
        {
            const int col = paletteSize % ES::MODAL_COLS;
            const int row = paletteSize / ES::MODAL_COLS;
            const float sx = gridX + static_cast<float>(col) * (sw + sp);
            const float sy = gridY + static_cast<float>(row) * (sw + sp);

            if (auto* pos = ecsRef->getComponent<PositionComponent>(modalAddBtn.id))
            { pos->setX(sx); pos->setY(sy); }
            if (auto* pos = ecsRef->getComponent<PositionComponent>(modalAddCrossH.id))
            { pos->setX(sx + sw * 0.5f - 8.0f); pos->setY(sy + sw * 0.5f - 2.0f); }
            if (auto* pos = ecsRef->getComponent<PositionComponent>(modalAddCrossV.id))
            { pos->setX(sx + sw * 0.5f - 2.0f); pos->setY(sy + sw * 0.5f - 8.0f); }
        }

        // Reposition color creator
        const float creatorY2 = gridY + static_cast<float>(rows) * (sw + sp) + pad;
        const float barH2 = static_cast<float>(ES::RGB_BAR_H);
        const float barGap2 = static_cast<float>(ES::RGB_BAR_GAP);
        const float barX2 = mx + pad + 40.0f;

        auto setXY = [this](const EntityRef& e, float x, float y)
        {
            if (auto* pos = ecsRef->getComponent<PositionComponent>(e.id))
            { pos->setX(x); pos->setY(y); }
        };

        setXY(creatorLabelR, mx + pad, creatorY2 + 2.0f);
        setXY(creatorBarR, barX2, creatorY2);
        setXY(creatorFillR, barX2, creatorY2);

        const float gBarY2 = creatorY2 + barH2 + barGap2;
        setXY(creatorLabelG, mx + pad, gBarY2 + 2.0f);
        setXY(creatorBarG, barX2, gBarY2);
        setXY(creatorFillG, barX2, gBarY2);

        const float bBarY2 = gBarY2 + barH2 + barGap2;
        setXY(creatorLabelB, mx + pad, bBarY2 + 2.0f);
        setXY(creatorBarB, barX2, bBarY2);
        setXY(creatorFillB, barX2, bBarY2);

        const float previewY2 = bBarY2 + barH2 + barGap2;
        setXY(creatorPreview, barX2, previewY2);

        const float addBtnX2 = barX2 + sw + barGap2;
        setXY(creatorAddBtn, addBtnX2, previewY2);
        setXY(creatorAddLabel, addBtnX2 + 8.0f, previewY2 + 6.0f);

        const float cancelBtnX2 = addBtnX2 + 50.0f + barGap2;
        setXY(creatorCancelBtn, cancelBtnX2, previewY2);
        setXY(creatorCancelLabel, cancelBtnX2 + 4.0f, previewY2 + 6.0f);

        // Update overlay size
        if (auto* pos = ecsRef->getComponent<PositionComponent>(modalOverlay.id))
        {
            pos->setWidth(screenW);
            pos->setHeight(screenH);
        }

        cachedPaletteSize = paletteSize;
    }

    // =========================================================================
    // Resize
    // =========================================================================

    void EditorUISystem::onEvent(const ResizeEvent& event)
    {
        if (event.width <= 0.0f || event.height <= 0.0f)
            return;

        screenW = event.width;
        screenH = event.height;

        if (uiCamera)
        {
            uiCamera->setWidth(screenW);
            uiCamera->setHeight(screenH);
        }

        repositionLayerPanel();

        // Reposition gizmo cube to top-right
        {
            using ES = EditorSystem;
            const float btn  = static_cast<float>(ES::GIZMO_BTN);
            const float gap  = static_cast<float>(ES::GIZMO_GAP);
            const float step = btn + gap;
            const float mg   = static_cast<float>(ES::GIZMO_MARGIN);
            const float gx0  = screenW - mg - 3.0f * step + gap;
            const float gy0  = mg;

            const int cols[] = { 1, 0, 1, 2, 1, 1 };
            const int rows[] = { 0, 1, 1, 1, 2, 3 };

            for (int i = 0; i < GIZMO_FACE_COUNT; ++i)
            {
                const float fx = gx0 + static_cast<float>(cols[i]) * step;
                const float fy = gy0 + static_cast<float>(rows[i]) * step;

                if (auto* p = ecsRef->getComponent<PositionComponent>(gizmoFaces[i].id))
                {
                    p->setX(fx);
                    p->setY(fy);
                }
                if (auto* p = ecsRef->getComponent<PositionComponent>(gizmoLabels[i].id))
                {
                    p->setX(fx + 4.0f);
                    p->setY(fy + 12.0f);
                }
            }
        }

        // Reposition action bar to top-centre
        {
            using ES = EditorSystem;
            const float btnW   = static_cast<float>(ES::ACTION_BAR_BTN_W);
            const float gap    = static_cast<float>(ES::ACTION_BAR_GAP);
            const float pad    = static_cast<float>(ES::ACTION_BAR_PAD);
            const float margin = static_cast<float>(ES::ACTION_BAR_MARGIN);

            const float contentW = btnW * 2.0f + gap;
            const float totalW   = contentW + pad * 2.0f;

            const float x0 = (screenW - totalW) * 0.5f;
            const float y0 = margin;

            auto setPos = [this](const EntityRef& e, float x, float y) {
                if (auto* p = ecsRef->getComponent<PositionComponent>(e.id))
                { p->setX(x); p->setY(y); }
            };

            setPos(actionBarBackdrop, x0, y0);

            const float openX = x0 + pad;
            const float btnY  = y0 + pad;
            setPos(actionBarOpenBtn,   openX,         btnY);
            setPos(actionBarOpenLabel, openX + 12.0f, btnY + 6.0f);

            const float saveX = openX + btnW + gap;
            setPos(actionBarSaveBtn,   saveX,         btnY);
            setPos(actionBarSaveLabel, saveX + 12.0f, btnY + 6.0f);
        }

        // Reposition modal overlay size
        if (auto* pos = ecsRef->getComponent<PositionComponent>(modalOverlay.id))
        {
            pos->setWidth(screenW);
            pos->setHeight(screenH);
        }

        // If modal is open, rebuild to re-center
        if (editor->paletteModalOpen)
            rebuildModalSwatches();
    }

    // =========================================================================
    // Execute
    // =========================================================================

    void EditorUISystem::execute()
    {
        if (!editor)
            return;

        // Toggle visibility on edit mode change
        if (editor->editMode != cachedEditMode)
        {
            cachedEditMode = editor->editMode;
            setAllVisible(cachedEditMode);
        }

        if (!editor->editMode)
            return;

        updateToolbar();
        updateLayerPanel();
        updatePaletteModal();
    }

    // =========================================================================
    // Update toolbar
    // =========================================================================

    void EditorUISystem::updateToolbar()
    {
        using ES = EditorSystem;
        const float ty = static_cast<float>(ES::TOOLBAR_Y);
        const float btn = static_cast<float>(ES::TOOLBAR_BTN);
        const float gap = static_cast<float>(ES::TOOLBAR_GAP);

        // Move highlight to active tool
        if (editor->activeTool != cachedTool)
        {
            cachedTool = editor->activeTool;

            float hlY = ty; // Place (slot 0)
            if (cachedTool == EditorTool::Erase)
                hlY = ty + btn + gap;            // slot 1
            else if (cachedTool == EditorTool::ColorPick)
                hlY = ty + (btn + gap) * 2.0f;   // slot 2

            if (auto* pos = ecsRef->getComponent<PositionComponent>(toolHighlight.id))
                pos->setY(hlY);
        }

        // Update color swatch button to reflect active color
        if (editor->activeColor != cachedToolColor)
        {
            cachedToolColor = editor->activeColor;

            if (cachedToolColor >= 0 && cachedToolColor < static_cast<int>(editor->canvas->palette.size()))
            {
                const glm::vec4& col = editor->canvas->palette[static_cast<size_t>(cachedToolColor)];
                if (auto* obj = ecsRef->getComponent<Simple2DObject>(toolColorBtn.id))
                    obj->setColors({col.r, col.g, col.b, 255.0f});
            }
        }
    }

    // =========================================================================
    // Update palette modal
    // =========================================================================

    void EditorUISystem::updatePaletteModal()
    {
        // Show/hide modal based on state
        if (editor->paletteModalOpen != cachedModalOpen)
        {
            cachedModalOpen = editor->paletteModalOpen;
            if (cachedModalOpen)
                showPaletteModal();
            else
                hidePaletteModal();
        }

        if (!editor->paletteModalOpen)
            return;

        // Rebuild swatches if palette size changed
        const int paletteSize = static_cast<int>(editor->canvas->palette.size());
        if (paletteSize != cachedPaletteSize)
            rebuildModalSwatches();

        // Update highlight position on active swatch
        if (editor->activeColor != cachedActiveColor)
        {
            cachedActiveColor = editor->activeColor;
            if (cachedActiveColor >= 0 && cachedActiveColor < static_cast<int>(modalSwatches.size()))
            {
                auto* hlPos = ecsRef->getComponent<PositionComponent>(modalHighlight.id);
                auto* swPos = ecsRef->getComponent<PositionComponent>(
                    modalSwatches[static_cast<size_t>(cachedActiveColor)].id);
                if (hlPos && swPos)
                {
                    hlPos->setX(swPos->x - 2.0f);
                    hlPos->setY(swPos->y - 2.0f);
                }
            }
        }

        // Show/hide color creator based on state
        if (editor->colorCreatorOpen != cachedCreatorOpen)
        {
            cachedCreatorOpen = editor->colorCreatorOpen;
            if (cachedCreatorOpen)
                showColorCreator();
            else
                hideColorCreator();
        }

        // Update color creator fill bars if creator is open
        if (editor->colorCreatorOpen)
            updateColorCreatorFill();
    }

    // =========================================================================
    // Update layer panel
    // =========================================================================

    void EditorUISystem::updateLayerPanel()
    {
        const int nbLayers = editor->canvas
            ? static_cast<int>(editor->canvas->layers.size())
            : 0;

        // Build current visibility bitmask
        uint16_t visBits = 0;
        for (int i = 0; i < nbLayers && i < 16; ++i)
            if (editor->canvas->layers[static_cast<size_t>(i)].visible)
                visBits |= (1u << i);

        const bool layerCountChanged = (nbLayers != cachedLayerCount);
        const bool activeChanged     = (editor->activeLayer != cachedActiveLayer);
        const bool visChanged        = (visBits != cachedVisBits);

        if (!layerCountChanged && !activeChanged && !visChanged)
            return;

        cachedLayerCount  = nbLayers;
        cachedActiveLayer = editor->activeLayer;
        cachedVisBits     = visBits;

        using ES = EditorSystem;
        const float pw = static_cast<float>(ES::LAYER_PANEL_W);
        const float rh = static_cast<float>(ES::LAYER_ROW_H);
        const float ew = static_cast<float>(ES::LAYER_EYE_W);
        const float bh = static_cast<float>(ES::LAYER_ADD_BTN_H);

        const float gizmoBottomY = static_cast<float>(ES::GIZMO_MARGIN)
                                 + 4.0f * static_cast<float>(ES::GIZMO_BTN + ES::GIZMO_GAP)
                                 + 8.0f;
        const float px = screenW - static_cast<float>(ES::LAYER_PANEL_MARGIN_RIGHT) - pw;
        const float py = gizmoBottomY;

        // Add new rows if layers were added
        auto layoutComp = layerLayout->get<VerticalLayout>();
        while (static_cast<int>(layerRows.size()) < nbLayers)
        {
            const int i = static_cast<int>(layerRows.size());
            LayerRow row;

            // Container quad for the row background
            row.container = makeUIQuad(ecsRef, 0.0f, 0.0f, 1.f,
                                       pw, rh - 2.0f,
                                       {60.0f, 60.0f, 100.0f, 200.0f});

            // Eye visibility toggle — anchored to container
            row.eye = makeUIQuad(ecsRef, 0.0f, 0.0f, 2.f,
                                 ew - 4.0f, rh - 10.0f,
                                 {200.0f, 200.0f, 200.0f, 240.0f});
            {
                auto eyeAnchor = row.eye->get<UiAnchor>();
                auto containerAnchor = row.container->get<UiAnchor>();
                eyeAnchor->setLeftAnchor(containerAnchor->left);
                eyeAnchor->setLeftMargin(2.0f);
                eyeAnchor->setTopAnchor(containerAnchor->top);
                eyeAnchor->setTopMargin(4.0f);
            }

            // Layer name label — anchored to container
            row.label = makeUIText(ecsRef, 0.0f, 0.0f, 3.f,
                                   "Layer " + std::to_string(i + 1), 1.f,
                                   {220.0f, 220.0f, 220.0f, 255.0f});
            {
                auto labelAnchor = row.label->get<UiAnchor>();
                auto containerAnchor = row.container->get<UiAnchor>();
                labelAnchor->setLeftAnchor(containerAnchor->left);
                labelAnchor->setLeftMargin(ew + 4.0f);
                labelAnchor->setTopAnchor(containerAnchor->top);
                labelAnchor->setTopMargin(4.0f);
            }

            if (layoutComp)
                layoutComp->addEntity(row.container);

            layerRows.push_back(row);
        }

        // Remove excess rows if layers were removed
        while (static_cast<int>(layerRows.size()) > nbLayers)
        {
            auto& row = layerRows.back();
            if (layoutComp)
                layoutComp->removeEntity(row.container);
            ecsRef->removeEntity(row.eye.id);
            ecsRef->removeEntity(row.label.id);
            layerRows.pop_back();
        }

        // Update backdrop height and position
        if (layerCountChanged)
        {
            const float panelH = static_cast<float>(nbLayers) * rh + bh + 4.0f;
            auto bdPos = layerBackdrop->get<PositionComponent>();
            if (bdPos)
            {
                bdPos->setX(px - 2.0f);
                bdPos->setY(py - 2.0f);
                bdPos->setHeight(panelH + 4.0f);
            }
        }

        // Update existing rows — active highlight, visibility alpha, label text
        for (int i = 0; i < nbLayers; ++i)
        {
            auto& row = layerRows[static_cast<size_t>(i)];

            auto rowObj = row.container->get<Simple2DObject>();
            if (rowObj)
            {
                if (i == editor->activeLayer)
                    rowObj->setColors({60.0f, 60.0f, 100.0f, 200.0f});
                else
                    rowObj->setColors({60.0f, 60.0f, 100.0f, 0.0f});
            }

            auto eyeObj = row.eye->get<Simple2DObject>();
            if (eyeObj)
            {
                const bool vis = editor->canvas->layers[static_cast<size_t>(i)].visible;
                eyeObj->setOpacity(vis ? 240.0f : 80.0f);
            }

            auto ttf = row.label->get<TTFText>();
            if (ttf)
            {
                const auto& layerName = editor->canvas->layers[static_cast<size_t>(i)].name;
                if (ttf->text != layerName)
                    ttf->setText(layerName);
            }
        }

        // Reposition "+" button below last layer (only when count changed)
        if (layerCountChanged)
        {
            const float addY = py + static_cast<float>(nbLayers) * rh;
            auto addPos = layerAddBtn->get<PositionComponent>();
            if (addPos)
            {
                addPos->setX(px);
                addPos->setY(addY);
            }

            auto crossHPos = layerAddCrossH->get<PositionComponent>();
            if (crossHPos)
            {
                crossHPos->setX(px + pw * 0.5f - 8.0f);
                crossHPos->setY(addY + bh * 0.5f - 2.0f);
            }

            auto crossVPos = layerAddCrossV->get<PositionComponent>();
            if (crossVPos)
            {
                crossVPos->setX(px + pw * 0.5f - 2.0f);
                crossVPos->setY(addY + bh * 0.5f - 8.0f);
            }
        }
    }

    // =========================================================================
    // Reposition layer panel (on resize)
    // =========================================================================

    void EditorUISystem::repositionLayerPanel()
    {
        using ES = EditorSystem;
        const float pw = static_cast<float>(ES::LAYER_PANEL_W);
        const float gizmoBottomY = static_cast<float>(ES::GIZMO_MARGIN)
                                 + 4.0f * static_cast<float>(ES::GIZMO_BTN + ES::GIZMO_GAP)
                                 + 8.0f;
        const float px = screenW - static_cast<float>(ES::LAYER_PANEL_MARGIN_RIGHT) - pw;
        const float py = gizmoBottomY;

        // Reposition the layout origin
        if (auto* pos = ecsRef->getComponent<PositionComponent>(layerLayout.id))
        {
            pos->setX(px);
            pos->setY(py);
        }

        // Force update to recalculate
        cachedLayerCount = -1;
    }

    // =========================================================================
    // Show/hide all UI
    // =========================================================================

    void EditorUISystem::setAllVisible(bool visible)
    {
        auto setVis = [this, visible](const EntityRef& e)
        {
            if (e.empty()) return;
            auto* pos = ecsRef->getComponent<PositionComponent>(e.id);
            if (pos) pos->setVisibility(visible);
        };

        // Toolbar
        setVis(toolbarBackdrop);
        setVis(toolHighlight);
        setVis(toolPlaceBtn);
        setVis(toolPlaceLabel);
        setVis(toolEraseBtn);
        setVis(toolEraseLabel);
        setVis(toolColorPickBtn);
        setVis(toolColorPickLabel);
        setVis(toolColorBtn);

        // Layer panel
        setVis(layerBackdrop);
        setVis(layerAddBtn);
        setVis(layerAddCrossH);
        setVis(layerAddCrossV);
        setVis(layerLayout);

        for (auto& row : layerRows)
        {
            setVis(row.container);
            setVis(row.eye);
            setVis(row.label);
        }

        // Gizmo cube
        for (int i = 0; i < GIZMO_FACE_COUNT; ++i)
        {
            setVis(gizmoFaces[i]);
            setVis(gizmoLabels[i]);
        }

        // Action bar
        setVis(actionBarBackdrop);
        setVis(actionBarOpenBtn);
        setVis(actionBarSaveBtn);
        setVis(actionBarOpenLabel);
        setVis(actionBarSaveLabel);

        // Modal: only show if it's actually open
        if (!visible || !editor->paletteModalOpen)
        {
            hidePaletteModal();
        }
        else
        {
            showPaletteModal();
        }
    }

} // namespace pg
