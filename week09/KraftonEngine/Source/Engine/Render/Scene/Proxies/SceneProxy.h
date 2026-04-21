#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/DirtyFlag.h"

/*
    FSceneProxy는 Scene이 공통으로 관리하는 모든 프록시의 베이스 타입입니다.
    슬롯 인덱스, dirty 상태, selection 인덱스처럼 Scene registry 관점의 공통 상태를 보관합니다.
*/
class FSceneProxy
{
public:
    virtual ~FSceneProxy() = default;

    void MarkDirty(EDirtyFlag Flag) { DirtyFlags |= Flag; }
    void ClearDirty(EDirtyFlag Flag) { DirtyFlags &= ~Flag; }
    bool IsDirty(EDirtyFlag Flag) const { return HasFlag(DirtyFlags, Flag); }
    bool IsAnyDirty() const { return DirtyFlags != EDirtyFlag::None; }

    uint32 ProxyId = UINT32_MAX;
    uint32 SelectedListIndex = UINT32_MAX;

    EDirtyFlag DirtyFlags = EDirtyFlag::All;
    bool bQueuedForDirtyUpdate = false;
};
