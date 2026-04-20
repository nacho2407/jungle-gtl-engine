#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/DirtyFlag.h"
#include "Render/Resources/RenderResources.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

class UPrimitiveComponent;
class FShader;
class FMeshBuffer;
class FScene;
struct FSceneView;
using FFrameContext = FSceneView;

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
    uint32 ProxyId = UINT32_MAX;          // FScene 내 인덱스
    UPrimitiveComponent* Owner = nullptr; // 소유 컴포넌트 (역참조용)
    uint32 SelectedListIndex = UINT32_MAX;

    // --- 변경 추적 ---
    EDirtyFlag DirtyFlags = EDirtyFlag::All;
    bool bQueuedForDirtyUpdate = false;

    // --- LOD ---
    FVector CachedWorldPos; // Transform 갱신 시 캐싱 — LOD 거리 계산용
    uint32 CurrentLOD = 0;
    virtual void UpdateLOD(uint32 /*LODLevel*/) {}

    // --- Per-viewport 갱신 (bPerViewportUpdate=true 프록시만) ---
    // 매 프레임, 각 뷰포트의 카메라 데이터로 프록시 상태를 갱신
    virtual void UpdatePerViewport(const FFrameContext& Frame) {}

    // 선택된 프록시의 소유 액터 컴포넌트에서 디버그 시각화 수집
    void CollectSelectedVisuals(FScene& Scene) const;

    // --- 가시성·선택 ---
    bool bVisible = true;
    bool bSelected = false;
    bool bSupportsOutline = true;
    bool bNeverCull = false; // true면 frustum culling 대상에서 제외 (Gizmo 등 에디터 프록시)
    bool bShowAABB = true;   // 선택 시 AABB 디버그 라인 표시 여부 (Billboard/SubUV 등은 false)

    // --- 렌더 패스 ---
    ERenderPass Pass = ERenderPass::Opaque;

    // 머티리얼 기반 렌더 상태 (개별 섹션에 정보가 없을 때 사용)
    EBlendState Blend = EBlendState::Opaque;
    EDepthStencilState DepthStencil = EDepthStencilState::Default;
    ERasterizerState Rasterizer = ERasterizerState::SolidBackCull;

    // --- 캐싱된 렌더 데이터 (등록 시 초기화, dirty 시만 갱신) ---
    FShader* Shader = nullptr;
    FMeshBuffer* MeshBuffer = nullptr;
    FPerObjectConstants PerObjectConstants = {};
    FBoundingBox CachedBounds;
    mutable bool bPerObjectCBDirty = true;

    // 섹션별 드로우 정보 (메시/머티리얼 변경 시만 재구축)
    TArray<FMeshSectionDraw> SectionDraws;

    // 특수 CB (Gizmo, SubUV 등)
    FConstantBufferBinding ExtraCB;

    // 텍스처 바인딩 (SubUV, Billboard 등 프록시 직접 렌더링용)
    ID3D11ShaderResourceView* DiffuseSRV = nullptr;

    // 뷰포트별 갱신이 필요한 프록시 (Gizmo, Billboard 등)
    bool bPerViewportUpdate = false;
    bool bFontBatched = false; // true면 FFontGeometry 배칭 경로 사용 (TextRenderProxy)

    // 큰 씬에서는 visible proxy 빌드 중 LOD 갱신을 프레임 분산한다.
    uint32 LastLODUpdateFrame = UINT32_MAX;

    void MarkPerObjectCBDirty() const { bPerObjectCBDirty = true; }
    void ClearPerObjectCBDirty() const { bPerObjectCBDirty = false; }
    bool NeedsPerObjectCBUpload() const { return bPerObjectCBDirty; }
};
