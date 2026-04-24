// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"

// UDecalComponent 컴포넌트이다.
class UDecalComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

    UDecalComponent() = default;
    ~UDecalComponent() override = default;

    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

    FPrimitiveSceneProxy* CreateSceneProxy() override;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    // Color (with Color)
    void SetColor(FVector4 InColor) { Color = InColor; }
    FVector4 GetColor() const;

    // --- Material ---
    void SetMaterial(int32 ElementIndex, UMaterial* InMaterial) override;
    UMaterial* GetMaterial(int32 ElementIndex) const override { return Material; }

    void OnTransformDirty() override;

private:
    bool ShouldRenderDebugBox() const;
    void HandleFade(float DeltaTime);
    void RenderDebugBox();

private:
    FMaterialSlot MaterialSlot;
    UMaterial* Material = nullptr;
    FVector4 Color = { 1, 1, 1, 1 };
    float FadeInDelay = 0;
    float FadeInDuration = 0;
    float FadeOutDelay = 0;
    float FadeOutDuration = 0;
    float FadeTimer = 0;
    float FadeOpacity = 1.0f;
};
