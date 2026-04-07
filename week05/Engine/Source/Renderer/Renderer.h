#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/SubUVRenderer.h"
#include "Renderer/TextMeshBuilder.h"
#include "ShaderManager.h"
#include <d3d11.h>
#include <dxgi1_5.h>
#include <filesystem>
#include <functional>
#include <memory>

struct FVertex;
struct FRenderMesh;
struct FMeshDrawCommand;
class FPixelShader;
class FComputeShader;
class FMaterial;
class UScene;
class FSceneRenderer;
class FPassExecutor;
class FObjectUniformStream;
struct FSceneRenderFrame;

struct ENGINE_API FDepthStencilTextureResources
{
	ID3D11Texture2D* Texture = nullptr;
	ID3D11DepthStencilView* DSV = nullptr;
	ID3D11ShaderResourceView* DepthSRV = nullptr;
	uint32 Width = 0;
	uint32 Height = 0;
};

struct ENGINE_API FHZBResources
{
	ID3D11Texture2D* Texture = nullptr;
	ID3D11ShaderResourceView* FullSRV = nullptr;
	TArray<ID3D11ShaderResourceView*> MipSRVs;
	TArray<ID3D11UnorderedAccessView*> MipUAVs;
	uint32 Width = 0;
	uint32 Height = 0;
	uint32 MipCount = 0;
};

struct ENGINE_API FHZBBuildConstants
{
	uint32 SourceWidth = 0;
	uint32 SourceHeight = 0;
	uint32 DestWidth = 0;
	uint32 DestHeight = 0;
};

struct ENGINE_API FGpuOcclusionCandidate
{
	FVector BoundsCenter = FVector::ZeroVector;
	float BoundsRadius = 0.0f;
	FVector BoundsExtent = FVector::ZeroVector;
	uint32 DenseIndex = 0;
};

struct ENGINE_API FOcclusionPassConstants
{
	FMatrix View = FMatrix::Identity;
	FMatrix Projection = FMatrix::Identity;
	FMatrix ViewProjection = FMatrix::Identity;

	uint32 ViewWidth = 0;
	uint32 ViewHeight = 0;
	uint32 CandidateCount = 0;
	uint32 HZBMipCount = 0;

	// float DepthBias = 0.0000125f;// Flickering 생기는 bias
	float DepthBias = 25e-7f;		// 가장 이상적인 bias
	// float DepthBias = 0.001f;	// Flickering이 생기지 않지만, 컬링이 안되는 bias
	float NearPlaneEpsilon = 0.0001f;
	float Padding[2] = {};
};

struct ENGINE_API FStaticMeshOcclusionGpuResources
{
	ID3D11Buffer* CandidateBuffer = nullptr;
	ID3D11ShaderResourceView* CandidateSRV = nullptr;
	ID3D11Buffer* VisibilityBuffer = nullptr;
	ID3D11UnorderedAccessView* VisibilityUAV = nullptr;
	uint32 CandidateCapacity = 0;
};

struct ENGINE_API FStaticMeshOcclusionReadbackSlot
{
	ID3D11Buffer* StagingBuffer = nullptr;
	uint32 CandidateCapacity = 0;
	uint32 CandidateCount = 0;
	uint64 FrameSerial = 0;
	bool bCopyIssued = false;
	bool bCompleted = false;
	FStaticMeshOcclusionFrameSnapshot Snapshot;
	TArray<uint32> VisibilityValues;
	uint32 VisibleCount = 0;
	uint32 OccludedCount = 0;

	void ClearFrameState()
	{
		CandidateCount = 0;
		FrameSerial = 0;
		bCopyIssued = false;
		bCompleted = false;
		Snapshot.Clear();
		VisibilityValues.clear();
		VisibleCount = 0;
		OccludedCount = 0;
	}
};

struct ENGINE_API FStaticMeshOcclusionReadbackResult
{
	FStaticMeshOcclusionFrameSnapshot Snapshot;
	TArray<uint32> VisibilityValues;
	uint32 CandidateCount = 0;
	uint32 VisibleCount = 0;
	uint32 OccludedCount = 0;
	uint64 FrameSerial = 0;
	bool bReady = false;
	bool bSizeMatched = false;

	void Reset()
	{
		Snapshot.Clear();
		VisibilityValues.clear();
		CandidateCount = 0;
		VisibleCount = 0;
		OccludedCount = 0;
		FrameSerial = 0;
		bReady = false;
		bSizeMatched = false;
	}
};

