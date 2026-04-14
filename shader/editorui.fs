in vec4 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor.rgb / 255.0, vColor.a / 255.0);
}
