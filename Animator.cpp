#include "Animator.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>


void Animator::update(
    Skeleton& skeleton,
    float deltaTime,
    float targetHeadYaw,
    float targetHeadPitch,
    float targetHeadRoll,
    bool waving)
{
    float dt = std::clamp(
        deltaTime, 0.0f, 0.05f
    );

    float smooth =
        1.0f - std::exp(-8.0f * dt);

    // 각도 제한
    targetHeadYaw = std::clamp(
        targetHeadYaw, -30.0f, 30.0f
    );

    targetHeadPitch = std::clamp(
        targetHeadPitch, -20.0f, 20.0f
    );

    targetHeadRoll = std::clamp(
        targetHeadRoll, -20.0f, 20.0f
    );

    // 부드러운 움직임
    headYaw +=
        (targetHeadYaw - headYaw) * smooth;

    headPitch +=
        (targetHeadPitch - headPitch) * smooth;

    headRoll +=
        (targetHeadRoll - headRoll) * smooth;

    float targetWave =
        waving ? 1.0f : 0.0f;

    waveBlend +=
        (targetWave - waveBlend) * smooth;

    wavePhase += dt * 3.0f;

    skeleton.resetPose();

    // Yaw: Y축 회전
    glm::quat yaw = glm::angleAxis(
        glm::radians(headYaw),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    // Pitch: X축 회전
    glm::quat pitch = glm::angleAxis(
        glm::radians(headPitch),
        glm::vec3(1.0f, 0.0f, 0.0f)
    );

    // Roll: Z축 회전
    glm::quat roll = glm::angleAxis(
        glm::radians(headRoll),
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    glm::quat headRotation =
        glm::normalize(yaw * pitch * roll);

    // VRM 표준 Humanoid 본 적용
    skeleton.setHumanoidRotation(
        "head", headRotation
    );

    // 왼팔 흔들기
    float armAngle =
        glm::radians(25.0f) *
        std::sin(wavePhase) *
        waveBlend;

    glm::quat armRotation = glm::angleAxis(
        armAngle,
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    skeleton.setHumanoidRotation(
        "leftUpperArm", armRotation
    );

    skeleton.update();
}

