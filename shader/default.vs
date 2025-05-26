layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

layout (location = 2) in vec3 aWorldPos;
layout (location = 3) in vec2 aSize;

layout (location = 4) in float rotation;

layout (location = 5) in float aOpacity;
layout (location = 6) in vec3 aMixColor;
layout (location = 7) in float aMixColorRatio;

uniform mat4 model;
uniform mat4 scale;
uniform mat4 view;
uniform mat4 projection;

uniform float sWidth;
uniform float sHeight;

out vec2 TexCoord;
out float opacity;
out vec3 mixColor;
out float mixColorRatio;

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

    float radRotation = rad(rotation);

    mat4 rotateMat = mat4(
        vec4(cos(radRotation), -sin(radRotation), 0.0, 0.0),
        vec4(sin(radRotation), cos(radRotation), 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(0.0, 0.0, 0.0, 1.0));

    vec2 centerNDC = vec2(0.5*aSize.x, -0.5*aSize.y);

    mat4 Tneg = mat4(1.0);
    Tneg[3].xy = -centerNDC;

    mat4 Tpos = mat4(1.0);
    Tpos[3].xy =  centerNDC;

    gl_Position = projection * posMat * view * scale * Tpos * rotateMat * Tneg * scaleMat * model * vec4(aPos.x, aPos.y, aPos.z, 1.0f);

	TexCoord = aTexCoord;

    opacity = aOpacity;
    mixColor = aMixColor;
    mixColorRatio = aMixColorRatio;
}