#pragma once

#include "Math/Vector.h"
#include "Math/Matrix.h"

class FAxisRenderer
{
public:
    // World 좌표계 축 (항상 표시)
    void RenderWorldAxis(/* TODO: 렌더러 파라미터 */);

    // Local 좌표계 축 (선택된 오브젝트 기준)
    void RenderLocalAxis(const FVector& Position, const FMatrix& RotationMatrix);

    // 축 색상: X=빨강(1,0,0), Y=초록(0,1,0), Z=파랑(0,0,1) — UE5 기준
};
