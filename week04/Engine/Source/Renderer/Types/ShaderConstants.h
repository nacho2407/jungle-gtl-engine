#pragma once

#include "Core/Math/Matrix.h"
#include "Core/Math/Color.h"
#include "Core/Math/Vector4.h"

// 1. 단순 메쉬/위젯용 (기본)
struct alignas(16) FMeshUnlitConstants
{
    FMatrix MVP;
    FColor  BaseColor;
};

// 2. 인스턴싱 메쉬용 (VP 행렬만 공유)
struct alignas(16) FMeshUnlitInstancedConstants
{
    FMatrix VP;
};

// 3. 메쉬 전용 (라이팅 + UV 스크롤 + 월드 행렬)
struct alignas(16) FMeshLitConstants
{
    FMatrix MVP;
    FMatrix World;
    FColor  BaseColor;
    uint32  bEnableLighting;
    float   Time;
    float   ScrollSpeedX;
    float   ScrollSpeedY;
};

struct alignas(16) FLineConstants
{
    FMatrix VP;
    float   CameraPos[3];
    float   MaxDistance;
};

struct alignas(16) FFontConstants
{
    FMatrix VP;
    FColor  TintColor;
};

struct alignas(16) FSpriteConstants
{
    FMatrix VP;
};
