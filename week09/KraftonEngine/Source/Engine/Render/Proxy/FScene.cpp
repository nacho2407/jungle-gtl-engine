#include "Render/Proxy/FScene.h"
#include "Component/PrimitiveComponent.h"
#include "Component/LightComponent.h"
#include "Profiling/Stats.h"
#include <algorithm>

namespace
{
	void EnqueueDirtyProxy(TArray<FPrimitiveSceneProxy*>& DirtyList, FPrimitiveSceneProxy* Proxy)
	{
		if (!Proxy || Proxy->bQueuedForDirtyUpdate)
		{
			return;
		}

		Proxy->bQueuedForDirtyUpdate = true;
		DirtyList.push_back(Proxy);
	}

	void EnqueueDirtyLightProxy(TArray<FLightSceneProxy*>& DirtyList, FLightSceneProxy* Proxy)
	{
		if (!Proxy || Proxy->bQueuedForDirtyUpdate)
		{
			return;
		}

		Proxy->bQueuedForDirtyUpdate = true;
		DirtyList.push_back(Proxy);
	}

	void RemoveSelectedProxyFast(TArray<FPrimitiveSceneProxy*>& SelectedList, FPrimitiveSceneProxy* Proxy)
	{
		if (!Proxy || Proxy->SelectedListIndex == UINT32_MAX)
		{
			return;
		}

		const uint32 Index = Proxy->SelectedListIndex;
		const uint32 LastIndex = static_cast<uint32>(SelectedList.size() - 1);
		if (Index != LastIndex)
		{
			FPrimitiveSceneProxy* LastProxy = SelectedList.back();
			SelectedList[Index] = LastProxy;
			LastProxy->SelectedListIndex = Index;
		}

		SelectedList.pop_back();
		Proxy->SelectedListIndex = UINT32_MAX;
	}
}

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

	for (FLightSceneProxy* Proxy : LightProxies)
	{
		delete Proxy;
	}
	LightProxies.clear();
	DirtyLightProxies.clear();
	FreeLightSlots.clear();
}

// ============================================================
// RegisterPrimitiveProxy — 프록시를 슬롯에 배치하고 DirtyList에 추가
// ============================================================
void FScene::RegisterPrimitiveProxy(FPrimitiveSceneProxy* Proxy)
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

	EnqueueDirtyProxy(DirtyProxies, Proxy);

	if (Proxy->bNeverCull)
		NeverCullProxies.push_back(Proxy);
}

// ============================================================
// AddPrimitive — Component의 CreateSceneProxy()로 구체 프록시 생성 후 등록
// ============================================================
FPrimitiveSceneProxy* FScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component) return nullptr;

	// 컴포넌트가 자신에 맞는 구체 프록시를 생성 (다형성)
	FPrimitiveSceneProxy* Proxy = Component->CreateSceneProxy();
	if (!Proxy) return nullptr;

	RegisterPrimitiveProxy(Proxy);
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
	if (Proxy->bQueuedForDirtyUpdate)
	{
		auto DirtyIt = std::find(DirtyProxies.begin(), DirtyProxies.end(), Proxy);
		if (DirtyIt != DirtyProxies.end())
		{
			*DirtyIt = DirtyProxies.back();
			DirtyProxies.pop_back();
		}
		Proxy->bQueuedForDirtyUpdate = false;
	}

	if (Proxy->SelectedListIndex != UINT32_MAX)
	{
		RemoveSelectedProxyFast(SelectedProxies, Proxy);
	}

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
	SCOPE_STAT_CAT("UpdateDirtyProxies", "3_Collect");

	//Update 중 Transform/Mesh 업데이트가 다시 MarkProxyDirty를 호출할 수 있으므로
	//현재 배치는 스냅샷으로 분리해 순회한다.
	TArray<FPrimitiveSceneProxy*> PendingDirtyProxies = std::move(DirtyProxies);
	DirtyProxies.clear();

	for (FPrimitiveSceneProxy* Proxy : PendingDirtyProxies)
	{
		if (!Proxy)
		{
			continue;
		}

		Proxy->bQueuedForDirtyUpdate = false;
		if (!Proxy->Owner) continue;

		// 현재 프레임에 처리할 dirty만 캡처하고, 처리 중 새로 발생한 dirty는
		// 다음 배치/다음 프레임에 남겨둔다.
		const EDirtyFlag FlagsToProcess = Proxy->DirtyFlags;
		Proxy->DirtyFlags = EDirtyFlag::None;

		// 가상 함수를 통해 서브클래스별 갱신 로직 호출
		if (HasFlag(FlagsToProcess, EDirtyFlag::Mesh))
		{
			Proxy->UpdateMesh();
		}
		else if (HasFlag(FlagsToProcess, EDirtyFlag::Material))
		{
			// Mesh가 이미 갱신됐으면 Material도 포함되므로 else if
			Proxy->UpdateMaterial();
		}

		if (HasFlag(FlagsToProcess, EDirtyFlag::Transform))
		{
			Proxy->UpdateTransform();
		}
		if (HasFlag(FlagsToProcess, EDirtyFlag::Visibility))
		{
			Proxy->UpdateVisibility();
		}
	}
}

