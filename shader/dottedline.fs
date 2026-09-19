out vec4 FragColor;

in vec4  ourColor;
in vec2  fragUV;
in vec2  fragSize;
flat in float fragPeriod;
flat in float fragDotRadius;

void main()
{
    vec2 p = (fragUV - 0.5) * fragSize;                // centred, pixels
    float x = mod(p.x + fragSize.x * 0.5, fragPeriod); // 0..period along the line
    vec2 c  = vec2(x - fragPeriod * 0.5, p.y);         // offset from the nearest dot centre
    float d = length(c) - fragDotRadius;
    float aa = fwidth(length(c));
    float dot = 1.0 - smoothstep(0.0, aa, d);

    vec4 color = ourColor / 255.0;
    FragColor  = vec4(color.rgb, color.a * dot);
}
