#pragma once

#include "LightComponent.h"

/**
 * UDirectionalLightComponent: UE5 스타일의 직사광 조명 컴포넌트입니다.
 * 태양광처럼 거리에 상관없이 모든 오브젝트에 평행한 빛을 쏩니다.
 */
class UDirectionalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	UDirectionalLightComponent();

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    FLightSceneProxy* CreateLightSceneProxy() override;

protected:
    FVector Direction = FVector(0.0f, 0.0f, -1.0f); // 기본값: 아래로 향하는 빛
};