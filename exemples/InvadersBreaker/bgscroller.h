#pragma once

#include <vector>
#include <cmath>
#include <unordered_map>
#include <memory>

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"
#include "ECS/entitysystem.h"
#include "pgconstant.h"

namespace pg
{
    using Vector2D = constant::Vector2D;
    using Vector4D = constant::Vector4D;

    // Enhanced tile component with additional metadata
    struct BackgroundTileComponent : public Component {
        int gridX = 0;          // Current grid position X
        int gridY = 0;          // Current grid position Y
        int originalGridX = 0;  // Original grid position X (for color calculation)
        int originalGridY = 0;  // Original grid position Y (for color calculation)
        float baseX = 0.0f;     // Base position before scrolling offset
        float baseY = 0.0f;     // Base position before scrolling offset
        bool isDark = false;    // Track tile color type for efficient updates
        uint32_t poolIndex = 0; // Index in the tile pool for reuse

        BackgroundTileComponent(int gx, int gy, float bx, float by, bool dark = false)
            : gridX(gx), gridY(gy), originalGridX(gx), originalGridY(gy),
              baseX(bx), baseY(by), isDark(dark) {}

        DEFAULT_COMPONENT_MEMBERS(BackgroundTileComponent)
    };

    // Configuration structure for better parameter management
    struct ScrollConfig {
        float scrollSpeed = 15.0f;
        float tileSize = 80.0f;
        Vector4D darkTileColor = {40.0f, 40.0f, 45.0f, 55.0f};
        Vector4D lightTileColor = {188.0f, 188.0f, 188.0f, 55.0f};
        // Todo this failes when not moving diagnally ( colors swaps during wrap )
        Vector2D scrollDirection = {1.0f, 1.0f}; // Normalized direction vector
        bool enableDiagonalScrolling = true;
        float scrollSpeedMultiplier = 1.0f;

        // Validate and normalize configuration
        void validate() {
            scrollSpeed = std::max(scrollSpeed, 0.0f);
            tileSize = std::max(tileSize, 1.0f);
            scrollSpeedMultiplier = std::max(scrollSpeedMultiplier, 0.0f);

            // Normalize scroll direction
            float magnitude = std::sqrt(scrollDirection.x * scrollDirection.x +
                                      scrollDirection.y * scrollDirection.y);
            if (magnitude > 0.0f) {
                scrollDirection.x /= magnitude;
                scrollDirection.y /= magnitude;
            } else {
                scrollDirection = {1.0f, 1.0f}; // Default diagonal
                magnitude = std::sqrt(2.0f);
                scrollDirection.x /= magnitude;
                scrollDirection.y /= magnitude;
            }
        }
    };

    // Tile pool for efficient memory management
    class TilePool {
    public:
        struct TileData {
            _unique_id entityId = 0;
            bool inUse = false;
            int gridX = 0, gridY = 0;
        };

        std::vector<TileData> tiles;
        std::vector<uint32_t> freeTiles;

        uint32_t acquireTile() {
            if (!freeTiles.empty()) {
                uint32_t index = freeTiles.back();
                freeTiles.pop_back();
                tiles[index].inUse = true;
                return index;
            }

            // Create new tile slot
            tiles.push_back({0, true, 0, 0});
            return static_cast<uint32_t>(tiles.size() - 1);
        }

        void releaseTile(uint32_t index) {
            if (index < tiles.size() && tiles[index].inUse) {
                tiles[index].inUse = false;
                freeTiles.push_back(index);
            }
        }

        void clear() {
            tiles.clear();
            freeTiles.clear();
        }
    };

    // Enhanced background scrolling system with improved performance and features
    class BackgroundScrollerSystem : public System<InitSys, QueuedListener<TickEvent>, QueuedListener<ResizeEvent>>
    {
    private:
        ScrollConfig config;
        TilePool tilePool;

        // Screen parameters
        float screenWidth = 820.0f;
        float screenHeight = 640.0f;

        // Grid configuration
        int tilesX = 0;
        int tilesY = 0;
        int bufferTiles = 2; // Extra tiles for seamless wrapping

