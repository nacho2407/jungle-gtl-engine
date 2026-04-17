#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/VertexTypes.h"

#include "Render/Scene/RenderBus.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Render/LineBatcher.h"
#include "Render/FontBatcher.h"
#include "Render/SubUVBatcher.h"

#include <cstddef>
#include <functional>

#include "Render/Renderer/RenderFlow/RenderPipeline.h"

class FSceneViewport;

/**
 * Renderer 가 Viewport 별로 소유하는 데이터를 나타내는 구조체
 */
struct FViewportRenderResource
{
    TComPtr<ID3D11Texture2D> ColorTex;
    TComPtr<ID3D11RenderTargetView> ColorRTV;
    TComPtr<ID3D11ShaderResourceView> ColorSRV;

    TComPtr<ID3D11Texture2D> NormalTex;
    TComPtr<ID3D11RenderTargetView> NormalRTV;
    TComPtr<ID3D11ShaderResourceView> NormalSRV;

    TComPtr<ID3D11Texture2D> LightTex;
    TComPtr<ID3D11RenderTargetView> LightRTV;
    TComPtr<ID3D11ShaderResourceView> LightSRV;

    TComPtr<ID3D11Texture2D> FogTex;
    TComPtr<ID3D11RenderTargetView> FogRTV;
    TComPtr<ID3D11ShaderResourceView> FogSRV;

    TComPtr<ID3D11Texture2D> WorldPosTex;
    TComPtr<ID3D11RenderTargetView> WorldPosRTV;
    TComPtr<ID3D11ShaderResourceView> WorldPosSRV;

    TComPtr<ID3D11Texture2D> FXAATex;
    TComPtr<ID3D11RenderTargetView> FXAARTV;
    TComPtr<ID3D11ShaderResourceView> FXAASRV;

    TComPtr<ID3D11Texture2D> SelectionMaskTex;
    TComPtr<ID3D11RenderTargetView> SelectionMaskRTV;
    TComPtr<ID3D11ShaderResourceView> SelectionMaskSRV;

    TComPtr<ID3D11Texture2D> DepthTex;
    TComPtr<ID3D11DepthStencilView> DepthStencilView;
    TComPtr<ID3D11ShaderResourceView> DepthStencilSRV;

    uint32 Width = 0;
    uint32 Height = 0;

    FRenderTargetSet RenderTargetSet;

	FRenderTargetSet& GetView()
    {
        RenderTargetSet.SceneColorRTV = ColorRTV.Get();
        RenderTargetSet.SceneColorSRV = ColorSRV.Get();

        RenderTargetSet.SceneNormalRTV = NormalRTV.Get();
        RenderTargetSet.SceneNormalSRV = NormalSRV.Get();

        RenderTargetSet.SceneLightRTV = LightRTV.Get();
        RenderTargetSet.SceneLightSRV = LightSRV.Get();

        RenderTargetSet.SceneFogRTV = FogRTV.Get();
        RenderTargetSet.SceneFogSRV = FogSRV.Get();

        RenderTargetSet.SceneWorldPosRTV = WorldPosRTV.Get();
        RenderTargetSet.SceneWorldPosSRV = WorldPosSRV.Get();

        RenderTargetSet.SceneFXAARTV = FXAARTV.Get();
        RenderTargetSet.SceneFXAASRV = FXAASRV.Get();

        RenderTargetSet.SceneDepthSRV = DepthStencilSRV.Get();
        RenderTargetSet.SelectionMaskRTV = SelectionMaskRTV.Get();
        RenderTargetSet.SelectionMaskSRV = SelectionMaskSRV.Get();
        RenderTargetSet.DepthStencilView = DepthStencilView.Get();
        RenderTargetSet.Width = static_cast<float>(Width);
        RenderTargetSet.Height = static_cast<float>(Height);
        return RenderTargetSet;
	}

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
	UShader*                 Shader         = nullptr; // nullptr → batcher가 자체 셰이더 사용
	bool                     bWireframeAware = false;  // Wireframe 모드 시 래스터라이저 전환
};

