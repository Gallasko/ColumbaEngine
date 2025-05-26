layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

layout (location = 2) in float aVisible;
layout (location = 3) in vec3 aTransform;
layout (location = 4) in float aTexOffset;
layout (location = 5) in float aParticleScale;

out vec2 TexCoord;
out float visible;

uniform mat4 model;
uniform mat4 scale;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * scale * model * vec4((aParticleScale * aPos) + aTransform, 1.0f);
	TexCoord = vec2(aTexCoord.x + aTexOffset, aTexCoord.y);
    visible = aVisible;
}
