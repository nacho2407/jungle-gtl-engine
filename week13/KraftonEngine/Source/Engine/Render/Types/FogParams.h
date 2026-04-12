#pragma once

#include "Math/Vector.h"

struct FFogParams
{
	float Density          = 0.02f;
	float HeightFalloff    = 0.2f;
	float StartDistance    = 0.0f;
	float CutoffDistance   = 0.0f;   // 0 = 무제한
	float MaxOpacity       = 1.0f;
	float FogBaseHeight    = 0.0f;   // 컴포넌트 WorldZ
	FVector4 InscatteringColor = FVector4(0.45f, 0.55f, 0.65f, 1.0f);
};
