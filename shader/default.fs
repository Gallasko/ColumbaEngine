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
}

// #version 330 core

// out vec4 FragColor;
// // in vec3 ourColor;

// void main()
// {
//     // FragColor = vec4(ourColor, 1.0);
//     // FragColor = vec4(ourColor.x / 255.0, ourColor.y / 255.0, ourColor.z / 255.0, 1.0);
//     FragColor = vec4(0.0, 1.0, 0.0, 1.0);
// }