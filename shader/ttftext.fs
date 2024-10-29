#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

in vec4 mainColor;
in vec4 outline1;
in vec4 outline2;
in float effect;

// texture samplers
uniform sampler2D texture1;
uniform int time;

void main()
{
	vec4 pixelColor = vec4(1.0, 1.0, 1.0, texture(texture1, TexCoord).r);

	FragColor = pixelColor;
}