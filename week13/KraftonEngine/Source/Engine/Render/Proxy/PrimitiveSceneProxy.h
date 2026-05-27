#pragma once

#include "Core/Types/CoreTypes.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Types/RenderTypes.h"
#include "Object/Ptr/WeakObjectPtr.h"

class UPrimitiveComponent;
class FShader;
class FMeshBuffer;
class FScene;
class UMaterial;
class FReferenceCollector;
struct FDrawCommandBuffer;
struct FFrameContext;

// ============================================================
// EPrimitiveProxyFlags — Owner 역참조 없이 프록시 타입/특성 식별
// ============================================================
enum class EPrimitiveProxyFlags : uint16
{
	None              = 0,
	PerViewportUpdate = 1 << 0, // 매 프레임 카메라 기반 갱신 (Billboard, Gizmo)
	FontBatched       = 1 << 1, // FFontGeometry 배칭 경로 (TextRender)
	Decal             = 1 << 2, // Decal 프록시 (Receiver 순회 필요)
	NeverCull         = 1 << 3, // Frustum culling 제외 (Gizmo 등)
	SupportsOutline   = 1 << 4, // 선택 시 아웃라인 지원
	ShowAABB          = 1 << 5, // 선택 시 AABB 표시
	EditorOnly        = 1 << 6, // 에디터 전용 — PIE/Game 월드에서 비가시
	WireShape         = 1 << 7, // 와이어프레임 Shape — EditorLines 패스에 라인 기여
	BoneDebug         = 1 << 8, // 본 디버그 프록시 (본 위치/방향 표시)
	StaticMesh        = 1 << 9,
	SkeletalMesh      = 1 << 10,
	Particle          = 1 << 11,
};

inline EPrimitiveProxyFlags  operator|(EPrimitiveProxyFlags A, EPrimitiveProxyFlags B)  { return static_cast<EPrimitiveProxyFlags>(static_cast<uint16>(A) | static_cast<uint16>(B)); }
inline EPrimitiveProxyFlags  operator&(EPrimitiveProxyFlags A, EPrimitiveProxyFlags B)  { return static_cast<EPrimitiveProxyFlags>(static_cast<uint16>(A) & static_cast<uint16>(B)); }
inline EPrimitiveProxyFlags& operator|=(EPrimitiveProxyFlags& A, EPrimitiveProxyFlags B) { A = A | B; return A; }
inline EPrimitiveProxyFlags& operator&=(EPrimitiveProxyFlags& A, EPrimitiveProxyFlags B) { A = A & B; return A; }
inline EPrimitiveProxyFlags  operator~(EPrimitiveProxyFlags A) { return static_cast<EPrimitiveProxyFlags>(~static_cast<uint16>(A)); }

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
	virtual ~FPrimitiveSceneProxy();

	virtual void AddReferencedObjects(FReferenceCollector& Collector);
	bool HasValidOwner() const;

	// ================================================================
	// 읽기 전용 인터페이스 (DrawCommandBuilder, RenderCollector용)
	// ================================================================

	// --- 식별 ---
	uint32                GetProxyId()    const { return ProxyId; }
	uint32                GetProxyGeneration() const { return ProxyGeneration; }
	EPrimitiveProxyFlags  GetProxyFlags() const { return ProxyFlags; }
	bool HasProxyFlag(EPrimitiveProxyFlags F) const { return (ProxyFlags & F) != EPrimitiveProxyFlags::None; }

	// --- 가시성 / 선택 / 그림자 ---
	bool IsVisible()    const { return bVisible; }
	bool IsSelected()   const { return bSelected; }
	bool CastsShadow()  const { return bCastShadow; }
	bool CastsShadowAsTwoSided() const { return bCastShadowAsTwoSided; }

	// --- 렌더 데이터 (DrawCommandBuilder가 읽음) ---
	ERenderPass        GetRenderPass()  const;
	FShader*           GetShader()      const;
	FMeshBuffer*       GetMeshBuffer()  const { return MeshBuffer; }

	const FPerObjectConstants&      GetPerObjectConstants() const { return PerObjectConstants; }
	const FBoundingBox&             GetCachedBounds()       const { return CachedBounds; }
	const FVector&                  GetCachedWorldPos()     const { return CachedWorldPos; }
	const TArray<FMeshSectionDraw>& GetSectionDraws()       const { return SectionDraws; }

	// --- PerObject CB 상태 ---
	void MarkPerObjectCBDirty()   const { bPerObjectCBDirty = true; }
	void ClearPerObjectCBDirty()  const { bPerObjectCBDirty = false; }
	bool NeedsPerObjectCBUpload() const { return bPerObjectCBDirty; }

	// --- LOD (RenderCollector에서 접근) ---
	uint32 GetCurrentLOD()         const { return CurrentLOD; }
	uint32 GetLastLODUpdateFrame() const { return LastLODUpdateFrame; }
	void   SetLastLODUpdateFrame(uint32 Frame) { LastLODUpdateFrame = Frame; }

	// ================================================================
	// 가상 갱신 인터페이스 (서브클래스가 오버라이드)
	// ================================================================
	virtual void UpdateTransform();
	virtual void UpdateMaterial();
	virtual void UpdateVisibility();
	virtual void UpdateMesh();
	virtual void UpdateLOD(uint32 /*LODLevel*/) {}
	virtual void UpdatePerViewport(const FFrameContext& /*Frame*/) {}

	virtual bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
		FDrawCommandBuffer& OutBuffer) const;

