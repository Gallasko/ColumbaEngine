#pragma once

#include <vector>
#include <cmath>

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"
#include "ECS/entitysystem.h"
#include "constant.h"

namespace pg
{
    using Vector2D = constant::Vector2D;

    // Component to track background tiles
    struct BackgroundTileComponent : public Component {
        int gridX = 0;          // Grid position X
        int gridY = 0;          // Grid position Y
        float baseX = 0.0f;     // Base position before scrolling offset
        float baseY = 0.0f;     // Base position before scrolling offset
        
        BackgroundTileComponent() = default;
        BackgroundTileComponent(int gx, int gy, float bx, float by) 
            : gridX(gx), gridY(gy), baseX(bx), baseY(by) {}
    };

    // Background scrolling system that creates an infinite diagonal scrolling grid
    struct BackgroundScrollerSystem : public System<InitSys, QueuedListener<TickEvent>>
    {
        // Screen and tile parameters
        float screenWidth = 820.0f;
        float screenHeight = 640.0f;
        float tileSize = 40.0f;              // Size of each tile (square)
        float scrollSpeed = 15.0f;           // Pixels per second diagonal movement
        
        // Grid configuration
        int tilesX = 0;                      // Number of tiles horizontally (+1 for wrapping)
        int tilesY = 0;                      // Number of tiles vertically (+1 for wrapping)
        
        // Scroll offset tracking
        float scrollOffsetX = 0.0f;
        float scrollOffsetY = 0.0f;
        
        // Tile appearance
        constant::Vector4D darkTileColor = {40.0f, 40.0f, 45.0f, 55.0f};  // Dark gray tiles
        constant::Vector4D lightTileColor = {188.0f, 188.0f, 188.0f, 55.0f};  // Dark gray tiles
        
        virtual void init() override
        {
            LOG_THIS_MEMBER("BackgroundScrollerSystem");
            
            // Register group for background tiles
            registerGroup<PositionComponent, BackgroundTileComponent>();
            
            // Calculate grid dimensions (add 2 extra tiles for seamless wrapping)
            tilesX = static_cast<int>(std::ceil(screenWidth / tileSize)) + 2;
            tilesY = static_cast<int>(std::ceil(screenHeight / tileSize)) + 2;
            
            // Create the initial grid
            createTileGrid();
            
            LOG_INFO("BackgroundScrollerSystem", "Background grid initialized - " 
                    << tilesX << "x" << tilesY << " tiles, size: " << tileSize 
                    << "px, speed: " << scrollSpeed << " px/s");
        }
        
        virtual void onProcessEvent(const TickEvent& event) override
        {
            float deltaTime = event.tick / 1000.0f; // Convert to seconds
            
            // Update scroll offset (diagonal movement)
            float diagonalSpeed = scrollSpeed * 0.707107f; // sqrt(2)/2 for 45° diagonal
            scrollOffsetX += diagonalSpeed * deltaTime;
            scrollOffsetY += diagonalSpeed * deltaTime;
            
            // Handle wrapping when offset exceeds tile size
            if (scrollOffsetX >= tileSize) {
                scrollOffsetX -= tileSize;
                wrapTilesHorizontally();
            }
            
            if (scrollOffsetY >= tileSize) {
                scrollOffsetY -= tileSize;
                wrapTilesVertically();
            }
            
            // Update all tile positions
            updateTilePositions();
        }
        
        void createTileGrid()
        {
            // Clear any existing tiles
            std::vector<_unique_id> tilesToRemove;
            for (const auto& ent : viewGroup<BackgroundTileComponent>()) {
                tilesToRemove.push_back(ent->entityId);
            }
            for (auto id : tilesToRemove) {
                ecsRef->removeEntity(id);
            }
            
            // Create grid of tiles
            for (int y = 0; y < tilesY; ++y) {
                for (int x = 0; x < tilesX; ++x) {
                    createTileAt(x, y);
                }
            }
        }
        
