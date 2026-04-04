#include "Render/Proxy/FScene.h"
#include "Component/PrimitiveComponent.h"
#include <algorithm>

// ============================================================
// 소멸자 — 모든 프록시 정리
// ============================================================
FScene::~FScene()
{
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		delete Proxy;
	}
	Proxies.clear();
	DirtyProxies.clear();
	SelectedProxies.clear();
	NeverCullProxies.clear();
	FreeSlots.clear();
}

// ============================================================
// AddPrimitive — Component의 CreateSceneProxy()로 구체 프록시 생성 후 등록
// ============================================================
// ============================================================
// RegisterProxy — 프록시를 슬롯에 배치하고 DirtyList에 추가
// ============================================================
void FScene::RegisterProxy(FPrimitiveSceneProxy* Proxy)
{
	if (!Proxy) return;

	Proxy->DirtyFlags = EDirtyFlag::All; // 초기 등록 시 전체 갱신

	// 빈 슬롯 재활용 또는 새 슬롯 할당
	if (!FreeSlots.empty())
	{
		uint32 Slot = FreeSlots.back();
		FreeSlots.pop_back();
		Proxy->ProxyId = Slot;
		Proxies[Slot] = Proxy;
	}
	else
	{
		Proxy->ProxyId = static_cast<uint32>(Proxies.size());
		Proxies.push_back(Proxy);
	}

	DirtyProxies.insert(Proxy);

	if (Proxy->bNeverCull)
		NeverCullProxies.push_back(Proxy);
}

FPrimitiveSceneProxy* FScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component) return nullptr;

	// 컴포넌트가 자신에 맞는 구체 프록시를 생성 (다형성)
	FPrimitiveSceneProxy* Proxy = Component->CreateSceneProxy();
	if (!Proxy) return nullptr;

	RegisterProxy(Proxy);
	return Proxy;
}

// ============================================================
// RemovePrimitive — 프록시 해제 및 슬롯 반환
// ============================================================
void FScene::RemovePrimitive(FPrimitiveSceneProxy* Proxy)
{
	if (!Proxy || Proxy->ProxyId == UINT32_MAX) return;

	uint32 Slot = Proxy->ProxyId;

	// 각 목록에서 제거
	DirtyProxies.erase(Proxy);
	SelectedProxies.erase(Proxy);

	if (Proxy->bNeverCull)
	{
		auto it = std::find(NeverCullProxies.begin(), NeverCullProxies.end(), Proxy);
		if (it != NeverCullProxies.end()) NeverCullProxies.erase(it);
	}

	// 슬롯 비우고 재활용 목록에 추가
	Proxies[Slot] = nullptr;
	FreeSlots.push_back(Slot);

	delete Proxy;
}

// ============================================================
// UpdateDirtyProxies — 변경된 프록시만 갱신 (프레임당 1회)
// ============================================================
void FScene::UpdateDirtyProxies()
{
	for (FPrimitiveSceneProxy* Proxy : DirtyProxies)
	{
		if (!Proxy || !Proxy->Owner) continue;

		// 가상 함수를 통해 서브클래스별 갱신 로직 호출
		if (Proxy->IsDirty(EDirtyFlag::Mesh))
		{
			Proxy->UpdateMesh();
		}
		else if (Proxy->IsDirty(EDirtyFlag::Material))
		{
			// Mesh가 이미 갱신됐으면 Material도 포함되므로 else if
			Proxy->UpdateMaterial();
		}

		if (Proxy->IsDirty(EDirtyFlag::Transform))
		{
			Proxy->UpdateTransform();
		}
		if (Proxy->IsDirty(EDirtyFlag::Visibility))
		{
			Proxy->UpdateVisibility();
		}

		Proxy->DirtyFlags = EDirtyFlag::None;
	}

	DirtyProxies.clear();
}

// ============================================================
// MarkProxyDirty — 외부에서 프록시의 특정 필드를 dirty로 마킹
// ============================================================
void FScene::MarkProxyDirty(FPrimitiveSceneProxy* Proxy, EDirtyFlag Flag)
{
	if (!Proxy) return;
	Proxy->MarkDirty(Flag);
	DirtyProxies.insert(Proxy);
}

// ============================================================
// 선택 관리
// ============================================================
void FScene::SetProxySelected(FPrimitiveSceneProxy* Proxy, bool bSelected)
{
	if (!Proxy) return;
	Proxy->bSelected = bSelected;

	if (bSelected)
		SelectedProxies.insert(Proxy);
	else
		SelectedProxies.erase(Proxy);
}

bool FScene::IsProxySelected(const FPrimitiveSceneProxy* Proxy) const
{
	return SelectedProxies.count(const_cast<FPrimitiveSceneProxy*>(Proxy)) > 0;
}
