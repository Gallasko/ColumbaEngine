// Voxel meshes of the arena: the world, the fighters, the weapons, the sky and the debug boxes.
// Per vertex: position, normal, colour, ambient occlusion.
// Per instance: model matrix, view * model (the camera of the frame this instance was built for), tint, flash.

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in float aAo;

layout (location = 4) in vec4 iModel0;
layout (location = 5) in vec4 iModel1;
layout (location = 6) in vec4 iModel2;
layout (location = 7) in vec4 iModel3;
layout (location = 8) in vec4 iEye0;
layout (location = 9) in vec4 iEye1;
layout (location = 10) in vec4 iEye2;
layout (location = 11) in vec4 iEye3;
layout (location = 12) in vec4 iTint;   // rgb multiplies the colour, a is the opacity
layout (location = 13) in vec4 iFlash;  // rgb is mixed in by a (hit flash, wind-up glow)

uniform mat4 projection;

out vec3 vColor;
out vec3 vNormal;
out float vAo;
out vec4 vTint;
out vec4 vFlash;
out float vDepth;
out vec3 vLocal;

void main()
{
    mat4 model = mat4(iModel0, iModel1, iModel2, iModel3);
    vec4 eye = mat4(iEye0, iEye1, iEye2, iEye3) * vec4(aPos, 1.0);

    vColor = aColor;
    vNormal = normalize(mat3(model) * aNormal);
    vAo = aAo;
    vTint = iTint;
    vFlash = iFlash;
    vDepth = -eye.z;
    vLocal = aPos;

    gl_Position = projection * eye;
}