protected:
	// ================================================================
	// 서브클래스용 — Update*()에서 쓰기 가능한 캐시 데이터
	// ================================================================

	// Owner 접근 — valid owner만 반환한다. PendingKill/Garbage cleanup 경로는
	// GetOwnerEvenIfPendingKill()를 명시적으로 사용해야 한다.
	UPrimitiveComponent* GetOwner() const { return Owner.Get(); }
	UPrimitiveComponent* GetOwnerEvenIfPendingKill() const { return Owner.GetEvenIfPendingKill(); }

	// 프록시 특성 플래그 (서브클래스 생성자에서 설정)
	EPrimitiveProxyFlags ProxyFlags = EPrimitiveProxyFlags::SupportsOutline
	                                | EPrimitiveProxyFlags::ShowAABB;

	// 렌더 데이터 캐시 (Update*에서 갱신)
	FMeshBuffer* MeshBuffer = nullptr;

	// 기본 Material — Material 없는 프록시의 폴백
	UMaterial* DefaultMaterial = nullptr;

	FPerObjectConstants PerObjectConstants = {};
	FBoundingBox        CachedBounds;
	FVector             CachedWorldPos;

	TArray<FMeshSectionDraw>  SectionDraws;

	// 가시성 (서브클래스 UpdateVisibility/UpdatePerViewport에서 변경)
	bool bVisible = true;
	bool bCastShadow = true;
	bool bCastShadowAsTwoSided = false;

	// LOD (서브클래스 UpdateLOD에서 변경)
	uint32 CurrentLOD = 0;

private:
	// ================================================================
	// 내부 관리 상태 — FScene만 friend로 접근
	// ================================================================
	friend class FScene;

	TWeakObjectPtr<UPrimitiveComponent> Owner;

	FScene*		Scene			  = nullptr;
	uint32      ProxyId           = UINT32_MAX;
	uint32      ProxyGeneration   = 0;
	uint32      SelectedListIndex = UINT32_MAX;
	EDirtyFlag  DirtyFlags        = EDirtyFlag::All;
	bool        bQueuedForDirtyUpdate = false;
	bool        bSelected         = false;

	void MarkDirty(EDirtyFlag Flag)  { DirtyFlags |= Flag; }
	void ClearDirty(EDirtyFlag Flag) { DirtyFlags &= ~Flag; }

	uint32 LastLODUpdateFrame = UINT32_MAX;
	mutable bool bPerObjectCBDirty = true;
};
