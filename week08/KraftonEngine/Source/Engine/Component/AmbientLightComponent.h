// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "LightComponent.h"

// UAmbientLightComponent 컴포넌트이다.
class UAmbientLightComponent : public ULightComponent
{
public:
    DECLARE_CLASS(UAmbientLightComponent, ULightComponent)
    UAmbientLightComponent();

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    FLightSceneProxy* CreateLightSceneProxy() override;
};