        // Scroll tracking
        float scrollOffsetX = 0.0f;
        float scrollOffsetY = 0.0f;
        float totalScrollX = 0.0f; // Track total scroll for statistics
        float totalScrollY = 0.0f;

        // Performance tracking
        mutable size_t frameCount = 0;
        mutable float lastUpdateTime = 0.0f;

        // Cached calculations
        float cachedDiagonalSpeed = 0.0f;
        bool needsRecalculation = true;

    public:
        std::string getSystemName() const override { return "Enhanced Background Scroller System"; }

        virtual void init() override {
            LOG_THIS_MEMBER("BackgroundScrollerSystem");

            registerGroup<PositionComponent, BackgroundTileComponent>();

            config.validate();
            recalculateGrid();
            createTileGrid();
            updateCachedValues();

            LOG_INFO("BackgroundScrollerSystem",
                    "Enhanced background system initialized - " << tilesX << "x" << tilesY
                    << " tiles, size: " << config.tileSize << "px, speed: " << config.scrollSpeed
                    << " px/s, direction: (" << config.scrollDirection.x << ", "
                    << config.scrollDirection.y << ")");
        }

        virtual void onProcessEvent(const TickEvent& event) override {
            float deltaTime = event.tick / 1000.0f;
            lastUpdateTime = deltaTime;
            ++frameCount;

            if (needsRecalculation) {
                updateCachedValues();
                needsRecalculation = false;
            }

            updateScrollOffset(deltaTime);
            handleWrapping();
            updateTilePositions();
        }

        virtual void onProcessEvent(const ResizeEvent& event) override {
            if (event.width != screenWidth || event.height != screenHeight) {
                screenWidth = event.width;
                screenHeight = event.height;
                recalculateGrid();
                createTileGrid();

                LOG_INFO("BackgroundScrollerSystem",
                        "Screen resized - new grid: " << tilesX << "x" << tilesY << " tiles");
            }
        }

        // Configuration methods with validation
        void setScrollConfig(const ScrollConfig& newConfig) {
            ScrollConfig validatedConfig = newConfig;
            validatedConfig.validate();

            bool needsGridRecreation = (validatedConfig.tileSize != config.tileSize) ||
                                     (validatedConfig.darkTileColor.x != config.darkTileColor.x) ||
                                     (validatedConfig.lightTileColor.x != config.lightTileColor.x);

            config = validatedConfig;
            needsRecalculation = true;

            if (needsGridRecreation) {
                resetScroll();
            }
        }

        void setScrollSpeed(float speed) {
            config.scrollSpeed = std::max(speed, 0.0f);
            needsRecalculation = true;
        }

        void setScrollDirection(const Vector2D& direction) {
            config.scrollDirection = direction;
            config.validate();
            needsRecalculation = true;
        }

        void setScrollDirection(float dirX, float dirY) {
            setScrollDirection({dirX, dirY});
        }

        void setTileSize(float size) {
            if (size > 0.0f && size != config.tileSize) {
                config.tileSize = size;
                recalculateGrid();
                resetScroll();
            }
        }

        void setTileColors(const Vector4D& lightColor, const Vector4D& darkColor) {
            config.lightTileColor = lightColor;
            config.darkTileColor = darkColor;
            updateTileColors();
        }

        void setDiagonalScrolling(bool enabled) {
            config.enableDiagonalScrolling = enabled;
            if (enabled) {
                setScrollDirection(1.0f, 1.0f);
            } else {
                setScrollDirection(1.0f, 0.0f); // Horizontal only
            }
        }

        void setScrollSpeedMultiplier(float multiplier) {
            config.scrollSpeedMultiplier = std::max(multiplier, 0.0f);
            needsRecalculation = true;
        }

        // Advanced control methods
        void pauseScrolling() {
            config.scrollSpeedMultiplier = 0.0f;
            needsRecalculation = true;
        }

        void resumeScrolling(float multiplier = 1.0f) {
            config.scrollSpeedMultiplier = std::max(multiplier, 0.0f);
            needsRecalculation = true;
        }

        void reverseScrolling() {
            config.scrollDirection.x = -config.scrollDirection.x;
            config.scrollDirection.y = -config.scrollDirection.y;
            needsRecalculation = true;
        }

