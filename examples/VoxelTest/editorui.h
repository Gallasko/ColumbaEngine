#pragma once

#include "ECS/system.h"
#include "ECS/entityref.h"
#include "Systems/coresystems.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"
#include "UI/sizer.h"
#include "Renderer/camera.h"

#include "editorsystem.h"

namespace pg
{
    static constexpr int MAX_UI_LAYERS = 16;

    static const std::string EDITOR_FONT = "inter";

    struct EditorUISystem
        : public System<InitSys, Listener<ResizeEvent>>
    {
        EditorUISystem(MasterRenderer* masterRenderer, EditorSystem* editor);

        std::string getSystemName() const override { return "Editor UI System"; }

        void init() override;
        void execute() override;

        void onEvent(const ResizeEvent& event) override;

        BaseCamera2D* uiCamera = nullptr;

    private:
        // Creation
        void createToolbar();
        void createLayerPanel();
        void createGizmoCube();
        void createActionBar();
        void createPaletteModal();

        // Updates
        void updateToolbar();
        void updateLayerPanel();
        void updatePaletteModal();

        // Helpers
        void repositionLayerPanel();
        void setAllVisible(bool visible);

        // Palette modal show/hide
        void showPaletteModal();
        void hidePaletteModal();
        void rebuildModalSwatches();
        void showColorCreator();
        void hideColorCreator();
        void updateColorCreatorFill();

        EditorSystem* editor = nullptr;
        MasterRenderer* masterRenderer = nullptr;

        float screenW = 1280.0f;
        float screenH = 720.0f;

        // ---- Toolbar (left side) ----
        EntityRef toolbarBackdrop;
        EntityRef toolPlaceBtn;
        EntityRef toolPlaceLabel;
        EntityRef toolEraseBtn;
        EntityRef toolEraseLabel;
        EntityRef toolColorPickBtn;
        EntityRef toolColorPickLabel;
        EntityRef toolColorBtn;       // swatch showing active color, opens modal
        EntityRef toolHighlight;      // selection indicator on active tool

        EditorTool cachedTool = EditorTool::Place;
        int cachedToolColor = -1;

        // ---- Layer panel (right side, below gizmo) ----
        EntityRef layerBackdrop;
        EntityRef layerAddBtn;
        EntityRef layerAddCrossH;
        EntityRef layerAddCrossV;

        struct LayerRow
        {
            EntityRef container;
            EntityRef eye;
            EntityRef label;
        };
        EntityRef layerLayout;
        std::vector<LayerRow> layerRows;

        int cachedActiveColor    = -1;
        int cachedActiveLayer    = -1;
        int cachedLayerCount     = -1;
        uint16_t cachedVisBits   = 0;
        bool cachedEditMode      = true;

        // ---- Gizmo cube (top-right) ----
        static constexpr int GIZMO_FACE_COUNT = 6;
        EntityRef gizmoFaces[GIZMO_FACE_COUNT];
        EntityRef gizmoLabels[GIZMO_FACE_COUNT];

        // ---- Action bar (top-centre) ----
        EntityRef actionBarBackdrop;
        EntityRef actionBarOpenBtn;
        EntityRef actionBarSaveBtn;
        EntityRef actionBarOpenLabel;
        EntityRef actionBarSaveLabel;

        // ---- Palette modal ----
        EntityRef modalOverlay;        // full-screen semi-transparent backdrop
        EntityRef modalPanel;          // modal background
        EntityRef modalTitle;          // "Palette" text
        std::vector<EntityRef> modalSwatches; // color swatch grid
        EntityRef modalAddBtn;         // "+" button
        EntityRef modalAddCrossH;     // horizontal bar of "+"
        EntityRef modalAddCrossV;     // vertical bar of "+"
        EntityRef modalHighlight;      // highlight on selected swatch

        // Color creator (inside modal)
        EntityRef creatorBarR, creatorBarG, creatorBarB;       // background bars
        EntityRef creatorFillR, creatorFillG, creatorFillB;    // fill quads
        EntityRef creatorLabelR, creatorLabelG, creatorLabelB; // R/G/B value text
        EntityRef creatorPreview;                              // preview swatch
        EntityRef creatorAddBtn, creatorAddLabel;
        EntityRef creatorCancelBtn, creatorCancelLabel;

        bool cachedModalOpen = false;
        bool cachedCreatorOpen = false;
        int cachedPaletteSize = -1;
    };

} // namespace pg
