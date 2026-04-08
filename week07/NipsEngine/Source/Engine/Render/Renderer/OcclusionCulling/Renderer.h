#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/VertexTypes.h"

#include "Render/Scene/RenderBus.h"
// #include "Misc/ObjViewer/Render/RenderDataManager.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
// #include "Render/Renderer/OcclusionCulling.h"
#include "Render/LineBatcher.h"
#include "Render/FontBatcher.h"
#include "Render/SubUVBatcher.h"
#include "Core/Logging/Stats.h"

#include <cstddef>
#include <functional>

// 패스별 렌더 큐 정렬 통계 (CPU side, STATS 빌드 전용)
struct FRenderQueueStats
{
	uint32 CommandCount      = 0;  // 패스에 제출된 총 커맨드 수
	uint32 TypeChangeCount   = 0;  // 셰이더 타입 변경 횟수 (= 셰이더 재바인딩)
	uint32 MeshRebindCount   = 0;  // VB/IB 재바인딩 횟수
	uint32 SRVRebindCount    = 0;  // Texture SRV 재바인딩 횟수
	uint32 CBufferUpdateCount = 0; // cbuffer Map/Unmap 횟수
};

// 패스별 Batcher 바인딩 — Clear → Collect → Flush 패턴
struct FPassBatcherBinding
{
	std::function<void()> Clear;
	std::function<void(const FRenderCommand&, const FRenderBus&)> Collect;
	std::function<void(ERenderPass, const FRenderBus&, ID3D11DeviceContext*)> Flush;

	explicit operator bool() const { return Flush != nullptr; }
};

// 패스별 기본 렌더 상태 — Single Source of Truth
struct FPassRenderState
{
	EDepthStencilState       DepthStencil   = EDepthStencilState::Default;
	EBlendState              Blend          = EBlendState::Opaque;
	ERasterizerState         Rasterizer     = ERasterizerState::SolidBackCull;
	D3D11_PRIMITIVE_TOPOLOGY Topology       = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	FShader*                 Shader         = nullptr; // nullptr → batcher가 자체 셰이더 사용
	bool                     bWireframeAware = false;  // Wireframe 모드 시 래스터라이저 전환
};

class FRenderer
{
public:
	void Create(HWND hWindow);
	void Release();

	void PrepareBatchers(const FRenderBus& InRenderBus);
	void BeginFrame();
	void Render(const FRenderBus& InRenderBus);

	// Depth Prepass / Occlusion Cull에 쓸 현재 프레임 VP를 미리 설정
	// Render(FRenderDataManager&) 호출 직전에 호출해야 함
	void SetCurrentViewProjection(const FMatrix& InVP);
	void SetCurrentOcclusionCameraPosition(const FVector& InPosition);
	void SetCurrentOcclusionCameraBasis(const FVector& InForward, const FVector& InRight,
	                                    const FVector& InUp);
	void SetOcclusionCullingEnabled(bool bEnabled);

	void EndFrame();
	void UseBackBufferRenderTargets();
	void UseViewportRenderTargets();

	//대회용
	void Render(const FRenderDataManager& InRenderDataManager, const FRenderBus& InRenderBus);
	void RenderStaticMeshes(const TArray<FRenderCommand>& InRenderDataArray, ID3D11DeviceContext* InDeviceContext);

	FD3DDevice& GetFD3DDevice() { return Device; }
	FRenderResources& GetResources() { return Resources; }

#if STATS
	const FRenderQueueStats* GetPassStats() const { return PassStats; }
#endif
	const FOcclusionDebugState& GetOcclusionDebugState() const { return OcclusionDebugState; }

private:
	void InitializePassRenderStates();
	void InitializePassBatchers();

	void ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context, EViewMode ViewMode);
	void BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context);

	void DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand);
	void DrawPostProcessOutline(ID3D11DeviceContext* InDeviceContext);
	void ExecuteDepthPrepass(const TArray<FRenderCommand>& Commands, ID3D11DeviceContext* Context);
	void ExecuteHiZCopyMip0(ID3D11DeviceContext* Context);
	void ExecuteHiZDownsample(ID3D11DeviceContext* Context);
	void ExecuteOcclusionCull(
		ID3D11DeviceContext* Context,
		const TArray<uint32>& VisibleIndices,
		const TArray<FAABB>& AABBBoundsArray);
	bool TryConsumeOcclusionCullResult(ID3D11DeviceContext* Context, uint32 TotalObjectCount,
	                                   bool bApplyHideHysteresis);
	void BuildTemporalSurvivorIndices(const TArray<uint32>& VisibleIndices,
	                                  uint32               TotalObjectCount,
	                                  TArray<uint32>&      OutSurvivorIndices) const;
	void ResetOcclusionCullState();
	void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);

	// 기본 패스 실행기 — SetupRenderState + DrawCommand 루프
	void ExecuteDefaultPass(ERenderPass Pass, const TArray<FRenderCommand>& Commands, const FRenderBus& Bus, ID3D11DeviceContext* Context);

	// LineBatcher Flush 공통 — EditorConstants 업데이트 + EditorShader 바인딩
	void FlushLineBatcher(FLineBatcher& Batcher, ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Context);

