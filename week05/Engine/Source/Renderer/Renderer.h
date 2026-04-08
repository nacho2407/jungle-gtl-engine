#pragma once

#include "CoreMinimal.h"
#include "Level/SceneRenderPacket.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RenderDevice.h"
#include "Renderer/Feature/DebugLineRenderFeature.h"
#include "Renderer/Feature/OutlineRenderFeature.h"
#include "Renderer/Feature/SubUVRenderFeature.h"
#include "Renderer/Feature/TextRenderFeature.h"
#include "Renderer/RenderFeatureInterfaces.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/ScreenUIRenderer.h"
#include "Renderer/UIDrawList.h"
#include "Renderer/ViewportCompositor.h"
#include "ShaderManager.h"

#include <d3d11.h>
#include <filesystem>
#include <memory>

struct FVertex;
struct FRenderMesh;
class FPixelShader;
class FMaterial;
class ULevel;

struct FSceneViewRenderRequest
{
	// 대상 씬 패스에서 사용할 카메라 뷰 행렬이다.
	FMatrix ViewMatrix = FMatrix::Identity;
	// 위 뷰 행렬과 짝을 이루는 투영 행렬이다.
	FMatrix ProjectionMatrix = FMatrix::Identity;
	// 빌보드나 카메라 정렬 프리미티브가 참조할 월드 공간 카메라 위치다.
	FVector CameraPosition = FVector::ZeroVector;
	// SubUV 같은 시간 기반 기능이 참조할 누적 프레임 시간이다.
	float TotalTimeSeconds = 0.0f;
};

struct FGameFrameRequest
{
	// 현재 게임 월드에서 수집한 씬 패킷이다.
	FSceneRenderPacket ScenePacket;
	// 씬 패스 실행에 필요한 카메라 데이터다.
	FSceneViewRenderRequest SceneView;
	// 씬 패킷 외부에서 따로 만든 추가 메시 커맨드 큐다.
	// 메인 씬 큐와 섞지 않고 별도 제출/실행 경로를 탄다.
	FRenderCommandQueue AdditionalCommands;
	// 프레임 끝에 덧그릴 디버그 라인 요청이다.
	FDebugLineRenderRequest DebugLineRequest;
	// 씬 머티리얼을 강제로 와이어프레임 머티리얼로 바꿀지 여부다.
	bool bForceWireframe = false;
	FMaterial* WireframeMaterial = nullptr;
	// 게임 씬 타깃을 비울 때 사용할 색상이다.
	float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
};

struct FViewportScenePassRequest
{
	// 이 뷰포트 씬 패스가 그릴 대상 렌더 서피스다.
	ID3D11RenderTargetView* RenderTargetView = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;
	D3D11_VIEWPORT Viewport = {};
	// 이 뷰포트에서 수집한 씬 패킷이다.
	FSceneRenderPacket ScenePacket;
	// 이 뷰포트 씬 패스에 대응하는 카메라 데이터다.
	FSceneViewRenderRequest SceneView;
	// 그리드나 기즈모처럼 씬 패킷과 분리된 추가 메시 커맨드 큐다.
	FRenderCommandQueue AdditionalCommands;
	// 씬 패스 뒤에 실행할 아웃라인 요청이다.
	FOutlineRenderRequest OutlineRequest;
	// 씬 패스 뒤에 실행할 디버그 라인 요청이다.
	FDebugLineRenderRequest DebugLineRequest;
	bool bForceWireframe = false;
	FMaterial* WireframeMaterial = nullptr;
	float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

	// 유효한 렌더 타깃과 뷰포트 크기가 들어 있으면 true를 반환한다.
	bool IsValid() const
	{
		return RenderTargetView != nullptr && DepthStencilView != nullptr && Viewport.Width > 0.0f && Viewport.Height > 0.0f;
	}
};