        // Getters
        const ScrollConfig& getScrollConfig() const { return config; }
        float getScrollSpeed() const { return config.scrollSpeed; }
        float getTileSize() const { return config.tileSize; }
        Vector4D getLightTileColor() const { return config.lightTileColor; }
        Vector4D getDarkTileColor() const { return config.darkTileColor; }
        Vector2D getScrollDirection() const { return config.scrollDirection; }
        int getTileCountX() const { return tilesX; }
        int getTileCountY() const { return tilesY; }
        float getTotalScrollX() const { return totalScrollX; }
        float getTotalScrollY() const { return totalScrollY; }

        // Performance metrics
        size_t getFrameCount() const { return frameCount; }
        float getLastUpdateTime() const { return lastUpdateTime; }
        size_t getActiveTileCount() const {
            return std::count_if(tilePool.tiles.begin(), tilePool.tiles.end(),
                               [](const TilePool::TileData& tile) { return tile.inUse; });
        }

        void resetScroll() {
            scrollOffsetX = 0.0f;
            scrollOffsetY = 0.0f;
            totalScrollX = 0.0f;
            totalScrollY = 0.0f;
            frameCount = 0;
            createTileGrid();
        }

        // Debug and utility methods
        void logPerformanceStats() const {
            LOG_INFO("BackgroundScrollerSystem",
                    "Performance Stats - Frames: " << frameCount
                    << ", Active tiles: " << getActiveTileCount()
                    << ", Total scroll: (" << totalScrollX << ", " << totalScrollY << ")"
                    << ", Last update time: " << lastUpdateTime << "ms");
        }

    private:
        void updateCachedValues() {
            float effectiveSpeed = config.scrollSpeed * config.scrollSpeedMultiplier;
            cachedDiagonalSpeed = effectiveSpeed;
        }

        void recalculateGrid() {
            tilesX = static_cast<int>(std::ceil(screenWidth / config.tileSize)) + bufferTiles;
            tilesY = static_cast<int>(std::ceil(screenHeight / config.tileSize)) + bufferTiles;
        }

        void updateScrollOffset(float deltaTime) {
            float moveX = cachedDiagonalSpeed * config.scrollDirection.x * deltaTime;
            float moveY = cachedDiagonalSpeed * config.scrollDirection.y * deltaTime;

            scrollOffsetX += moveX;
            scrollOffsetY += moveY;
            totalScrollX += moveX;
            totalScrollY += moveY;
        }

        void handleWrapping() {
            // Handle X wrapping
            if (scrollOffsetX >= config.tileSize) {
                int wraps = static_cast<int>(scrollOffsetX / config.tileSize);
                scrollOffsetX -= wraps * config.tileSize;

                for (int i = 0; i < wraps; ++i) {
                    wrapTilesHorizontally();
                }
            } else if (scrollOffsetX <= -config.tileSize) {
                int wraps = static_cast<int>(-scrollOffsetX / config.tileSize);
                scrollOffsetX += wraps * config.tileSize;

                for (int i = 0; i < wraps; ++i) {
                    wrapTilesHorizontallyReverse();
                }
            }

            // Handle Y wrapping
            if (scrollOffsetY >= config.tileSize) {
                int wraps = static_cast<int>(scrollOffsetY / config.tileSize);
                scrollOffsetY -= wraps * config.tileSize;

                for (int i = 0; i < wraps; ++i) {
                    wrapTilesVertically();
                }
            } else if (scrollOffsetY <= -config.tileSize) {
                int wraps = static_cast<int>(-scrollOffsetY / config.tileSize);
                scrollOffsetY += wraps * config.tileSize;

                for (int i = 0; i < wraps; ++i) {
                    wrapTilesVerticallyReverse();
                }
            }
        }

        void createTileGrid() {
            clearExistingTiles();

            for (int y = 0; y < tilesY; ++y) {
                for (int x = 0; x < tilesX; ++x) {
                    createTileAt(x, y);
                }
            }
        }

