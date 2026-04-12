#include "stdafx.h"

#include "editorui.h"

#include "Renderer/mesh.h"
#include "logger.h"

namespace pg
{
    namespace
    {
        constexpr const char* DOM = "EditorUI";

        // Floats per instance: pixelX, pixelY, pixelW, pixelH, R, G, B, A
        constexpr size_t STRIDE = 8;
    }

    // =========================================================================
    // Construction / init
    // =========================================================================

    EditorUIRenderer::EditorUIRenderer(MasterRenderer* masterRenderer,
                                       const EditorSystem* editor)
        : AbstractRenderer(masterRenderer, RenderStage::PostProcess)
        , editor(editor)
    {}

    void EditorUIRenderer::init()
    {
        // Mesh: SimpleSquareMesh with instance attributes {2, 2, 4}
        // → location 1: vec2 pixelPos, location 2: vec2 pixelSize,
        //   location 3: vec4 color (0..255)
        squareMesh = std::make_shared<SimpleSquareMesh>(
            std::vector<size_t>{ 2, 2, 4 });

        Material mat;
        mat.shader       = masterRenderer->getShader("editorui");
        mat.nbTextures   = 0;
        mat.nbAttributes = STRIDE;

        mat.uniformMap["uScreenSize"] = UniformValue(
            glm::vec2(screenW, screenH));

        mat.mesh = squareMesh;

        matId = masterRenderer->registerMaterial("__editorUI", mat);

        // Force first build
        changed = true;
        dirty   = true;
    }

    // =========================================================================
    // Resize
    // =========================================================================

    void EditorUIRenderer::onEvent(const ResizeEvent& event)
    {
        if (event.width <= 0.0f || event.height <= 0.0f)
            return;

        screenW = event.width;
        screenH = event.height;

        masterRenderer->setMaterialUniform("__editorUI", "uScreenSize",
                                           UniformValue(glm::vec2(screenW, screenH)));

        changed = true;
        dirty   = true;
    }

    // =========================================================================
    // Execute
    // =========================================================================

    void EditorUIRenderer::execute()
    {
        rebuildCalls();
        finishChanges();
    }

    // =========================================================================
    // Rebuild render call list
    // =========================================================================

