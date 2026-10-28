
#include "Animator.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <algorithm>

void Animator::update(
    Skeleton& skeleton,
    float deltaTime,
    float targetHeadYaw,
    bool waving)
{
    // 프레임 시간 안정화
    float dt = std::clamp(
        deltaTime, 0.0f, 0.05f
    );

    // 부드러운 보간
    float smooth = 1.0f - std::exp(-8.0f * dt);

    targetHeadYaw = std::clamp(
        targetHeadYaw, -30.0f, 30.0f
    );

    headYaw += (targetHeadYaw - headYaw) * smooth;

    float targetWave = waving ? 1.0f : 0.0f;

    waveBlend +=
        (targetWave - waveBlend) * smooth;

    wavePhase += dt * 3.0f;

    // 기본 자세로 초기화
    skeleton.resetPose();

    // 머리 좌우 회전
    glm::quat headRotation = glm::angleAxis(
        glm::radians(headYaw),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    skeleton.setRotation(
        "J_Bip_C_Head", headRotation
    );

    // 왼팔 움직임
    float armAngle =
        glm::radians(25.0f) *
        std::sin(wavePhase) *
        waveBlend;

    glm::quat armRotation = glm::angleAxis(
        armAngle,
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    skeleton.setRotation(
        "J_Bip_L_UpperArm", armRotation
    );

    // 변경된 자세 반영
    skeleton.update();
}
