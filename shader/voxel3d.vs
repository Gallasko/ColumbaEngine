layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aWorldPos;
layout (location = 2) in vec3 aSize;
layout (location = 3) in vec4 aColors;

uniform mat4 view;
uniform mat4 projection;

out vec4 ourColor;
out vec3 vWorldPos;

void main()
{
    // Local-space unit cube scaled by aSize and offset by aWorldPos.
    vec3 worldPos = aWorldPos + aPos * aSize;
    vWorldPos = worldPos;

    gl_Position = projection * view * vec4(worldPos, 1.0);

    ourColor = aColors;
}
