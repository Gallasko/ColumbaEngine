#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"
#include "ECS/entitysystem_fwd.h"

#include <cstdint>
#include <string>

#include "tutorialevents.h"

using namespace pg;

class CameraSystem;

// Imperative overlay used by the tutorial to focus the player on a single
// target (a world tile, a UI element, or a fixed screen rect). Renders:
//   - Four dim "frame" rects that surround the target (a cheap cutout
//     equivalent — preserves spatial context without needing an alpha mask),
//   - A small bouncing arrow indicator next to the target,
//   - A persistent corner objective panel (title + body),
//   - An optional "Skip Tutorial" button on the corner panel.
//
// The TutorialSystem drives it via show()/hide(); the overlay handles all
// rendering, animation, and target tracking (camera pan, UI reflow).
class SpotlightOverlaySystem : public System<InitSys,
                                              Listener<TickEvent>,
                                              Listener<ResizeEvent>,
                                              Listener<OnMouseClick>>
{
public:
    static constexpr size_t UI_VP = 2;

    static constexpr float DIM_FRAME_Z       = 105.0f;
    static constexpr float ARROW_Z           = 106.0f;
    static constexpr float CORNER_Z          = 106.0f;
    static constexpr float CORNER_TEXT_Z     = 107.0f;
    static constexpr float SKIP_Z            = 107.0f;
    static constexpr float SKIP_TEXT_Z       = 108.0f;

    static constexpr float CORNER_W          = 320.0f;
    static constexpr float CORNER_H          = 80.0f;
    static constexpr float CORNER_PAD        = 10.0f;
    static constexpr float TITLE_SCALE       = 0.42f;
    static constexpr float BODY_SCALE        = 0.30f;

    static constexpr float SKIP_W            = 88.0f;
    static constexpr float SKIP_H            = 24.0f;
    static constexpr float SKIP_TEXT_SCALE   = 0.26f;

    static constexpr float ARROW_SIZE        = 18.0f;
    static constexpr float ARROW_OFFSET      = 8.0f;     // gap between target and arrow
    static constexpr float ARROW_BOB_PIXELS  = 5.0f;
    static constexpr float ARROW_BOB_HZ      = 1.6f;

    static constexpr float DIM_ALPHA         = 175.0f;   // 0..255

    static constexpr const char* FONT_PATH =
        "res/font/Inter/static/Inter_28pt-Light.ttf";

    enum class TargetKind : uint8_t
    {
        None = 0,
        WorldTile,
        ScreenRect,
        UiEntity,
    };

    enum class ArrowSide : uint8_t
    {
        Top = 0,    // arrow above the target, pointing down
        Bottom,     // arrow below the target, pointing up
        Left,       // arrow left of the target, pointing right
        Right,      // arrow right of the target, pointing left
    };

    struct Target
    {
        TargetKind kind = TargetKind::None;

        // WorldTile
        int gridX = 0;
        int gridY = 0;
        int gridW = 1;          // tiles wide
        int gridH = 1;          // tiles tall

        // ScreenRect
        float sx = 0, sy = 0, sw = 0, sh = 0;

        // UiEntity
        uint64_t entityId = 0;

        // Optional override for arrow placement. When non-zero, the arrow
        // points at this entity's rect instead of the spotlight cutout — used
        // when the dim cutout spans a large region (e.g. inventory + hotbar)
        // but the actionable element is just one part of it (e.g. a single
        // hotbar slot).
        uint64_t arrowEntityId = 0;
    };

    SpotlightOverlaySystem(CameraSystem* cameraSystem,
                           float screenWidth, float screenHeight)
        : cameraSystem(cameraSystem),
          screenWidth(screenWidth),
          screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override
        { return "Spotlight Overlay System"; }

    void init() override;

    virtual void onEvent(const TickEvent& event) override;
    virtual void onEvent(const ResizeEvent& event) override;
    virtual void onEvent(const OnMouseClick& event) override;

    // Show the spotlight on `target`, with `arrowSide` choosing which side of
    // the target the arrow points from. `title` and `body` populate the
    // corner objective panel. `showSkip` shows the skip-tutorial button.
    void show(const Target& target,
              ArrowSide arrowSide,
              const std::string& title,
              const std::string& body,
              bool showSkip);

    void hide();

    // Trigger a brief shake of the arrow indicator, called by the tutorial
    // system when the player clicks outside the spotlighted target.
    void shakeTarget();

    bool isVisible() const { return visible; }

private:
    // Resolve the current target into a screen-space rect (sx, sy, sw, sh).
    // Returns false if the target is None or invalid (entity destroyed,
    // off-screen world tile clipped, etc.).
    bool resolveTargetRect(float& sx, float& sy, float& sw, float& sh) const;

    // Resolve the rect the arrow should anchor to. Falls back to the main
    // target rect when no override entity is set.
    bool resolveArrowRect(float& sx, float& sy, float& sw, float& sh) const;

    // Reposition all frame rects so that they cover the screen except for
    // the target rect (`sx, sy, sw, sh`).
    void layoutDimFrames(float sx, float sy, float sw, float sh);

    void layoutArrow(float sx, float sy, float sw, float sh, float bobOffset);

    void setEntityVisibility(uint64_t id, bool vis);
    void setEntityXY(uint64_t id, float x, float y);
    void setEntityWH(uint64_t id, float w, float h);

    CameraSystem* cameraSystem = nullptr;
    float screenWidth;
    float screenHeight;

    bool created = false;
    bool visible = false;

    Target currentTarget;
    ArrowSide currentArrowSide = ArrowSide::Top;
    bool currentShowSkip = false;

    // Arrow animation
    float bobAccumMs = 0.0f;
    float shakeRemainingMs = 0.0f;

    // Entity ids
    uint64_t dimFrameIds[4] = {};   // top, bottom, left, right
    uint64_t arrowId = 0;

    uint64_t cornerBgId = 0;
    uint64_t cornerTitleId = 0;
    uint64_t cornerBodyId = 0;

    uint64_t skipBgId = 0;
    uint64_t skipTextId = 0;
};
