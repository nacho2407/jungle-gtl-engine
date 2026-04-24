// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "SceneComponent.h"
#include "Component/FogParams.h"

// UHeightFogComponent 컴포넌트이다.
class UHeightFogComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UHeightFogComponent, USceneComponent)

    UHeightFogComponent();

    void CreateRenderState() override;
    void DestroyRenderState() override;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;

    // Updates FogBaseHeight when the component transform changes.
    void OnTransformDirty() override;

private:
    void PushToScene();

    float FogDensity = 0.02f;
    float FogHeightFalloff = 0.2f;
    float StartDistance = 0.0f;
    float FogCutoffDistance = 0.0f;
    float FogMaxOpacity = 1.0f;
    FVector4 FogInscatteringColor = FVector4(0.45f, 0.55f, 0.65f, 1.0f);
};
