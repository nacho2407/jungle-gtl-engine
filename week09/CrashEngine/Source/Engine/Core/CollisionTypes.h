// 엔진 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Math/Vector.h" // 필요한 최소한의 수학 라이브러리만

struct FOverlapInfo
{

};

// FHitResult는 엔진 처리에 필요한 데이터를 묶는 구조체입니다.
struct FHitResult
{
    class UPrimitiveComponent* HitComponent = nullptr;

    float Distance = 3.402823466e+38F; // FLT_MAX
    FVector WorldHitLocation = { 0, 0, 0 };
    FVector WorldNormal = { 0, 0, 0 };
    int FaceIndex = -1;

    bool bHit = false;
};
