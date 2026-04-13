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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/threading.h>
#include <cstdio>
#include <fstream>
#else
#include "Helpers/tinyfiledialogs.h"
#endif

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

        // Ctrl+S — save project
        if (ctrl && event.key == SDL_SCANCODE_S)
        {
            saveProject();
            return;
        }

        // Ctrl+O — load project
        if (ctrl && event.key == SDL_SCANCODE_O)
        {
            loadProject();
            return;
        }

        // Ctrl+E — export OBJ
        if (ctrl && event.key == SDL_SCANCODE_E)
        {
            exportToOBJ();
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

        const glm::vec4 col = canvas->palette[static_cast<size_t>(activeColor)];
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

        // Handle RGB bar dragging
        if (draggingRGBBar != 0)
        {
            // Stop dragging if left button is released
            if (!(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT)))
            {
                draggingRGBBar = 0;
            }
            else if (paletteModalOpen && colorCreatorOpen)
            {
                // Recompute bar X from modal layout
                const int paletteSize = static_cast<int>(canvas->palette.size());
                const int totalItems = paletteSize + 1;
                const int rows = (totalItems + MODAL_COLS - 1) / MODAL_COLS;
                const int gridW = MODAL_COLS * (MODAL_SWATCH + MODAL_SWATCH_PAD) - MODAL_SWATCH_PAD;
                const int modalW = gridW + MODAL_PAD * 2;
                const int mx = (screenW - modalW) / 2;
                const int barX = mx + MODAL_PAD + 40;

                const int value = std::clamp((mouseX - barX) * 255 / RGB_BAR_W, 0, 255);

                if (draggingRGBBar == 1)      creatorR = value;
                else if (draggingRGBBar == 2) creatorG = value;
                else if (draggingRGBBar == 3) creatorB = value;

                return; // skip ghost update while dragging
            }
        }

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

        // Color picker tool: sample color from clicked voxel
        if (activeTool == EditorTool::ColorPick && !rightBtn)
        {
            if (hit && canvas->inBounds(hitCell.x, hitCell.y, hitCell.z)
                    && !canvas->at(hitCell.x, hitCell.y, hitCell.z).empty())
            {
                auto vc = canvas->at(hitCell.x, hitCell.y, hitCell.z)->get<VoxelComponent>();
                if (vc)
                {
                    // Find matching palette index
                    const glm::vec4& col = vc->color;
                    int foundIdx = -1;
                    for (int i = 0; i < static_cast<int>(canvas->palette.size()); ++i)
                    {
                        const glm::vec4& pc = canvas->palette[static_cast<size_t>(i)];
                        if (std::abs(pc.r - col.r) < 0.5f && std::abs(pc.g - col.g) < 0.5f
                         && std::abs(pc.b - col.b) < 0.5f && std::abs(pc.a - col.a) < 0.5f)
                        {
                            foundIdx = i;
                            break;
                        }
                    }

                    if (foundIdx >= 0)
                    {
                        activeColor = foundIdx;
                    }
                    else
                    {
                        // Color not in palette — add it
                        canvas->palette.push_back(col);
                        activeColor = static_cast<int>(canvas->palette.size()) - 1;
                    }
                }
            }
            return;
        }

        // Place tool: left = place, right = remove
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
        // --- Palette modal (consumes all clicks when open) ---
        if (paletteModalOpen)
        {
            const int paletteSize = static_cast<int>(canvas->palette.size());
            const int sw  = MODAL_SWATCH;
            const int sp  = MODAL_SWATCH_PAD;
            const int pad = MODAL_PAD;

            const int totalItems = paletteSize + 1;
            const int rows = (totalItems + MODAL_COLS - 1) / MODAL_COLS;
            const int gridW = MODAL_COLS * (sw + sp) - sp;
            const int gridH = rows * (sw + sp) - sp;

            const int modalW = gridW + pad * 2;
            const int creatorH = RGB_BAR_H * 3 + RGB_BAR_GAP * 4 + 30 + 10;
            const int titleH = 28;
            const int modalH = titleH + gridH + pad * 2 + creatorH + pad;

            const int mx = (screenW - modalW) / 2;
            const int my = (screenH - modalH) / 2;

            const int gridX = mx + pad;
            const int gridY = my + titleH + pad;

            // Check color creator first (if open)
            if (colorCreatorOpen)
            {
                const int creatorY = gridY + rows * (sw + sp) + pad;
                const int barX = mx + pad + 40;
                const int barW = RGB_BAR_W;

                // R bar
                if (px >= barX && px < barX + barW && py >= creatorY && py < creatorY + RGB_BAR_H)
                {
                    creatorR = std::clamp((px - barX) * 255 / barW, 0, 255);
                    draggingRGBBar = 1;
                    return true;
                }
                // G bar
                const int gBarY = creatorY + RGB_BAR_H + RGB_BAR_GAP;
                if (px >= barX && px < barX + barW && py >= gBarY && py < gBarY + RGB_BAR_H)
                {
                    creatorG = std::clamp((px - barX) * 255 / barW, 0, 255);
                    draggingRGBBar = 2;
                    return true;
                }
                // B bar
                const int bBarY = gBarY + RGB_BAR_H + RGB_BAR_GAP;
                if (px >= barX && px < barX + barW && py >= bBarY && py < bBarY + RGB_BAR_H)
                {
                    creatorB = std::clamp((px - barX) * 255 / barW, 0, 255);
                    draggingRGBBar = 3;
                    return true;
                }
                // Add button
                const int previewY = bBarY + RGB_BAR_H + RGB_BAR_GAP;
                const int addBtnX = barX + sw + RGB_BAR_GAP;
                if (px >= addBtnX && px < addBtnX + 50 && py >= previewY && py < previewY + sw)
                {
                    addColorToPalette({static_cast<float>(creatorR),
                                       static_cast<float>(creatorG),
                                       static_cast<float>(creatorB), 255.0f});
                    return true;
                }
                // Cancel button
                const int cancelBtnX = addBtnX + 50 + RGB_BAR_GAP;
                if (px >= cancelBtnX && px < cancelBtnX + 60 && py >= previewY && py < previewY + sw)
                {
                    cancelColorCreator();
                    return true;
                }
            }

            // Check swatch clicks
            for (int i = 0; i < paletteSize; ++i)
            {
                const int col = i % MODAL_COLS;
                const int row = i / MODAL_COLS;
                const int sx = gridX + col * (sw + sp);
                const int sy = gridY + row * (sw + sp);

                if (px >= sx && px < sx + sw && py >= sy && py < sy + sw)
                {
                    activeColor = i;
                    paletteModalOpen = false;
                    colorCreatorOpen = false;
                    return true;
                }
            }

            // Check "+" button
            {
                const int col = paletteSize % MODAL_COLS;
                const int row = paletteSize / MODAL_COLS;
                const int sx = gridX + col * (sw + sp);
                const int sy = gridY + row * (sw + sp);

                if (px >= sx && px < sx + sw && py >= sy && py < sy + sw)
                {
                    startColorCreator();
                    return true;
                }
            }

            // Click outside modal panel → close
            if (px < mx || px > mx + modalW || py < my || py > my + modalH)
            {
                paletteModalOpen = false;
                colorCreatorOpen = false;
            }

            return true; // consume all clicks while modal is open
        }

        // --- Toolbar (left side) ---
        {
            const int tx = TOOLBAR_X;
            const int ty = TOOLBAR_Y;
            const int btn = TOOLBAR_BTN;
            const int gap = TOOLBAR_GAP;

            // Place button
            if (px >= tx && px < tx + btn && py >= ty && py < ty + btn)
            {
                activeTool = EditorTool::Place;
                return true;
            }

            // Pick button
            const int pickY = ty + btn + gap;
            if (px >= tx && px < tx + btn && py >= pickY && py < pickY + btn)
            {
                activeTool = EditorTool::ColorPick;
                return true;
            }

            // Color swatch button (opens palette modal)
            const int colorY = ty + (btn + gap) * 2;
            if (px >= tx && px < tx + btn && py >= colorY && py < colorY + btn)
            {
                togglePaletteModal();
                return true;
            }
        }

        // --- Layers panel (right side, below gizmo) ---
        {
            const int gizmoBottomY = GIZMO_MARGIN + 4 * (GIZMO_BTN + GIZMO_GAP) + 8;
            const int x0 = screenW - LAYER_PANEL_MARGIN_RIGHT - LAYER_PANEL_W;
            const int x1 = x0 + LAYER_PANEL_W;
            const int panelY = gizmoBottomY;

            if (px >= x0 && px <= x1)
            {
                const int nbLayers = static_cast<int>(canvas->layers.size());

                for (int i = 0; i < nbLayers; ++i)
                {
                    const int ry0 = panelY + i * LAYER_ROW_H;
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
                const int addY0 = panelY + nbLayers * LAYER_ROW_H;
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

        // --- Action bar (top-centre) ---
        {
            const int btnW    = ACTION_BAR_BTN_W;
            const int btnH    = ACTION_BAR_BTN_H;
            const int gap     = ACTION_BAR_GAP;
            const int pad     = ACTION_BAR_PAD;
            const int margin  = ACTION_BAR_MARGIN;

            const int contentW = btnW * 2 + gap;
            const int totalW   = contentW + pad * 2;
            const int totalH   = btnH + pad * 2;

            const int x0 = (screenW - totalW) / 2;
            const int y0 = margin;

            if (px >= x0 && px <= x0 + totalW && py >= y0 && py <= y0 + totalH)
            {
                const int btnX0_open = x0 + pad;
                const int btnX1_open = btnX0_open + btnW;
                const int btnX0_save = btnX1_open + gap;
                const int btnX1_save = btnX0_save + btnW;
                const int btnY0      = y0 + pad;
                const int btnY1      = btnY0 + btnH;

                if (py >= btnY0 && py <= btnY1)
                {
                    if (px >= btnX0_open && px <= btnX1_open)
                        loadProject();
                    else if (px >= btnX0_save && px <= btnX1_save)
                        saveProject();
                }
                return true;
            }
        }

        // --- Gizmo cube (top-right corner) ---
        {
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

        const glm::vec4 color = canvas->palette[static_cast<size_t>(activeColor)];
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

    // =========================================================================
    // Palette modal helpers
    // =========================================================================

    void EditorSystem::togglePaletteModal()
    {
        paletteModalOpen = !paletteModalOpen;
        if (!paletteModalOpen)
            colorCreatorOpen = false;
    }

    void EditorSystem::addColorToPalette(const glm::vec4& color)
    {
        canvas->palette.push_back(color);
        activeColor = static_cast<int>(canvas->palette.size()) - 1;
        colorCreatorOpen = false;
        // Reset creator values
        creatorR = 128;
        creatorG = 128;
        creatorB = 128;
    }

    void EditorSystem::startColorCreator()
    {
        colorCreatorOpen = true;
        creatorR = 128;
        creatorG = 128;
        creatorB = 128;
    }

    void EditorSystem::cancelColorCreator()
    {
        colorCreatorOpen = false;
        creatorR = 128;
        creatorG = 128;
        creatorB = 128;
    }

    // =========================================================================
    // Save / Load / Export
    // =========================================================================

    void EditorSystem::clearCanvas()
    {
        if (!canvas) return;

        const int total = canvas->W * canvas->H * canvas->D;
        for (int idx = 0; idx < total; ++idx)
        {
            auto& ent = canvas->cells[static_cast<size_t>(idx)];
            if (!ent.empty())
            {
                ecsRef->removeEntity(ent.entity);
                ent = EntityRef{};
            }
            canvas->cellLayer[static_cast<size_t>(idx)] = -1;
        }

        // Clear hidden data from all layers
        for (auto& layer : canvas->layers)
            layer.hiddenData.clear();

        undoStack.clear();
        redoStack.clear();
    }

    void EditorSystem::applyProjectData(const ProjectData& data)
    {
        clearCanvas();

        // Resize canvas if dimensions changed
        if (data.W != canvas->W || data.H != canvas->H || data.D != canvas->D)
        {
            canvas->W = data.W;
            canvas->H = data.H;
            canvas->D = data.D;
            const size_t sz = static_cast<size_t>(data.W * data.H * data.D);
            canvas->cells.assign(sz, EntityRef{});
            canvas->cellLayer.assign(sz, -1);
        }

        // Restore palette
        canvas->palette = data.palette;

        // Restore layers
        canvas->layers.clear();
        for (const auto& ld : data.layers)
            canvas->layers.push_back(Layer{ ld.name, ld.visible, {} });

        // Place cells
        for (const auto& cell : data.cells)
        {
            if (!canvas->inBounds(cell.pos.x, cell.pos.y, cell.pos.z))
                continue;

            const int ci = std::clamp(cell.colorIndex, 0, static_cast<int>(canvas->palette.size()) - 1);
            const glm::vec4 color = canvas->palette[static_cast<size_t>(ci)];
            const int li = std::clamp(cell.layerIndex, 0, static_cast<int>(canvas->layers.size()) - 1);

            doPlace(cell.pos, color, li);
        }

        activeLayer = 0;
        activeColor = 0;
        hideGhost();
    }

#ifdef __EMSCRIPTEN__
} // close namespace pg for extern "C" callback

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void editorLoadProjectCallback(uintptr_t selfPtr, const char* data, int len)
    {
        auto* editor = reinterpret_cast<pg::EditorSystem*>(selfPtr);
        std::string json(data, static_cast<size_t>(len));
        pg::ProjectData pd;
        if (pg::loadProjectFromString(json, pd))
            editor->applyProjectData(pd);
    }
}

namespace pg
{
    void EditorSystem::loadProject(const std::string& /*defaultPath*/)
    {
        if (!canvas) return;

        uintptr_t self = reinterpret_cast<uintptr_t>(this);
        MAIN_THREAD_EM_ASM({
            var self = $0;
            var input = document.createElement('input');
            input.type = 'file';
            input.accept = '.json';
            input.style.display = 'none';
            document.body.appendChild(input);
            input.onchange = function(e) {
                var file = e.target.files[0];
                if (!file) { document.body.removeChild(input); return; }
                var reader = new FileReader();
                reader.onload = function() {
                    var data = new Uint8Array(reader.result);
                    var buf = _malloc(data.length + 1);
                    HEAPU8.set(data, buf);
                    HEAPU8[buf + data.length] = 0;
                    _editorLoadProjectCallback(self, buf, data.length);
                    _free(buf);
                };
                reader.readAsArrayBuffer(file);
                document.body.removeChild(input);
            };
            input.click();
        }, self);
    }

    void EditorSystem::saveProject(const std::string& /*defaultPath*/)
    {
        if (!canvas) return;

        auto data = gatherProjectData(*canvas);
        std::string json = saveProjectToString(data);

        MAIN_THREAD_EM_ASM({
            var json = UTF8ToString($0);
            var blob = new Blob([json], {type: 'application/json'});
            var a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = 'project.vxl.json';
            document.body.appendChild(a);
            a.click();
            setTimeout(function() {
                document.body.removeChild(a);
                URL.revokeObjectURL(a.href);
            }, 100);
        }, json.c_str());
    }

    void EditorSystem::exportToOBJ(const std::string& /*defaultPath*/)
    {
        if (!canvas) return;

        // Write to virtual filesystem temp location
        exportOBJ(*canvas, "/tmp/voxel_export");

        // Read back and trigger browser downloads
        auto readFile = [](const std::string& path) -> std::string {
            std::ifstream f(path);
            if (!f.is_open()) return "";
            return std::string((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        };

        std::string objData = readFile("/tmp/voxel_export.obj");
        std::string mtlData = readFile("/tmp/voxel_export.mtl");

        auto triggerDownload = [](const char* content, const char* filename) {
            MAIN_THREAD_EM_ASM({
                var text = UTF8ToString($0);
                var blob = new Blob([text], {type: 'application/octet-stream'});
                var a = document.createElement('a');
                a.href = URL.createObjectURL(blob);
                a.download = UTF8ToString($1);
                document.body.appendChild(a);
                a.click();
                setTimeout(function() {
                    document.body.removeChild(a);
                    URL.revokeObjectURL(a.href);
                }, 100);
            }, content, filename);
        };

        if (!objData.empty())
            triggerDownload(objData.c_str(), "export.obj");
        if (!mtlData.empty())
            triggerDownload(mtlData.c_str(), "export.mtl");

        std::remove("/tmp/voxel_export.obj");
        std::remove("/tmp/voxel_export.mtl");
    }

#else

    void EditorSystem::loadProject(const std::string& /*defaultPath*/)
    {
        if (!canvas) return;

        char const* filters[] = { "*.vxl.json" };
        char* result = tinyfd_openFileDialog(
            "Open Project", "",
            1, filters, "Voxel Project (*.vxl.json)", 0);
        if (!result) return;

        ProjectData data;
        if (!loadProjectFromFile(std::string(result), data))
            return;

        applyProjectData(data);
    }

    void EditorSystem::saveProject(const std::string& /*defaultPath*/)
    {
        if (!canvas) return;

        char const* filters[] = { "*.vxl.json" };
        char* result = tinyfd_saveFileDialog(
            "Save Project", "project.vxl.json",
            1, filters, "Voxel Project (*.vxl.json)");
        if (!result) return;

        auto data = gatherProjectData(*canvas);
        saveProjectToFile(data, std::string(result));
    }

    void EditorSystem::exportToOBJ(const std::string& /*defaultPath*/)
    {
        if (!canvas) return;

        char const* filters[] = { "*.obj" };
        char* result = tinyfd_saveFileDialog(
            "Export OBJ", "export.obj",
            1, filters, "Wavefront OBJ (*.obj)");
        if (!result) return;

        // Strip .obj extension if present — exportOBJ adds it back
        std::string basePath(result);
        if (basePath.size() > 4 && basePath.substr(basePath.size() - 4) == ".obj")
            basePath = basePath.substr(0, basePath.size() - 4);

        exportOBJ(*canvas, basePath);
    }

#endif

} // namespace pg
