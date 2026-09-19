out vec4 FragColor;

in vec4  ourColor;
in vec2  fragUV;
in vec2  fragSize;
flat in float fragStrokeWidth;
flat in float fragGap;
flat in float fragDoubled;
flat in float fragRadius;

float roundedBoxSDF(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

// Coverage of the band  -outer <= d <= -inner  (both measured inward from the edge).
float ring(float d, float inner, float outer)
{
    float lo = 1.0 - smoothstep(0.0, 1.5, d + inner);   // inside of the band
    float hi = smoothstep(0.0, 1.5, d + outer);          // outside of the band
    return lo * hi;
}

void main()
{
    vec2 p        = (fragUV - 0.5) * fragSize;
    vec2 halfSize = fragSize * 0.5;
    float radius  = min(fragRadius, min(halfSize.x, halfSize.y));
    float d       = roundedBoxSDF(p, halfSize, radius);

    float w = fragStrokeWidth;
    float coverage = ring(d, 0.0, w);
    if (fragDoubled > 0.5)
        coverage = max(coverage, ring(d, w + fragGap, 2.0 * w + fragGap));

    vec4 color = ourColor / 255.0;
    FragColor  = vec4(color.rgb, color.a * coverage);
}
