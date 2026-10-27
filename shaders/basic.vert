
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

layout (location = 3) in uvec4 aJoints;
layout (location = 4) in vec4 aWeights;

out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform int useSkinning;

// OpenGL 3.3 Uniform Buffer
layout(std140) uniform SkinMatrices
{
    mat4 jointMatrices[128];
};

void main()
{
    mat4 skinMatrix = mat4(1.0);

    if (useSkinning != 0)
    {
        float sumWeights = dot(
            aWeights,
            vec4(1.0)
        );

        if (sumWeights > 0.00001)
        {
            skinMatrix =
                aWeights.x *
                    jointMatrices[int(aJoints.x)] +
                aWeights.y *
                    jointMatrices[int(aJoints.y)] +
                aWeights.z *
                    jointMatrices[int(aJoints.z)] +
                aWeights.w *
                    jointMatrices[int(aJoints.w)];

            skinMatrix /= sumWeights;
        }
    }

    vec4 skinnedPosition =
        skinMatrix * vec4(aPos, 1.0);

    vec4 worldPosition =
        model * skinnedPosition;

    gl_Position =
        projection * view * worldPosition;

    mat3 normalMatrix =
        transpose(inverse(mat3(model)));

    Normal = normalize(
        normalMatrix *
        mat3(skinMatrix) *
        aNormal
    );

    TexCoord = aTexCoord;
}
