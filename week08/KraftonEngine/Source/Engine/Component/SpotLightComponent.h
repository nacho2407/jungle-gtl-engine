// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "PointLightComponent.h"

// USpotLightComponent 컴포넌트이다.
class USpotLightComponent : public UPointLightComponent
{
public:
    DECLARE_CLASS(USpotLightComponent, UPointLightComponent)
    USpotLightComponent() = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    FLightSceneProxy* CreateLightSceneProxy() override;

    float GetInnerConeAngle() const { return InnerConeAngle; }
    float GetOuterConeAngle() const { return OuterConeAngle; }

private:
    // ─── UE5 Default Value ───
    float InnerConeAngle = 0.0f;  // SpotLight 중심 빛이 최대 강도를 가지는 원뿔의 각도
    float OuterConeAngle = 44.0f; // SpotLight의 빛이 완전히 사라지는 원뿔의 각도
};
