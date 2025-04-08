#version 330 core

out vec4 FragColor;

in vec2 finalUV;
in float opacity;
in vec3 mixColor;
in float mixColorRatio;

uniform sampler2D texture1;  // The atlas texture

void main()
{
	// Sample from the atlas using finalUV.
    vec4 sampleColor = texture(texture1, finalUV);
    // The atlas stores glyphs in the red channel, so we use .r to determine opacity.
    vec4 pixelColor = vec4(1.0, 1.0, 1.0, sampleColor.r);

    // Mix the pixel color with a provided mixColor.
    vec3 color = mix(pixelColor.rgb, mixColor, mixColorRatio);

    // The final color uses the computed alpha.
    FragColor = vec4(color, pixelColor.a);
}
