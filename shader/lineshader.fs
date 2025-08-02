out vec4 FragColor;

in vec2 TexCoord;

// texture samplers
uniform sampler2D texture0;

void main()
{
	vec4 pixelColor = texture(texture0, TexCoord);

	FragColor = pixelColor;
}