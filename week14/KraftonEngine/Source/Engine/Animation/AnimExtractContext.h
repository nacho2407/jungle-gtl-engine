#pragma once

#include "Core/CoreTypes.h"

// AnimSequence 에서 특정 시점의 포즈를 뽑을 때 전달되는 입력.
struct FAnimExtractContext
{
	float CurrentTime        = 0.0f;
	bool  bLooping           = true;
	bool  bExtractRootMotion = false; // (선택) 추후 Root Motion 확장 여지
};