private:
	FD3DDevice Device;
	FRenderTargetSet CurrentRenderTargets;
	FRenderResources Resources;
	FLineBatcher   EditorLineBatcher;
	FLineBatcher   GridLineBatcher;
	FFontBatcher   FontBatcher;
	FSubUVBatcher  SubUVBatcher;

	// 패스별 커맨드 정렬이 필요한 경우 정렬된 복사본 반환, 아니면 원본 참조
	const TArray<FRenderCommand>& GetAlignedCommands(ERenderPass Pass, const TArray<FRenderCommand>& Commands);
	TArray<FRenderCommand> SortedCommandBuffer;  // 재할당 방지용 멤버 버퍼

	FPassRenderState    PassRenderStates[(uint32)ERenderPass::MAX];
	FPassBatcherBinding PassBatchers[(uint32)ERenderPass::MAX];
	ID3D11ShaderResourceView* SubUVCachedSRV = nullptr;

	// Occlusion Culling GPU 버퍼 (Step 5)
	static constexpr uint32 MaxOcclusionObjects = 65536;
	static constexpr uint32 MinOcclusionCullObjects = 256;
	static constexpr uint32 MinOcclusionCullSavings = 64;
	static constexpr uint8  OcclusionHideHysteresisFrames = 11;
	TComPtr<ID3D11Buffer>              OcclusionAABBBuffer;
	TComPtr<ID3D11ShaderResourceView>  OcclusionAABBSRV;
	TComPtr<ID3D11Buffer>              OcclusionResultBuffer;
	TComPtr<ID3D11UnorderedAccessView> OcclusionResultUAV;
	TComPtr<ID3D11Buffer>              OcclusionResultStaging[2];  // 더블버퍼 async readback
	TComPtr<ID3D11Query>               OcclusionResultQueries[2];
	TArray<uint32>                     OcclusionSubmittedVisibleIndices[2];
	FMatrix                            OcclusionSubmittedViewProjection[2];
	TArray<uint32>                     CachedOcclusionVisibleIndices;
	TArray<uint32>                     CachedOcclusionSurvivorIndices;
	TArray<uint8>                      OcclusionConsecutiveOccludedCounts;
	uint32                             StagingWriteIdx = 0;        // 현재 프레임 write 인덱스
	uint32                             OcclusionPendingSlot = 0;
	bool                               bStagingValid   = false;    // 첫 프레임 여부

	FMatrix CachedViewProjection;  // UpdateFrameBuffer에서 갱신 (temporal occlusion)
	FVector CachedOcclusionCameraPosition = FVector::ZeroVector;
	FVector CachedOcclusionCameraForward = FVector::ForwardVector;
	FVector CachedOcclusionCameraRight = FVector::RightVector;
	FVector CachedOcclusionCameraUp = FVector::UpVector;
	FMatrix PreviousOcclusionViewProjection;
	FMatrix CachedOcclusionResultViewProjection;
	bool bHasPreviousOcclusionViewProjection = false;
	bool bHasCachedOcclusionResults = false;
	bool bHasAcceptedOcclusionResults = false;
	bool bOcclusionReadbackPending = false;
	bool bOcclusionCullingEnabled = true;
	FOcclusionDebugState OcclusionDebugState;

	// StaticMesh 바인딩 캐시 — 변경 시에만 재바인딩
	uint32                          LastComponentIndex = UINT32_MAX;
	ERenderCommandType				LastRenderCommandType = ERenderCommandType::None;
	FMeshBuffer*                    LastMeshBuffer = nullptr;
	ID3D11ShaderResourceView*       LastDiffuseSRV = nullptr;
	FPerformanceStaticMeshConstants LastPerfConst  = {};

#if STATS
	FRenderQueueStats PassStats[(uint32)ERenderPass::MAX] = {};
#endif STATS

	//	Primitive and Gizmo Input Layout
	D3D11_INPUT_ELEMENT_DESC PrimitiveInputLayout[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  static_cast<uint32>(offsetof(FVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	// StaticMesh (FNormalVertex) Input Layout — Normal 필드 제거됨
	D3D11_INPUT_ELEMENT_DESC NormalVertexInputLayout[3] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<uint32>(offsetof(FNormalVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, Color)),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, static_cast<uint32>(offsetof(FNormalVertex, UVs)),      D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	// PerformanceStaticMesh 전용 Input Layout — FPerformanceVertex(16 bytes) 기준
	// Position(float3) + UV(R16G16_FLOAT half2) = 16 bytes/vertex
	D3D11_INPUT_ELEMENT_DESC PerformanceMeshInputLayout[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FPerformanceVertex, Position)),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT,    0, static_cast<uint32>(offsetof(FPerformanceVertex, PackedUV)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	// Depth Prepass 전용 Input Layout — Position만 선언 (FPerformanceVertex VB와 호환, stride=16)
	D3D11_INPUT_ELEMENT_DESC DepthPrepassInputLayout[1] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
};

