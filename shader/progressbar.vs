#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

layout (location = 2) in vec3 aWorldPos;
layout (location = 3) in vec2 aSize;

layout (location = 4) in float rotation;

layout (location = 5) in float aFillRatio;
layout (location = 6) in float aDirection;

uniform mat4 model;
uniform mat4 scale;
uniform mat4 view;
uniform mat4 projection;

uniform float sWidth;
uniform float sHeight;

out vec2 TexCoord;
out float fillRatio;
out float direction;

float M_PI = 3.1415926535897932384626433832795;

float rad(float degree)
{
    return degree * M_PI / 180.0f;
}

void main()
{
    mat4 posMat = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(-1.0f + 2.0 * aWorldPos.x * (1.0 / sWidth), 1.0 + 2.0 * -(aWorldPos.y) * (1.0 / sHeight), aWorldPos.z / 100.0f, 1.0) );

    mat4 scaleMat = mat4(
        vec4(aSize.x, 0.0, 0.0, 0.0),
        vec4(0.0, aSize.y, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(0.0 , 0.0, 0.0, 1.0));

    mat4 rotateMat = mat4(
        vec4(cos(rotation), -sin(rotation), 0.0, 0.0),
        vec4(sin(rotation), cos(rotation), 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(0.0 , 0.0, 0.0, 1.0));

    mat4 trans = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(-aSize.x * (1.0 / sWidth), aSize.y * (1.0 / sHeight), 0.0, 1.0));

    mat4 invtrans = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(aSize.x * (1.0 / sWidth), -aSize.y * (1.0 / sHeight), 0.0, 1.0));

    gl_Position = projection * posMat * view * invtrans * rotateMat * trans * scale * scaleMat * model * vec4(aPos.x, aPos.y, aPos.z, 1.0f);

	TexCoord = aTexCoord;

    fillRatio = aFillRatio;
    direction = aDirection;
}