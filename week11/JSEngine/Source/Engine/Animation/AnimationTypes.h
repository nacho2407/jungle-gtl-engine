#pragma once

#include "Core/CoreTypes.h"

enum class EAnimationMode : uint8
{
    None,
    SingleNode,
    AnimInstance,
};

/**
 * @brief animation asset에서 특정 시점의 pose를 뽑아낸 구조체
 */
struct FAnimExtractContext
{
    float CurrentTime = 0.0f;
    bool bLooping = false;

    FAnimExtractContext() = default;
    FAnimExtractContext(float InCurrentTime, bool bInLooping)
        : CurrentTime(InCurrentTime)
        , bLooping(bInLooping)
    {
    }
};
