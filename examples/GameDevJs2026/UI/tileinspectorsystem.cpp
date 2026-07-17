#include "tileinspectorsystem.h"

#include "2D/simple2dobject.h"
#include "2D/position.h"
#include "UI/ttftext.h"
#include "Renderer/camera.h"

#include "2D/camerasystem.h"
#include "gridsystem.h"
#include "hotbarsystem.h"
#include "hudbarsystem.h"
#include "terrain.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace pg;

namespace
{
    const char* terrainName(TerrainType t)
    {
        switch (t)
        {
        case TerrainType::Grass:     return "Grass";
        case TerrainType::OreIron:   return "Iron Ore";
        case TerrainType::OreCopper: return "Copper Ore";
        case TerrainType::OreCoal:   return "Coal";
        case TerrainType::OreStone:  return "Stone Ore";
        case TerrainType::Tree:      return "Tree";
        case TerrainType::Rock:      return "Rock";
        case TerrainType::Water:     return "Water";
        default:                     return "—";
        }
    }

    int dropCount(TerrainType t)
    {
        if (t == TerrainType::Tree or t == TerrainType::Rock)
            return 2;
        if (isMinableTerrain(t))
            return 1;
        return 0;
    }

    const char* tierToToolName(uint8_t tier)
    {
        switch (tier)
        {
        case 0:  return "Bare hands";
        case 1:  return "Stone Pickaxe";
        case 2:  return "Iron Pickaxe";
        default: return "Unknown tool";
        }
    }
}

// ---------------------------------------------------------------------------
// Init / layout
// ---------------------------------------------------------------------------

void TileInspectorSystem::init()
{
    if (created)
        return;
    created = true;

    float panelH = PANEL_PAD * 2 + LINE_GAP * NUM_LINES;

    // Backdrop anchored to the right edge of the main window and pinned to
    // the bottom of the HUD's currency pill so it always reflows when the
    // window resizes or the HUD changes layout.
    auto bd = makeUiSimple2DShape(ecsRef, Shape2D::Square, PANEL_W, panelH,
        constant::Vector4D{15.0f, 15.0f, 25.0f, 220.0f});
    bd.get<PositionComponent>()->setZ(Z_BG);
    bd.get<ViewportComponent>()->setViewport(UI_VP);
    backdropId = bd.entity->id;

    auto bdAnchor = bd.get<UiAnchor>();

    auto windowEnt = ecsRef->getEntity("__MainWindow");
    if (windowEnt)
    {
        auto windowAnchor = windowEnt->get<UiAnchor>();
        if (windowAnchor)
        {
            bdAnchor->setRightAnchor(windowAnchor->right);
            bdAnchor->setRightMargin(MARGIN_RIGHT);
        }
    }

    if (auto* hudBar = ecsRef->getSystem<HudBarSystem>();
        hudBar and hudBar->getTicketDisplayEntityId() != 0)
    {
        bdAnchor->setTopAnchor(PosAnchor{hudBar->getTicketDisplayEntityId(),
                                         AnchorType::Bottom});
        bdAnchor->setTopMargin(MARGIN_TOP);
    }

    for (int i = 0; i < NUM_LINES; ++i)
    {
        auto t = makeTTFText(ecsRef, 0.0f, 0.0f, Z_TEXT,
            FONT_PATH, "", (i == 0) ? TITLE_SCALE : BODY_SCALE,
            constant::Vector4D{220.0f, 220.0f, 220.0f, 255.0f});
        t.get<ViewportComponent>()->setViewport(UI_VP);
        t.get<PositionComponent>()->setVisibility(false);
        lineIds[i] = t.entity->id;

        auto txtAnchor = ecsRef->attach<UiAnchor>(t.entity);
        txtAnchor->setLeftAnchor(PosAnchor{backdropId, AnchorType::Left});
        txtAnchor->setLeftMargin(PANEL_PAD);
        txtAnchor->setTopAnchor(PosAnchor{backdropId, AnchorType::Top});
        txtAnchor->setTopMargin(PANEL_PAD + i * LINE_GAP);
    }
}