using FGUICallback = std::function<void()>;
class FRenderer;
using FPostRenderCallback = std::function<void(FRenderer*)>;

/**
 * Direct3D 11 기반의 메인 렌더러다.
 * 렌더 커맨드 큐를 받아 GPU 상태를 갱신하고, 장면/오버레이/UI 패스를 순서대로 실행한다.
 */
class ENGINE_API FRenderer
{
	friend class FSceneRenderer;
	friend class FPassExecutor;

public:
	FRenderer(HWND InHwnd, int32 InWidth, int32 InHeight);
	~FRenderer();

	/** D3D11 디바이스, 스왑체인, 기본 머티리얼 등 렌더러가 필요한 모든 자원을 만든다. */
	bool Initialize(HWND InHwnd, int32 InWidth, int32 InHeight);

	/** 프레임 시작 시 렌더 타깃을 비우고, GUI 새 프레임과 커맨드 목록 초기화를 수행한다. */
	void BeginFrame();

	/** GUI를 그린 뒤 Present를 호출해 백버퍼를 화면에 출력한다. */
	void EndFrame();

	/** 렌더러가 잡고 있는 모든 GPU/CPU 자원을 해제한다. */
	void Release();

	/** 창이 가려진 상태라 Present가 불필요한지 검사한다. */
	bool IsOccluded();

	/** 백버퍼와 깊이 버퍼를 새 해상도에 맞게 다시 생성한다. */
	void OnResize(int32 NewWidth, int32 NewHeight);

	/** 특정 뷰포트용 렌더 타깃을 임시로 사용하도록 설정한다. */
	void SetSceneRenderTarget(ID3D11RenderTargetView* InRenderTargetView, ID3D11DepthStencilView* InDepthStencilView, ID3D11ShaderResourceView* InDepthShaderResourceView, const D3D11_VIEWPORT& InViewport);
	/** 임시 씬 렌더 타깃 오버라이드를 해제하고 스왑체인 백버퍼로 되돌린다. */
	void ClearSceneRenderTarget();

	/** 외부 패스가 자체 RTV/DSV/뷰포트를 쓰고 싶을 때 렌더 상태를 그쪽으로 전환한다. */
	void BeginScenePass(ID3D11RenderTargetView* InRTV, ID3D11DepthStencilView* InDSV, ID3D11ShaderResourceView* InDepthSRV, const D3D11_VIEWPORT& InVP);
	/** BeginScenePass와 쌍을 이루는 종료 훅이다. 현재는 자리만 잡아둔 상태다. */
	void EndScenePass();
	/** 렌더 타깃을 다시 스왑체인 백버퍼로 바인딩한다. */
	void BindSwapChainRTV();

	void SetVSync(bool bEnable) { bVSyncEnabled = bEnable; }
	bool IsVSyncEnabled() const { return bVSyncEnabled; }

	bool bSwapChainOccluded = false;

	/** ImGui 같은 GUI 시스템의 초기화/프레임/렌더 콜백을 렌더러에 등록한다. */
	void SetGUICallbacks(FGUICallback InInit, FGUICallback InShutdown, FGUICallback InNewFrame, FGUICallback InRender, FGUICallback InPostPresent = nullptr);
	/** 현재 뷰포트가 등록한 GUI 콜백을 전부 제거한다. */
	void ClearViewportCallbacks();
	/** GUI 프레임 중 논리 상태 갱신만 따로 실행하고 싶을 때 등록한다. */
	void SetGUIUpdateCallback(FGUICallback InUpdate);
	/** 3D 패스 이후 추가 후처리를 실행할 콜백을 등록한다. */
	void SetPostRenderCallback(FPostRenderCallback InCallback) { PostRenderCallback = std::move(InCallback); }

	/** 게임/에디터가 수집한 렌더 커맨드를 내부 커맨드 리스트로 복사한다. */
	void SubmitCommands(const FRenderCommandQueue& Queue);
	void SubmitCommands(FRenderCommandQueue&& Queue);
	/** 커맨드 리스트를 정렬하고 패스별로 GPU 드로우콜을 실행한다. */
	void ExecuteCommands();

