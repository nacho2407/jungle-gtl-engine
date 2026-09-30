#pragma once

#include "Core/CoreTypes.h"

/*
    렌더 프록시의 어떤 데이터가 변경되었는지 기록하는 비트마스크입니다.
    게임 스레드에서 표시한 변경 사항을 렌더 씬 갱신 단계에서 필요한 작업으로 분기합니다.
    현재 사용 중인 Transform/Material/Visibility/Mesh 외에도
    조명 파라미터, 선택 상태, bounds, shadow 관련 확장을 위한 비트를 미리 확보합니다.
*/
enum class ESceneProxyDirtyFlag : uint32
{
    None       = 0,
    Transform  = 1 << 0,
    Material   = 1 << 1,
    Visibility = 1 << 2,
    Mesh       = 1 << 3,
    Lighting   = 1 << 4,
    Selection  = 1 << 5,
    Bounds     = 1 << 6,
    Shadow     = 1 << 7,
    All        = 0xFFFFFFFF,
};

inline ESceneProxyDirtyFlag operator|(ESceneProxyDirtyFlag A, ESceneProxyDirtyFlag B)
{
    return static_cast<ESceneProxyDirtyFlag>(static_cast<uint32>(A) | static_cast<uint32>(B));
}

inline ESceneProxyDirtyFlag operator&(ESceneProxyDirtyFlag A, ESceneProxyDirtyFlag B)
{
    return static_cast<ESceneProxyDirtyFlag>(static_cast<uint32>(A) & static_cast<uint32>(B));
}

inline ESceneProxyDirtyFlag& operator|=(ESceneProxyDirtyFlag& A, ESceneProxyDirtyFlag B)
{
    A = A | B;
    return A;
}

inline ESceneProxyDirtyFlag& operator&=(ESceneProxyDirtyFlag& A, ESceneProxyDirtyFlag B)
{
    A = A & B;
    return A;
}

inline ESceneProxyDirtyFlag operator~(ESceneProxyDirtyFlag A)
{
    return static_cast<ESceneProxyDirtyFlag>(~static_cast<uint32>(A));
}

inline bool HasFlag(ESceneProxyDirtyFlag Flags, ESceneProxyDirtyFlag Test)
{
    return (Flags & Test) != ESceneProxyDirtyFlag::None;
}
