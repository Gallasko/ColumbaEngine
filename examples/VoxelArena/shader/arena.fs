in vec3 vColor;
in vec3 vNormal;
in float vAo;
in vec4 vTint;
in vec4 vFlash;
in float vDepth;
in vec3 vLocal;

uniform int uMode;   // 0 lit voxels, 1 sky, 2 unlit (debug boxes)

out vec4 FragColor;

const vec3 SUN = normalize(vec3(0.35, 0.85, -0.4));
const vec3 SKY_TOP = vec3(0.36, 0.58, 0.86);
const vec3 SKY_HORIZON = vec3(0.78, 0.86, 0.93);
const vec3 GROUND_BOUNCE = vec3(0.45, 0.40, 0.33);
const float FOG_START = 40.0;
const float FOG_END = 140.0;

void main()
{
    if (uMode == 1)
    {
        // The sky box is a unit cube scaled around the camera: its local height gives the gradient
        float h = clamp(vLocal.y * 2.0 - 1.0, -1.0, 1.0);
        vec3 sky = mix(SKY_HORIZON, SKY_TOP, smoothstep(0.0, 0.7, h));
        sky = mix(sky, SKY_HORIZON * 0.9, smoothstep(0.0, -0.5, h));
        FragColor = vec4(sky, 1.0);
        return;
    }

    vec3 base = vColor * vTint.rgb;

    if (uMode == 2)
    {
        FragColor = vec4(mix(base, vFlash.rgb, vFlash.a), vTint.a);
        return;
    }

    vec3 n = normalize(vNormal);

    float sun = max(dot(n, SUN), 0.0);
    // Sky light from above, a warm bounce from the ground below
    vec3 ambient = mix(GROUND_BOUNCE, SKY_TOP, n.y * 0.5 + 0.5) * 0.7 + vec3(0.08);
    vec3 light = ambient * (0.35 + 0.65 * vAo) + vec3(1.0, 0.96, 0.88) * sun * 0.7 * (0.6 + 0.4 * vAo);

    vec3 color = base * light;
    color = mix(color, vFlash.rgb, vFlash.a);

    float fog = smoothstep(FOG_START, FOG_END, vDepth);
    color = mix(color, SKY_HORIZON, fog);

    FragColor = vec4(color, vTint.a);
}
