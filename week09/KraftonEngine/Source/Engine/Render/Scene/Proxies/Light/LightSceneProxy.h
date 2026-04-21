#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/Proxies/SceneProxy.h"
#include "Render/Resources/RenderResources.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

class ULightComponent;
class FScene;

// 모든 LightSceneProxy에 대응되는 데이터를 저장하고 있는 공통 구조체, Render Collector에서 불러올 때 이 구조체에서 변환하여 사용
struct FLightConstants
{
    FVector Position;        // 12B  — Point/Spot 월드 위치
    float Intensity;         //  4B
    FVector Direction;       // 12B  — Ambient/Spot 방향 (정규화)
    float AttenuationRadius; //  4B  — Point/Spot 감쇠 반경
    FVector4 LightColor;     // 16B  — linear RGBA
    float InnerConeAngle;    //  4B  — Spot 내부 코사인 반각
    float OuterConeAngle;    //  4B  — Spot 외부 코사인 반각
    uint32 LightType;        //  4B  — ELightType 캐스트
    float Padding;           //  4B  — 16B 경계 맞춤
};

class FLightSceneProxy : public FSceneProxy
{
public:
    FLightSceneProxy(ULightComponent* InComponent);
    virtual ~FLightSceneProxy() = default;

    // ─── 컴포넌트 → 프록시 데이터 동기화 (개별 서브클래스가 오버라이드) ───
    virtual void UpdateLightConstants(); // LightConstants 전체 갱신
    virtual void UpdateTransform();      // Position/Direction만 갱신

    // ─── 에디터 디버그 시각화 (와이어프레임 화살표/구/콘) ───
    virtual void VisualizeLightsInEditor(FScene& Scene) const {}

    // ─── 소유 컴포넌트 / 캐싱된 렌더 데이터 ───
    ULightComponent* Owner = nullptr; // 소유 컴포넌트 (역참조용)

    // ─── 캐싱된 렌더 데이터 (등록 시 초기화, dirty 시에만 갱신) ───
    FLightConstants LightConstants = {};

    // ─── 가시성 ───
    bool bVisible = true;
    bool bAffectsWorld = true;
};