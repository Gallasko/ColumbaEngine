#include "spotlightoverlaysystem.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"
#include "Renderer/camera.h"

#include "camerasystem.h"
#include "grid.h"

#include <cmath>

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void SpotlightOverlaySystem::init()
{
    if (created)
        return;
    created = true;

    const constant::Vector4D dimColor{0.0f, 0.0f, 0.0f, DIM_ALPHA};

    for (int i = 0; i < 4; ++i)
    {
        auto frame = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, dimColor);
        auto pos = frame.get<PositionComponent>();
        pos->setZ(DIM_FRAME_Z);
        pos->setVisibility(false);
        frame.get<ViewportComponent>()->setViewport(UI_VP);
        dimFrameIds[i] = frame.entity->id;
    }

    {
        auto arrow = makeSimple2DShape(ecsRef, Shape2D::Triangle, ARROW_SIZE, ARROW_SIZE,
            constant::Vector4D{255.0f, 210.0f, 80.0f, 255.0f});
        auto pos = arrow.get<PositionComponent>();
        pos->setZ(ARROW_Z);
        pos->setVisibility(false);
        arrow.get<ViewportComponent>()->setViewport(UI_VP);
        arrowId = arrow.entity->id;
    }

    // Corner objective panel — pinned to the bottom-left of the screen.
    {
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square,
            CORNER_W, CORNER_H,
            constant::Vector4D{15.0f, 15.0f, 25.0f, 220.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setZ(CORNER_Z);
        pos->setVisibility(false);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        cornerBgId = bg.entity->id;

        auto title = makeTTFText(ecsRef, 0.0f, 0.0f, CORNER_TEXT_Z,
            FONT_PATH, "", TITLE_SCALE,
            constant::Vector4D{255.0f, 210.0f, 80.0f, 255.0f});
        title.get<ViewportComponent>()->setViewport(UI_VP);
        title.get<PositionComponent>()->setVisibility(false);
        cornerTitleId = title.entity->id;

        auto body = makeTTFText(ecsRef, 0.0f, 0.0f, CORNER_TEXT_Z,
            FONT_PATH, "", BODY_SCALE,
            constant::Vector4D{210.0f, 210.0f, 210.0f, 255.0f});
        body.get<ViewportComponent>()->setViewport(UI_VP);
        body.get<PositionComponent>()->setVisibility(false);
        cornerBodyId = body.entity->id;
    }

    {
        auto bg = makeSimple2DShape(ecsRef, Shape2D::Square,
            SKIP_W, SKIP_H,
            constant::Vector4D{60.0f, 60.0f, 70.0f, 230.0f});
        auto pos = bg.get<PositionComponent>();
        pos->setZ(SKIP_Z);
        pos->setVisibility(false);
        bg.get<ViewportComponent>()->setViewport(UI_VP);
        skipBgId = bg.entity->id;

        auto txt = makeTTFText(ecsRef, 0.0f, 0.0f, SKIP_TEXT_Z,
            FONT_PATH, "Skip Tutorial", SKIP_TEXT_SCALE,
            constant::Vector4D{220.0f, 220.0f, 220.0f, 255.0f});
        txt.get<ViewportComponent>()->setViewport(UI_VP);
        txt.get<PositionComponent>()->setVisibility(false);
        skipTextId = txt.entity->id;
    }
}

// ---------------------------------------------------------------------------
// Show / hide
// ---------------------------------------------------------------------------

void SpotlightOverlaySystem::show(const Target& target,
                                  ArrowSide arrowSide,
                                  const std::string& title,
                                  const std::string& body,
                                  bool showSkip)
{
    if (not created)
        init();

    currentTarget = target;
    currentArrowSide = arrowSide;
    currentShowSkip = showSkip;
    visible = true;
    bobAccumMs = 0.0f;
    shakeRemainingMs = 0.0f;

    auto titleEnt = ecsRef->getEntity(cornerTitleId);
    if (titleEnt)
        titleEnt->get<TTFText>()->setText(title);

    auto bodyEnt = ecsRef->getEntity(cornerBodyId);
    if (bodyEnt)
        bodyEnt->get<TTFText>()->setText(body);

    // Position corner panel at bottom-left.
    float cornerX = 16.0f;
    float cornerY = screenHeight - CORNER_H - 16.0f;
    setEntityXY(cornerBgId, cornerX, cornerY);
    setEntityXY(cornerTitleId, cornerX + CORNER_PAD, cornerY + CORNER_PAD);
    setEntityXY(cornerBodyId, cornerX + CORNER_PAD, cornerY + CORNER_PAD + 22.0f);

    // Skip button sits inside the corner panel, top-right corner.
    float skipX = cornerX + CORNER_W - SKIP_W - CORNER_PAD;
    float skipY = cornerY + CORNER_PAD;
    setEntityXY(skipBgId, skipX, skipY);
    setEntityXY(skipTextId, skipX + 8.0f, skipY + 4.0f);

    setEntityVisibility(cornerBgId, true);
    setEntityVisibility(cornerTitleId, true);
    setEntityVisibility(cornerBodyId, true);
    setEntityVisibility(skipBgId, showSkip);
    setEntityVisibility(skipTextId, showSkip);

    setEntityVisibility(arrowId, target.kind != TargetKind::None);
    for (int i = 0; i < 4; ++i)
        setEntityVisibility(dimFrameIds[i], target.kind != TargetKind::None);

    // First-frame layout so the overlay isn't stale before the next tick.
    float sx, sy, sw, sh;
    if (resolveTargetRect(sx, sy, sw, sh))
    {
        layoutDimFrames(sx, sy, sw, sh);
        layoutArrow(sx, sy, sw, sh, 0.0f);
    }
}