struct FEditorFrameRequest
{
	// 합성 전에 먼저 렌더해야 하는 오프스크린 씬 패스 목록이다.
	TArray<FViewportScenePassRequest> ScenePasses;
	// 뷰포트 결과 텍스처를 백버퍼에 배치하는 합성 단계 목록이다.
	TArray<FViewportCompositeItem> CompositeItems;
	// 마지막에 백버퍼 위로 그릴 화면 UI 드로우 리스트다.
	FUIDrawList ScreenDrawList;
};

/**
 * 렌더러 전체 프레임 진입점이다.
 * 실제 장면 렌더링, 화면 UI, 뷰포트 합성, 특수 기능 실행은 하위 서브시스템에 위임하고
 * 여기서는 프레임 순서와 데이터 연결만 조정한다.
 */
class ENGINE_API FRenderer
{
public:
	FRenderer(HWND InHwnd, int32 InWidth, int32 InHeight);
	~FRenderer();

	// 렌더러가 소유하는 코어 서브시스템과 공용 자원을 생성한다.
	bool Initialize(HWND InHwnd, int32 InWidth, int32 InHeight);
	// 새 프레임을 시작하고 프레임 단위 상태를 초기화한다.
	void BeginFrame();
	// 현재 프레임의 백버퍼를 화면에 출력한다.
	void EndFrame();
	// 렌더러가 소유한 GPU 자원을 모두 해제한다.
	void Release();
	// 스왑체인이 가려져 렌더를 건너뛰어야 하면 true를 반환한다.
	bool IsOccluded();
	// 창 크기 변경 후 백버퍼 크기 기반 자원을 다시 만든다.
	void OnResize(int32 NewWidth, int32 NewHeight);

	// 프레젠트 시 VSync 사용 여부를 설정한다.
	void SetVSync(bool bEnable) { RenderDevice.SetVSync(bEnable); }
	// 현재 VSync 설정 상태를 반환한다.
	bool IsVSyncEnabled() const { return RenderDevice.IsVSyncEnabled(); }

	// 최종 화면 UI 드로우 리스트를 백버퍼에 렌더링한다.
	bool RenderScreenUIDrawList(const FUIDrawList& DrawList);
	// 하나 이상의 뷰포트 결과 텍스처를 백버퍼에 합성한다.
	bool ComposeViewports(const TArray<FViewportCompositeItem>& Items);
	// 게임 프레임 요청 하나를 끝까지 렌더링한다.
	bool RenderGameFrame(const FGameFrameRequest& Request);
	// 에디터 프레임 요청 하나를 끝까지 렌더링한다.
	bool RenderEditorFrame(const FEditorFrameRequest& Request);
	// 현재 바인딩된 렌더 타깃 위에 디버그 라인을 그린다.
	bool RenderDebugLines(const FDebugLineRenderRequest& Request);

	// stb_image로 텍스처 파일을 읽어 SRV를 생성한다.
	bool CreateTextureFromSTB(ID3D11Device* Device, const char* FilePath, ID3D11ShaderResourceView** OutSRV);
	// stb_image로 텍스처 파일을 읽어 SRV를 생성한다.
	bool CreateTextureFromSTB(ID3D11Device* Device, const std::filesystem::path& FilePath, ID3D11ShaderResourceView** OutSRV);

	// 기본 단색 머티리얼을 반환한다.
	FMaterial* GetDefaultMaterial() const { return DefaultMaterial.get(); }
	// 기본 텍스처 머티리얼을 반환한다.
	FMaterial* GetDefaultTextureMaterial() const { return DefaultTextureMaterial.get(); }
	// 직전 프레임의 씬 커맨드 개수를 반환해 reserve 힌트로 사용한다.
	size_t GetPrevCommandCount() const;
	// 공용 렌더 상태 매니저에 접근한다.
	std::unique_ptr<FRenderStateManager>& GetRenderStateManager() { return RenderStateManager; }
	// D3D11 디바이스 접근자다.
	ID3D11Device* GetDevice() const { return RenderDevice.GetDevice(); }
	// D3D11 디바이스 컨텍스트 접근자다.
	ID3D11DeviceContext* GetDeviceContext() const { return RenderDevice.GetDeviceContext(); }
	// 현재 백버퍼 RTV 접근자다.
	ID3D11RenderTargetView* GetRenderTargetView() const { return RenderDevice.GetRenderTargetView(); }
	// 현재 백버퍼 DSV 접근자다.
	ID3D11DepthStencilView* GetDepthStencilView() const;
	// 스왑체인 접근자다.
	IDXGISwapChain* GetSwapChain() const { return RenderDevice.GetSwapChain(); }
	// 렌더 윈도우 핸들을 반환한다.
	HWND GetHwnd() const { return RenderDevice.GetHwnd(); }
	// 백버퍼 전체를 덮는 기본 뷰포트를 반환한다.
	const D3D11_VIEWPORT& GetBackBufferViewport() const { return RenderDevice.GetViewport(); }

