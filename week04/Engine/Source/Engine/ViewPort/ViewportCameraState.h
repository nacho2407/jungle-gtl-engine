#pragma once
#include "Core/CoreMinimal.h"

struct FViewportCameraState
{
    FVector Location    = FVector::Zero();
    FQuat   Rotation    = FQuat::Identity;
    float   FOV         = 3.141592f * 0.5f;
    float   NearPlane   = 0.1f;
    float   FarPlane    = 2000.0f;
    float   OrthoHeight = 100.0f;
};
