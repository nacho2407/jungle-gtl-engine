#pragma once

#include "LightComponent.h"

/**
 * UAmbientLightComponent: UE5 SkyLight의 단순화 버전,
 * 위치·방향 속성이 없고, Intensity, LightColor만으로 씬 전체에 환경광을 더합니다.
 */
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
