
#pragma once

#include <random>

// 렌더러에 전달할 표정 가중치
struct FaceWeights
{
    float blink = 0.0f;
    float mouth = 0.0f;
    float happy = 0.0f;
};

class FaceAnimator
{
public:
    FaceAnimator();

    FaceWeights update(
        float deltaTime,
        bool blinkPressed,
        bool mouthPressed,
        bool happyPressed
    );

private:
    // 수동 입력의 부드러운 보간값
    float manualBlinkWeight = 0.0f;
    float mouthWeight = 0.0f;
    float happyWeight = 0.0f;

    // 자동 눈 깜빡임 상태
    float elapsedSinceBlink = 0.0f;
    float nextBlinkInterval = 3.5f;
    float blinkPhase = 0.0f;
    bool blinking = false;

    // 깜빡임 간격에 약간의 변화 부여
    std::mt19937 rng;
    std::uniform_real_distribution<float>
        blinkIntervalDistribution{ 2.8f, 5.0f };
};
