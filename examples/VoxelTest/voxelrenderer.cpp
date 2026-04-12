#include "stdafx.h"

#include "voxelrenderer.h"

#include "Renderer/renderer.h"

#include "logger.h"

#include "Helpers/openglobject.h"

namespace pg
{
    namespace
    {
        constexpr const char* DOM = "VoxelRenderSystem";
    }

    VoxelRenderSystem::VoxelRenderSystem(MasterRenderer* masterRenderer, float initialAspect)
        : SimpleRenderer(masterRenderer)
    {
        masterRenderer->getCamera().setAspectRatio(initialAspect);
    }

    void VoxelRenderSystem::setupRenderer()
    {
        // Depth test isn't enabled by the engine (it's a 2D-first renderer)
        // but the depth buffer IS cleared each frame, so flipping this on
        // here is enough to get correct 3D occlusion for our voxels.
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Base material — every voxel uses this material so the engine's
        // same-key batching merges all instance data into a single draw.
        auto* mat = newMaterial("voxelBase");
        mat->shader      = masterRenderer->getShader("voxel3d");
        mat->nbTextures  = 0;
        mat->nbAttributes = CubeMesh::INSTANCE_STRIDE_FLOATS;
    }

    void VoxelRenderSystem::onResize(float width, float height)
    {
        if (width <= 0.0f or height <= 0.0f)
            return;

        masterRenderer->getCamera().setAspectRatio(width / height);
    }

    RenderCall VoxelRenderSystem::createRenderCall(CompRef<VoxelComponent> voxel)
    {
        if (not sharedCube)
        {
            sharedCube = std::make_shared<CubeMesh>();
        }

        RenderCall call(sharedCube);

        call.setRenderStage(renderStage);
        call.setViewport(0);
        call.setOpacity(OpacityType::Opaque);

        // Allow engine-side batching: multiple voxel calls sharing the
        // same material key get their `data` concatenated and drawn as
        // a single instanced call.
        call.batchable = true;

        applyMaterial(call, "voxelBase");

        // Pack per-instance attributes matching the CubeMesh vertex layout:
        //   [0..2] aWorldPos  (vec3)
        //   [3..5] aSize      (vec3)
        //   [6..9] aColors    (vec4, 0..255 per channel)
        call.data[0] = voxel->position.x;
        call.data[1] = voxel->position.y;
        call.data[2] = voxel->position.z;

        call.data[3] = voxel->size.x;
        call.data[4] = voxel->size.y;
        call.data[5] = voxel->size.z;

        call.data[6] = voxel->color.r;
        call.data[7] = voxel->color.g;
        call.data[8] = voxel->color.b;
        call.data[9] = voxel->color.a;

        return call;
    }
}