    void EditorUIRenderer::rebuildCalls()
    {
        renderCallList.clear();

        if (!editor || !editor->editMode)
            return; // nothing to draw when not in edit mode

        const float sw = screenW;
        const float sh = screenH;

        // Collect all quads in a single batchable render call
        RenderCall call;
        call.setVisibility(true);
        call.setRenderStage(RenderStage::PostProcess);
        call.setViewport(0);
        call.setOpacity(OpacityType::Opaque);
        call.setDepth(0);
        call.setMaterial(matId);
        call.batchable = true;
        call.mesh      = squareMesh;

        auto& d = call.data;

        // -----------------------------------------------------------------
        // 1. Crosshair (centre of screen)
        // -----------------------------------------------------------------
        {
            constexpr float CX_LONG  = 20.0f;
            constexpr float CX_SHORT =  2.0f;
            constexpr float R = 255.0f, G = 255.0f, B = 255.0f, A = 200.0f;

            const float cx = sw * 0.5f - CX_LONG * 0.5f;
            const float cy = sh * 0.5f - CX_SHORT * 0.5f;

            // Horizontal bar
            pushQuad(d, cx,              cy,              CX_LONG, CX_SHORT, R, G, B, A);
            // Vertical bar
            pushQuad(d, cx + CX_LONG * 0.5f - CX_SHORT * 0.5f,
                        cy - CX_LONG * 0.5f + CX_SHORT * 0.5f,
                        CX_SHORT, CX_LONG, R, G, B, A);
        }

        // -----------------------------------------------------------------
        // 2. Palette bar (bottom-centre)
        // -----------------------------------------------------------------
        {
            using ES = EditorSystem;
            const float ss = static_cast<float>(ES::SWATCH_SIZE);
            const float sp = static_cast<float>(ES::SWATCH_PADDING);
            const float margin = static_cast<float>(ES::PALETTE_MARGIN);

            const float totalW = static_cast<float>(PALETTE_SIZE) * (ss + sp) - sp;
            const float x0 = (sw - totalW) * 0.5f;
            const float y0 = sh - ss - margin;

            // Dark backdrop
            pushQuad(d, x0 - sp, y0 - sp, totalW + sp * 2.0f, ss + sp * 2.0f,
                     20.0f, 20.0f, 20.0f, 180.0f);

            for (int i = 0; i < PALETTE_SIZE; ++i)
            {
                const glm::vec4& col = EDITOR_PALETTE[i];
                const float xi = x0 + static_cast<float>(i) * (ss + sp);

                // White border on selected swatch
                if (i == editor->activeColor)
                    pushQuad(d, xi - 2.0f, y0 - 2.0f, ss + 4.0f, ss + 4.0f,
                             255.0f, 255.0f, 255.0f, 255.0f);

                pushQuad(d, xi, y0, ss, ss, col.r, col.g, col.b, 255.0f);
            }
        }

        // -----------------------------------------------------------------
        // 3. Layers panel (top-left)
        // -----------------------------------------------------------------
        {
            using ES = EditorSystem;
            const float px  = static_cast<float>(ES::LAYER_PANEL_X);
            const float py  = static_cast<float>(ES::LAYER_PANEL_Y);
            const float pw  = static_cast<float>(ES::LAYER_PANEL_W);
            const float rh  = static_cast<float>(ES::LAYER_ROW_H);
            const float ew  = static_cast<float>(ES::LAYER_EYE_W);
            const float bh  = static_cast<float>(ES::LAYER_ADD_BTN_H);

            const int nbLayers = static_cast<int>(editor->canvas
                                                 ? editor->canvas->layers.size()
                                                 : 0);

            // Panel backdrop
            const float panelH = static_cast<float>(nbLayers) * rh + bh + 4.0f;
            pushQuad(d, px - 2.0f, py - 2.0f, pw + 4.0f, panelH + 4.0f,
                     20.0f, 20.0f, 20.0f, 180.0f);

            for (int i = 0; i < nbLayers; ++i)
            {
                const float ry = py + static_cast<float>(i) * rh;
                const bool  isActive  = (i == editor->activeLayer);
                const bool  isVisible = editor->canvas->layers[static_cast<size_t>(i)].visible;

                // Row highlight
                if (isActive)
                    pushQuad(d, px, ry, pw, rh - 2.0f,
                             60.0f, 60.0f, 100.0f, 200.0f);

                // Eye icon (filled = visible, dim = hidden)
                const float eyeAlpha = isVisible ? 240.0f : 80.0f;
                pushQuad(d, px + 2.0f, ry + 4.0f, ew - 4.0f, rh - 10.0f,
                         200.0f, 200.0f, 200.0f, eyeAlpha);

                // Colour chip from active color (just a neutral grey here)
                pushQuad(d, px + ew + 2.0f, ry + 4.0f, 14.0f, rh - 10.0f,
                         160.0f, 160.0f, 160.0f, 220.0f);
            }

            // "+" button
            const float addY = py + static_cast<float>(nbLayers) * rh;
            pushQuad(d, px, addY, pw, bh - 2.0f,
                     40.0f, 80.0f, 40.0f, 200.0f);
            // "+" crosshair on the button (two bars)
            pushQuad(d, px + pw * 0.5f - 8.0f, addY + bh * 0.5f - 2.0f,
                     16.0f, 4.0f, 220.0f, 220.0f, 220.0f, 255.0f);
            pushQuad(d, px + pw * 0.5f - 2.0f, addY + bh * 0.5f - 8.0f,
                     4.0f, 16.0f, 220.0f, 220.0f, 220.0f, 255.0f);
        }

        if (!d.empty())
            renderCallList.push_back(std::move(call));
    }

    // =========================================================================
    // Helper
    // =========================================================================

    void EditorUIRenderer::pushQuad(std::vector<float>& data,
                                    float x, float y, float w, float h,
                                    float r, float g, float b, float a)
    {
        data.push_back(x);
        data.push_back(y);
        data.push_back(w);
        data.push_back(h);
        data.push_back(r);
        data.push_back(g);
        data.push_back(b);
        data.push_back(a);
    }

} // namespace pg
