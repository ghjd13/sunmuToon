
#version 330 core

in vec3 Normal;
in vec3 WorldPos;
in vec2 TexCoord;

out vec4 FragColor;

// 기존 텍스처 및 머터리얼
uniform sampler2D baseTexture;
uniform int hasTexture;
uniform vec4 baseColorFactor;
uniform sampler2D normalMapTexture;
uniform int hasNormalMap;
uniform float normalScale;

// MToon 그림자 텍스처
uniform sampler2D shadeTexture;
uniform int hasShadeTexture;

// 새 MToon 파라미터
uniform int hasMToon;
uniform vec4 mtoonShadeColor;
uniform float mtoonShadingShift;
uniform float mtoonShadingToony;

uniform int alphaMode;       // 0: OPAQUE, 1: MASK, 2: BLEND
uniform float alphaCutoff;   // MASK 기준값

// 월드 공간에서 표면을 향해 비추는 방향광
uniform vec4 lightDirection;


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


vec3 getShadedNormal()
{
    vec3 N = normalize(Normal);

    if (hasNormalMap == 0)
        return N;

    // 노멀맵은 선형 데이터:
    // sRGB 변환을 하지 않습니다.
    vec3 mapNormal =
        texture(normalMapTexture, TexCoord).rgb
        * 2.0 - 1.0;

    mapNormal.xy *= normalScale;

    // 화면에서 변화하는 월드 좌표와 UV
    vec3 dpdx = dFdx(WorldPos);
    vec3 dpdy = dFdy(WorldPos);

    vec2 duvdx = dFdx(TexCoord);
    vec2 duvdy = dFdy(TexCoord);

    float det =
        duvdx.x * duvdy.y -
        duvdx.y * duvdy.x;

    // UV가 퇴화된 영역은 원래 법선 사용
    if (abs(det) < 0.00000001)
        return N;

    // 접선 및 종법선 계산
    vec3 T =
        (dpdx * duvdy.y -
         dpdy * duvdx.y) / det;

    vec3 referenceB =
        (dpdy * duvdx.x -
         dpdx * duvdy.x) / det;

    // 법선과 직교하도록 보정
    T -= N * dot(N, T);

    if (dot(T, T) < 0.000000000001)
        return N;

    T = normalize(T);

    vec3 B = cross(N, T);

    // 반전된 UV에서도 방향 유지
    if (dot(B, referenceB) < 0.0)
        B = -B;

    // 접선 공간의 노멀을 월드 공간으로 변환
    mat3 TBN = mat3(T, B, N);

    return normalize(
        TBN * normalize(mapNormal)
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

    // MASK: 지정된 기준보다 투명하면 픽셀 제거
    if (alphaMode == 1 && baseColor.a < alphaCutoff)
    {
        discard;
    }

    // BLEND: 완전히 투명한 픽셀만 제거
    if (alphaMode == 2 && baseColor.a <= 0.001)
    {
        discard;
    }

    // 2. 기본 방향광
    vec3 N = getShadedNormal();
    vec3 L = normalize(lightDirection.xyz);

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

        
        // 기본값은 흰색: 텍스처가 없으면 계수만 적용
        vec3 shadeTexColor = vec3(1.0);

        if (hasShadeTexture != 0)
        {
            // 현재 Texture.cpp는 GL_RGBA8을 사용하므로
            // 셰이더에서 sRGB -> Linear 변환
            shadeTexColor = srgbToLinear(
                texture(shadeTexture, TexCoord).rgb
            );
        }

        // 공식 MToon 그림자색 계산
        vec3 shadeColor =
            shadeTexColor * mtoonShadeColor.rgb;


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

    // BLEND만 원래 알파값 사용
    float outputAlpha =
        (alphaMode == 2) ? baseColor.a : 1.0;

    FragColor = vec4(
        linearToSrgb(finalColor),
        outputAlpha
    );
}