// ============================================================
// UpdateDirtyLightProxies — 변경된 Light 프록시만 갱신 (프레임당 1회)
// ============================================================
void FScene::UpdateDirtyLightProxies()
{
	TArray<FLightSceneProxy*> Pending = std::move(DirtyLightProxies);
	DirtyLightProxies.clear();

	for (FLightSceneProxy* Proxy : Pending)
	{
		if (!Proxy) continue;

		Proxy->bQueuedForDirtyUpdate = false;
		if (!Proxy->Owner) continue;

		const EDirtyFlag FlagsToProcess = Proxy->DirtyFlags;
		Proxy->DirtyFlags = EDirtyFlag::None;

		if (HasFlag(FlagsToProcess, EDirtyFlag::Transform))
		{
			Proxy->UpdateTransform();
		}

		// Transform 외 모든 변경(색상, 강도, 반경 등)은 UpdateLightConstants로 처리
		if (HasFlag(FlagsToProcess, EDirtyFlag::Material) ||
			HasFlag(FlagsToProcess, EDirtyFlag::Visibility))
		{
			Proxy->UpdateLightConstants();
		}
	}
}

// ============================================================
// MarkProxyDirty — 외부에서 프록시의 특정 필드를 dirty로 마킹
// ============================================================
void FScene::MarkProxyDirty(FPrimitiveSceneProxy* Proxy, EDirtyFlag Flag)
{
	if (!Proxy) return;
	Proxy->MarkDirty(Flag);
	EnqueueDirtyProxy(DirtyProxies, Proxy);
}

void FScene::MarkLightProxyDirty(FLightSceneProxy* Proxy, EDirtyFlag Flag)
{
	if (!Proxy) return;
	Proxy->MarkDirty(Flag);
	EnqueueDirtyLightProxy(DirtyLightProxies, Proxy);
}

void FScene::MarkAllPerObjectCBDirty()
{
	for (FPrimitiveSceneProxy* Proxy : Proxies)
	{
		if (Proxy)
		{
			Proxy->MarkPerObjectCBDirty();
		}
	}
}

// ============================================================
// 선택 관리
// ============================================================
void FScene::SetProxySelected(FPrimitiveSceneProxy* Proxy, bool bSelected)
{
	if (!Proxy) return;
	Proxy->bSelected = bSelected;

	if (bSelected)
	{
		if (Proxy->SelectedListIndex == UINT32_MAX)
		{
			Proxy->SelectedListIndex = static_cast<uint32>(SelectedProxies.size());
			SelectedProxies.push_back(Proxy);
		}
	}
	else
	{
		RemoveSelectedProxyFast(SelectedProxies, Proxy);
	}
}

