out vec4 FragColor;

in vec4  ourColor;
in vec2  fragUV;
in vec2  fragSize;
flat in float fragSpacing;
flat in float fragLineWidth;
flat in float fragAngle;
flat in float fragRadius;

float roundedBoxSDF(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
    vec2 p        = (fragUV - 0.5) * fragSize;
    vec2 halfSize = fragSize * 0.5;
    float radius  = min(fragRadius, min(halfSize.x, halfSize.y));

    float d    = roundedBoxSDF(p, halfSize, radius);
    float mask = 1.0 - smoothstep(0.0, 1.5, d);

    // Distance along the hatch normal, wrapped to one period.
    float a   = radians(fragAngle);
    vec2 n    = vec2(cos(a), sin(a));
    float t   = mod(dot(p, n) + 4096.0, fragSpacing);
    float dl  = min(t, fragSpacing - t) - fragLineWidth * 0.5;   // signed distance to the nearest line edge
    float aa  = fwidth(dot(p, n));
    float line = 1.0 - smoothstep(0.0, aa, dl);

    vec4 color = ourColor / 255.0;
    FragColor  = vec4(color.rgb, color.a * line * mask);
}
