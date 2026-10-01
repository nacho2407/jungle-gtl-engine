#include "CameraManage/CameraModifiers/CameraModifier.h"

#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraModifier, UObject)

bool UCameraModifier::ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView)
{
    (void)DeltaTime;
    (void)InOutView;
    return false;
}
