out vec4 FragColor;

in vec2 TexCoord;
in float visible;

// texture samplers
uniform sampler2D texture1;

void main()
{
    if(visible <= 0.0f)
        discard;

	//FragColor = vec4(color, 1.0f);
    FragColor = texture(texture1, TexCoord);
}