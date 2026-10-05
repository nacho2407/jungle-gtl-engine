#include "CameraModifier.h"

bool UCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutView)
{
    // Alpha 업데이트
    UpdateAlpha(DeltaTime);

    // 완전히 꺼진 상태면 영향 없음
    if (Alpha <= 0.f)
        return false;

    // 실제 효과 적용 (자식 클래스에서 override)
    return ApplyCamera(DeltaTime, InOutView);
}

void UCameraModifier::UpdateAlpha(float DeltaTime)
{
    if (bPendingDisable)
    {
        // Fade Out
        if (AlphaOutTime > 0.f)
            Alpha -= DeltaTime / AlphaOutTime;
        else
            Alpha = 0.f;

        if (Alpha <= 0.f)
        {
            Alpha = 0.f;
            bDisabled = true;
        }
    }
    else
    {
        // Fade In
        if (AlphaInTime > 0.f)
            Alpha += DeltaTime / AlphaInTime;
        else
            Alpha = 1.f;

        Alpha = std::min(Alpha, 1.f);
    }
}
