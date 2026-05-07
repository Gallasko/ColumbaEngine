out vec4 FragColor;

in vec4 ourColor;
in vec2 fragUV;
in vec2 fragSize;

void main()
{
    float W = fragSize.x;
    float H = fragSize.y;
    float px = fragUV.x * W;
    float py = fragUV.y * H;

    float invSlant = inversesqrt(4.0 * H * H + W * W);
    float dLeft  = -(2.0 * H * px + W * py - W * H) * invSlant;
    float dRight =  (2.0 * H * px - W * py - W * H) * invSlant;
    float dBase  =  py - H;

    float sdf = max(max(dLeft, dRight), dBase);

    float alpha = 1.0 - smoothstep(0.0, 1.5, sdf);

    vec4 color = ourColor / 255.0;
    FragColor = vec4(color.rgb, color.a * alpha);
}