void TileInspectorSystem::layoutPanel()
{
    // Anchored layout means we no longer need to recompute positions on
    // resize — the anchor system handles it. Kept as a no-op for the
    // ResizeEvent path so the existing call site stays valid.
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void TileInspectorSystem::onProcessEvent(const OnSDLMouseMotion& event)
{
    cursorX = static_cast<float>(event.x);
    cursorY = static_cast<float>(event.y);
}

void TileInspectorSystem::onEvent(const TickEvent&)
{
    if (not created)
        return;

    auto* cameraSystem = ecsRef->getSystem<CameraSystem>();
    auto* gridSystem   = ecsRef->getSystem<GridSystem>();
    if (not cameraSystem or not gridSystem)
        return;

    int gx = -1;
    int gy = -1;

    auto camEnt = cameraSystem->getCameraEntity();
    if (camEnt)
    {
        auto cam = camEnt->get<BaseCamera2D>();
        if (cam and cam->getWidth() > 0.0f)
        {
            float zoom = screenWidth / cam->getWidth();
            float worldX = cam->x + cursorX / zoom;
            float worldY = cam->y + cursorY / zoom;
            auto cell = gridSystem->getGrid().worldToGrid(worldX, worldY);
            gx = cell.first;
            gy = cell.second;
            if (gx < 0 or gx >= Grid::WIDTH or gy < 0 or gy >= Grid::HEIGHT)
            {
                gx = -1;
                gy = -1;
            }
        }
    }

    // Also rebuild when the equipped tool changes (so the tier line updates),
    // but for simplicity rebuild when the cell changes plus once per second.
    if (gx == lastHoverGX and gy == lastHoverGY and not pendingRebuild)
        return;

    lastHoverGX = gx;
    lastHoverGY = gy;
    pendingRebuild = false;
    rebuildContent(gx, gy);
}

void TileInspectorSystem::onEvent(const ResizeEvent& event)
{
    screenWidth  = event.width;
    screenHeight = event.height;
    layoutPanel();
}

// ---------------------------------------------------------------------------
// Content
// ---------------------------------------------------------------------------

void TileInspectorSystem::rebuildContent(int gx, int gy)
{
    auto bdEnt = ecsRef->getEntity(backdropId);
    if (not bdEnt)
        return;

    if (gx < 0 or gy < 0)
    {
        // Mouse off-grid — keep the panel visible but show only "—".
        bdEnt->get<PositionComponent>()->setVisibility(true);
        setLine(0, "—",
            constant::Vector4D{160.0f, 160.0f, 170.0f, 255.0f}, true);
        for (int i = 1; i < NUM_LINES; ++i)
            setLine(i, "", {}, false);
        return;
    }

    TerrainType t = ecsRef->getSystem<GridSystem>()->getTerrainAt(gx, gy);
    const constant::Vector4D titleColor{255.0f, 235.0f, 180.0f, 255.0f};
    const constant::Vector4D bodyColor {210.0f, 210.0f, 210.0f, 255.0f};
    const constant::Vector4D dimColor  {150.0f, 150.0f, 160.0f, 255.0f};
    const constant::Vector4D goodColor {130.0f, 220.0f, 140.0f, 255.0f};
    const constant::Vector4D badColor  {235.0f, 110.0f, 110.0f, 255.0f};

    bdEnt->get<PositionComponent>()->setVisibility(true);

    setLine(0, terrainName(t), titleColor, true);

    const ItemId dropId = terrainToItem(t);
    if (dropId != ITEM_NONE)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Drops: %d× %s",
            dropCount(t), itemRegistry->get(dropId).name.c_str());
        setLine(1, buf, bodyColor, true);
    }
    else
    {
        setLine(1, "Drops: —", dimColor, true);
    }

    if (isMinableTerrain(t))
    {
        const uint8_t reqTier = terrainTier(t);
        uint8_t equippedTier = 0;
        if (auto* hotbar = ecsRef->getSystem<HotbarSystem>())
        {
            const auto& sel = hotbar->getSelectedItem();
            if (not sel.isEmpty())
                equippedTier = itemRegistry->get(sel.id).toolTier;
        }
        if (equippedTier >= reqTier)
        {
            setLine(2, "Mineable", goodColor, true);
        }
        else
        {
            std::string msg = "Requires ";
            msg += tierToToolName(reqTier);
            setLine(2, msg, badColor, true);
        }
    }
    else
    {
        setLine(2, "", {}, false);
    }

    if (dropId != ITEM_NONE and recipeRegistry)
    {
        std::set<std::string> machines;
        for (const auto& recipe : recipeRegistry->all())
        {
            if (recipe.category != RecipeCategory::Furnace and
                recipe.category != RecipeCategory::Assembler)
                continue;
            for (const auto& in : recipe.inputs)
            {
                if (in.id == dropId)
                {
                    machines.insert(recipe.machineName);
                    break;
                }
            }
        }
        if (not machines.empty())
        {
            std::string line = "Process: ";
            bool first = true;
            for (const auto& m : machines)
            {
                if (not first)
                    line += ", ";
                line += m;
                first = false;
            }
            setLine(3, line, bodyColor, true);
        }
        else
        {
            setLine(3, "", {}, false);
        }
    }
    else
    {
        setLine(3, "", {}, false);
    }
}

void TileInspectorSystem::setLine(int idx, const std::string& text,
                                  const constant::Vector4D& color, bool show)
{
    if (idx < 0 or idx >= NUM_LINES)
        return;
    auto ent = ecsRef->getEntity(lineIds[idx]);
    if (not ent)
        return;
    auto txt = ent->get<TTFText>();
    auto pos = ent->get<PositionComponent>();
    if (show)
    {
        txt->setText(text);
        txt->setColors(color);
        pos->setVisibility(true);
    }
    else
    {
        pos->setVisibility(false);
    }
}
