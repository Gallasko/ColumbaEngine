#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

layout (location = 2) in vec3 aWorldPos;
layout (location = 3) in vec2 aSize;
layout (location = 4) in vec4 aColors;

uniform mat4 model;
uniform mat4 scale;
uniform mat4 view;
uniform mat4 projection;

uniform float sWidth;
uniform float sHeight;

out vec4 ourColor;

void main()
{
    mat4 posMat = mat4(
        vec4( 1.0, 0.0, 0.0, 0.0),
        vec4( 0.0, 1.0, 0.0, 0.0),
        vec4( 0.0, 0.0, 1.0, 0.0),
        vec4( -1.0f + 2.0f * aWorldPos.x * (1.0f / sWidth), 1.0f + 2.0f * -(aWorldPos.y) * (1.0f / sHeight), aWorldPos.z / 100.0f, 1.0f) );

    mat4 scaleMat = mat4(
        vec4( aSize.x, 0.0f, 0.0f, 0.0f),
        vec4( 0.0f, aSize.y, 0.0f, 0.0f),
        vec4( 0.0f, 0.0f, 1.0f, 0.0f),
        vec4( 0.0f, 0.0f, 0.0f, 1.0f) );

    gl_Position = projection * posMat * view * scale * scaleMat * model * vec4(aPos.x, aPos.y, aPos.z, 1.0f);

    ourColor = aColors;
}