        void createTileAt(int gridX, int gridY)
        {
            // Calculate base position for this grid cell
            float baseX = gridX * tileSize - tileSize; // Start one tile off-screen left
            float baseY = gridY * tileSize - tileSize; // Start one tile off-screen top

            auto tileColor = ((gridX + gridY) % 2) == 0 ? darkTileColor : lightTileColor; // Offset for checkerboard pattern
            
            // Create the visual tile
            auto tile = makeSimple2DShape(ecsRef, Shape2D::Square, tileSize, tileSize, tileColor);
            
            // Attach background tile component
            tile.attach<BackgroundTileComponent>(gridX, gridY, baseX, baseY);
            
            // Set initial position (will be updated by updateTilePositions)
            auto pos = tile.get<PositionComponent>();
            pos->setX(baseX);
            pos->setY(baseY);
        }
        
        void updateTilePositions()
        {
            // Update all tile positions based on their base position + scroll offset
            for (const auto& ent : viewGroup<PositionComponent, BackgroundTileComponent>()) {
                auto pos = ent->get<PositionComponent>();
                auto tile = ent->get<BackgroundTileComponent>();
                
                if (!pos || !tile) continue;
                
                // Apply scroll offset to base position
                float newX = tile->baseX + scrollOffsetX;
                float newY = tile->baseY + scrollOffsetY;
                
                pos->setX(newX);
                pos->setY(newY);
            }
        }
        
        void wrapTilesHorizontally()
        {
            // Find tiles that have moved off the right edge and wrap them to the left
            for (const auto& ent : viewGroup<BackgroundTileComponent>()) {
                auto tile = ent->get<BackgroundTileComponent>();
                if (!tile) continue;
                
                // Check if tile is off the right edge
                if (tile->baseX + scrollOffsetX >= screenWidth + tileSize) {
                    // Move this tile to the left edge
                    tile->baseX -= tilesX * tileSize;
                }
            }
        }
        
        void wrapTilesVertically()
        {
            // Find tiles that have moved off the bottom edge and wrap them to the top
            for (const auto& ent : viewGroup<BackgroundTileComponent>()) {
                auto tile = ent->get<BackgroundTileComponent>();
                if (!tile) continue;
                
                // Check if tile is off the bottom edge
                if (tile->baseY + scrollOffsetY >= screenHeight + tileSize) {
                    // Move this tile to the top edge
                    tile->baseY -= tilesY * tileSize;
                }
            }
        }
        
        // Public configuration methods
        void setScrollSpeed(float speed) {
            scrollSpeed = std::max(speed, 0.0f);
        }
        
        void setTileSize(float size) {
            if (size > 0.0f && size != tileSize) {
                tileSize = size;
                // Recalculate grid and recreate tiles
                tilesX = static_cast<int>(std::ceil(screenWidth / tileSize)) + 2;
                tilesY = static_cast<int>(std::ceil(screenHeight / tileSize)) + 2;
                scrollOffsetX = 0.0f;
                scrollOffsetY = 0.0f;
                createTileGrid();
            }
        }
        
        void setTileColor(const constant::Vector4D& lightColor, const constant::Vector4D& darkColor) {
            lightTileColor = lightColor;
            darkTileColor = darkColor;

            // // Update existing tiles
            // for (const auto& ent : viewGroup<BackgroundTileComponent>()) {
            //     // Note: You'd need to update the visual component here
            //     // This depends on your engine's implementation of color changes
            //     // For now, recreating the grid is the simplest approach
            // }

            createTileGrid(); // Recreate with new color
        }
        
        void setScreenSize(float width, float height) {
            if (width != screenWidth || height != screenHeight) {
                screenWidth = width;
                screenHeight = height;
                // Recalculate grid
                tilesX = static_cast<int>(std::ceil(screenWidth / tileSize)) + 2;
                tilesY = static_cast<int>(std::ceil(screenHeight / tileSize)) + 2;
                createTileGrid();
            }
        }
        
        // Getters
        float getScrollSpeed() const { return scrollSpeed; }
        float getTileSize() const { return tileSize; }
        constant::Vector4D getLightTileColor() const { return lightTileColor; }
        constant::Vector4D getDarkTileColor() const { return darkTileColor; }
        int getTileCountX() const { return tilesX; }
        int getTileCountY() const { return tilesY; }
        
        // Reset the scroll state
        void resetScroll() {
            scrollOffsetX = 0.0f;
            scrollOffsetY = 0.0f;
            createTileGrid();
        }
    };
}