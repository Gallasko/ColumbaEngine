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
            auto comp = makeSimple2DShape(ecs, Shape2D::Square, w, h, color);
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
            auto bdAnchor = ecsRef->attach<UiAnchor>(paletteBackdrop);
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
            const glm::vec4& col = EDITOR_PALETTE[i];

            paletteSwatches[i] = makeUIQuad(
                ecsRef, 0.0f, 0.0f, 2.0f, ss, ss,
                {col.r, col.g, col.b, 255.0f});

            layoutComp->addEntity(paletteSwatches[i]);
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
        layerAddBtn = makeUIQuad(ecsRef, px, py, 0.1f,
                                 pw, bh - 2.0f,
                                 {40.0f, 80.0f, 40.0f, 200.0f});

        // "+" crosshair bars
        layerAddCrossH = makeUIQuad(ecsRef,
                                    px + pw * 0.5f - 8.0f, py + bh * 0.5f - 2.0f, 0.2f,
                                    16.0f, 4.0f,
                                    {220.0f, 220.0f, 220.0f, 255.0f});
        layerAddCrossV = makeUIQuad(ecsRef,
                                    px + pw * 0.5f - 2.0f, py + bh * 0.5f - 8.0f, 0.2f,
                                    4.0f, 16.0f,
                                    {220.0f, 220.0f, 220.0f, 255.0f});

        // "+" label
        layerAddLabel = makeUIText(ecsRef, px + pw * 0.5f + 12.0f, py + 2.0f, 0.3f,
                                   "+", 0.5f, {220.0f, 220.0f, 220.0f, 255.0f});
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
        auto* swatchPos = ecsRef->getComponent<PositionComponent>(
            paletteSwatches[static_cast<size_t>(cachedActiveColor)].id);
        auto* highlightPos = ecsRef->getComponent<PositionComponent>(paletteHighlight.id);
        if (swatchPos && highlightPos)
        {
            highlightPos->setX(swatchPos->x - 2.0f);
            highlightPos->setY(swatchPos->y - 2.0f);
        }
    }

    // =========================================================================
    // Update layer panel
    // =========================================================================

    void EditorUISystem::updateLayerPanel()
    {
        const int nbLayers = editor->canvas
            ? static_cast<int>(editor->canvas->layers.size())
            : 0;

        const bool layerCountChanged = (nbLayers != cachedLayerCount);
        const bool activeChanged = (editor->activeLayer != cachedActiveLayer);

        if (!layerCountChanged && !activeChanged)
        {
            // Still need to check visibility changes per layer
            bool visChanged = false;
            for (int i = 0; i < nbLayers && i < static_cast<int>(layerRows.size()); ++i)
            {
                auto* eyeObj = ecsRef->getComponent<Simple2DObject>(layerRows[static_cast<size_t>(i)].eye.id);
                if (!eyeObj) continue;
                const bool vis = editor->canvas->layers[static_cast<size_t>(i)].visible;
                const float targetAlpha = vis ? 240.0f : 80.0f;
                if (eyeObj->colors.w != targetAlpha)
                {
                    visChanged = true;
                    break;
                }
            }
            if (!visChanged) return;
        }

        cachedLayerCount = nbLayers;
        cachedActiveLayer = editor->activeLayer;

        using ES = EditorSystem;
        const float px = static_cast<float>(ES::LAYER_PANEL_X);
        const float py = static_cast<float>(ES::LAYER_PANEL_Y);
        const float pw = static_cast<float>(ES::LAYER_PANEL_W);
        const float rh = static_cast<float>(ES::LAYER_ROW_H);
        const float ew = static_cast<float>(ES::LAYER_EYE_W);
        const float bh = static_cast<float>(ES::LAYER_ADD_BTN_H);

        // Add new rows if layers were added
        auto* layoutComp = ecsRef->getComponent<VerticalLayout>(layerLayout.id);
        while (static_cast<int>(layerRows.size()) < nbLayers)
        {
            const int i = static_cast<int>(layerRows.size());
            LayerRow row;

            // Container quad for the row background
            row.container = makeUIQuad(ecsRef, 0.0f, 0.0f, 0.1f,
                                       pw, rh - 2.0f,
                                       {60.0f, 60.0f, 100.0f, 200.0f});

            // Eye visibility toggle
            row.eye = makeUIQuad(ecsRef, px + 2.0f, 0.0f, 0.2f,
                                 ew - 4.0f, rh - 10.0f,
                                 {200.0f, 200.0f, 200.0f, 240.0f});

            // Layer name label
            row.label = makeUIText(ecsRef, px + ew + 4.0f, 0.0f, 0.3f,
                                   "Layer " + std::to_string(i + 1), 0.4f,
                                   {220.0f, 220.0f, 220.0f, 255.0f});

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
        const float panelH = static_cast<float>(nbLayers) * rh + bh + 4.0f;
        auto* bdPos = ecsRef->getComponent<PositionComponent>(layerBackdrop.id);
        if (bdPos) bdPos->setHeight(panelH + 4.0f);

        // Update existing rows
        for (int i = 0; i < nbLayers; ++i)
        {
            auto& row = layerRows[static_cast<size_t>(i)];

            // Active layer highlight
            auto* rowObj = ecsRef->getComponent<Simple2DObject>(row.container.id);
            if (rowObj)
            {
                if (i == editor->activeLayer)
                    rowObj->setColors({60.0f, 60.0f, 100.0f, 200.0f});
                else
                    rowObj->setColors({60.0f, 60.0f, 100.0f, 0.0f});
            }

            // Eye visibility alpha
            auto* eyeObj = ecsRef->getComponent<Simple2DObject>(row.eye.id);
            if (eyeObj)
            {
                const bool vis = editor->canvas->layers[static_cast<size_t>(i)].visible;
                eyeObj->setOpacity(vis ? 240.0f : 80.0f);
            }

            // Position eye and label relative to the row container
            auto* containerPos = ecsRef->getComponent<PositionComponent>(row.container.id);
            if (containerPos)
            {
                const float ry = containerPos->y;

                auto* eyePos = ecsRef->getComponent<PositionComponent>(row.eye.id);
                if (eyePos)
                {
                    eyePos->setX(containerPos->x + 2.0f);
                    eyePos->setY(ry + 4.0f);
                }

                auto* labelPos = ecsRef->getComponent<PositionComponent>(row.label.id);
                if (labelPos)
                {
                    labelPos->setX(containerPos->x + ew + 4.0f);
                    labelPos->setY(ry + 4.0f);
                }
            }

            // Update label text if layer name changed
            auto* ttf = ecsRef->getComponent<TTFText>(row.label.id);
            if (ttf)
            {
                const auto& layerName = editor->canvas->layers[static_cast<size_t>(i)].name;
                if (ttf->text != layerName)
                    ttf->setText(layerName);
            }
        }

        // Reposition "+" button below last layer
        const float addY = py + static_cast<float>(nbLayers) * rh;
        auto* addPos = ecsRef->getComponent<PositionComponent>(layerAddBtn.id);
        if (addPos) addPos->setY(addY);

        auto* crossHPos = ecsRef->getComponent<PositionComponent>(layerAddCrossH.id);
        if (crossHPos) crossHPos->setY(addY + bh * 0.5f - 2.0f);

        auto* crossVPos = ecsRef->getComponent<PositionComponent>(layerAddCrossV.id);
        if (crossVPos) crossVPos->setY(addY + bh * 0.5f - 8.0f);

        auto* addLabelPos = ecsRef->getComponent<PositionComponent>(layerAddLabel.id);
        if (addLabelPos) addLabelPos->setY(addY + 2.0f);
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
        setVis(layerAddLabel);
        setVis(layerLayout);

        for (auto& row : layerRows)
        {
            setVis(row.container);
            setVis(row.eye);
            setVis(row.label);
        }
    }

} // namespace pg
