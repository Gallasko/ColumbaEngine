#pragma once

#include "Renderer/mesh.h"

namespace pg
{
    /**
     * Unit cube mesh spanning local-space (0,0,0)..(1,1,1).
     * The vertex shader offsets by `aWorldPos` and scales by `aSize` to
     * place the cube in the world.
     *
     * Instance VBO layout (stride = INSTANCE_STRIDE_FLOATS floats):
     *   location 1: vec3 aWorldPos   offset 0
     *   location 2: vec3 aSize       offset 3
     *   location 3: vec4 aColors     offset 6
     */
    struct CubeMesh : public Mesh
    {
        static constexpr size_t INSTANCE_STRIDE_FLOATS = 10;

        CubeMesh();
        virtual ~CubeMesh();

        void generateMesh() override;
    };
}
