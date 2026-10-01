#pragma once

#include "CameraManage/CameraTypes.h"
#include "Object/Object.h"

class APlayerCameraManager;

class UCameraModifier : public UObject
{
public:
    DECLARE_CLASS(UCameraModifier, UObject)

    virtual bool ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView);

    uint8 GetPriority() const { return Priority; }
    void SetPriority(uint8 InPriority) { Priority = InPriority; }

    bool IsDisabled() const { return bDisabled; }
    void SetDisabled(bool bInDisabled) { bDisabled = bInDisabled; }

    bool IsFinished() const { return bFinished; }

protected:
    void Finish() { bFinished = true; }

    uint8 Priority = 128;
    bool bDisabled = false;
    bool bFinished = false;
};
