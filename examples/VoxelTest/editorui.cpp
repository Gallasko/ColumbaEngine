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
                                   const EditorSystem* editor)
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

        createPalette();
        createLayerPanel();
        createGizmoCube();
    }

    // =========================================================================
    // Palette creation
    // =========================================================================

    void EditorUISystem::createPalette()
    {
        using ES = EditorSystem;
        const float ss = static_cast<float>(ES::SWATCH_SIZE);
        const float sp = static_cast<float>(ES::SWATCH_PADDING);
        const float margin = static_cast<float>(ES::PALETTE_MARGIN);
        const float totalW = static_cast<float>(PALETTE_SIZE) * (ss + sp) - sp;

        // Create horizontal layout for swatches
        auto layout = makeHorizontalLayout(ecsRef, 0.0f, 0.0f, totalW, ss, false);
        layout.get<HorizontalLayout>()->spacing = static_cast<size_t>(sp);
        layout.get<PositionComponent>()->setZ(4.0f);
        paletteLayout = layout.entity;

        // Anchor layout to horizontal center + bottom of screen
        auto* mainWindow = ecsRef->getEntity("__MainWindow");
        if (mainWindow)
        {
            auto layoutAnchor = layout.get<UiAnchor>();
            auto windowAnchor = mainWindow->get<UiAnchor>();
            layoutAnchor->setHorizontalCenter(windowAnchor->horizontalCenter);
            layoutAnchor->setBottomAnchor(windowAnchor->bottom);
            layoutAnchor->setBottomMargin(margin);
        }

        // Backdrop (anchored relative to layout, will follow it)
        paletteBackdrop = makeUIQuad(ecsRef, 0.0f, 0.0f, 0.0f,
                                     totalW + sp * 2.0f, ss + sp * 2.0f,
                                     {20.0f, 20.0f, 20.0f, 180.0f});
        {
            auto layoutAnchor = layout.get<UiAnchor>();
            auto bdAnchor = paletteBackdrop->get<UiAnchor>();
            bdAnchor->setTopAnchor(layoutAnchor->top);
            bdAnchor->setTopMargin(-sp);
            bdAnchor->setLeftAnchor(layoutAnchor->left);
            bdAnchor->setLeftMargin(-sp);
        }

        // Highlight (white border behind active swatch)
        paletteHighlight = makeUIQuad(ecsRef, 0.0f, 0.0f, 1.0f,
                                      ss + 4.0f, ss + 4.0f,
                                      {255.0f, 255.0f, 255.0f, 255.0f});

        // Swatches — add to layout
        auto layoutComp = layout.get<HorizontalLayout>();
        for (size_t i = 0; i < PALETTE_SIZE; ++i)
        {
            const glm::vec4& col = editor->canvas->palette[i];

            paletteSwatches[i] = makeUIQuad(
                ecsRef, 0.0f, 0.0f, 4.0f, ss, ss,
                {col.r, col.g, col.b, 255.0f});

            layoutComp->addEntity(paletteSwatches[i]);
        }

        // Anchor highlight to first swatch so it starts in the right place
        {
            auto hlAnchor = paletteHighlight->get<UiAnchor>();
            auto swAnchor = paletteSwatches[0]->get<UiAnchor>();
            hlAnchor->setTopAnchor(swAnchor->top);
            hlAnchor->setTopMargin(-2.0f);
            hlAnchor->setLeftAnchor(swAnchor->left);
            hlAnchor->setLeftMargin(-2.0f);
        }
    }

    // =========================================================================
    // Layer panel creation
    // =========================================================================

    void EditorUISystem::createLayerPanel()
    {
        using ES = EditorSystem;
        const float px = static_cast<float>(ES::LAYER_PANEL_X);
        const float py = static_cast<float>(ES::LAYER_PANEL_Y);
        const float pw = static_cast<float>(ES::LAYER_PANEL_W);
        const float bh = static_cast<float>(ES::LAYER_ADD_BTN_H);

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
    // Gizmo cube creation
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

        updatePalette();
        updateLayerPanel();
    }

    // =========================================================================
    // Update palette highlight
    // =========================================================================

    void EditorUISystem::updatePalette()
    {
        if (editor->activeColor == cachedActiveColor)
            return;

        cachedActiveColor = editor->activeColor;

        // Position highlight relative to the active swatch's actual position
        auto hlAnchor = paletteHighlight->get<UiAnchor>();
        auto swAnchor = paletteSwatches[cachedActiveColor]->get<UiAnchor>();
        hlAnchor->setTopAnchor(swAnchor->top);
        hlAnchor->setTopMargin(-2.0f);
        hlAnchor->setLeftAnchor(swAnchor->left);
        hlAnchor->setLeftMargin(-2.0f);

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
        const float py = static_cast<float>(ES::LAYER_PANEL_Y);
        const float pw = static_cast<float>(ES::LAYER_PANEL_W);
        const float rh = static_cast<float>(ES::LAYER_ROW_H);
        const float ew = static_cast<float>(ES::LAYER_EYE_W);
        const float bh = static_cast<float>(ES::LAYER_ADD_BTN_H);

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

        // Update backdrop height
        if (layerCountChanged)
        {
            const float panelH = static_cast<float>(nbLayers) * rh + bh + 4.0f;
            auto bdPos = layerBackdrop->get<PositionComponent>();
            if (bdPos) bdPos->setHeight(panelH + 4.0f);
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
            if (addPos) addPos->setY(addY);

            auto crossHPos = layerAddCrossH->get<PositionComponent>();
            if (crossHPos) crossHPos->setY(addY + bh * 0.5f - 2.0f);

            auto crossVPos = layerAddCrossV->get<PositionComponent>();
            if (crossVPos) crossVPos->setY(addY + bh * 0.5f - 8.0f);
        }
    }

    // =========================================================================
    // Reposition layer panel (on resize) — panel is top-left, no change needed
    // =========================================================================

    void EditorUISystem::repositionLayerPanel()
    {
        // Layer panel is anchored to top-left, so no repositioning needed on resize.
        // Force update to recalculate anything that might depend on screen size.
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

        setVis(paletteLayout);
        setVis(paletteBackdrop);
        setVis(paletteHighlight);
        for (auto& s : paletteSwatches) setVis(s);

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

        for (int i = 0; i < GIZMO_FACE_COUNT; ++i)
        {
            setVis(gizmoFaces[i]);
            setVis(gizmoLabels[i]);
        }
    }

} // namespace pg
