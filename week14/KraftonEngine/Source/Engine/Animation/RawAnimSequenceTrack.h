#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

// 한 본(bone)에 대한 키프레임 raw 데이터.
// PosKeys/RotKeys/ScaleKeys 는 시간축에 균등 간격으로 샘플링된 값이라고 가정한다.
// 키 개수가 1 이면 정적 transform, 0 이면 해당 채널은 ref pose 사용.
struct FRawAnimSequenceTrack
{
	TArray<FVector> PosKeys;
	TArray<FQuat>   RotKeys;
	TArray<FVector> ScaleKeys;
};
