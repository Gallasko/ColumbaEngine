out vec4 FragColor;

in vec4  ourColor;
in vec2  fragUV;
in vec2  fragSize;
in float fragRadius;

// Signed distance function for a rounded rectangle.
// p        : fragment position relative to rect center (same units as halfSize/radius)
// halfSize : half-extents of the rectangle
// radius   : corner radius
float roundedBoxSDF(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
    // Map UV [0,1] to coordinates centered on the rectangle, in pixel units.
    vec2 p        = (fragUV - 0.5) * fragSize;
    vec2 halfSize = fragSize * 0.5;

    // Clamp radius so it never exceeds half the smallest dimension.
    float radius = min(fragRadius, min(halfSize.x, halfSize.y));

    float d = roundedBoxSDF(p, halfSize, radius);

    // Smooth the edge over ~1.5 pixels for anti-aliasing.
    float alpha = 1.0 - smoothstep(0.0, 1.5, d);

    vec4 color = ourColor / 255.0;
    FragColor = vec4(color.rgb, color.a * alpha);
}
