#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/SceneProxyDirtyFlag.h"

/*
    FScene이 공통으로 관리하는 모든 렌더 프록시의 기본 타입입니다.
    프록시 식별자, 선택 목록 인덱스, dirty 상태 같은 씬 레지스트리 공통 데이터를 보관합니다.
*/
class FSceneProxy
{
public:
    virtual ~FSceneProxy() = default;

    void MarkDirty(ESceneProxyDirtyFlag Flag) { DirtyFlags |= Flag; }
    void ClearDirty(ESceneProxyDirtyFlag Flag) { DirtyFlags &= ~Flag; }
    bool IsDirty(ESceneProxyDirtyFlag Flag) const { return HasFlag(DirtyFlags, Flag); }
    bool IsAnyDirty() const { return DirtyFlags != ESceneProxyDirtyFlag::None; }

    uint32 ProxyId           = UINT32_MAX;
    uint32 SelectedListIndex = UINT32_MAX;

    ESceneProxyDirtyFlag DirtyFlags            = ESceneProxyDirtyFlag::All;
    bool                 bQueuedForDirtyUpdate = false;
};
