#pragma once
#include "Object/Object.h"
#include "Component/CameraComponent.h"

class APlayerCameraManager;

class UCameraModifier : public UObject
{
public:
    virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutView);

    void Enable()
    {
        bDisabled = false;
        bPendingDisable = false;
    }

    void Disable()
    {
        bPendingDisable = true; // 바로 꺼지지 않고 fade out
    }

    bool IsDisabled() const { return bDisabled && Alpha <= 0.f; }

    void SetOwner(APlayerCameraManager* InOwner)
    {
        CameraOwner = InOwner;
    }

	uint8 GetPriority() const { return Priority; }

protected:
    virtual bool ApplyCamera(float DeltaTime, FMinimalViewInfo& InOutView)
    {
        // 기본은 아무것도 안함
        return true;
    }

    void UpdateAlpha(float DeltaTime);

protected:
    APlayerCameraManager* CameraOwner = nullptr;

    float Alpha = 1.f;
    float AlphaInTime = 0.1f;
    float AlphaOutTime = 0.1f;

    bool bDisabled = false;
    bool bPendingDisable = false;

    uint8 Priority = 0;
};