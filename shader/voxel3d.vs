#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aWorldPos;
layout (location = 2) in vec3 aSize;
layout (location = 3) in vec4 aColors;

// Engine-provided uniform (set each frame from masterRenderer->camera.getViewMatrix()).
uniform mat4 view;

// Our own perspective projection — we use a unique name so it doesn't collide
// with the engine's hard-coded identity `projection` uniform that would
// overwrite whatever we put in the material's uniform map.
uniform mat4 uProjection;

out vec4 ourColor;
out vec3 vWorldPos;

void main()
{
    // Local-space unit cube scaled by aSize and offset by aWorldPos.
    vec3 worldPos = aWorldPos + aPos * aSize;
    vWorldPos = worldPos;

    gl_Position = uProjection * view * vec4(worldPos, 1.0);

    ourColor = aColors;
}
