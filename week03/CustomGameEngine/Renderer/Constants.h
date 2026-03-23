#pragma once
#include "Math/Matrix.h"
#include "EngineCore/Color.h"

struct FObjectConstant
{
	FMatrix World;
	FColor Color;
	int bIsSelected;
	float Padding[3];
};

/**
 * @brief ShaderFont를 위한 구조체
 */
struct FFontConstant
{
    FVector CameraRight{};
    float Padding0 = 0.0f;

    FVector CameraUp{};
    float Padding1 = 0.0f;

    FColor TextColor{};
};
