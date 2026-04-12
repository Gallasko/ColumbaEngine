#pragma once

#include "Renderer/renderer.h"
#include "ECS/entitysystem_fwd.h"

#include "editorsystem.h"

#include <memory>

namespace pg
{
    /**
     * Overlay UI renderer for the voxel editor.
     *
     * Draws (only when editMode == true):
     *   – Crosshair at screen centre
     *   – Palette bar along the bottom
     *   – Layers panel on the top-left
     *
     * Uses the "editorui" shader with SimpleSquareMesh({2,2,4}) so every quad
     * is specified as (pixelX, pixelY, pixelW, pixelH, R, G, B, A) per instance.
     * The shader maps pixel coords → NDC at z = -1 (near plane), which always
     * passes the GL_LESS depth test and renders on top of all 3-D geometry.
     */
    struct EditorUIRenderer
        : public AbstractRenderer,
          public System<InitSys, Listener<ResizeEvent>>
    {
        EditorUIRenderer(MasterRenderer* masterRenderer, const EditorSystem* editor);

        std::string getSystemName() const override { return "Editor UI Renderer"; }

        void init() override;
        void execute() override;

        void onEvent(const ResizeEvent& event) override;

    private:
        // Emit one quad (x, y, w, h, r, g, b, a — pixels / 0..255) into data.
        static void pushQuad(std::vector<float>& data,
                             float x, float y, float w, float h,
                             float r, float g, float b, float a = 255.0f);

        void rebuildCalls();

        const EditorSystem* editor = nullptr;

        float screenW = 1280.0f;
        float screenH = 720.0f;

        size_t matId = 0;
        std::shared_ptr<Mesh> squareMesh;
    };

} // namespace pg
