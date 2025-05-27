layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

layout (location = 2) in vec3 aWorldPos;
layout (location = 3) in vec2 aSize;

layout (location = 4) in float rotation;

layout (location = 5) in vec4 aTextureLimits;

layout (location = 6) in float aOpacity;
layout (location = 7) in vec3 aMixColor;
layout (location = 8) in float aMixColorRatio;

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

void main()
{
    mat4 posMat = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(-1.0f + 2.0 * aWorldPos.x * (1.0f / sWidth), 1.0f + 2.0f * -(aWorldPos.y) * (1.0f / sHeight), aWorldPos.z / 100.0f, 1.0f) );

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

    gl_Position = projection * posMat * view * invtrans * rotateMat * trans * scale * scaleMat * model *  vec4(aPos.x, aPos.y, aPos.z, 1.0f);
    //gl_Position = projection * posMat * view * rotateMat * scale * scaleMat * model * trans * vec4(aPos.x, aPos.y, aPos.z, 1.0f);

	//TexCoord = vec2((1 - aPos.x) * aTexCoord.x  + aPos.x * aTexCoord.z, (1 + aPos.y) * aTexCoord.y  + (-1.0f * aPos.y) * aTexCoord.a);
	// TexCoord = vec2((aTexCoord.x) *   + (1.0f - aTexCoord.x) * , (1.0f + aTexCoord.y) *   + (1.0f * aTexCoord.y) * aTextureLimits.a);
	// TexCoord = vec2((aTexCoord.x) * 1 + (1.0f - aTexCoord.x) * 0, (-1.0f + aTexCoord.y) * 0  + aTexCoord.y * 1);
	// TexCoord = vec2((aTexCoord.x) * 0.867257 + (1.0f - aTexCoord.x) * 0.752212, (1.0f - aTexCoord.y) * 0.300000  + aTexCoord.y * 0.409091);
    // TexCoord = vec2((aPos.x) * float(aTextureLimits.z) + (1.0f - aPos.x) * float(aTextureLimits.x), (1.0f - aPos.y) * float(aTextureLimits.y) + aPos.y * float(aTextureLimits.a));
    TexCoord = vec2((aTexCoord.x) * float(aTextureLimits.z) + (1.0f - aTexCoord.x) * float(aTextureLimits.x), (1.0f - aTexCoord.y) * float(aTextureLimits.y) + aTexCoord.y * float(aTextureLimits.a));

    opacity = aOpacity;
    mixColor = aMixColor;
    mixColorRatio = aMixColorRatio;
}