#pragma once
#include "Math/Vector.h" // 필요한 최소한의 수학 라이브러리만

struct FHitResult 
{
    class UPrimitiveComponent* HitComponent = nullptr;

    float Distance = 3.402823466e+38F; // FLT_MAX
    FVector WorldHitLocation = { 0, 0, 0 };
    FVector WorldNormal = { 0, 0, 0 };
    int FaceIndex = -1; 

    bool bHit = false;
};