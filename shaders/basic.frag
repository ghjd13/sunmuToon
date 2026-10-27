
#version 330 core

in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D baseTexture;
uniform int hasTexture;
uniform vec4 baseColorFactor;

void main()
{
    vec4 color = baseColorFactor;

    if (hasTexture != 0)
    {
        color *= texture(baseTexture, TexCoord);
    }

    // 기본 방향광
    vec3 lightDir = normalize(
        vec3(0.4, 0.8, 0.6)
    );

    vec3 N = normalize(Normal);

    float diffuse = max(
        dot(N, lightDir),
        0.0
    );

    float lighting = 0.35 + diffuse * 0.65;

    FragColor = vec4(
        color.rgb * lighting,
        color.a
    );
}
