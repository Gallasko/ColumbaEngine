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
        EditorUISystem(MasterRenderer* masterRenderer, const EditorSystem* editor);

        std::string getSystemName() const override { return "Editor UI System"; }

        void init() override;
        void execute() override;

        void onEvent(const ResizeEvent& event) override;

        BaseCamera2D* uiCamera = nullptr;

    private:
        void createPalette();
        void createLayerPanel();
        void updatePalette();
        void updateLayerPanel();
        void repositionLayerPanel();
        void setAllVisible(bool visible);

        const EditorSystem* editor = nullptr;
        MasterRenderer* masterRenderer = nullptr;

        float screenW = 1280.0f;
        float screenH = 720.0f;

        // Palette entities
        EntityRef paletteLayout;
        EntityRef paletteBackdrop;
        EntityRef paletteHighlight;
        std::array<EntityRef, PALETTE_SIZE> paletteSwatches;

        // Layer panel entities
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
    };

} // namespace pg
