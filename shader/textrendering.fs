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
	vec4 pixelColor = texture(texture1, TexCoord);

	// vec4 pixelColor = vec4(TexCoord.x, TexCoord.y, 0.0f, 1.0f);
	// vec4 pixelColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);

	// // Branchless color picking based on alpha
	// pixelColor = mainColor * float(pixelColor.a == (255.0 / 255.0)) +
	// 			 outline1 * float(pixelColor.a == (180.0 / 255.0)) +
	// 			 outline2 * float(pixelColor.a == (205.0 / 255.0));

	if(pixelColor.a == (255.0 / 255.0)) // Charactere Color
	{
		//pixelColor = vec4(abs(sin(time / 1000.0)), 0.0, 0.0, 1.0);
		pixelColor = mainColor / 255.0;
		// pixelColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
	}
	else if (pixelColor.a == (205.0 / 255.0)) // First Outline
	{
		pixelColor = outline1 / 255.0;
		//pixelColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
	}
	else if (pixelColor.a == (180.0 / 255.0)) // Second Outline
	{
		pixelColor = outline2 / 255.0;
		// pixelColor = vec4(0.0f, 0.0f, 1.0f, 1.0f);
	}
	else
	{
		discard;
	}


	FragColor = pixelColor;

}