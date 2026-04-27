#pragma once

#include "Core/CoreTypes.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Scene/SceneEnvironment.h"
#include "Debug/DebugDrawQueue.h"

class AActor;
class UPrimitiveComponent;
class UWorld;
struct FFrameContext;

// ============================================================
// FScene — FPrimitiveSceneProxy의 소유자 겸 변경 추적 컨테이너
// ============================================================
// UWorld와 1:1 대응. PrimitiveComponent 등록/해제 시 프록시를 관리하고,
// 프레임마다 DirtyList의 프록시만 갱신한 뒤 RenderCollector에 전달한다.
// 또한 매 프레임 수집되는 경량 디버그/에디터 데이터(라인, AABB, 텍스트, 그리드)를 소유.
class FScene
{
public:
	FScene() = default;
	~FScene();

	// --- 프록시 등록/해제 ---
	FPrimitiveSceneProxy* AddPrimitive(UPrimitiveComponent* Component);
	void RegisterProxy(FPrimitiveSceneProxy* Proxy);
	void RemovePrimitive(FPrimitiveSceneProxy* Proxy);

	// --- 프레임 갱신 ---
	void UpdateDirtyProxies();
	void MarkProxyDirty(FPrimitiveSceneProxy* Proxy, EDirtyFlag Flag);
	void MarkAllPerObjectCBDirty();

	// --- 선택 ---
	void SetProxySelected(FPrimitiveSceneProxy* Proxy, bool bSelected);
	bool IsProxySelected(const FPrimitiveSceneProxy* Proxy) const;
	const TSet<AActor*>& GetSelectedActors() const { return SelectedActors; }

	// --- 조회 ---
	const TArray<FPrimitiveSceneProxy*>& GetAllProxies() const { return Proxies; }
	const TArray<FPrimitiveSceneProxy*>& GetNeverCullProxies() const { return NeverCullProxies; }
	uint32 GetProxyCount() const { return static_cast<uint32>(Proxies.size()); }

	// ===== Per-frame ephemeral data (cleared each viewport render) =====
	void ClearFrameData();

	// --- Overlay text (screen-space) ---
	struct FOverlayText { FString Text; FVector2 Position; float Scale; };
	void AddOverlayText(FString Text, const FVector2& Position, float Scale);
	const TArray<FOverlayText>& GetOverlayTexts() const { return OverlayTexts; }

	// --- Debug AABB ---
	struct FDebugAABB { FVector Min; FVector Max; FColor Color; };
	void AddDebugAABB(const FVector& Min, const FVector& Max, const FColor& Color);
	const TArray<FDebugAABB>& GetDebugAABBs() const { return DebugAABBs; }

	// --- Debug lines ---
	struct FDebugLine { FVector Start; FVector End; FColor Color; };
	void AddDebugLine(const FVector& Start, const FVector& End, const FColor& Color);
	const TArray<FDebugLine>& GetDebugLines() const { return DebugLines; }

	// --- Grid ---
	struct FGridParams { float Spacing = 0.0f; int32 HalfLineCount = 0; bool bEnabled = false; };
	void SetGrid(float Spacing, int32 HalfLineCount);
	bool HasGrid() const { return Grid.bEnabled; }
	float GetGridSpacing() const { return Grid.Spacing; }
	int32 GetGridHalfLineCount() const { return Grid.HalfLineCount; }

	// --- DebugDraw (Duration 기반 디버그 라인) ---
	FDebugDrawQueue& GetDebugDrawQueue() { return DebugDrawQueue; }
	const FDebugDrawQueue& GetDebugDrawQueue() const { return DebugDrawQueue; }

	// --- Shadow frustum 디버그 라인 제출 ---
	void SubmitShadowFrustumDebug(UWorld* World, const FFrameContext& Frame);

	// --- 환경 데이터 (Fog, Light) ---
	FSceneEnvironment& GetEnvironment() { return Environment; }
	const FSceneEnvironment& GetEnvironment() const { return Environment; }

private:
	// --- 내부 헬퍼 (friend 경유로 Proxy private 멤버 접근) ---
	static void EnqueueDirtyProxy(TArray<FPrimitiveSceneProxy*>& DirtyList, FPrimitiveSceneProxy* Proxy);
	static void RemoveSelectedProxyFast(TArray<FPrimitiveSceneProxy*>& SelectedList, FPrimitiveSceneProxy* Proxy);

	// 전체 프록시 목록 (ProxyId = 인덱스)
	TArray<FPrimitiveSceneProxy*> Proxies;

	// 프레임 내 변경된 프록시 dense 목록
	TArray<FPrimitiveSceneProxy*> DirtyProxies;

	// 선택된 프록시 dense 목록
	TArray<FPrimitiveSceneProxy*> SelectedProxies;

	// 선택된 Actor (프록시 선택/해제 시 자동 관리)
	TSet<AActor*> SelectedActors;

	// bNeverCull 프록시 (Gizmo 등) — Frustum 쿼리와 무관하게 항상 수집
	TArray<FPrimitiveSceneProxy*> NeverCullProxies;

	// 삭제된 슬롯 재활용
	TArray<uint32> FreeSlots;

	// --- Per-frame ephemeral data ---
	TArray<FOverlayText> OverlayTexts;
	TArray<FDebugAABB>   DebugAABBs;
	TArray<FDebugLine>   DebugLines;

	FGridParams Grid;
	FDebugDrawQueue DebugDrawQueue;

	FSceneEnvironment Environment;
};
