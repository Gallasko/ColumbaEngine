#version 330 core
layout (location = 0) in vec3 aPos;

layout (location = 1) in vec3 aWorldPos;
layout (location = 2) in vec4 aTexCoord;
layout (location = 3) in vec2 aSize;

layout (location = 4) in vec4 aMainColor;
layout (location = 5) in vec4 aOutline1;
layout (location = 6) in vec4 aOutline2;
layout (location = 7) in float aEffect;

out vec2 TexCoord;

out vec4 mainColor;
out vec4 outline1;
out vec4 outline2;
out float effect;

out vec4 glPosition;

uniform mat4 model;
uniform mat4 scale;
uniform mat4 view;
uniform mat4 projection;

uniform float sWidth;
uniform float sHeight;

// Size of the texture (width, height)
uniform float tWidth;
uniform float tHeight;

void main()
{
	mat4 posMat = mat4(
        vec4( 1.0, 0.0, 0.0, 0.0),
        vec4( 0.0, 1.0, 0.0, 0.0),
        vec4( 0.0, 0.0, 1.0, 0.0),
        vec4( -1.0f + 2.0 * aWorldPos.x * (1.0 / sWidth), 1.0 + 2.0 * -(aWorldPos.y) * (1.0 / sHeight), 0.0, 1.0) );

    mat4 scaleMat = mat4(
        vec4( aSize.x, 0.0, 0.0, 0.0),
        vec4( 0.0, aSize.y, 0.0, 0.0),
        vec4( 0.0, 0.0, 1.0, 0.0),
        vec4( 0.0 , 0.0, 0.0, 1.0) );

	//gl_Position = projection * posMat * view * scale * scaleMat * model * vec4(aPos.x, aPos.y, aPos.z, 1.0f);
	//gl_Position =  view * scaleMat * vec4(aPos.x, aPos.y, aPos.z, 1.0f);
	gl_Position =  view * vec4(aPos.x, aPos.y, aPos.z, 1.0f);

	TexCoord = vec2((1 - aPos.x) * aTexCoord.x  + aPos.x * aTexCoord.z, (1 + aPos.y) * aTexCoord.y  + (-1.0f * aPos.y) * aTexCoord.a);

	mainColor = aMainColor;
	outline1 = aOutline1;
	outline2 = aOutline2;
	effect = aEffect;
}