	/** 지정한 렌더 레이어 하나만 골라 실제 드로우콜을 수행한다. */
	/** 디버그 선 하나를 임시 버퍼에 추가한다. */
	void DrawLine(const FVector& Start, const FVector& End, const FVector4& Color);
	/** 박스 외곽선을 선 목록으로 변환해 추가한다. */
	void DrawCube(const FVector& Center, const FVector& BoxExtent, const FVector4& Color);
	/** 누적된 디버그 선 버퍼를 한 번에 렌더링한다. */
	void ExecuteLineCommands();

	/** 외곽선 마스크/후처리 셰이더 등 외곽선 렌더링에 필요한 리소스를 준비한다. */
	bool InitOutlineResources();
	/** 선택된 메시 목록을 외곽선 패스로 렌더링한다. */
	void RenderOutlines(const TArray<FOutlineRenderItem>& Items);

	// Texture 생성 경로를 임시로 렌더러에 두고 있다. 이후 TextureManager가 생기면 분리될 가능성이 있다.
	bool CreateTextureFromSTB(ID3D11Device* Device, const char* FilePath, ID3D11ShaderResourceView** OutSRV);
	bool CreateTextureFromSTB(ID3D11Device* Device, const std::filesystem::path& FilePath, ID3D11ShaderResourceView** OutSRV);

	/** 렌더러가 기본으로 사용하는 단색 머티리얼을 반환한다. */
	FMaterial* GetDefaultMaterial() const { return DefaultMaterial.get(); }
	/** 렌더러가 기본으로 사용하는 텍스처 머티리얼을 반환한다. */
	FMaterial* GetDefaultTextureMaterial() const { return DefaultTextureMaterial.get(); }
	/** 직전 프레임의 커맨드 개수를 반환해 다음 프레임 reserve 힌트로 사용한다. */
	size_t GetPrevCommandCount() const { return PrevCommandCount; }
	uint32 GetFrameDrawCallCount() const { return FrameDrawCallCount; }
	uint32 GetFrameStaticMeshDrawCallCount() const;
	uint32 GetFrameStaticMeshSkippedDrawCallCount() const { return FrameStaticMeshSkippedDrawCallCount; }
	uint32 GetFrameStaticMeshPotentialDrawCallCount() const { return FrameStaticMeshPotentialDrawCallCount; }
	uint32 GetFrameStaticMeshSkippedBeforeBuildDrawCommandsCount() const { return FrameStaticMeshSkippedBeforeBuildDrawCommandsCount; }
	uint32 GetFrameStaticMeshSkippedLateDrawCount() const { return FrameStaticMeshSkippedLateDrawCount; }
	std::unique_ptr<FRenderStateManager>& GetRenderStateManager() { return RenderStateManager; }
	ID3D11Device* GetDevice() const { return Device; }
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }
	ID3D11RenderTargetView* GetRenderTargetView() const { return RenderTargetView; }
	ID3D11DepthStencilView* GetDepthStencilView() const;
	ID3D11ShaderResourceView* GetDepthShaderResourceView() const;
	ID3D11ShaderResourceView* GetActiveSceneDepthShaderResourceView() const { return ActiveSceneDepthShaderResourceView; }
	IDXGISwapChain* GetSwapChain() const { return SwapChain; }
	HWND GetHwnd() const { return Hwnd; }

	FTextMeshBuilder& GetTextRenderer() { return TextRenderer; }
	FSubUVRenderer& GetSubUVRenderer() { return SubUVRenderer; }
	/** 현재 ViewMatrix를 역변환해 카메라 월드 위치를 반환한다. */
	FVector GetCameraPosition() const;

	ID3D11ShaderResourceView* GetFolderIconSRV() const { return FolderIconSRV; }
	ID3D11ShaderResourceView* GetFileIconSRV() const { return FileIconSRV; }

