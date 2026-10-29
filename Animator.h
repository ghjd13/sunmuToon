
#pragma once

#include "Skeleton.h"

class Animator
{
public:
    void update(
        Skeleton& skeleton,
        float deltaTime,
        float targetHeadYaw,
        float targetHeadPitch,
        float targetHeadRoll,
        bool waving
    );

private:
    float headYaw = 0.0f;
    float headPitch = 0.0f;
    float headRoll = 0.0f;

    float waveBlend = 0.0f;
    float wavePhase = 0.0f;
};
