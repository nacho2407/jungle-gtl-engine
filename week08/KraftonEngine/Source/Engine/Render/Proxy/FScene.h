#pragma once

#include "Core/CoreTypes.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include <unordered_set>

class UPrimitiveComponent;

// ============================================================
// FScene — FPrimitiveSceneProxy의 소유자 겸 변경 추적 컨테이너
// ============================================================
// UWorld와 1:1 대응. PrimitiveComponent 등록/해제 시 프록시를 관리하고,
// 프레임마다 DirtyList의 프록시만 갱신한 뒤 Renderer에 전달한다.
class FScene
{
public:
	FScene() = default;
	~FScene();

	// --- 프록시 등록/해제 ---
	// Component의 CreateSceneProxy()를 호출하여 구체 프록시 생성 후 등록
	FPrimitiveSceneProxy* AddPrimitive(UPrimitiveComponent* Component);

	// 이미 생성된 프록시를 직접 등록 (Gizmo Inner 등 추가 프록시용)
	void RegisterProxy(FPrimitiveSceneProxy* Proxy);

	// Component가 EndPlay 또는 소멸 시 호출
	void RemovePrimitive(FPrimitiveSceneProxy* Proxy);

	// --- 프레임 갱신 ---
	// 매 프레임 Render 직전에 호출 — DirtyList의 프록시만 갱신
	void UpdateDirtyProxies();

	// 외부에서 프록시를 Dirty로 마킹 (SceneComponent::MarkTransformDirty 등에서 호출)
	void MarkProxyDirty(FPrimitiveSceneProxy* Proxy, EDirtyFlag Flag);

	// --- 선택 ---
	void SetProxySelected(FPrimitiveSceneProxy* Proxy, bool bSelected);
	bool IsProxySelected(const FPrimitiveSceneProxy* Proxy) const;
	const std::unordered_set<FPrimitiveSceneProxy*>& GetSelectedProxies() const { return SelectedProxies; }

	// --- 조회 ---
	const TArray<FPrimitiveSceneProxy*>& GetAllProxies() const { return Proxies; }
	const TArray<FPrimitiveSceneProxy*>& GetNeverCullProxies() const { return NeverCullProxies; }
	uint32 GetProxyCount() const { return static_cast<uint32>(Proxies.size()); }

private:
	// 전체 프록시 목록 (ProxyId = 인덱스)
	TArray<FPrimitiveSceneProxy*> Proxies;

	// 프레임 내 변경된 프록시 (중복 방지를 위해 set)
	std::unordered_set<FPrimitiveSceneProxy*> DirtyProxies;

	// 선택된 프록시 (O(1) 조회)
	std::unordered_set<FPrimitiveSceneProxy*> SelectedProxies;

	// bNeverCull 프록시 (Gizmo 등) — Frustum 쿼리와 무관하게 항상 수집
	TArray<FPrimitiveSceneProxy*> NeverCullProxies;

	// 삭제된 슬롯 재활용
	TArray<uint32> FreeSlots;
};
