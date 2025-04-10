#version 330 core

// Original vertex attributes
layout (location = 0) in vec3 aPos;
// Although aTexCoord is declared, we now compute final UV from the atlas region.
layout (location = 1) in vec2 aTexCoord;

// Remaining transformation attributes
layout (location = 2) in vec3 aWorldPos;
layout (location = 3) in vec2 aSize;
layout (location = 4) in float rotation;
layout (location = 5) in float aOpacity;
layout (location = 6) in vec3 aMixColor;
layout (location = 7) in float aMixColorRatio;

// *** New attributes: glyph atlas UV region ***
// These represent the top-left (u1,v1) and bottom-right (u2,v2) coordinates of the glyph in the atlas.
layout (location = 8) in vec4 aUV;

uniform mat4 model;
uniform mat4 scale;
uniform mat4 view;
uniform mat4 projection;

uniform float sWidth;
uniform float sHeight;

out vec2 finalUV;
out float opacity;
out vec3 mixColor;
out float mixColorRatio;

float M_PI = 3.1415926535897932384626433832795;

float rad(float degree) {
    return degree * M_PI / 180.0;
}

void main() {
    mat4 posMat = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(-1.0 + 2.0 * aWorldPos.x * (1.0 / sWidth), 1.0 + 2.0 * -(aWorldPos.y) * (1.0 / sHeight), aWorldPos.z / 100.0, 1.0)
    );

    mat4 scaleMat = mat4(
        vec4(aSize.x, 0.0, 0.0, 0.0),
        vec4(0.0, aSize.y, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );

    mat4 rotateMat = mat4(
        vec4(cos(rotation), -sin(rotation), 0.0, 0.0),
        vec4(sin(rotation), cos(rotation), 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );

    mat4 trans = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(-aSize.x * (1.0 / sWidth), aSize.y * (1.0 / sHeight), 0.0, 1.0)
    );

    mat4 invtrans = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(aSize.x * (1.0 / sWidth), -aSize.y * (1.0 / sHeight), 0.0, 1.0)
    );

    // Final transformation calculation (adjusted based on your existing setup)
    gl_Position = projection * posMat * view * invtrans * rotateMat * trans * scale * scaleMat * model * vec4(aPos, 1.0);
    // gl_Position =  view * posMat * scale * scaleMat * model * vec4(aPos.x, aPos.y, aPos.z, 1.0f);

    // --- Compute Final UV Coordinates Using the Atlas Region ---
    // Interpolate between u1 and u2 along aTexCoord.x,
    // and between v1 and v2 along aTexCoord.y.
    // finalUV = vec2((1 - aPos.x) * aUV.x  + aPos.x * aUV.z, (1 + aPos.y) * aUV.y  + (-1.0f * aPos.y) * aUV.a);
    finalUV = vec2(mix(aUV.x, aUV.z, aTexCoord.x), mix(aUV.y, aUV.a, aTexCoord.y));

    opacity = aOpacity;
    mixColor = aMixColor;
    mixColorRatio = aMixColorRatio;
}