        void clearExistingTiles() {
            std::vector<_unique_id> tilesToRemove;
            for (const auto& ent : viewGroup<BackgroundTileComponent>()) {
                tilesToRemove.push_back(ent->entityId);
            }

            for (auto id : tilesToRemove) {
                ecsRef->removeEntity(id);
            }

            tilePool.clear();
        }

        void createTileAt(int gridX, int gridY) {
            float baseX = gridX * config.tileSize - config.tileSize;
            float baseY = gridY * config.tileSize - config.tileSize;

            // Use original grid position for consistent checkerboard pattern
            bool isDark = ((gridX + gridY) % 2) == 0;
            auto tileColor = isDark ? config.darkTileColor : config.lightTileColor;

            auto tile = makeSimple2DShape(ecsRef, Shape2D::Square,
                                        config.tileSize, config.tileSize, tileColor);

            uint32_t poolIndex = tilePool.acquireTile();
            tilePool.tiles[poolIndex].entityId = tile.entity.id;
            tilePool.tiles[poolIndex].gridX = gridX;
            tilePool.tiles[poolIndex].gridY = gridY;

            auto bgComponent = tile.attach<BackgroundTileComponent>(gridX, gridY, baseX, baseY, isDark);
            bgComponent->poolIndex = poolIndex;

            auto pos = tile.get<PositionComponent>();
            pos->setX(baseX);
            pos->setY(baseY);
        }

        void updateTilePositions() {
            for (const auto& ent : viewGroup<PositionComponent, BackgroundTileComponent>()) {
                auto pos = ent->get<PositionComponent>();
                auto tile = ent->get<BackgroundTileComponent>();

                if (!pos || !tile) continue;

                float newX = tile->baseX + scrollOffsetX;
                float newY = tile->baseY + scrollOffsetY;

                pos->setX(newX);
                pos->setY(newY);
            }
        }

        void updateTileColors() {
            for (const auto& ent : viewGroup<BackgroundTileComponent>()) {
                auto tile = ent->get<BackgroundTileComponent>();
                if (!tile) continue;

                auto tileColor = tile->isDark ? config.darkTileColor : config.lightTileColor;
                // Note: You'll need to implement color updating in your visual component
                // This is engine-specific, so for now we recreate the grid
            }
            createTileGrid(); // Fallback - recreate with new colors
        }

        void wrapTilesHorizontally() {
            for (const auto& ent : viewGroup<BackgroundTileComponent>()) {
                auto tile = ent->get<BackgroundTileComponent>();
                if (!tile) continue;

                if (tile->baseX + scrollOffsetX >= screenWidth + config.tileSize) {
                    tile->baseX -= tilesX * config.tileSize;
                    // Update current grid position but keep original for color consistency
                    tile->gridX -= tilesX;
                }
            }
        }

        void wrapTilesHorizontallyReverse() {
            for (const auto& ent : viewGroup<BackgroundTileComponent>()) {
                auto tile = ent->get<BackgroundTileComponent>();
                if (!tile) continue;

                if (tile->baseX + scrollOffsetX < -config.tileSize * 2) {
                    tile->baseX += tilesX * config.tileSize;
                    // Update current grid position but keep original for color consistency
                    tile->gridX += tilesX;
                }
            }
        }

        void wrapTilesVertically() {
            for (const auto& ent : viewGroup<BackgroundTileComponent>()) {
                auto tile = ent->get<BackgroundTileComponent>();
                if (!tile) continue;

                if (tile->baseY + scrollOffsetY >= screenHeight + config.tileSize) {
                    tile->baseY -= tilesY * config.tileSize;
                    // Update current grid position but keep original for color consistency
                    tile->gridY -= tilesY;
                }
            }
        }

        void wrapTilesVerticallyReverse() {
            for (const auto& ent : viewGroup<BackgroundTileComponent>()) {
                auto tile = ent->get<BackgroundTileComponent>();
                if (!tile) continue;

                if (tile->baseY + scrollOffsetY < -config.tileSize * 2) {
                    tile->baseY += tilesY * config.tileSize;
                    // Update current grid position but keep original for color consistency
                    tile->gridY += tilesY;
                }
            }
        }
    };
}