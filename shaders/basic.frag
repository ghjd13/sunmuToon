
#version 330 core

in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

// 기존 텍스처 및 머터리얼
uniform sampler2D baseTexture;
uniform int hasTexture;
uniform vec4 baseColorFactor;

// 새 MToon 파라미터
uniform int hasMToon;
uniform vec4 mtoonShadeColor;
uniform float mtoonShadingShift;
uniform float mtoonShadingToony;


vec3 srgbToLinear(vec3 color)
{
    color = clamp(color, 0.0, 1.0);

    vec3 low = color / 12.92;

    vec3 high = pow(
        (color + 0.055) / 1.055,
        vec3(2.4)
    );

    return mix(
        low,
        high,
        step(vec3(0.04045), color)
    );
}

vec3 linearToSrgb(vec3 color)
{
    color = clamp(color, 0.0, 1.0);

    vec3 low = color * 12.92;

    vec3 high =
        1.055 * pow(color, vec3(1.0 / 2.4))
        - 0.055;

    return mix(
        low,
        high,
        step(vec3(0.0031308), color)
    );
}


void main()
{
    // 1. 기본 텍스처
    vec4 texColor = vec4(1.0);

    if (hasTexture != 0)
    {
        texColor = texture(baseTexture, TexCoord);
    }

    vec4 baseColor = vec4(
        srgbToLinear(texColor.rgb) * baseColorFactor.rgb,
        texColor.a * baseColorFactor.a
    );

    if (baseColor.a < 0.05)
        discard;

    // 2. 기본 방향광
    vec3 N = normalize(Normal);
    vec3 L = normalize(vec3(-0.3, 0.8, 0.6));

    float NdotL = dot(N, L);

    vec3 finalColor;

    if (hasMToon != 0)
    {
        // ===== VRM 1.0 MToon 기본 명암 =====

        // 명암 경계 위치 조정
        float shading =
            NdotL + mtoonShadingShift;

        // Toony 1.0 = 매우 날카로운 경계
        // Toony 0.0 = 넓고 부드러운 경계
        float toony = clamp(
            mtoonShadingToony,
            0.0,
            1.0
        );

        float lower = -1.0 + toony;
        float upper =  1.0 - toony;

        float lighting;

        if (upper - lower < 0.0001)
        {
            // 폭이 0에 가까운 경계
            lighting = step(0.0, shading);
        }
        else
        {
            // MToon의 linearstep 방식
            lighting = clamp(
                (shading - lower) /
                (upper - lower),
                0.0,
                1.0
            );
        }

        // 현재는 shadeMultiplyTexture 미지원
        // 따라서 그림자색 계수만 사용
        // 임시 보정: 기본 텍스처를 그림자 텍스처 대신 사용
        vec3 shadeColor =
            baseColor.rgb * mtoonShadeColor.rgb;

        finalColor = mix(
            shadeColor,
            baseColor.rgb,
            lighting
        );
    }
    else
    {
        // ===== 기존 툰 셰이더 유지 =====

        float shadeThreshold = 0.05;
        float shadeSoftness = 0.04;

        float lighting = smoothstep(
            shadeThreshold - shadeSoftness,
            shadeThreshold + shadeSoftness,
            NdotL
        );

        vec3 shadeColor =
            baseColor.rgb *
            vec3(0.65, 0.70, 0.80);

        finalColor = mix(
            shadeColor,
            baseColor.rgb,
            lighting
        );
    }

    FragColor = vec4(
        linearToSrgb(finalColor),
        baseColor.a
    );
}
