
#pragma once

#include "Skeleton.h"

class Animator
{
public:
    void update(
        Skeleton& skeleton,
        float deltaTime,
        float targetHeadYaw,
        bool waving
    );

private:
    float headYaw = 0.0f;
    float waveBlend = 0.0f;
    float wavePhase = 0.0f;
};
