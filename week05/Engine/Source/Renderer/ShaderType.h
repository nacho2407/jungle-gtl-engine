#pragma once
#include "Math/Matrix.h"

// b0: 프레임당 1회 업데이트 (카메라)
struct FFrameConstantBuffer
{
	FMatrix View;
	FMatrix Projection;
};

// b1: 오브젝트당 업데이트
struct FObjectConstantBuffer
{
	FMatrix World;
};
