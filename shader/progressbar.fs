#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

in float fillRatio;
in float direction;

// texture samplers
uniform sampler2D texture0;
uniform sampler2D texture1;

void main()
{
	vec4 pixelColor;

	if (direction == 0.0f)
	{
		pixelColor = TexCoord.x > fillRatio ? texture(texture0, TexCoord) : texture(texture1, TexCoord);
	}
	else if (direction == 1.0f)
	{
		pixelColor = TexCoord.y > fillRatio ? texture(texture0, TexCoord) : texture(texture1, TexCoord);
	}
	else
	{
		pixelColor = texture(texture0, TexCoord);
	}

	FragColor = pixelColor;
}