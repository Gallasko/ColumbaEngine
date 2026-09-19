#include "stdafx.h"

#include "decoratedshapes.h"

#include "logger.h"

#include "Renderer/renderer.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Decorated Shapes";
    }

    // ---------------------------------------------------------------------------
    // HatchRect2DObjectSystem
    // ---------------------------------------------------------------------------

    void HatchRect2DObjectSystem::setup()
    {
        LOG_THIS_MEMBER(DOM);

        Material mat;

        mat.shader = masterRenderer->getShader("HatchRect");

        mat.nbTextures = 0;

        mat.uniformMap.emplace("sWidth", "ScreenWidth");
        mat.uniformMap.emplace("sHeight", "ScreenHeight");

        // pos(3), size(2), rotation(1), color(4), spacing(1), lineWidth(1), angle(1), cornerRadius(1) = 14 floats
        mat.setSimpleMesh({3, 2, 1, 4, 1, 1, 1, 1});

        materialId = masterRenderer->registerMaterial(mat);
    }

    RenderCall HatchRect2DObjectSystem::createRenderCall(CompRef<HatchRect2DObject> obj, CompRef<PositionComponent> ui, CompRef<ViewportComponent> vp)
    {
        LOG_THIS_MEMBER(DOM);

        RenderCall call;

        call.processPositionComponent(ui);

        call.setOpacity(OpacityType::Additive);

        call.setRenderStage(renderStage);

        call.setMaterial(materialId);

        call.setViewport(vp->viewport);

        // 14 floats: x, y, z, width, height, rotation, r, g, b, a, spacing, lineWidth, angle, cornerRadius
        call.data.resize(14);

        call.data[0]  = ui->x;
        call.data[1]  = ui->y;
        call.data[2]  = ui->z;
        call.data[3]  = ui->width;
        call.data[4]  = ui->height;
        call.data[5]  = ui->rotation;
        call.data[6]  = obj->colors.x;
        call.data[7]  = obj->colors.y;
        call.data[8]  = obj->colors.z;
        call.data[9]  = obj->colors.w;
        call.data[10] = obj->spacing;
        call.data[11] = obj->lineWidth;
        call.data[12] = obj->angle;
        call.data[13] = obj->cornerRadius;

        return call;
    }

    // ---------------------------------------------------------------------------
    // DottedLine2DObjectSystem
    // ---------------------------------------------------------------------------

    void DottedLine2DObjectSystem::setup()
    {
        LOG_THIS_MEMBER(DOM);

        Material mat;

        mat.shader = masterRenderer->getShader("DottedLine");

        mat.nbTextures = 0;

        mat.uniformMap.emplace("sWidth", "ScreenWidth");
        mat.uniformMap.emplace("sHeight", "ScreenHeight");

        // pos(3), size(2), rotation(1), color(4), period(1), dotRadius(1) = 12 floats
        mat.setSimpleMesh({3, 2, 1, 4, 1, 1});

        materialId = masterRenderer->registerMaterial(mat);
    }

    RenderCall DottedLine2DObjectSystem::createRenderCall(CompRef<DottedLine2DObject> obj, CompRef<PositionComponent> ui, CompRef<ViewportComponent> vp)
    {
        LOG_THIS_MEMBER(DOM);

        RenderCall call;

        call.processPositionComponent(ui);

        call.setOpacity(OpacityType::Additive);

        call.setRenderStage(renderStage);

        call.setMaterial(materialId);

        call.setViewport(vp->viewport);

        // 12 floats: x, y, z, width, height, rotation, r, g, b, a, period, dotRadius
        call.data.resize(12);

        call.data[0]  = ui->x;
        call.data[1]  = ui->y;
        call.data[2]  = ui->z;
        call.data[3]  = ui->width;
        call.data[4]  = ui->height;
        call.data[5]  = ui->rotation;
        call.data[6]  = obj->colors.x;
        call.data[7]  = obj->colors.y;
        call.data[8]  = obj->colors.z;
        call.data[9]  = obj->colors.w;
        call.data[10] = obj->period;
        call.data[11] = obj->dotRadius;

        return call;
    }
}
