#include "room.h"

namespace pg
{
    void drawDebugGrid(EntitySystem* ecs, int worldW, int worldH)
    {
        // fetch grid sizes
        auto colSys = ecs->getSystem<CollisionSystem>();
        auto pageSz = colSys->pageSize;     // pages per axis
        auto cellSz  = colSys->cellSize;    // pixels per cell

        int pagePixelW = int(pageSz.x * cellSz.x);
        int pagePixelH = int(pageSz.y * cellSz.y);

        constant::Vector4D light  = {255, 255, 255, 32};
        constant::Vector4D dark   = {0, 0, 0, 32};
        constant::Vector4D border = {0, 0, 0, 255};
        float borderThickness = 2.0f;

        // 1) Checkerboard of cells
        for (int y = 0; y < worldH; y += int(cellSz.y))
        {
            for (int x = 0; x < worldW; x += int(cellSz.x))
            {
                bool even = (((x / int(cellSz.x)) + (y / int(cellSz.y))) % 2) == 0;
                auto color = even ? light : dark;
                auto ent = makeSimple2DShape(ecs, Shape2D::Square,
                                            float(cellSz.x), float(cellSz.y),
                                            {color.x, color.y, color.z, color.w});
                ent.get<Simple2DObject>()->setViewport(1);
                auto p = ent.get<PositionComponent>();
                p->setX((float)x);
                p->setY((float)y);
                p->setZ(70.f);

                ecs->attach<CollisionComponent>(ent.entity, 0, 1.f);
                ecs->attach<TestGridFlag>(ent.entity, color);
            }
        }

        // 2) Hollow page outlines
        for (int py = 0; py < worldH; py += pagePixelH)
        {
            for (int px = 0; px < worldW; px += pagePixelW)
            {
                // Top border
                {
                    auto ent = makeSimple2DShape(ecs, Shape2D::Square,
                                                float(pagePixelW), borderThickness,
                                                {border.x, border.y, border.z, border.w});
                    ent.get<Simple2DObject>()->setViewport(1);
                    auto p = ent.get<PositionComponent>();
                    p->setX((float)px);
                    p->setY((float)py);
                    p->setZ(71.f);
                }
                // Bottom border
                {
                    auto ent = makeSimple2DShape(ecs, Shape2D::Square,
                                                float(pagePixelW), borderThickness,
                                                {border.x, border.y, border.z, border.w});
                    ent.get<Simple2DObject>()->setViewport(1);
                    auto p = ent.get<PositionComponent>();
                    p->setX((float)px);
                    p->setY((float)(py + pagePixelH - borderThickness));
                    p->setZ(71.f);
                }
                // Left border
                {
                    auto ent = makeSimple2DShape(ecs, Shape2D::Square,
                                                borderThickness, float(pagePixelH),
                                                {border.x, border.y, border.z, border.w});
                    ent.get<Simple2DObject>()->setViewport(1);
                    auto p = ent.get<PositionComponent>();
                    p->setX((float)px);
                    p->setY((float)py);
                    p->setZ(71.f);
                }
                // Right border
                {
                    auto ent = makeSimple2DShape(ecs, Shape2D::Square,
                                                borderThickness, float(pagePixelH),
                                                {border.x, border.y, border.z, border.w});
                    ent.get<Simple2DObject>()->setViewport(1);
                    auto p = ent.get<PositionComponent>();
                    p->setX((float)(px + pagePixelW - borderThickness));
                    p->setY((float)py);
                    p->setZ(71.f);
                }
            }
        }
    }
}