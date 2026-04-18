#pragma once

#include "Core/CoreTypes.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Types/RenderTypes.h"

class ULightComponent;

/**
 * Light는 SceneComponent이므로 FPrimitiveSceneProxy와는 구분됩니다.
 */
class FLightSceneProxy
{
public:
    FLightSceneProxy(ULightComponent* InComponent);
    virtual ~FLightSceneProxy() = default;

    // ─── 컴포넌트 → 프록시 데이터 동기화 (개별 서브클래스가 오버라이드) ───
    virtual void UpdateLightConstants(); // LightConstants 전체 갱신
    virtual void UpdateTransform();		 // Position/Direction만 갱신

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