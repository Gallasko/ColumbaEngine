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
     * Projection is seeded at init from the initial window aspect ratio and
     * refreshed in onResize() whenever the window size changes (F9 toggles
     * borderless fullscreen via Window::toggleFullscreen). The engine's
     * `projection` uniform is always identity for viewport 0, so this
     * renderer uses a custom `uProjection` uniform that it owns and mutates
     * directly on the registered material.
     */
    struct VoxelRenderSystem : public SimpleRenderer<VoxelComponent>
    {
        VoxelRenderSystem(MasterRenderer* masterRenderer, float initialAspect);

        std::string getSystemName() const override { return "Voxel Render System"; }

        void setupRenderer() override;

        RenderCall createRenderCall(CompRef<VoxelComponent> voxel) override;

        // Rebuild the perspective projection from the new aspect ratio and
        // push it onto the registered material so the next frame uses it.
        void onResize(float width, float height) override;

        // Camera parameters, matching a classic FPS projection.
        float fovDegrees = 60.0f;
        float nearPlane  = 0.1f;
        float farPlane   = 500.0f;

    private:
        float aspect = 16.0f / 9.0f;

        // Shared across all voxel render calls so the engine can batch them.
        std::shared_ptr<CubeMesh> sharedCube;
    };
}
