#include "stdafx.h"

#include "editorsystem.h"

#include "camera3dcontrollereditor.h"
#include "voxelcomponents.h"

#include "Renderer/renderer.h"
#include "window.h"
#include "logger.h"

#ifdef __linux__
#  include <SDL2/SDL.h>
#elif _WIN32
#  include <SDL.h>
#endif

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <cmath>
#include <algorithm>

namespace pg
{
    namespace
    {
        constexpr const char* DOM = "EditorSystem";
    }

    EditorSystem::EditorSystem(MasterRenderer*     mr,
                               Window*             window,
                               Camera3DControllerEditor* cam,
                               Canvas*             canvas)
        : mr(mr), window(window), cam(cam), canvas(canvas)
    {}

    void EditorSystem::init()
    {
        // Create a persistent ghost entity (no VoxelComponent yet — added on demand).
        ghostEntity = ecsRef->createEntity();

        // Start in edit mode — sync camera and cursor state.
        if (cam)
            cam->editMode = true;
        if (window)
            window->setCursorLocked(false);
    }

    // =========================================================================
    // Keyboard
    // =========================================================================

    void EditorSystem::onEvent(const OnSDLScanCode& event)
    {
        // Tab — toggle edit / fly mode
        if (event.key == SDL_SCANCODE_TAB)
        {
            editMode = !editMode;

            if (cam)
                cam->editMode = editMode;

            if (window)
                window->setCursorLocked(!editMode);

            LOG_INFO(DOM, "Edit mode: " << (editMode ? "ON" : "OFF"));
            return;
        }

        const bool ctrl = (event.mod & KMOD_CTRL) != 0;

        // Ctrl+Z — undo
        if (ctrl && event.key == SDL_SCANCODE_Z && !undoStack.empty())
        {
            auto cmd = undoStack.back();
            undoStack.pop_back();

            std::visit([this](auto&& c)
            {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, PlaceCmd>)
                    doRemove(c.pos);
                else
                    doPlace(c.pos, c.color, c.layer);
            }, cmd);

            redoStack.push_back(cmd);
            return;
        }

        // Ctrl+Y — redo
        if (ctrl && event.key == SDL_SCANCODE_Y && !redoStack.empty())
        {
            auto cmd = redoStack.back();
            redoStack.pop_back();

            std::visit([this](auto&& c)
            {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, PlaceCmd>)
                    doPlace(c.pos, c.color, c.layer);
                else
                    doRemove(c.pos);
            }, cmd);

