#pragma once

#include "ECS/system.h"
#include "Systems/coresystems.h"
#include "Input/inputcomponent.h"

#include "editorstate.h"

namespace pg
{
    class MasterRenderer;
    class Window;
    struct Camera3DController;

    /**
     * Core voxel-editor logic system.
     *
     * Keyboard:
     *   Tab       — toggle edit / fly mode
     *   Ctrl+Z    — undo last command
     *   Ctrl+Y    — redo
     *
     * Mouse (edit mode only):
     *   Left-click  — place block at the hovered empty cell adjacent to the
     *                 first occupied cell (or canvas boundary) in view.
     *   Right-click — remove the hovered block.
     *
     * Clicks over UI panels (palette, layers) are consumed by the UI and do
     * not reach the 3D world.
     */
    struct EditorSystem
        : public System<InitSys,
                        Listener<OnSDLScanCode>,
                        Listener<OnMouseClick>>
    {
        EditorSystem(MasterRenderer*     mr,
                     Window*             window,
                     Camera3DController* cam,
                     Canvas*             canvas);

        std::string getSystemName() const override { return "Editor System"; }

        void init() override {}

        // Fill y=0 plane with a grey floor block on layer 0.
        void buildFloor();

        void onEvent(const OnSDLScanCode& event) override;
        void onEvent(const OnMouseClick&  event) override;

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

        // Public state — read by EditorUIRenderer to draw the overlay.
        bool    editMode    = false;
        int     activeColor = 0;   // index into EDITOR_PALETTE
        int     activeLayer = 0;   // index into canvas->layers
        int     screenW     = 1280;
        int     screenH     = 720;
        Canvas* canvas      = nullptr; // non-owning pointer to the app-owned Canvas

    private:
        // Returns true if the screen pixel (px, py) falls inside any UI panel
        // and handles the interaction (color pick, layer toggle, etc.).
        bool handleUIClick(int px, int py, bool rightBtn);

        // DDA voxel raycast along the camera look direction.
        // hitCell:   first occupied cell found; only valid when return is true.
        // placeCell: last empty in-bounds cell before the hit.
        bool raycast(glm::ivec3& hitCell, glm::ivec3& placeCell) const;

        // High-level place/remove — update undo stack and call do* variants.
        void placeBlock(const glm::ivec3& pos);
        void removeBlock(const glm::ivec3& pos);

        // Low-level place/remove — do not recurse into undo/redo stacks.
        void doPlace(const glm::ivec3& pos, const glm::vec4& color, int layerIdx);
        void doRemove(const glm::ivec3& pos);

        MasterRenderer*      mr     = nullptr;
        Window*              window = nullptr;
        Camera3DController*  cam    = nullptr;

        std::vector<EditorCmd> undoStack;
        std::vector<EditorCmd> redoStack;
    };

} // namespace pg
