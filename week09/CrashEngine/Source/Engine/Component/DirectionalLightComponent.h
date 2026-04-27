// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "LightComponent.h"

// UDirectionalLightComponent 컴포넌트이다.
class UDirectionalLightComponent : public ULightComponent
{
public:
    DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
    UDirectionalLightComponent();

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    FLightProxy* CreateLightProxy() override;

    int32 GetCascadeCount() const { return CascadeCount; }
    float GetDynamicShadowDistance() const { return DynamicShadowDistance; }
    float GetCascadeDistribution() const { return CascadeDistribution; }

protected:
    FVector Direction = FVector(0.0f, 0.0f, -1.0f); // 기본값: 아래로 향하는 빛
    
    // TODO: Cascade 구현
    int32 CascadeCount = 4;
    float DynamicShadowDistance = 2000.0f;
    float CascadeDistribution = 1.0f;
};


