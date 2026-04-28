// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Component/SceneComponent.h"
#include "Object/ObjectFactory.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

// ULightComponentBase 클래스이다.
class ULightComponentBase : public USceneComponent
{
public:
    DECLARE_CLASS(ULightComponentBase, USceneComponent)

    ULightComponentBase();

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    float GetIntensity() const { return Intensity; }
    const FVector4& GetLightColor() const { return LightColor; }
    bool AffectsWorld() const { return bAffectsWorld; }
    bool DoesCastShadows() const { return bCastShadows; }
    uint32 GetShadowResolution() const { return static_cast<uint32>(ShadowResolution); }
    float GetShadowBias() const { return ShadowBias; }
    float GetShadowSlopeBias() const { return ShadowSlopeBias; }
    float GetShadowNormalBias() const { return ShadowNormalBias; }
    void SetShadowResolution(uint32 InResolution) { ShadowResolution = static_cast<int32>(InResolution); }

protected:
    float Intensity = 2.5f;
    FVector4 LightColor = { 1, 1, 1, 1 }; // linear RGBA (0~1)
    bool bAffectsWorld = true;            // 조명의 영향 여부를 켜고 끕니다.
    bool bCastShadows = true;             // Shadow 구현 주차에 사용: 조명이 그림자를 드리울지 여부를 켜고 끕니다.
    int32 ShadowResolution = 1024;
    float ShadowBias = 0.005f;
    float ShadowSlopeBias = 0.5f;
    float ShadowNormalBias = 1.0f;
};
