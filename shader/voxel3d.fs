in vec4 ourColor;
in vec3 vWorldPos;

out vec4 FragColor;

void main()
{
    // Cheap face shading from screen-space derivatives of the world position.
    // Gives each cube face a distinct shade without any normals or lights.
    vec3 dx = dFdx(vWorldPos);
    vec3 dy = dFdy(vWorldPos);
    vec3 n  = normalize(cross(dx, dy));

    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.5));
    float ndotl = clamp(abs(dot(n, lightDir)), 0.0, 1.0);
    float diffuse = 0.35 + 0.65 * ndotl;

    vec3 rgb = ourColor.rgb / 255.0;
    float a  = ourColor.a   / 255.0;

    FragColor = vec4(rgb * diffuse, a);
}
