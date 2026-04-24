// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Math/Vector.h"

// FFogParams는 컴포넌트 처리에 필요한 데이터를 묶는 구조체입니다.
struct FFogParams
{
    float    Density           = 0.02f;
    float    HeightFalloff     = 0.2f;
    float    StartDistance     = 0.0f;
    float    CutoffDistance    = 0.0f;
    float    MaxOpacity        = 1.0f;
    FVector4 InscatteringColor = FVector4(0.45f, 0.55f, 0.65f, 1.0f);
};