class FRenderer
{
public:
	void Create(HWND hWindow);
	void CreateResources();
	void Release();

	void PrepareBatchers(const FRenderBus& InRenderBus);
	void BeginFrame();
	// Viewport 로부터 RTV, SRV 등 정보를 받아서 세팅
    void BeginViewportFrame(FRenderTargetSet InRenderTargetSet);
	void Render(const FRenderBus& InRenderBus);
	void EndFrame();
	void UseBackBufferRenderTargets();
	
    void UseViewportRenderTargets(FRenderTargetSet InRenderTargetSet);
	void InvalidateSceneFinalTargets();

	FD3DDevice& GetFD3DDevice() { return Device; }
	FRenderResources& GetResources() { return Resources; }

	const ID3D11RenderTargetView*   GetCurrentSceneRTV() const { return SceneFinalRTV.Get(); }
    const ID3D11ShaderResourceView* GetCurrentSceneSRV() const { return SceneFinalSRV.Get(); }

	// 현재는 Resource 를 Handle 이 아니라, 고정된 4개의 Viewport 에 대한 Index 를 통해 관리
	// 추가로 VP 를 받아서 원래 해당하는 Resource 를 찾아야하는데 현재는 Index 로 찾는 중
	FViewportRenderResource& AcquireViewportResource(FSceneViewport* VP, uint32 W, uint32 H, int32 Index);
    void InitializeViewportResource(FSceneViewport* VP, uint32 Width, uint32 Height, int32 Index);
    void ReleaseViewportResource(FSceneViewport* VP, int32 Index);

private:
	void InitializePassRenderStates();
	void InitializePassBatchers();

	void ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context, EViewMode ViewMode);
	void BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context, ERenderCommandType& LastCommandType);

	void DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand);
	void DrawPostProcessOutline(ID3D11DeviceContext* InDeviceContext);
	void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus);

	// 기본 패스 실행기 — SetupRenderState + DrawCommand 루프
    void ExecuteDefaultPass(ERenderPass Pass, const TArray<FRenderCommand>& Commands, const FRenderBus& Bus,
                            ID3D11DeviceContext* Context);

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

	/** 모든 Render Pass 를 관리할 객체 */
	FRenderPipeline RenderPipeline;
    std::shared_ptr<FRenderPassContext> RenderPassContext;

	// 패스별 커맨드 정렬이 필요한 경우 정렬된 복사본 반환, 아니면 원본 참조
	const TArray<FRenderCommand>& GetAlignedCommands(ERenderPass Pass, const TArray<FRenderCommand>& Commands);
	TArray<FRenderCommand> SortedCommandBuffer;  // 재할당 방지용 멤버 버퍼

	FPassRenderState    PassRenderStates[(uint32)ERenderPass::MAX];
	FPassBatcherBinding PassBatchers[(uint32)ERenderPass::MAX];
	UTexture* SubUVCachedTexture = nullptr;

	//	Primitive and Gizmo Input Layout
	D3D11_INPUT_ELEMENT_DESC PrimitiveInputLayout[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  static_cast<uint32>(offsetof(FVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	// StaticMesh (FNormalVertex) Input Layout
	D3D11_INPUT_ELEMENT_DESC NormalVertexInputLayout[4] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<uint32>(offsetof(FNormalVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, Color)),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<uint32>(offsetof(FNormalVertex, Normal)),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, static_cast<uint32>(offsetof(FNormalVertex, UVs)),      D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3D11_INPUT_ELEMENT_DESC FontBatcherInputLayout[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	// FinalRTV 는 Render Pass 구성에 따라 달라지므로 Renderer 내에서 보관
	TComPtr<ID3D11RenderTargetView> SceneFinalRTV = nullptr;
    TComPtr<ID3D11ShaderResourceView> SceneFinalSRV = nullptr;
	constexpr static uint32 MaxRTVCount = 3;

	// 지금은 4개 Viewport 고정 존재 상황이라 다음과 같이 처리
	FViewportRenderResource ViewportResources[4];
};

