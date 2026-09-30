// 엔진 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Math/Vector.h"

// FRay는 충돌, 피킹, 공간 가속 처리를 담당합니다.
struct FRay
{
    FVector Origin;
    FVector Direction;
};
