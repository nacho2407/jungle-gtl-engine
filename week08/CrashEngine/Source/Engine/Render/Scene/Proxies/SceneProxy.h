#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/SceneProxyDirtyFlag.h"

// FSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
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