private:
	/** 프레임/오브젝트 상수 버퍼를 셰이더 슬롯에 연결한다. */
	void SetConstantBuffers();
	/** 내부 커맨드 리스트를 비우고 다음 프레임용 예약 크기를 유지한다. */
	void ClearCommandList();
	/** D3D11 디바이스와 스왑체인을 생성한다. */
	bool CreateDeviceAndSwapChain(HWND InHwnd, int32 Width, int32 Height);
	/** 현재 해상도용 백버퍼 RTV와 깊이 버퍼를 만든다. */
	bool CreateRenderTargetAndDepthStencil(int32 Width, int32 Height);
	bool CreateDepthStencilTextureResources(int32 Width, int32 Height, FDepthStencilTextureResources& OutResources);
	void ReleaseDepthStencilTextureResources(FDepthStencilTextureResources& InOutResources);
	bool CreateHZBConstantBuffer();
	bool CreateOcclusionConstantBuffer();
	/** 프레임/오브젝트/외곽선 상수 버퍼를 생성한다. */
	bool CreateConstantBuffers();
	/** 기본 텍스처 샘플러와 외곽선 샘플러를 생성한다. */
	bool CreateSamplers();
	/** 외곽선 마스크용 오프스크린 텍스처를 필요한 크기로 보장한다. */
	bool EnsureOutlineMaskResources(uint32 Width, uint32 Height);
	/** 외곽선 마스크 렌더 타깃 관련 자원을 해제한다. */
	void ReleaseOutlineMaskResources();
	/** 현재 시스템/드라이버가 DXGI tearing present를 지원하는지 검사한다. */
	bool CheckTearingSupport() const;
	/** 현재 View/Projection과 시간 정보를 프레임 상수 버퍼에 업로드한다. */
	void UpdateFrameConstantBuffer();
	/** 개별 오브젝트의 월드 행렬을 오브젝트 상수 버퍼에 업로드한다. */
	void UpdateObjectConstantBuffer(const FMatrix& WorldMatrix);
	/** 외곽선 후처리 색상/두께/임계값을 픽셀 셰이더 상수 버퍼에 업로드한다. */
	void UpdateOutlinePostConstantBuffer(const FVector4& OutlineColor, float OutlineThickness, float OutlineThreshold);
	/** 오버레이 패스 전에 깊이 버퍼만 선택적으로 비운다. */
	void ClearDepthBuffer();
	bool EnsureHZBResources(uint32 Width, uint32 Height);
	void ReleaseHZBResources();
	bool InitializeHZBShaders();
	void UpdateHZBConstantBuffer(const FHZBBuildConstants& Constants);
	void BuildHZB();
	bool EnsureStaticMeshOcclusionResources(uint32 CandidateCount);
	void ReleaseStaticMeshOcclusionResources();
	bool InitializeOcclusionShaders();
	void BuildStaticMeshOcclusionSnapshot();
	bool UploadStaticMeshOcclusionCandidates();
	void UpdateOcclusionConstantBuffer(const FOcclusionPassConstants& Constants);
	void ExecuteStaticMeshOcclusionPass();

public:
	bool ShouldSkipStaticMeshCandidate(uint32 CandidateIndex) const;
	bool ShouldSkipStaticMeshCandidate(const FStaticMeshOcclusionCandidate& Candidate, uint32 CandidateIndex) const;
	void RegisterStaticMeshBuiltDrawCommands(FRenderMesh* RenderMesh);
	void RegisterStaticMeshSkippedBeforeBuild(FRenderMesh* RenderMesh);

private:
	bool EnsureStaticMeshOcclusionReadbackBuffer(FStaticMeshOcclusionReadbackSlot& InOutSlot, uint32 CandidateCount);
	FStaticMeshOcclusionReadbackSlot* AcquireStaticMeshOcclusionReadbackSlot();
	void IssueStaticMeshOcclusionReadback();
	void PollStaticMeshOcclusionReadback();
	void ReleaseStaticMeshOcclusionReadbackResources();
	uint32 EstimateStaticMeshDrawCallCount(const FRenderMesh* RenderMesh) const;

