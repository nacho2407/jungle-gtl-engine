#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/TextMeshBuilder.h"
#include "Renderer/SubUVRenderer.h"
#include "ShaderManager.h"
#include "Primitive/PrimitiveBase.h"
#include "PrimitiveVertex.h"
#include <d3d11.h>
#include <functional>
#include <memory>

class FPixelShader;
class FMaterial;
class UScene;

using FGUICallback = std::function<void()>;
class CRenderer;
using FPostRenderCallback = std::function<void(CRenderer*)>;

/**
 * 엔진의 핵심 렌더링 시스템
 * 렌더링 정책에 따라 제출된 명령들을 GPU에서 실행함
 */
class ENGINE_API CRenderer
{
public:
	CRenderer(HWND InHwnd, int32 InWidth, int32 InHeight);
	~CRenderer();

	/** 시스템 초기화 및 D3D11 장치 생성 */
	bool Initialize(HWND InHwnd, int32 InWidth, int32 InHeight);
	
	/** 프레임 시작 처리 (렌더 타겟 클리어 등) */
	void BeginFrame();
	
	/** 프레임 종료 처리 (Present) */
	void EndFrame();
	
	/** 리소스 해제 */
	void Release();
	
	/** 화면 가림 여부 확인 */
	bool IsOccluded();
	
	/** 뷰포트 크기 변경 대응 */
	void OnResize(int32 NewWidth, int32 NewHeight);
	
	/** 씬 렌더 타겟 설정 (외부 오버라이드용) */
	void SetSceneRenderTarget(ID3D11RenderTargetView* InRenderTargetView, ID3D11DepthStencilView* InDepthStencilView, const D3D11_VIEWPORT& InViewport);
	void ClearSceneRenderTarget();

	void SetVSync(bool bEnable) { bVSyncEnabled = bEnable; }
	bool IsVSyncEnabled() const { return bVSyncEnabled; }

	bool bSwapChainOccluded = false;
	// ─── GUI 및 콜백 ───
	/** ImGui 등 외부 GUI 시스템 연동용 콜백 */
	void SetGUICallbacks(FGUICallback InInit, FGUICallback InShutdown, FGUICallback InNewFrame, FGUICallback InRender, FGUICallback InPostPresent = nullptr);
	void ClearViewportCallbacks();
	void SetGUIUpdateCallback(FGUICallback InUpdate);
	void SetPostRenderCallback(FPostRenderCallback InCallback) { PostRenderCallback = std::move(InCallback); }

	// ─── 명령 실행 ───
	/** 커맨드 큐 제출 및 GPU 버퍼 업데이트 */
	void SubmitCommands(const FRenderCommandQueue& Queue);
	
	/** 수집된 커맨드 정렬 및 실행 */
	void ExecuteCommands();
	
	/** 특정 레이어의 명령들을 실제 드로우 콜로 변환 */
	void ExecuteRenderPass(ERenderLayer RenderLayer);

	// ─── 디버그 및 라인 렌더링 ───
	void DrawLine(const FVector& Start, const FVector& End, const FVector4& Color);
	void DrawCube(const FVector& Center, const FVector& BoxExtent, const FVector4& Color);
	void ExecuteLineCommands();

	// ─── 특수 효과 ───
	/** 선택된 오브젝트 등의 아웃라인 렌더링 */
	bool InitOutlineResources();
	void RenderOutline(FMeshData* Mesh, const FMatrix& WorldMatrix, float OutlineScale = 1.05f);

	// ─── 접근자 ───
	FMaterial* GetDefaultMaterial() const { return DefaultMaterial.get(); }
	size_t GetPrevCommandCount() const { return PrevCommandCount; }
	std::unique_ptr<CRenderStateManager>& GetRenderStateManager() { return RenderStateManager; }
	ID3D11Device* GetDevice() const { return Device; }
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }
	ID3D11RenderTargetView* GetRenderTargetView() const { return RenderTargetView; }
	IDXGISwapChain* GetSwapChain() const { return SwapChain; };
	HWND GetHwnd() const { return Hwnd; }

	CTextMeshBuilder& GetTextRenderer() { return TextRenderer; }
	CSubUVRenderer& GetSubUVRenderer() { return SubUVRenderer; }
	FVector GetCameraPosition() const;

	ID3D11ShaderResourceView* GetFolderIconSRV() const { return FolderIconSRV; }
	ID3D11ShaderResourceView* GetFileIconSRV() const { return FileIconSRV; }

private:
	void SetConstantBuffers();
	void AddCommand(const FRenderCommand& Command);
	void ClearCommandList();
	bool CreateDeviceAndSwapChain(HWND InHwnd, int32 Width, int32 Height);
	bool CreateRenderTargetAndDepthStencil(int32 Width, int32 Height);
	bool CreateConstantBuffers();
	void UpdateFrameConstantBuffer();
	void UpdateObjectConstantBuffer(const FMatrix& WorldMatrix);
	void ClearDepthBuffer();

	bool CreateTextureFromSTB(ID3D11Device* Device, const char* FilePath, ID3D11ShaderResourceView** OutSRV);

private:
	std::unique_ptr<CRenderStateManager> RenderStateManager = nullptr;

	HWND Hwnd = nullptr;
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;
	ID3D11RenderTargetView* RenderTargetView = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;
	
	ID3D11Buffer* FrameConstantBuffer = nullptr;
	ID3D11Buffer* ObjectConstantBuffer = nullptr;
	
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	D3D11_VIEWPORT Viewport = {};
	
	ID3D11RenderTargetView* SceneRenderTargetView = nullptr;
	ID3D11DepthStencilView* SceneDepthStencilView = nullptr;
	D3D11_VIEWPORT SceneViewport = {};
	bool bUseSceneRenderTargetOverride = false;
	bool bVSyncEnabled = false;

	/** 통합된 렌더링 명령 리스트 */
	TArray<FRenderCommand> CommandList;
	size_t PrevCommandCount = 0;

	/** 라인 렌더링용 임시 리소스 */
	TArray<FPrimitiveVertex> LineVertices;
	ID3D11Buffer* LineVertexBuffer = nullptr;
	UINT LineVertexBufferSize = 0;

	/** 아웃라인(스텐실) 리소스 */
	ID3D11DepthStencilState* StencilWriteState = nullptr;
	ID3D11DepthStencilState* StencilTestState = nullptr;
	std::shared_ptr<FPixelShader> OutlinePS;

	FGUICallback GUIInit;
	FGUICallback GUIShutdown;
	FGUICallback GUINewFrame;
	FGUICallback GUIUpdate;
	FGUICallback GUIRender;
	FGUICallback GUIPostPresent;
	FPostRenderCallback PostRenderCallback;

	/** 기본 공유 리소스 */
	std::shared_ptr<FMaterial> DefaultMaterial;
	std::shared_ptr<FMaterial> DefaultTextureMaterial;

	CTextMeshBuilder TextRenderer;
	CSubUVRenderer SubUVRenderer;

	ID3D11ShaderResourceView* FolderIconSRV = nullptr;
	ID3D11ShaderResourceView* FileIconSRV = nullptr;

	/** SubUV, Text 이외 일반 material texture sample 용도 */
	ID3D11SamplerState* NormalSampler = nullptr;

public:
	CShaderManager ShaderManager;
};