bool FScene::IsProxySelected(const FPrimitiveSceneProxy* Proxy) const
{
	return Proxy && Proxy->SelectedListIndex != UINT32_MAX;
}

// ============================================================
// Per-frame ephemeral data — 매 뷰포트 렌더 시작 시 Clear
// ============================================================
void FScene::ClearFrameData()
{
	OverlayTexts.clear();
	DebugAABBs.clear();
	DebugLines.clear();
	Grid = {};
}

void FScene::AddOverlayText(FString Text, const FVector2& Position, float Scale)
{
	OverlayTexts.push_back({ std::move(Text), Position, Scale });
}

void FScene::AddDebugAABB(const FVector& Min, const FVector& Max, const FColor& Color)
{
	DebugAABBs.push_back({ Min, Max, Color });
}

void FScene::AddDebugLine(const FVector& Start, const FVector& End, const FColor& Color)
{
	DebugLines.push_back({ Start, End, Color });
}

void FScene::SetGrid(float Spacing, int32 HalfLineCount)
{
	Grid.Spacing = Spacing;
	Grid.HalfLineCount = HalfLineCount;
	Grid.bEnabled = true;
}

void FScene::AddFog(const UHeightFogComponent* Owner, const FFogParams& Params)
{
	for (auto& Entry : Fogs)
	{
		if (Entry.Owner == Owner)
		{
			Entry.Params = Params;
			return;
		}
	}
	Fogs.push_back({ Owner, Params });
}

void FScene::RemoveFog(const UHeightFogComponent* Owner)
{
	Fogs.erase(
		std::remove_if(Fogs.begin(), Fogs.end(),
			[Owner](const FFogEntry& E) { return E.Owner == Owner; }),
		Fogs.end());
}

// ============================================================
// AddLight — Component의 CreateLightSceneProxy()로 구체 프록시 생성 후 등록
// ============================================================
FLightSceneProxy* FScene::AddLight(ULightComponent* Component)
{
	if (!Component) return nullptr;

	FLightSceneProxy* Proxy = Component->CreateLightSceneProxy();
	if (!Proxy) return nullptr;

	RegisterLightProxy(Proxy);
	return Proxy;
}

// ============================================================
// RegisterLightProxy — Light 프록시를 슬롯에 배치하고 DirtyList에 추가
// ============================================================
void FScene::RegisterLightProxy(FLightSceneProxy* Proxy)
{
	if (!Proxy) return;

	Proxy->DirtyFlags = EDirtyFlag::All;

	if (!FreeLightSlots.empty())
	{
		uint32 Slot = FreeLightSlots.back();
		FreeLightSlots.pop_back();
		Proxy->ProxyId = Slot;
		LightProxies[Slot] = Proxy;
	}
	else
	{
		Proxy->ProxyId = static_cast<uint32>(LightProxies.size());
		LightProxies.push_back(Proxy);
	}

	EnqueueDirtyLightProxy(DirtyLightProxies, Proxy);
}

// ============================================================
// RemoveLight — Light 프록시 해제 및 슬롯 반환
// ============================================================
void FScene::RemoveLight(FLightSceneProxy* Proxy)
{
	if (!Proxy || Proxy->ProxyId == UINT32_MAX) return;

	uint32 Slot = Proxy->ProxyId;

	if (Proxy->bQueuedForDirtyUpdate)
	{
		auto It = std::find(DirtyLightProxies.begin(), DirtyLightProxies.end(), Proxy);
		if (It != DirtyLightProxies.end())
		{
			*It = DirtyLightProxies.back();
			DirtyLightProxies.pop_back();
		}
		Proxy->bQueuedForDirtyUpdate = false;
	}

	LightProxies[Slot] = nullptr;
	FreeLightSlots.push_back(Slot);

	delete Proxy;
}