private:
	std::unique_ptr<FRenderStateManager> RenderStateManager = nullptr;
	std::unique_ptr<FSceneRenderer> SceneRenderer = nullptr;
	std::unique_ptr<FPassExecutor> PassExecutor = nullptr;
	std::unique_ptr<FObjectUniformStream> ObjectUniformStream = nullptr;
	std::unique_ptr<FSceneRenderFrame> CurrentRenderFrame = nullptr;

	HWND Hwnd = nullptr;
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;
	ID3D11RenderTargetView* RenderTargetView = nullptr;
	FDepthStencilTextureResources DepthTextureResources;

	ID3D11Buffer* FrameConstantBuffer = nullptr;
	ID3D11Buffer* ObjectConstantBuffer = nullptr;
	ID3D11Buffer* OutlinePostConstantBuffer = nullptr;
	ID3D11Buffer* HZBConstantBuffer = nullptr;
	ID3D11Buffer* OcclusionConstantBuffer = nullptr;

	FMatrix ViewMatrix = FMatrix::Identity;
	FMatrix ProjectionMatrix = FMatrix::Identity;
	D3D11_VIEWPORT Viewport = {};

	ID3D11RenderTargetView* SceneRenderTargetView = nullptr;
	ID3D11DepthStencilView* SceneDepthStencilView = nullptr;
	ID3D11ShaderResourceView* SceneDepthShaderResourceView = nullptr;
	ID3D11ShaderResourceView* ActiveSceneDepthShaderResourceView = nullptr;
	D3D11_VIEWPORT SceneViewport = {};
	bool bUseSceneRenderTargetOverride = false;
	bool bVSyncEnabled = false;
	bool bAllowTearing = false;

	/** 이번 프레임에 실제 실행할 렌더 커맨드 리스트다. */
	FRenderCommandQueue PendingCommandQueue;
	size_t PrevCommandCount = 0;
	uint32 FrameDrawCallCount = 0;
	uint32 FrameStaticMeshDrawCallCount = 0;
	uint32 FrameStaticMeshSkippedDrawCallCount = 0;
	uint32 FrameStaticMeshPotentialDrawCallCount = 0;
	uint32 FrameStaticMeshSkippedBeforeBuildDrawCommandsCount = 0;
	uint32 FrameStaticMeshSkippedLateDrawCount = 0;

	/** 디버그 선 렌더링을 위한 임시 CPU/GPU 버퍼다. */
	TArray<FVertex> LineVertices;
	ID3D11Buffer* LineVertexBuffer = nullptr;
	UINT LineVertexBufferSize = 0;

	/** 외곽선 렌더링에 필요한 스텐실/블렌드/셰이더 자원들이다. */
	ID3D11DepthStencilState* StencilWriteState = nullptr;
	ID3D11DepthStencilState* StencilEqualState = nullptr;
	ID3D11DepthStencilState* StencilNotEqualState = nullptr;
	ID3D11BlendState* OutlineBlendState = nullptr;
	ID3D11RasterizerState* OutlineRasterizerState = nullptr;
	ID3D11SamplerState* OutlineSampler = nullptr;
	ID3D11VertexShader* OutlinePostVS = nullptr;
	ID3D11PixelShader* OutlineMaskPS = nullptr;
	ID3D11PixelShader* OutlineSobelPS = nullptr;
	ID3D11Texture2D* OutlineMaskTexture = nullptr;
	ID3D11RenderTargetView* OutlineMaskRTV = nullptr;
	ID3D11ShaderResourceView* OutlineMaskSRV = nullptr;
	uint32 OutlineMaskWidth = 0;
	uint32 OutlineMaskHeight = 0;

	FGUICallback GUIInit;
	FGUICallback GUIShutdown;
	FGUICallback GUINewFrame;
	FGUICallback GUIUpdate;
	FGUICallback GUIRender;
	FGUICallback GUIPostPresent;
	FPostRenderCallback PostRenderCallback;

	/** 기본 메시에 fallback으로 사용할 머티리얼이다. */
	std::shared_ptr<FMaterial> DefaultMaterial;
	std::shared_ptr<FMaterial> DefaultTextureMaterial;

	FTextMeshBuilder TextRenderer;
	FSubUVRenderer SubUVRenderer;

	ID3D11ShaderResourceView* FolderIconSRV = nullptr;
	ID3D11ShaderResourceView* FileIconSRV = nullptr;

	/** SubUV, Text 등 텍스처 샘플링이 필요한 패스에서 사용하는 기본 샘플러다. */
	ID3D11SamplerState* NormalSampler = nullptr;
	FHZBResources HZBResources;
	FStaticMeshOcclusionGpuResources StaticMeshOcclusionResources;
	FStaticMeshOcclusionFrameSnapshot StaticMeshOcclusionSnapshot;
	TArray<FGpuOcclusionCandidate> GpuOcclusionCandidatesScratch;
	static constexpr uint32 OcclusionReadbackSlotCount = 3;
	FStaticMeshOcclusionReadbackSlot StaticMeshOcclusionReadbackSlots[OcclusionReadbackSlotCount];
	FStaticMeshOcclusionReadbackResult LatestStaticMeshOcclusionReadbackResult;
	uint32 NextOcclusionReadbackSlotIndex = 0;
	uint64 OcclusionFrameSerial = 0;
	std::shared_ptr<FComputeShader> HZBInitializeComputeShader;
	std::shared_ptr<FComputeShader> HZBReduceComputeShader;
	std::shared_ptr<FComputeShader> StaticMeshOcclusionComputeShader;

public:
	FShaderManager ShaderManager;
};