void SpotlightOverlaySystem::hide()
{
    visible = false;
    currentTarget.kind = TargetKind::None;

    setEntityVisibility(cornerBgId, false);
    setEntityVisibility(cornerTitleId, false);
    setEntityVisibility(cornerBodyId, false);
    setEntityVisibility(skipBgId, false);
    setEntityVisibility(skipTextId, false);
    setEntityVisibility(arrowId, false);
    for (int i = 0; i < 4; ++i)
        setEntityVisibility(dimFrameIds[i], false);
}

void SpotlightOverlaySystem::shakeTarget()
{
    shakeRemainingMs = 220.0f;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void SpotlightOverlaySystem::onEvent(const TickEvent& event)
{
    if (not visible)
        return;

    bobAccumMs += event.tick;
    if (shakeRemainingMs > 0.0f)
        shakeRemainingMs -= event.tick;

    float sx, sy, sw, sh;
    if (not resolveTargetRect(sx, sy, sw, sh))
        return;

    layoutDimFrames(sx, sy, sw, sh);

    // Bob motion — sin wave; shake adds a faster lateral wiggle when active.
    float bob = std::sin(bobAccumMs * 0.001f * ARROW_BOB_HZ * 2.0f * 3.14159265f) * ARROW_BOB_PIXELS;
    float shake = 0.0f;
    if (shakeRemainingMs > 0.0f)
        shake = std::sin(bobAccumMs * 0.05f) * 4.0f;
    layoutArrow(sx, sy, sw, sh, bob + shake);
}

void SpotlightOverlaySystem::onEvent(const ResizeEvent& event)
{
    screenWidth = event.width;
    screenHeight = event.height;

    if (visible)
    {
        // Reposition corner panel to new screen size.
        float cornerX = 16.0f;
        float cornerY = screenHeight - CORNER_H - 16.0f;
        setEntityXY(cornerBgId, cornerX, cornerY);
        setEntityXY(cornerTitleId, cornerX + CORNER_PAD, cornerY + CORNER_PAD);
        setEntityXY(cornerBodyId, cornerX + CORNER_PAD, cornerY + CORNER_PAD + 22.0f);

        float skipX = cornerX + CORNER_W - SKIP_W - CORNER_PAD;
        float skipY = cornerY + CORNER_PAD;
        setEntityXY(skipBgId, skipX, skipY);
        setEntityXY(skipTextId, skipX + 8.0f, skipY + 4.0f);
    }
}

void SpotlightOverlaySystem::onEvent(const OnMouseClick& event)
{
    if (not visible or not currentShowSkip)
        return;

    auto bgEnt = ecsRef->getEntity(skipBgId);
    if (not bgEnt)
        return;
    auto pos = bgEnt->get<PositionComponent>();
    float bx = pos->getX();
    float by = pos->getY();
    float bw = pos->getWidth();
    float bh = pos->getHeight();

    if (event.pos.x >= bx and event.pos.x <= bx + bw and
        event.pos.y >= by and event.pos.y <= by + bh)
    {
        ecsRef->sendEvent(TutorialSkipRequested{});
    }
}

// ---------------------------------------------------------------------------
// Target resolution
// ---------------------------------------------------------------------------

bool SpotlightOverlaySystem::resolveTargetRect(float& sx, float& sy,
                                                float& sw, float& sh) const
{
    switch (currentTarget.kind)
    {
        case TargetKind::None:
            return false;

        case TargetKind::ScreenRect:
            sx = currentTarget.sx;
            sy = currentTarget.sy;
            sw = currentTarget.sw;
            sh = currentTarget.sh;
            return sw > 0.0f and sh > 0.0f;

        case TargetKind::WorldTile:
        {
            if (not cameraSystem)
                return false;
            auto camEnt = cameraSystem->getCameraEntity();
            if (not camEnt)
                return false;
            auto cam = camEnt->get<BaseCamera2D>();
            if (not cam)
                return false;

            float zoom = (cam->getWidth() > 0.0f)
                ? (screenWidth / cam->getWidth())
                : 1.0f;
            float worldX = static_cast<float>(currentTarget.gridX * Grid::TILE_SIZE);
            float worldY = static_cast<float>(currentTarget.gridY * Grid::TILE_SIZE);
            float worldW = static_cast<float>(currentTarget.gridW * Grid::TILE_SIZE);
            float worldH = static_cast<float>(currentTarget.gridH * Grid::TILE_SIZE);

            sx = (worldX - cam->x) * zoom;
            sy = (worldY - cam->y) * zoom;
            sw = worldW * zoom;
            sh = worldH * zoom;
            return true;
        }

        case TargetKind::UiEntity:
        {
            if (currentTarget.entityId == 0)
                return false;
            auto ent = ecsRef->getEntity(currentTarget.entityId);
            if (not ent)
                return false;
            auto pos = ent->get<PositionComponent>();
            if (not pos)
                return false;
            sx = pos->getX();
            sy = pos->getY();
            sw = pos->getWidth();
            sh = pos->getHeight();
            return sw > 0.0f and sh > 0.0f;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void SpotlightOverlaySystem::layoutDimFrames(float sx, float sy, float sw, float sh)
{
    // Clamp the target rect to the screen so the surrounding frames don't
    // claim negative widths.
    float tx = std::max(0.0f, sx);
    float ty = std::max(0.0f, sy);
    float tr = std::min(screenWidth, sx + sw);
    float tb = std::min(screenHeight, sy + sh);
    if (tr < tx) tr = tx;
    if (tb < ty) tb = ty;

    // Top frame: above the target, full width.
    setEntityXY(dimFrameIds[0], 0.0f, 0.0f);
    setEntityWH(dimFrameIds[0], screenWidth, ty);

    // Bottom frame: below the target, full width.
    setEntityXY(dimFrameIds[1], 0.0f, tb);
    setEntityWH(dimFrameIds[1], screenWidth, std::max(0.0f, screenHeight - tb));

    // Left frame: same row as the target, left of it.
    setEntityXY(dimFrameIds[2], 0.0f, ty);
    setEntityWH(dimFrameIds[2], tx, std::max(0.0f, tb - ty));

    // Right frame: same row as the target, right of it.
    setEntityXY(dimFrameIds[3], tr, ty);
    setEntityWH(dimFrameIds[3], std::max(0.0f, screenWidth - tr), std::max(0.0f, tb - ty));
}

void SpotlightOverlaySystem::layoutArrow(float sx, float sy, float sw, float sh,
                                         float bobOffset)
{
    float ax = 0.0f, ay = 0.0f;
    switch (currentArrowSide)
    {
        case ArrowSide::Top:
            ax = sx + sw * 0.5f - ARROW_SIZE * 0.5f;
            ay = sy - ARROW_SIZE - ARROW_OFFSET - bobOffset;
            break;
        case ArrowSide::Bottom:
            ax = sx + sw * 0.5f - ARROW_SIZE * 0.5f;
            ay = sy + sh + ARROW_OFFSET + bobOffset;
            break;
        case ArrowSide::Left:
            ax = sx - ARROW_SIZE - ARROW_OFFSET - bobOffset;
            ay = sy + sh * 0.5f - ARROW_SIZE * 0.5f;
            break;
        case ArrowSide::Right:
            ax = sx + sw + ARROW_OFFSET + bobOffset;
            ay = sy + sh * 0.5f - ARROW_SIZE * 0.5f;
            break;
    }
    setEntityXY(arrowId, ax, ay);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void SpotlightOverlaySystem::setEntityVisibility(uint64_t id, bool vis)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
        ent->get<PositionComponent>()->setVisibility(vis);
}

void SpotlightOverlaySystem::setEntityXY(uint64_t id, float x, float y)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
    {
        auto pos = ent->get<PositionComponent>();
        pos->setX(x);
        pos->setY(y);
    }
}

void SpotlightOverlaySystem::setEntityWH(uint64_t id, float w, float h)
{
    if (id == 0) return;
    auto ent = ecsRef->getEntity(id);
    if (ent)
    {
        auto pos = ent->get<PositionComponent>();
        pos->setWidth(w);
        pos->setHeight(h);
    }
}
