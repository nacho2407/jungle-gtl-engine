// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

class FPrimitiveSceneProxy;

/*
    TSceneProxyRegistry는 특정 프록시 타입의 원본 목록, dirty 목록, selected 목록, free slot을 한 곳에 모읍니다.
    FScene은 Primitive / Light / Effect 별로 이 구조체를 인스턴스화해 공통 관리 로직을 공유합니다.
*/
template <typename ProxyT>
// TSceneProxyRegistry는 실행 시 필요한 타입과 규칙의 매핑을 보관합니다.
struct TSceneProxyRegistry
{
    TArray<ProxyT*> Proxies;
    TArray<ProxyT*> DirtyProxies;
    TArray<ProxyT*> SelectedProxies;
    TArray<uint32>  FreeSlots;

    void Reset()
    {
        Proxies.clear();
        DirtyProxies.clear();
        SelectedProxies.clear();
        FreeSlots.clear();
    }
};

/*
    Primitive registry는 공통 registry에 더해 NeverCull 전용 목록을 별도로 유지합니다.
*/
struct FPrimitiveSceneProxyRegistry : public TSceneProxyRegistry<FPrimitiveSceneProxy>
{
    TArray<FPrimitiveSceneProxy*> NeverCullProxies;

    void Reset()
    {
        TSceneProxyRegistry<FPrimitiveSceneProxy>::Reset();
        NeverCullProxies.clear();
    }
};
