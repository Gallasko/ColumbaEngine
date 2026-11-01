#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "Renderer/mesh.h"

#include "Core/mesher.h"

namespace arena
{
    /**
     * The block textures: a texture array, one layer per png (BlockLook::texture), all of one size.
     * Same threading as GpuMesh: the game thread hands pixels with setData(), the render thread
     * uploads them the next time a mesh that uses them is drawn.
     */
    struct GpuTextureArray
    {
        /** Any thread. rgba holds `layers` square images of size x size, one after the other. */
        void setData(int size, int layers, std::vector<uint8_t> rgba);

        /** Render thread: uploads what is pending and binds the array to texture unit 0 (uBlocks). */
        void bind();

    private:
        std::mutex mutex;
        std::vector<uint8_t> pending;
        int pendingSize = 0, pendingLayers = 0;
        std::atomic<bool> hasPending{false};
        unsigned int id = 0;
    };

    /**
     * A voxel mesh on the GPU that can be rebuilt while the game runs.
     *
     * The game thread hands new geometry with setData(); the render thread uploads it the
     * next time the mesh is drawn. The GL objects are made once and kept: a mesh is never
     * destroyed while the game runs (its last owner could be on a thread without a GL context).
     *
     * Instance layout (INSTANCE_FLOATS): model matrix (16, column major), view * model (16),
     * tint rgba (4), flash rgba (4).
     *
     * The view travels with each instance, not as a uniform: the engine draws a render list that
     * can be a frame old, while its camera uniform is read when the frame is drawn. Baking the view
     * in keeps every list drawn with the camera it was built for (else entities lag the camera).
     */
    struct GpuMesh : public pg::Mesh
    {
        static constexpr size_t INSTANCE_FLOATS = 40;

        GpuMesh() = default;

        /** Any thread. */
        void setData(MeshData data);

        /** Faces in the last geometry handed over: 0 means there is nothing to draw. */
        size_t faces() const { return faceCount.load(); }

        /** Render thread, called by the engine before each draw (Mesh::initialized stays false on purpose). */
        void generateMesh() override;

        /** Bound before each draw, for the meshes of blocks (the arena, falling pieces). */
        std::shared_ptr<GpuTextureArray> textures;

    private:
        std::mutex mutex;
        MeshData pending;
        std::atomic<bool> hasPending{false};
        std::atomic<size_t> faceCount{0};
        bool glReady = false;
    };
}
