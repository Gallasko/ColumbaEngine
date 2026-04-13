#pragma once

#include "ECS/system.h"
#include "ECS/entityref.h"
#include "Systems/coresystems.h"
#include "Input/inputcomponent.h"

#include "editorstate.h"
#include "voxelserializer.h"

namespace pg
{
    class MasterRenderer;
    class Window;
    struct Camera3DControllerEditor;

    /**
     * Core voxel-editor logic system.
     *
     * Keyboard:
     *   Tab       — toggle edit / fly mode
     *   Ctrl+Z    — undo last command
     *   Ctrl+Y    — redo
     *   Ctrl+S    — save project
     *   Ctrl+O    — load project
     *   Ctrl+E    — export OBJ
     *
     * Mouse (edit mode only):
     *   Left-click  — place block at the hovered empty cell under the cursor.
     *   Right-click — remove the hovered block.
     *
     * Clicks over UI panels (palette, layers) are consumed by the UI and do
     * not reach the 3D world.
     */
    struct EditorSystem
        : public System<InitSys,
                        Listener<OnSDLScanCode>,
                        Listener<OnSDLMouseMotion>,
                        Listener<OnMouseClick>>
    {
        EditorSystem(MasterRenderer*     mr,
                     Window*             window,
                     Camera3DControllerEditor* cam,
                     Canvas*             canvas);

        std::string getSystemName() const override { return "Editor System"; }

        void init() override;

        // Fill y=-1 plane with an alternating grey floor pattern on layer 0.
        void buildFloor();

        void onEvent(const OnSDLScanCode&   event) override;
        void onEvent(const OnSDLMouseMotion& event) override;
        void onEvent(const OnMouseClick&     event) override;

        // Toggle a layer's visibility (show/hide its blocks).
        void toggleLayerVisibility(int layerIdx);

        // UI panel layout — shared with EditorUISystem so both agree on rects.
        // All values are in pixels.
        static constexpr int SWATCH_SIZE    = 32;
        static constexpr int SWATCH_PADDING = 4;
        static constexpr int PALETTE_MARGIN = 12; // from bottom of screen

        static constexpr int LAYER_PANEL_X   = 10;
        static constexpr int LAYER_PANEL_Y   = 10;
        static constexpr int LAYER_PANEL_W   = 160;
        static constexpr int LAYER_ROW_H     = 28;
        static constexpr int LAYER_EYE_W     = 24;
        static constexpr int LAYER_ADD_BTN_H = 24;

        // Gizmo cube (top-right corner)
        static constexpr int GIZMO_BTN   = 40;   // button size in px
        static constexpr int GIZMO_GAP   = 2;    // gap between buttons
        static constexpr int GIZMO_MARGIN = 10;  // from top-right corner

        // Public state — read by EditorUIRenderer to draw the overlay.
        bool       editMode    = true;
        int        activeColor = 0;   // index into canvas->palette
        int        activeLayer = 0;   // index into canvas->layers
        int        screenW     = 1280;
        int        screenH     = 720;
        Canvas*    canvas      = nullptr; // non-owning pointer to the app-owned Canvas

        // Current ghost (preview) cell — {-1,-1,-1} when none.
        glm::ivec3 ghostCell   = { -1, -1, -1 };

        // Save/load/export
        void saveProject(const std::string& path = "project.vxl.json");
        void loadProject(const std::string& path = "project.vxl.json");
        void exportToOBJ(const std::string& basePath = "export");

        // Apply parsed project data to the canvas (used by both desktop and web load paths).
        void applyProjectData(const ProjectData& data);

    private:
        // Clear all entities from the canvas (used before loading).
        void clearCanvas();
        // Returns true if the screen pixel (px, py) falls inside any UI panel
        // and handles the interaction (color pick, layer toggle, etc.).
        bool handleUIClick(int px, int py, bool rightBtn);

        // Unproject screen pixel (px, py) → world-space ray direction.
        glm::vec3 screenToRay(int px, int py) const;

        // DDA voxel raycast from mouse cursor position.
        // hitCell:   first occupied cell found; only valid when return is true.
        // placeCell: last empty in-bounds cell before the hit.
        bool raycast(int mouseX, int mouseY, glm::ivec3& hitCell, glm::ivec3& placeCell);

        // Move/hide the ghost preview entity.
        void updateGhost(const glm::ivec3& cell);
        void hideGhost();

        // High-level place/remove — update undo stack and call do* variants.
        void placeBlock(const glm::ivec3& pos);
        void removeBlock(const glm::ivec3& pos);

        // Low-level place/remove — do not recurse into undo/redo stacks.
        void doPlace(const glm::ivec3& pos, const glm::vec4& color, int layerIdx);
        void doRemove(const glm::ivec3& pos);

        // Create a standalone voxel entity at an arbitrary position (no canvas/layer).
        EntityRef createVoxel(const glm::ivec3& pos, const glm::vec4& color);

        MasterRenderer*      mr     = nullptr;
        Window*              window = nullptr;
        Camera3DControllerEditor*  cam    = nullptr;

        // Ghost preview entity (not stored in canvas, not undo-able).
        EntityRef ghostEntity;

        // Last known mouse pixel position (updated by OnSDLMouseMotion).
        int mouseX = 0;
        int mouseY = 0;

        std::vector<EditorCmd> undoStack;
        std::vector<EditorCmd> redoStack;
    };

} // namespace pg
