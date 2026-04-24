#include "stdafx.h"

#include "simple2dobject.h"

#include "glm/gtc/matrix_transform.hpp"

#include "logger.h"

#include "Renderer/renderer.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Shape 2D";
    }

    void Simple2DObjectSystem::setup()
    {
        LOG_THIS_MEMBER(DOM);

        Material simpleShapeMaterial;

        simpleShapeMaterial.shader = masterRenderer->getShader("2DShapes");

        simpleShapeMaterial.nbTextures = 0;

        simpleShapeMaterial.uniformMap.emplace("sWidth", "ScreenWidth");
        simpleShapeMaterial.uniformMap.emplace("sHeight", "ScreenHeight");

        simpleShapeMaterial.setSimpleMesh({3, 2, 1, 4});

        materialId = masterRenderer->registerMaterial(simpleShapeMaterial);
    }

    RenderCall Simple2DObjectSystem::createRenderCall(CompRef<Simple2DObject> obj, CompRef<PositionComponent> ui, CompRef<ViewportComponent> vp)
    {
        LOG_THIS_MEMBER(DOM);

        RenderCall call;

        call.processPositionComponent(ui);

        // Todo
        // if (obj->colors.w == 255.0f)
        //     call.setOpacity(OpacityType::Opaque);
        // else
        //     call.setOpacity(OpacityType::Additive);

        // Cannot set to opaque because the z testing get messed up if you don't order it properly with other systems
        call.setOpacity(OpacityType::Additive);

        call.setRenderStage(renderStage);

        call.setMaterial(materialId);

        call.setViewport(vp->viewport);

        call.data.resize(10);

        call.data[0] = ui->x;
        call.data[1] = ui->y;
        call.data[2] = ui->z;
        call.data[3] = ui->width;
        call.data[4] = ui->height;
        call.data[5] = ui->rotation;
        call.data[6] = obj->colors.x;
        call.data[7] = obj->colors.y;
        call.data[8] = obj->colors.z;
        call.data[9] = obj->colors.w;

        return call;
    }

    // ---------------------------------------------------------------------------
    // RoundedRect2DObjectSystem
    // ---------------------------------------------------------------------------

    void RoundedRect2DObjectSystem::setup()
    {
        LOG_THIS_MEMBER(DOM);

        Material mat;

        mat.shader = masterRenderer->getShader("RoundedRect");

        mat.nbTextures = 0;

        mat.uniformMap.emplace("sWidth", "ScreenWidth");
        mat.uniformMap.emplace("sHeight", "ScreenHeight");

        // Instance layout: worldPos(3), size(2), rotation(1), color(4), cornerRadius(1) = 11 floats
        mat.setSimpleMesh({3, 2, 1, 4, 1});

        materialId = masterRenderer->registerMaterial(mat);
    }

    RenderCall RoundedRect2DObjectSystem::createRenderCall(CompRef<RoundedRect2DObject> obj, CompRef<PositionComponent> ui, CompRef<ViewportComponent> vp)
    {
        LOG_THIS_MEMBER(DOM);

        RenderCall call;

        call.processPositionComponent(ui);

        call.setOpacity(OpacityType::Additive);

        call.setRenderStage(renderStage);

        call.setMaterial(materialId);

        call.setViewport(vp->viewport);

        // 11 floats: x, y, z, width, height, rotation, r, g, b, a, cornerRadius
        call.data.resize(11);

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
        call.data[10] = obj->cornerRadius;

        return call;
    }
}
