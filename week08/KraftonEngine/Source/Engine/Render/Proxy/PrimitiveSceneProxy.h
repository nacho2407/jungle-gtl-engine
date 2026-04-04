#pragma once

#include "Core/CoreTypes.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Types/RenderTypes.h"

class UPrimitiveComponent;
class FShader;
class FMeshBuffer;
class FRenderBus;

// ============================================================
// FPrimitiveSceneProxy — UPrimitiveComponent의 렌더 데이터 미러 (기본 클래스)
// ============================================================
// 컴포넌트 등록 시 CreateSceneProxy()로 1회 생성.
// 이후 DirtyFlags가 켜진 필드만 가상 함수를 통해 갱신.
// Renderer가 매 프레임 이 프록시를 직접 순회하여 draw call 수행.
class FPrimitiveSceneProxy
{
public:
	FPrimitiveSceneProxy(UPrimitiveComponent* InComponent);
	virtual ~FPrimitiveSceneProxy() = default;

	// --- 가상 갱신 인터페이스 (서브클래스가 오버라이드) ---
	virtual void UpdateTransform();
	virtual void UpdateMaterial();
	virtual void UpdateVisibility();
	virtual void UpdateMesh();

	// --- Batcher Entry 수집 (bBatcherRendered 프록시가 오버라이드) ---
	virtual void CollectEntries(FRenderBus& Bus) {}

	// --- Dirty 관리 ---
	void MarkDirty(EDirtyFlag Flag) { DirtyFlags |= Flag; }
	void ClearDirty(EDirtyFlag Flag) { DirtyFlags &= ~Flag; }
	bool IsDirty(EDirtyFlag Flag) const { return HasFlag(DirtyFlags, Flag); }
	bool IsAnyDirty() const { return DirtyFlags != EDirtyFlag::None; }

	// --- 식별 ---
	uint32 ProxyId = UINT32_MAX;			// FScene 내 인덱스
	UPrimitiveComponent* Owner = nullptr;	// 소유 컴포넌트 (역참조용)

	// --- 변경 추적 ---
	EDirtyFlag DirtyFlags = EDirtyFlag::All;

	// --- Per-viewport 갱신 (bPerViewportUpdate=true 프록시만) ---
	// 매 프레임, 각 뷰포트의 카메라 데이터로 프록시 상태를 갱신
	virtual void UpdatePerViewport(const FRenderBus& Bus) {}

	// --- 가시성·선택 ---
	bool bVisible  = true;
	bool bSelected = false;
	bool bSupportsOutline = true;
	bool bNeverCull = false;		// true면 frustum culling 대상에서 제외 (Gizmo 등 에디터 프록시)

	// --- 렌더 패스 ---
	ERenderPass Pass = ERenderPass::Opaque;

	// --- 캐싱된 렌더 데이터 (등록 시 초기화, dirty 시만 갱신) ---
	FShader*     Shader     = nullptr;
	FMeshBuffer* MeshBuffer = nullptr;
	FPerObjectConstants PerObjectConstants = {};
	FBoundingBox CachedBounds;

	// 섹션별 드로우 정보 (메시/머티리얼 변경 시만 재구축)
	TArray<FMeshSectionDraw> SectionDraws;

	// 특수 CB (Gizmo 등)
	FConstantBufferBinding ExtraCB;

	// 뷰포트별 갱신이 필요한 프록시 (Gizmo, Billboard 등)
	bool bPerViewportUpdate = false;

	// true면 렌더링은 Batcher(Font/SubUV)가 담당 — CollectRender 호출 유지
	// false면 프록시가 직접 ProxyQueue에 제출
	bool bBatcherRendered = false;
};
