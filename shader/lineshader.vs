layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 scale;
uniform mat4 view;
uniform mat4 projection;

uniform float sWidth;
uniform float sHeight;

out vec2 TexCoord;

void main()
{
    mat4 posMat = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(-1.0f + 2.0 * aPos.x * (1.0 / sWidth), 1.0 + 2.0 * -(aPos.y) * (1.0 / sHeight), 2.0f, 1.0) );

    gl_Position = projection * posMat * view * scale * model * vec4(1.0f);

	TexCoord = aTexCoord;
}