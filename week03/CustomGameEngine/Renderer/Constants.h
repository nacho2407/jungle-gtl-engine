#pragma once
#include "Math/Matrix.h"
#include "EngineCore/Color.h"
#include "Math/Vector4.h"

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

struct FSubUVConstant
{
	FVector4 UVTransform = FVector4(1.0f, 1.0f, 0.0f, 0.0f);
	float Padding[3];
};