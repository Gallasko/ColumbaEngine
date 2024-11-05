#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in float opacity;
in vec3 mixColor;
in float mixColorRatio;

// texture samplers
uniform sampler2D texture1;
uniform int time;

void main()
{
	vec4 pixelColor = vec4(1.0, 1.0, 1.0, texture(texture1, TexCoord).r);

	vec3 color = mix(pixelColor.xyz, mixColor, mixColorRatio);

	FragColor = vec4(color, pixelColor.a);
}