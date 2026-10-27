#pragma once

#include <atomic>
#include <mutex>

#include "Renderer/mesh.h"

#include "Core/mesher.h"

namespace arena
{
    /**
     * A voxel mesh on the GPU that can be rebuilt while the game runs.
     *
     * The game thread hands new geometry with setData(); the render thread uploads it the
     * next time the mesh is drawn. The GL objects are made once and kept: a mesh is never
     * destroyed while the game runs (its last owner could be on a thread without a GL context).
     *
     * Instance layout (INSTANCE_FLOATS): model matrix (16, column major), tint rgba (4), flash rgba (4).
     */
    struct GpuMesh : public pg::Mesh
    {
        static constexpr size_t INSTANCE_FLOATS = 24;

        GpuMesh() = default;

        /** Any thread. */
        void setData(MeshData data);

        /** Faces in the last geometry handed over: 0 means there is nothing to draw. */
        size_t faces() const { return faceCount.load(); }

        /** Render thread, called by the engine before each draw (Mesh::initialized stays false on purpose). */
        void generateMesh() override;

    private:
        std::mutex mutex;
        MeshData pending;
        std::atomic<bool> hasPending{false};
        std::atomic<size_t> faceCount{0};
        bool glReady = false;
    };
}