            undoStack.push_back(cmd);
        }
    }

    // =========================================================================
    // Ghost preview helpers
    // =========================================================================

    void EditorSystem::updateGhost(const glm::ivec3& cell)
    {
        if (ghostEntity.empty())
            return;

        const glm::vec4 col = EDITOR_PALETTE[static_cast<size_t>(activeColor)];
        // Semi-transparent version of the active color (alpha ~40%).
        const glm::vec4 ghostColor{ col.r, col.g, col.b, 100.0f };

        if (ghostCell == cell)
        {
            // Same cell — just refresh the color in case activeColor changed.
            if (ghostEntity.has<VoxelComponent>())
                ghostEntity->get<VoxelComponent>()->color = ghostColor;
            return;
        }

        ghostCell = cell;

        // Detach any existing component and reattach at the new position.
        if (ghostEntity.has<VoxelComponent>())
            ecsRef->detach<VoxelComponent>(static_cast<Entity*>(ghostEntity));

        if (canvas->inBounds(cell.x, cell.y, cell.z))
        {
            ecsRef->attach<VoxelComponent>(
                ghostEntity,
                glm::vec3(cell),
                glm::vec3(1.0f),
                ghostColor);
        }
    }

    void EditorSystem::hideGhost()
    {
        if (ghostEntity.empty())
            return;
        if (ghostEntity.has<VoxelComponent>())
            ecsRef->detach<VoxelComponent>(static_cast<Entity*>(ghostEntity));
        ghostCell = { -1, -1, -1 };
    }

    // =========================================================================
    // Mouse motion — update ghost preview
    // =========================================================================

    void EditorSystem::onEvent(const OnSDLMouseMotion& event)
    {
        mouseX = event.x;
        mouseY = event.y;

        if (!editMode)
        {
            hideGhost();
            return;
        }

        // Refresh screen size from the renderer parameter table.
        if (mr)
        {
            const auto& rTable = mr->getParameter();
            screenW = rTable.at("ScreenWidth").get<int>();
            screenH = rTable.at("ScreenHeight").get<int>();
        }

        glm::ivec3 hitCell{ -1,-1,-1 }, placeCell{ -1,-1,-1 };
        const bool hit = raycast(mouseX, mouseY, hitCell, placeCell);

        if (hit && canvas->inBounds(placeCell.x, placeCell.y, placeCell.z)
                && canvas->at(placeCell.x, placeCell.y, placeCell.z).empty())
        {
            updateGhost(placeCell);
        }
        else
        {
            hideGhost();
        }
    }

    // =========================================================================
    // Mouse click
    // =========================================================================

    void EditorSystem::onEvent(const OnMouseClick& event)
    {
        if (!editMode)
            return;

        // Refresh screen dimensions
        if (mr)
        {
            const auto& rTable = mr->getParameter();
            screenW = rTable.at("ScreenWidth").get<int>();
            screenH = rTable.at("ScreenHeight").get<int>();
        }

        const int  px       = static_cast<int>(event.pos.x);
        const int  py       = static_cast<int>(event.pos.y);
        const bool rightBtn = (event.button == SDL_BUTTON_RIGHT);

        if (handleUIClick(px, py, rightBtn))
            return;

        // 3-D interaction
        glm::ivec3 hitCell{ -1,-1,-1 }, placeCell{ -1,-1,-1 };
        const bool hit = raycast(px, py, hitCell, placeCell);

        bool acted = false;
        if (rightBtn)
        {
            if (hit && canvas->inBounds(hitCell.x, hitCell.y, hitCell.z))
            {
                removeBlock(hitCell);
                acted = true;
            }
        }
        else
        {
            if (canvas->inBounds(placeCell.x, placeCell.y, placeCell.z)
                    && canvas->at(placeCell.x, placeCell.y, placeCell.z).empty())
            {
                placeBlock(placeCell);
                acted = true;
            }
        }

        // Re-evaluate ghost after the canvas changed.
        if (acted)
        {
            ghostCell = { -1, -1, -1 }; // force re-evaluation even if same cell
            glm::ivec3 newHit{ -1,-1,-1 }, newPlace{ -1,-1,-1 };
            const bool newRayHit = raycast(px, py, newHit, newPlace);
            if (newRayHit && canvas->inBounds(newPlace.x, newPlace.y, newPlace.z)
                          && canvas->at(newPlace.x, newPlace.y, newPlace.z).empty())
                updateGhost(newPlace);
            else
                hideGhost();
        }
    }

    // =========================================================================
    // UI hit-test
    // =========================================================================

    bool EditorSystem::handleUIClick(int px, int py, bool /*rightBtn*/)
    {
        // --- Palette bar (bottom-centre) ---
        {
            const int totalW = PALETTE_SIZE * (SWATCH_SIZE + SWATCH_PADDING) - SWATCH_PADDING;
            const int x0     = (screenW - totalW) / 2;
            const int y0     = screenH - SWATCH_SIZE - PALETTE_MARGIN;

            if (py >= y0 && py <= y0 + SWATCH_SIZE
             && px >= x0 && px <= x0 + totalW)
            {
                const int idx = (px - x0) / (SWATCH_SIZE + SWATCH_PADDING);
                if (idx >= 0 && idx < PALETTE_SIZE)
                    activeColor = idx;
                return true;
            }
        }

        // --- Layers panel (top-left) ---
        {
            const int x0 = LAYER_PANEL_X;
            const int x1 = LAYER_PANEL_X + LAYER_PANEL_W;

            if (px >= x0 && px <= x1)
            {
                const int nbLayers = static_cast<int>(canvas->layers.size());

                for (int i = 0; i < nbLayers; ++i)
                {
                    const int ry0 = LAYER_PANEL_Y + i * LAYER_ROW_H;
                    const int ry1 = ry0 + LAYER_ROW_H;

                    if (py >= ry0 && py < ry1)
                    {
                        if (px < x0 + LAYER_EYE_W)
                            toggleLayerVisibility(i);
                        else
                            activeLayer = i;
                        return true;
                    }
                }

                // "+" Add layer button
                const int addY0 = LAYER_PANEL_Y + nbLayers * LAYER_ROW_H;
                const int addY1 = addY0 + LAYER_ADD_BTN_H;

                if (py >= addY0 && py < addY1)
                {
                    canvas->layers.push_back(Layer{
                        "Layer " + std::to_string(canvas->layers.size() + 1)
                    });
                    activeLayer = static_cast<int>(canvas->layers.size()) - 1;
                    LOG_INFO(DOM, "Layer added: " << canvas->layers.back().name);
                    return true;
                }
            }
        }

        // --- Gizmo cube (top-right corner) ---
        {
            //  Layout (grid positions):
            //           [Top]          col 1, row 0
            //  [Left]  [Front] [Right] col 0,1,2  row 1
            //          [Bottom]        col 1, row 2
            //           [Back]         col 1, row 3
            const int step  = GIZMO_BTN + GIZMO_GAP;
            const int gx0   = screenW - GIZMO_MARGIN - 3 * step + GIZMO_GAP;
            const int gy0   = GIZMO_MARGIN;

            struct GizmoFace { int col; int row; glm::vec3 pos; float yaw; float pitch; };
            const GizmoFace faces[] = {
                { 1, 0, {  8.0f, 30.0f,   8.0f }, -90.0f, -89.0f }, // Top
                { 0, 1, {-14.0f, 10.0f,   8.0f },   0.0f, -15.0f }, // Left
                { 1, 1, {  8.0f, 10.0f,  30.0f }, -90.0f, -15.0f }, // Front
                { 2, 1, { 30.0f, 10.0f,   8.0f }, 180.0f, -15.0f }, // Right
                { 1, 2, {  8.0f,-20.0f,   8.0f }, -90.0f,  89.0f }, // Bottom
                { 1, 3, {  8.0f, 10.0f, -14.0f },  90.0f, -15.0f }, // Back
            };

            for (const auto& f : faces)
            {
                const int fx = gx0 + f.col * step;
                const int fy = gy0 + f.row * step;

                if (px >= fx && px < fx + GIZMO_BTN && py >= fy && py < fy + GIZMO_BTN)
                {
                    if (cam)
                        cam->snapTo(f.pos, f.yaw, f.pitch);

                    // Refresh the ghost preview with the new camera orientation.
                    glm::ivec3 newHit, newPlace;
                    if (raycast(mouseX, mouseY, newHit, newPlace)
                        && canvas->inBounds(newPlace.x, newPlace.y, newPlace.z)
                        && canvas->at(newPlace.x, newPlace.y, newPlace.z).empty())
                        updateGhost(newPlace);
                    else
                        hideGhost();

                    return true;
                }
            }
        }

        return false;
    }

    // =========================================================================
    // Layer visibility toggle
    // =========================================================================

    void EditorSystem::toggleLayerVisibility(int layerIdx)
    {
        if (!canvas
         || layerIdx < 0
         || layerIdx >= static_cast<int>(canvas->layers.size()))
            return;

        Layer& layer = canvas->layers[static_cast<size_t>(layerIdx)];
        layer.visible = !layer.visible;

        const int total = canvas->W * canvas->H * canvas->D;

        if (!layer.visible)
        {
            layer.hiddenData.clear();

            for (int idx = 0; idx < total; ++idx)
            {
                if (canvas->cellLayer[static_cast<size_t>(idx)] != layerIdx)
                    continue;

                EntityRef& ent = canvas->cells[static_cast<size_t>(idx)];
                if (ent.empty())
                    continue;

                const int x = idx % canvas->W;
                const int y = (idx / canvas->W) % canvas->H;
                const int z = idx / (canvas->W * canvas->H);

                auto comp = ent->get<VoxelComponent>();
                layer.hiddenData.push_back({ {x, y, z}, comp->color });

                ecsRef->detach<VoxelComponent>(static_cast<Entity*>(ent));
            }
        }
        else
        {
            for (const auto& hc : layer.hiddenData)
            {
                EntityRef& ent = canvas->at(hc.cell.x, hc.cell.y, hc.cell.z);
                if (ent.empty())
                    continue;

                ecsRef->attach<VoxelComponent>(
                    ent,
                    glm::vec3(hc.cell),
                    glm::vec3(1.0f),
                    hc.color);
            }
            layer.hiddenData.clear();
        }
    }

    // =========================================================================
    // DDA Voxel Raycast
    // =========================================================================

    glm::vec3 EditorSystem::screenToRay(int px, int py) const
    {
        const auto& cam = mr->getCamera();

        // Convert pixel to NDC: (0,0)=top-left → (-1,+1), (W,H)=bottom-right → (+1,-1)
        const float ndcX = (2.0f * px / static_cast<float>(screenW)) - 1.0f;
        const float ndcY = 1.0f - (2.0f * py / static_cast<float>(screenH));

        // Scale by half-FOV to get view-plane offsets (tan of the half-angles).
        const float tanHalfFov = std::tan(glm::radians(cam.getFovDegrees() * 0.5f));
        const float aspect     = static_cast<float>(screenW) / static_cast<float>(screenH);

        // Reconstruct world-space ray directly from the camera basis vectors.
        // This is equivalent to inverse(proj * view) * clipRay but avoids
        // matrix inversion and uses the exact same front/right/up the
        // voxel3d shader sees through the `view` uniform.
        const glm::vec3 dir = glm::normalize(
            cam.front
            + cam.right * (ndcX * aspect * tanHalfFov)
            + cam.up    * (ndcY * tanHalfFov));

        return dir;
    }

    bool EditorSystem::raycast(int mouseX, int mouseY, glm::ivec3& hitCell, glm::ivec3& placeCell)
    {
        if (!mr || !canvas)
            return false;

        const glm::vec3 origin = mr->getCamera().position;
        const glm::vec3 dir    = screenToRay(mouseX, mouseY);

        int cx = static_cast<int>(std::floor(origin.x));
        int cy = static_cast<int>(std::floor(origin.y));
        int cz = static_cast<int>(std::floor(origin.z));

        const int stepX = (dir.x >= 0.0f) ? 1 : -1;
        const int stepY = (dir.y >= 0.0f) ? 1 : -1;
        const int stepZ = (dir.z >= 0.0f) ? 1 : -1;

        auto tMaxFor = [](float o, float d, int step) -> float
        {
            if (std::abs(d) < 1e-9f)
                return 1e30f;
            const float boundary = (step > 0)
                ? std::floor(o) + 1.0f
                : std::floor(o);
            return (boundary - o) / d;
        };

        float tMaxX = tMaxFor(origin.x, dir.x, stepX);
        float tMaxY = tMaxFor(origin.y, dir.y, stepY);
        float tMaxZ = tMaxFor(origin.z, dir.z, stepZ);

        const float tDX = (std::abs(dir.x) > 1e-9f) ? std::abs(1.0f / dir.x) : 1e30f;
        const float tDY = (std::abs(dir.y) > 1e-9f) ? std::abs(1.0f / dir.y) : 1e30f;
        const float tDZ = (std::abs(dir.z) > 1e-9f) ? std::abs(1.0f / dir.z) : 1e30f;

        static constexpr float MAX_DIST = 40.0f;

        placeCell = { -1, -1, -1 };

        const int maxSteps = (canvas->W + canvas->H + canvas->D) * 3 + 60;

        for (int i = 0; i < maxSteps; ++i)
        {
            // The floor lives at y=-1 outside the canvas grid.
            // Treat it as a solid surface so placeCell (last empty in-bounds
            // cell above it) is valid.
            if (cy == -1 && cx >= 0 && cx < canvas->W && cz >= 0 && cz < canvas->D)
            {
                hitCell = { cx, cy, cz };
                return true;
            }

            if (canvas->inBounds(cx, cy, cz))
            {
                if (!canvas->at(cx, cy, cz).empty())
                {
                    hitCell = { cx, cy, cz };
                    return true;
                }
                placeCell = { cx, cy, cz };
            }

            const float tMin = std::min({ tMaxX, tMaxY, tMaxZ });
            if (tMin > MAX_DIST)
                break;

            if (tMaxX <= tMaxY && tMaxX <= tMaxZ)
            {
                cx    += stepX;
                tMaxX += tDX;
            }
            else if (tMaxY <= tMaxZ)
            {
                cy    += stepY;
                tMaxY += tDY;
            }
            else
            {
                cz    += stepZ;
                tMaxZ += tDZ;
            }
        }

        return false;
    }

    // =========================================================================
    // Block placement / removal (public, update undo stack)
    // =========================================================================

    void EditorSystem::placeBlock(const glm::ivec3& pos)
    {
        if (canvas->layers.empty())
        {
            canvas->layers.push_back(Layer{ "Layer 1" });
            activeLayer = 0;
        }

        const glm::vec4 color = EDITOR_PALETTE[static_cast<size_t>(activeColor)];
        const int       layer = std::min(activeLayer,
                                         static_cast<int>(canvas->layers.size()) - 1);

        doPlace(pos, color, layer);

        undoStack.push_back(PlaceCmd{ pos, color, layer });
        redoStack.clear();
    }

    void EditorSystem::removeBlock(const glm::ivec3& pos)
    {
        if (canvas->at(pos.x, pos.y, pos.z).empty())
            return;

        const glm::vec4 color = canvas->at(pos.x, pos.y, pos.z)->get<VoxelComponent>()->color;
        const int       layer = canvas->layerAt(pos.x, pos.y, pos.z);

        doRemove(pos);

        undoStack.push_back(RemoveCmd{ pos, color, layer });
        redoStack.clear();
    }

    // =========================================================================
    // Low-level helpers (no undo stack interaction)
    // =========================================================================

    void EditorSystem::doPlace(const glm::ivec3& pos,
                               const glm::vec4&  color,
                               int               layerIdx)
    {
        // Ensure the canvas slot is empty
        auto& slot = canvas->at(pos.x, pos.y, pos.z);
        if (!slot.empty())
            return;

        // Skip if the target layer is hidden
        if (layerIdx >= 0
         && layerIdx < static_cast<int>(canvas->layers.size())
         && !canvas->layers[static_cast<size_t>(layerIdx)].visible)
        {
            // Still record the entity but don't attach VoxelComponent
            auto ent = ecsRef->createEntity();
            slot = ent;
            canvas->setLayer(pos.x, pos.y, pos.z, layerIdx);
            // Add to layer's hidden data so it restores correctly on show
            canvas->layers[static_cast<size_t>(layerIdx)]
                  .hiddenData.push_back({ pos, color });
            return;
        }

        auto ent = createVoxel(pos, color);

        slot = ent;
        canvas->setLayer(pos.x, pos.y, pos.z, layerIdx);
    }

    void EditorSystem::doRemove(const glm::ivec3& pos)
    {
        auto& slot = canvas->at(pos.x, pos.y, pos.z);
        if (slot.empty())
            return;

        ecsRef->removeEntity(slot.entity);
        slot = EntityRef{};
        canvas->setLayer(pos.x, pos.y, pos.z, -1);
    }

    EntityRef EditorSystem::createVoxel(const glm::ivec3& pos, const glm::vec4& color)
    {
        auto ent = ecsRef->createEntity();
        ecsRef->attach<VoxelComponent>(
            ent,
            glm::vec3(pos),
            glm::vec3(1.0f),
            color);
        return ent;
    }

    void EditorSystem::buildFloor()
    {
        if (not canvas)
            return;

        const glm::vec4 colorA{ 180.0f, 180.0f, 180.0f, 255.0f };
        const glm::vec4 colorB{ 140.0f, 140.0f, 140.0f, 255.0f };

        for (int x = 0; x < canvas->W; ++x)
            for (int z = 0; z < canvas->D; ++z)
                createVoxel({ x, -1, z }, ((x + z) % 2 == 0) ? colorA : colorB);
    }

} // namespace pg
