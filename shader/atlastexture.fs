out vec4 FragColor;

in vec2 TexCoord;
in float opacity;
in vec3 mixColor;
in float mixColorRatio;

// texture samplers
uniform sampler2D texture0;

void main()
{
	vec4 pixelColor = texture(texture0, TexCoord);

	vec3 color = mix(pixelColor.xyz, mixColor, mixColorRatio);

	FragColor = vec4(color, pixelColor.a * opacity);
	// FragColor = vec4(pixelColor, pixelColor.a * opacity);

	// FragColor = texture(texture0, TexCoord);
}
