#pragma once

#include "Core/CoreTypes.h"
#include "Render/Pipeline/DirtyFlag.h"
#include "Render/Pipeline/RenderCommand.h"
#include "Render/Types/RenderTypes.h"

class UPrimitiveComponent;
class FShader;
class FMeshBuffer;

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

	// --- 가시성·선택 ---
	bool bVisible  = true;
	bool bSelected = false;

	// --- 렌더 패스 ---
	ERenderPass Pass = ERenderPass::Opaque;

	// --- 캐싱된 렌더 데이터 (등록 시 초기화, dirty 시만 갱신) ---
	FShader*     Shader     = nullptr;
	FMeshBuffer* MeshBuffer = nullptr;
	FPerObjectConstants PerObjectConstants = {};

	// 섹션별 드로우 정보 (메시/머티리얼 변경 시만 재구축)
	TArray<FMeshSectionDraw> SectionDraws;

	// 특수 CB (Gizmo 등)
	FConstantBufferBinding ExtraCB;

	// 뷰포트별 갱신이 필요한 프록시 (Gizmo, Billboard 등)
	bool bPerViewportUpdate = false;
};
