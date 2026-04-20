#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/Core/DirtyFlag.h"
#include "Render/Core/RenderConstants.h"
#include "Render/Types/RenderTypes.h"

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

class FLightSceneProxy
{
public:
    FLightSceneProxy(ULightComponent* InComponent);
    virtual ~FLightSceneProxy() = default;

    // ─── 컴포넌트 → 프록시 데이터 동기화 (개별 서브클래스가 오버라이드) ───
    virtual void UpdateLightConstants(); // LightConstants 전체 갱신
    virtual void UpdateTransform();		 // Position/Direction만 갱신

    // ─── 에디터 디버그 시각화 (와이어프레임 화살표/구/콘) ───
    virtual void VisualizeLightsInEditor(FScene& Scene) const {}

    // ─── Dirty 관리 ───
    void MarkDirty(EDirtyFlag Flag) { DirtyFlags |= Flag; }
    void ClearDirty(EDirtyFlag Flag) { DirtyFlags &= ~Flag; }
    bool IsDirty(EDirtyFlag Flag) const { return HasFlag(DirtyFlags, Flag); }
    bool IsAnyDirty() const { return DirtyFlags != EDirtyFlag::None; }

    // ─── 식별 ───
    uint32 ProxyId = UINT32_MAX;
    ULightComponent* Owner = nullptr; // 소유 컴포넌트 (역참조용)
    uint32 SelectedListIndex = UINT32_MAX;

    // ─── 변경 추적 ───
    EDirtyFlag DirtyFlags = EDirtyFlag::All;
    bool bQueuedForDirtyUpdate = false;

    // ─── 캐싱된 렌더 데이터 (등록 시 초기화, dirty 시에만 갱신) ───
    FLightConstants LightConstants = {};

    // ─── 가시성 ───
    bool bVisible = true;
    bool bAffectsWorld = true;
};