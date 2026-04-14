// Editor UI overlay shader — screen-space, always on top.
// Instance VBO layout (SimpleSquareMesh with attributes {2,2,4}):
//   location 1: vec2 aPixelPos   — top-left corner in pixels
//   location 2: vec2 aPixelSize  — width/height in pixels
//   location 3: vec4 aColor      — RGBA 0..255
//
// Vertex VBO layout (SimpleSquareMesh):
//   location 0: vec3 aPos        — unit quad: x in [0,1], y in [0,-1], z = 1

layout (location = 0) in vec3 aPos;

layout (location = 1) in vec2 aPixelPos;
layout (location = 2) in vec2 aPixelSize;
layout (location = 3) in vec4 aColor;

uniform vec2 uScreenSize;

out vec4 vColor;

void main()
{
    // Map quad vertex to pixel position, then to NDC.
    // aPos.x in [0,1], aPos.y in [0,-1].
    vec2 pixel = aPixelPos + vec2(aPos.x, -aPos.y) * aPixelSize;

    float ndcX = -1.0 + 2.0 * pixel.x / uScreenSize.x;
    float ndcY =  1.0 - 2.0 * pixel.y / uScreenSize.y;

    // z = -1 (near plane) so UI always passes GL_LESS depth test.
    gl_Position = vec4(ndcX, ndcY, -1.0, 1.0);

    vColor = aColor;
}
