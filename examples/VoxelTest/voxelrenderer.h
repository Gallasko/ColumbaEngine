#pragma once

#include <memory>

#include "simplerenderer.h"
#include "voxelcomponents.h"
#include "cubemesh.h"

namespace pg
{
    /**
     * Renders every VoxelComponent as an instanced unit cube using the
     * `voxel3d` shader. All voxels share a single CubeMesh; the engine's
     * batching logic (in MasterRenderer::execute) merges render calls
     * that share the same material into a single draw call, so one frame
     * with N voxels becomes one glDrawElementsInstanced with N instances.
     *
     * Projection is provided by Camera::getProjectionMatrix() and set by
     * the master renderer via the `projection` uniform each frame.
     */
    struct VoxelRenderSystem : public SimpleRenderer<VoxelComponent>
    {
        VoxelRenderSystem(MasterRenderer* masterRenderer, float initialAspect);

        std::string getSystemName() const override { return "Voxel Render System"; }

        void setupRenderer() override;

        RenderCall createRenderCall(CompRef<VoxelComponent> voxel) override;

        // Update the camera's aspect ratio from the new window dimensions.
        void onResize(float width, float height) override;

    private:
        // Shared across all voxel render calls so the engine can batch them.
        std::shared_ptr<CubeMesh> sharedCube;
    };
}
