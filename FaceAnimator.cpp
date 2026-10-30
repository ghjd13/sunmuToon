
#include "FaceAnimator.h"

#include <algorithm>
#include <cmath>

namespace
{
    // 0~1 값을 부드러운 곡선으로 변환
    float smoothStep(float value)
    {
        float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // 프레임 속도에 크게 의존하지 않는 보간
    void smoothApproach(
        float& current,
        float target,
        float speed,
        float dt)
    {
        float alpha = 1.0f - std::exp(-speed * dt);

        current += (target - current) * alpha;

        // 충분히 가까우면 정확한 목표값으로 고정
        if (std::abs(current - target) < 0.001f)
        {
            current = target;
        }
    }
}

FaceAnimator::FaceAnimator()
    : rng(std::random_device{}())
{
    nextBlinkInterval =
        blinkIntervalDistribution(rng);
}

FaceWeights FaceAnimator::update(
    float deltaTime,
    bool blinkPressed,
    bool mouthPressed,
    bool happyPressed,
    float microphoneMouthWeight)
{
    // 프레임 지연으로 인한 급격한 변화 방지
    const float dt = std::clamp(
        deltaTime, 0.0f, 0.05f
    );

    // ===== 1. 수동 표정 보간 =====

    smoothApproach(
        manualBlinkWeight,
        blinkPressed ? 1.0f : 0.0f,
        18.0f,
        dt
    );


    // 마이크 음량을 유효 범위로 제한
    microphoneMouthWeight = std::clamp(
        microphoneMouthWeight,
        0.0f,
        1.0f
    );

    // A키를 누르면 수동으로 완전히 입을 벌림
    // 그렇지 않으면 마이크 음량을 사용
    const float targetMouth = std::max(
        mouthPressed ? 1.0f : 0.0f,
        microphoneMouthWeight
    );

    // 기존 보간 기능 유지
    smoothApproach(
        mouthWeight,
        targetMouth,
        10.0f,
        dt
    );


    smoothApproach(
        happyWeight,
        happyPressed ? 1.0f : 0.0f,
        8.0f,
        dt
    );

    // ===== 2. 자동 눈 깜빡임 =====

    float automaticBlink = 0.0f;

    if (!blinking)
    {
        elapsedSinceBlink += dt;

        if (elapsedSinceBlink >= nextBlinkInterval)
        {
            blinking = true;
            blinkPhase = 0.0f;
            elapsedSinceBlink = 0.0f;
        }
    }

    if (blinking)
    {
        blinkPhase += dt;

        constexpr float CloseDuration = 0.08f;
        constexpr float OpenDuration = 0.13f;
        constexpr float TotalDuration =
            CloseDuration + OpenDuration;

        if (blinkPhase < CloseDuration)
        {
            // 눈 감는 단계
            float t = blinkPhase / CloseDuration;
            automaticBlink = smoothStep(t);
        }
        else if (blinkPhase < TotalDuration)
        {
            // 눈 뜨는 단계
            float t =
                (blinkPhase - CloseDuration) /
                OpenDuration;

            automaticBlink = 1.0f - smoothStep(t);
        }
        else
        {
            // 이번 깜빡임 종료
            blinking = false;
            blinkPhase = 0.0f;

            // 다음 깜빡임까지의 간격 재설정
            nextBlinkInterval =
                blinkIntervalDistribution(rng);
        }
    }

    // ===== 3. 최종 표정 가중치 =====

    FaceWeights result;

    // 수동/자동 중 더 많이 감긴 눈 상태 적용
    result.blink = std::max(
        manualBlinkWeight,
        automaticBlink
    );

    result.mouth = mouthWeight;
    result.happy = happyWeight;

    return result;
}
