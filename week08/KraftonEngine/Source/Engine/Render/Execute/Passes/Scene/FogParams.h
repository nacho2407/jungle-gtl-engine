    #pragma once

    #include "Math/Vector.h"

    /*
        높이 안개 컴포넌트가 씬에 전달하는 파라미터 구조체입니다.
        라이팅/포스트 프로세스 단계에서 그대로 사용됩니다.
    */

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
