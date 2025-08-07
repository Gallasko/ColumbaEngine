out vec4 FragColor;
in vec4 ourColor;

void main()
{
    FragColor = vec4(ourColor.x / 255.0, ourColor.y / 255.0, ourColor.z / 255.0, ourColor.w / 255.0);
    // FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}