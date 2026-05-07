#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"
#include "Input/sdlevents.h"
#include "ECS/entitysystem_fwd.h"

#include "itemregistry.h"
#include "reciperegistry.h"

#include <cstdint>
#include <string>

using namespace pg;

class CameraSystem;
class GridSystem;
class HotbarSystem;

// Always-visible tile-info HUD panel pinned to the top-right of the game
// scene. Shows the terrain under the mouse cursor along with what it drops,
// what tool tier is required to mine it, and which machines can process its
// drop. Empty / non-minable terrain shows just the name.
//
// Hover detection mirrors the TooltipSystem pattern: a QueuedListener for
// OnSDLMouseMotion buffers the cursor position, then a Listener<TickEvent>
// resolves the cursor into grid coords and rebuilds the panel only when the
// hovered cell changes.
class TileInspectorSystem : public System<InitSys,
                                           QueuedListener<OnSDLMouseMotion>,
                                           Listener<TickEvent>,
                                           Listener<ResizeEvent>>
{
public:
    static constexpr size_t UI_VP = 2;

    static constexpr float PANEL_W       = 220.0f;
    static constexpr float PANEL_PAD     = 10.0f;
    static constexpr float LINE_GAP      = 18.0f;
    static constexpr float TITLE_SCALE   = 0.36f;
    static constexpr float BODY_SCALE    = 0.28f;
    static constexpr float MARGIN_TOP    = 16.0f;
    static constexpr float MARGIN_RIGHT  = 16.0f;
    static constexpr float Z_BG          = 94.0f;
    static constexpr float Z_TEXT        = 95.0f;

    static constexpr int   NUM_LINES     = 4;   // title + drops + tier + machines

    static constexpr const char* FONT_PATH =
        "res/font/Inter/static/Inter_28pt-Light.ttf";

    TileInspectorSystem(CameraSystem* cameraSystem,
                        GridSystem* gridSystem,
                        HotbarSystem* hotbar,
                        ItemRegistry* itemRegistry,
                        RecipeRegistry* recipeRegistry,
                        float screenWidth, float screenHeight)
        : cameraSystem(cameraSystem),
          gridSystem(gridSystem),
          hotbar(hotbar),
          itemRegistry(itemRegistry),
          recipeRegistry(recipeRegistry),
          screenWidth(screenWidth),
          screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override
        { return "Tile Inspector System"; }

    void init() override;

    virtual void onProcessEvent(const OnSDLMouseMotion& event) override;
    virtual void onEvent(const TickEvent& event) override;
    virtual void onEvent(const ResizeEvent& event) override;

    void execute() override {}

private:
    void layoutPanel();
    void rebuildContent(int gx, int gy);

    void setLine(int idx, const std::string& text,
                 const constant::Vector4D& color, bool show);

    CameraSystem*   cameraSystem    = nullptr;
    GridSystem*     gridSystem      = nullptr;
    HotbarSystem*   hotbar          = nullptr;
    ItemRegistry*   itemRegistry    = nullptr;
    RecipeRegistry* recipeRegistry  = nullptr;

    float screenWidth  = 0.0f;
    float screenHeight = 0.0f;

    bool created = false;

    // Hover state — flushed during execute when the cell under the mouse
    // changes. -1 = no hover.
    float cursorX = 0.0f;
    float cursorY = 0.0f;
    int   lastHoverGX = -1000;
    int   lastHoverGY = -1000;
    bool  pendingRebuild = true;

    uint64_t backdropId = 0;
    uint64_t lineIds[NUM_LINES] = {};
};