	// 씬 텍스트 기능 인터페이스를 반환한다.
	ISceneTextFeature* GetSceneTextFeature() const { return TextFeature.get(); }
	// 씬 SubUV 기능 인터페이스를 반환한다.
	ISceneSubUVFeature* GetSceneSubUVFeature() const { return SubUVFeature.get(); }
	// 씬 렌더러에 접근한다.
	FSceneRenderer& GetSceneRenderer() { return SceneRenderer; }
	// 화면 UI 렌더러에 접근한다.
	FScreenUIRenderer& GetScreenUIRenderer() { return ScreenUIRenderer; }
	// 렌더 디바이스에 접근한다.
	FRenderDevice& GetRenderDevice() { return RenderDevice; }
	// 현재 ViewMatrix를 기준으로 카메라 월드 위치를 계산한다.
	FVector GetCameraPosition() const;

	// 에디터 폴더 아이콘 SRV를 반환한다.
	ID3D11ShaderResourceView* GetFolderIconSRV() const { return FolderIconSRV; }
	// 에디터 파일 아이콘 SRV를 반환한다.
	ID3D11ShaderResourceView* GetFileIconSRV() const { return FileIconSRV; }

private:
	friend class FSceneRenderer;
	friend class FOutlineRenderFeature;
	friend class FDebugLineRenderFeature;
	friend class FScreenUIRenderer;

	// 코어 씬 셰이더가 기대하는 프레임/오브젝트 상수 버퍼를 바인딩한다.
	void SetConstantBuffers();
	// 씬 렌더링과 디버그 렌더링이 함께 쓰는 상수 버퍼를 생성한다.
	bool CreateConstantBuffers();
	// 렌더러가 소유한 공용 머티리얼이 사용할 샘플러를 생성한다.
	bool CreateSamplers();
	// 현재 프레임 전체에 공통인 카메라/시간 상수를 업로드한다.
	void UpdateFrameConstantBuffer();
	// 현재 오브젝트의 월드 변환 행렬을 업로드한다.
	void UpdateObjectConstantBuffer(const FMatrix& WorldMatrix);
	// 현재 바인딩된 깊이 버퍼만 선택적으로 비운다.
	void ClearDepthBuffer();

private:
	std::unique_ptr<FRenderStateManager> RenderStateManager = nullptr;

	FRenderDevice RenderDevice;

	ID3D11Buffer* FrameConstantBuffer = nullptr;
	ID3D11Buffer* ObjectConstantBuffer = nullptr;

	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;

	std::shared_ptr<FMaterial> DefaultMaterial;
	std::shared_ptr<FMaterial> DefaultTextureMaterial;

	FSceneRenderer SceneRenderer;
	FViewportCompositor ViewportCompositor;
	FScreenUIRenderer ScreenUIRenderer;
	std::unique_ptr<FTextRenderFeature> TextFeature;
	std::unique_ptr<FSubUVRenderFeature> SubUVFeature;
	std::unique_ptr<FOutlineRenderFeature> OutlineFeature;
	std::unique_ptr<FDebugLineRenderFeature> DebugLineFeature;

	ID3D11ShaderResourceView* FolderIconSRV = nullptr;
	ID3D11ShaderResourceView* FileIconSRV = nullptr;
	ID3D11SamplerState* NormalSampler = nullptr;

public:
	FShaderManager ShaderManager;
};
