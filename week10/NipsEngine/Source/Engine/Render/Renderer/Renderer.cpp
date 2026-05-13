#include "Renderer.h"

#include <array>
#include <iostream>
#include <algorithm>
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Common/ShadowTypes.h"
#include "Render/Mesh/MeshManager.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Component/BillboardComponent.h"
#include "Component/PostProcess/Light/DirectionalLightComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SubUVComponent.h"
#include "GameFramework/AActor.h"
#include "Render/Renderer/RenderTarget/RenderTargetFactory.h"
#include "Render/Renderer/RenderTarget/DepthStencilFactory.h"
#include "Render/Resource/ShaderHelper.h"
#include "Render/Resource/ShaderPaths.h"
#include "Render/Resource/ShadowAtlasManager.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Core/Logging/Log.h"

#include <unordered_map>

namespace
{
	FShaderProgram* GetProgramForMaterialCommand(const FRenderCommand& Cmd)
	{
		if (!Cmd.Material)
		{
			return nullptr;
		}

		const FVertexFactoryDesc& VertexFactoryDesc = FVertexFactoryRegistry::Get(Cmd.VertexFactoryType);

		FShaderStageKey VSKey;
		VSKey.FilePath = VertexFactoryDesc.VertexShaderPath;
		VSKey.EntryPoint = VertexFactoryDesc.BasePassVSEntry;
		VSKey.Target = "vs_5_0";

		FShaderStageKey PSKey;
		PSKey.FilePath = Cmd.Material->GetPixelShaderPath();
		PSKey.EntryPoint = Cmd.Material->GetPixelShaderEntryPoint();
		PSKey.Target = "ps_5_0";

		return FResourceManager::Get().GetOrCreateShaderProgram(
			VSKey,
			PSKey,
			nullptr,
			nullptr,
			&VertexFactoryDesc.VertexLayout);
	}

	FShaderProgram* GetOutlineProgram(const UMaterialInterface* Material)
	{
		if (!Material)
		{
			return nullptr;
		}

		FShaderStageKey VSKey;
		VSKey.FilePath = Material->GetPixelShaderPath();
		VSKey.EntryPoint = "VS";
		VSKey.Target = "vs_5_0";

		FShaderStageKey PSKey;
		PSKey.FilePath = Material->GetPixelShaderPath();
		PSKey.EntryPoint = Material->GetPixelShaderEntryPoint();
		PSKey.Target = "ps_5_0";

		return FResourceManager::Get().GetOrCreateShaderProgram(VSKey, PSKey);
	}

	FShaderProgram* GetFullscreenProgram(const FString& ShaderPath, const FString& VSEntryPoint = "mainVS", const FString& PSEntryPoint = "mainPS")
	{
		FShaderStageKey VSKey;
		VSKey.FilePath = ShaderPath;
		VSKey.EntryPoint = VSEntryPoint;
		VSKey.Target = "vs_5_0";

		FShaderStageKey PSKey;
		PSKey.FilePath = ShaderPath;
		PSKey.EntryPoint = PSEntryPoint;
		PSKey.Target = "ps_5_0";

		return FResourceManager::Get().GetOrCreateShaderProgram(VSKey, PSKey);
	}

	FShaderProgram* GetEditorIdPickProgram(uint32 ShaderKey)
	{
		const char* VSEntryPoint = "VSPrimitive";
		const char* PSEntryPoint = "PSOpaque";
		const FVertexLayoutDesc* VertexLayout = &FVertexFactoryRegistry::Get(EVertexFactoryType::Primitive).SelectionLayout;
		switch (ShaderKey)
		{
		case 1:
			VSEntryPoint = "VSStaticMesh";
			PSEntryPoint = "PSTextured";
			VertexLayout = &FVertexFactoryRegistry::Get(EVertexFactoryType::StaticMesh).SelectionLayout;
			break;
		case 2:
			VSEntryPoint = "VSBillboard";
			PSEntryPoint = "PSTextured";
			VertexLayout = &FVertexFactoryRegistry::Get(EVertexFactoryType::Billboard).SelectionLayout;
			break;
		case 3:
			VSEntryPoint = "VSSkeletalMesh";
			PSEntryPoint = "PSTextured";
			VertexLayout = &FVertexFactoryRegistry::Get(EVertexFactoryType::SkeletalMesh).SelectionLayout;
			break;
		default:
			break;
		}

		FShaderStageKey VSKey;
		VSKey.FilePath = FShaderPaths::EditorIDPick;
		VSKey.EntryPoint = VSEntryPoint;
		VSKey.Target = "vs_5_0";

		FShaderStageKey PSKey;
		PSKey.FilePath = FShaderPaths::EditorIDPick;
		PSKey.EntryPoint = PSEntryPoint;
		PSKey.Target = "ps_5_0";

		return FResourceManager::Get().GetOrCreateShaderProgram(
			VSKey,
			PSKey,
			nullptr,
			nullptr,
			VertexLayout);
	}
}

static uint32 GetOrAssignEditorPickId(
    AActor* Actor,
    std::unordered_map<AActor*, uint32>& ActorToId,
    TArray<AActor*>& OutActors)
{
    if (Actor == nullptr)
    {
        return 0;
    }

    auto It = ActorToId.find(Actor);
    if (It != ActorToId.end())
    {
        return It->second;
    }

    const uint32 NewId = static_cast<uint32>(OutActors.size()) + 1u;
    ActorToId[Actor] = NewId;
    OutActors.push_back(Actor);
    return NewId;
}

static ID3D11ShaderResourceView* GetTextureSRVFromParam(const FMaterialParamValue& Param)
{
    if (Param.Type != EMaterialParamType::Texture || !std::holds_alternative<UTexture*>(Param.Value))
    {
        return nullptr;
    }

    UTexture* Texture = std::get<UTexture*>(Param.Value);
    return Texture ? Texture->GetSRV() : nullptr;
}

static ID3D11ShaderResourceView* GetDiffuseSRV(UMaterialInterface* Material)
{
    if (!Material || !Material->HasDiffuseMap())
    {
        return nullptr;
    }

    FMaterialParamValue Param;
    if (!Material->GetParam("DiffuseMap", Param))
    {
        return nullptr;
    }
    return GetTextureSRVFromParam(Param);
}

static FMatrix MakeEditorIdPickBillboardMatrix(const UBillboardComponent* Billboard, const FRenderBus& RenderBus)
{
    const FVector WorldScale = Billboard->GetBillboardWorldScale();
    return UBillboardComponent::MakeBillboardWorldMatrix(
        Billboard->GetWorldLocation(),
        FVector(
            WorldScale.X > 0.01f ? WorldScale.X : 0.01f,
            Billboard->GetWidth() * WorldScale.Y * 0.5f,
            Billboard->GetHeight() * WorldScale.Z * 0.5f),
        RenderBus.GetCameraForward(),
        RenderBus.GetCameraRight(),
        RenderBus.GetCameraUp());
}

static void DrawIdPickCommand(ID3D11DeviceContext* Context, const FRenderCommand& Command)
{
    if (!Context || !Command.MeshBuffer || !Command.MeshBuffer->IsValid())
    {
        return;
    }

    ID3D11Buffer* VertexBuffer = Command.MeshBuffer->GetVertexBuffer().GetBuffer();
    if (!VertexBuffer)
    {
        return;
    }

    uint32 Stride = Command.MeshBuffer->GetVertexBuffer().GetStride();
    uint32 Offset = 0;
    if (Stride == 0)
    {
        return;
    }

    Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);

    ID3D11Buffer* IndexBuffer = Command.MeshBuffer->GetIndexBuffer().GetBuffer();
    if (IndexBuffer)
    {
        Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        Context->DrawIndexed(Command.SectionIndexCount, Command.SectionIndexStart, 0);
    }
    else
    {
        Context->Draw(Command.MeshBuffer->GetVertexBuffer().GetVertexCount(), 0);
    }
}

void FRenderer::Create(HWND hWindow)
{
	Device.Create(hWindow);

	if (Device.GetDevice() == nullptr)
	{
		std::cout << "Failed to create D3D Device." << std::endl;
	}

	FResourceManager::Get().SetCachedDevice(Device.GetDevice());

	FResourceManager::Get().LoadComputeShader("Shaders/Compute/LightCullingCS.hlsl", "main",
		FShaderHelper::BuildLightCullingCSMacros(ELightCullMode::Clustered).data(), "LightCullingCS_Clustered");
	FResourceManager::Get().LoadComputeShader("Shaders/Compute/LightCullingCS.hlsl", "main",
		FShaderHelper::BuildLightCullingCSMacros(ELightCullMode::Tiled).data(), "LightCullingCS_Tiled");

    {
        auto Macros = FShaderHelper::BuildVSMBlurCSMacros(EVSMBlurPass::Horizontal);
        FResourceManager::Get().LoadComputeShader(
            "Shaders/Shadow/VSMBlurComputeShader.hlsl", "main", Macros.data(), "VSMBlur_H");
    }
    {
        auto Macros = FShaderHelper::BuildVSMBlurCSMacros(EVSMBlurPass::Vertical);
        FResourceManager::Get().LoadComputeShader(
            "Shaders/Shadow/VSMBlurComputeShader.hlsl", "main", Macros.data(), "VSMBlur_V");
    }

	// Uber ShadowMap
	for (uint32 ShadowMapIdx = 0; ShadowMapIdx < static_cast<uint32>(EShadowMap::MAX); ++ShadowMapIdx)
	{
		auto Macros = FShaderHelper::BuildShadowMapMacros(static_cast<EShadowMap>(ShadowMapIdx));
		FResourceManager::Get().GetOrCreateShaderProgram(
			FShaderStageKey{ FShaderPaths::Shadow, "ShadowVS", "vs_5_0", ShadowMapIdx },
			FShaderStageKey{ FShaderPaths::Shadow, "ShadowPS", "ps_5_0", ShadowMapIdx },
			Macros.data(),
			Macros.data(),
			&FVertexFactoryRegistry::Get(EVertexFactoryType::StaticMesh).PositionOnlyLayout);
	}

}

void FRenderer::CreateResources()
{
	Resources.PerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FPerObjectConstants));
	Resources.FrameBuffer.Create(Device.GetDevice(), sizeof(FFrameConstants));

	Resources.ShadowBuffer.Create(Device.GetDevice(), sizeof(FShadowConstants));
	Resources.LightBuffer.Create(Device.GetDevice(), sizeof(FUberConstants));
    Resources.LightShadowIndexBuffer.Create(Device.GetDevice(), sizeof(FLightShadowIndices), 1024);
    Resources.AtlasShadowBuffer.Create(Device.GetDevice(), sizeof(FShadowAtlasConstants), 1024);

	// Tile을 나누는 기준에 따라서 ByteWidth 설정 수정이 필요합니다.
	Resources.LightStructuredBuffer.Create(Device.GetDevice(), sizeof(FLightInfo), 1024);
	Resources.LightCulledIndexBuffer.Create(Device.GetDevice(), sizeof(uint32), 522240 * 24, true);
	Resources.LightTileBuffer.Create(Device.GetDevice(), sizeof(uint32) * 2, 522240 * 24, true);

	Resources.FogPassConstantBuffer.Create(Device.GetDevice(), sizeof(FFogPassConstants));
	Resources.FXAAConstantBuffer.Create(Device.GetDevice(), sizeof(FFXAAConstants));
    Resources.EditorPickingConstantBuffer.Create(Device.GetDevice(), sizeof(FEditorPickingConstants));
    Resources.SelectionMaskConstantBuffer.Create(Device.GetDevice(), sizeof(FSelectionMaskConstants));
    Resources.SandevistanCB.Create(Device.GetDevice(), sizeof(FSandevistanConstants));
    Resources.PostProcessCB.Create(Device.GetDevice(), sizeof(FPostProcessConstants));
    Resources.ScreenOverlayCB.Create(Device.GetDevice(), sizeof(FScreenOverlayConstants));
	Resources.LightPassConstantBuffer.Create(Device.GetDevice(), sizeof(FLightPassConstants));
	Resources.MPLightStructuredBuffer.Create(Device.GetDevice(), sizeof(FLightData), 256);
	Resources.DecalStructuredBuffer.Create(Device.GetDevice(), sizeof(FDecalInfo), 256);

	// VSM 전용 ComputeShader Constantbuffer
    Resources.VSMConstantBuffer.Create(Device.GetDevice(), sizeof(FVSMBlurConstants));

	//	MeshManager init
	FMeshManager::Initialize();
	FShadowAtlasManager::Get().Initialize(Device.GetDevice());
    //FShadowAtlasManager::Get().VSMInitialize(Device.GetDevice()); /// VSM 추가 -> ShadowAtlas의 Resource로 사용하도록 이 호출 삭제해주면 됨

	EditorLineBatcher.Create(Device.GetDevice());
	{
		FLineBatcherDesc OverlayDesc;
		OverlayDesc.MaterialName  = "LineMatOverlay";   // 별도 머티리얼 인스턴스 — 같은 .mat 자산 재활용
		OverlayDesc.MaterialPath  = "Asset/Material/LineMat.mat";
		OverlayDesc.DepthStencil  = EDepthStencilType::AlwaysOnTop;
		EditorOverlayLineBatcher.Create(Device.GetDevice(), OverlayDesc);
	}
	GridLineBatcher.Create(Device.GetDevice());

	// 텍스처는 ResourceManager가 소유 ? Batcher 는 셰이더/버퍼만 초기화
	FontBatcher.Create(Device.GetDevice());
	SubUVBatcher.Create(Device.GetDevice());

	InitializePassRenderStates();
	InitializePassBatchers();
	UseBackBufferRenderTargets();

	// GPU Profiler 초기화
	FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());

	RenderPipeline.Initialize();
    RenderPassContext = std::make_shared<FRenderPassContext>();
}

void FRenderer::Release()
{
	InvalidateSceneFinalTargets();
	ReleasePreviewResource();
    ReleaseRenderResource(GameFrameResource);
	for (int32 i = 0; i < 4; ++i)
	{
		ReleaseViewportResource(i);
	}

	RenderPipeline.Release();
    RenderPassContext.reset();

	Resources.PerObjectConstantBuffer.Release();
	Resources.FrameBuffer.Release();
	Resources.ShadowBuffer.Release();
	Resources.LightBuffer.Release();
	Resources.LightShadowIndexBuffer.Release();
	Resources.AtlasShadowBuffer.Release();
	Resources.LightStructuredBuffer.Release();
	Resources.LightCulledIndexBuffer.Release();
	Resources.LightTileBuffer.Release();
    Resources.MPLightStructuredBuffer.Release();
	Resources.DecalStructuredBuffer.Release();

    Resources.FogPassConstantBuffer.Release();
    Resources.SandevistanCB.Release();
    Resources.PostProcessCB.Release();
    Resources.ScreenOverlayCB.Release();
    Resources.FXAAConstantBuffer.Release();
    Resources.EditorPickingConstantBuffer.Release();
    Resources.SelectionMaskConstantBuffer.Release();
    Resources.LightPassConstantBuffer.Release();
    Resources.VSMConstantBuffer.Release();
	FGPUProfiler::Get().Shutdown();

	EditorLineBatcher.Release();
	EditorOverlayLineBatcher.Release();
	GridLineBatcher.Release();
	FontBatcher.Release();
	SubUVBatcher.Release();

    // Device::ReportLiveObjects 이전에 ResourceManager가 잡고 있던 D3D 객체를 먼저 해제한다.
    FShadowAtlasManager::Get().Release();
    FResourceManager::Get().ReleaseGPUResources();

	Device.Release();
}

//	Bus → Batcher 데이터 수집 (CPU). BeginFrame 이전에 호출.
void FRenderer::PrepareBatchers(const FRenderBus& InRenderBus)
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		if (!PassBatchers[i]) continue;

		const auto& Commands = InRenderBus.GetCommands(static_cast<ERenderPass>(i));
		const auto& AlignedCommands = GetAlignedCommands(static_cast<ERenderPass>(i), Commands);

		PassBatchers[i].Clear();
		for (const auto& Cmd : AlignedCommands)
			PassBatchers[i].Collect(Cmd, InRenderBus);
	}
}

const TArray<FRenderCommand>& FRenderer::GetAlignedCommands(ERenderPass Pass, const TArray<FRenderCommand>& Commands)
{
	// SubUV 패스: Particle(SRV) 포인터 기준 정렬 → 같은 텍스쳐끼리 연속 배치
	if (Pass == ERenderPass::SubUV && Commands.size() > 1)
	{
		SortedCommandBuffer.assign(Commands.begin(), Commands.end());

		std::sort(SortedCommandBuffer.begin(), SortedCommandBuffer.end(),
			[](const FRenderCommand& A, const FRenderCommand& B) {
				return A.Constants.SubUV.Particle < B.Constants.SubUV.Particle;
			});

		return SortedCommandBuffer;
	}

	return Commands;
}

//	GPU 프레임 시작. 반드시 Render 이전에 호출되어야 함.
void FRenderer::BeginFrame()
{
	Device.BeginFrame();
	UseBackBufferRenderTargets();

#if STATS
	FGPUProfiler::Get().BeginFrame();
#endif
}

void FRenderer::BeginViewportFrame(FRenderTargetSet InRenderTargetSet)
{
    Device.BeginViewportFrame(InRenderTargetSet);
    UseViewportRenderTargets(InRenderTargetSet);
}

FRenderTargetSet FRenderer::BeginGameFrame(uint32 Width, uint32 Height)
{
    static bool bLoggedGameFrameTargets = false;

    FViewportRenderResource& Res = GameFrameResource;
    if (Device.GetDevice() == nullptr || Width == 0 || Height == 0)
    {
        ReleaseRenderResource(Res);
        UseBackBufferRenderTargets();
        return CurrentRenderTargets;
    }

    const bool bSameSize = (Res.Width == Width) && (Res.Height == Height);
    const bool bResourcesValid =
        (Res.ColorRTV != nullptr) &&
        (Res.NormalRTV != nullptr) &&
        (Res.LightRTV != nullptr) &&
        (Res.FogRTV != nullptr) &&
        (Res.WorldPosRTV != nullptr) &&
        (Res.FXAARTV != nullptr) &&
        (Res.SelectionMaskRTV != nullptr) &&
        (Res.DepthStencilView != nullptr);

    if (!bSameSize || !bResourcesValid)
    {
        ReleaseRenderResource(Res);
        InitializeRenderResource(Res, Width, Height);
        bLoggedGameFrameTargets = false;
    }

    FRenderTargetSet Targets = Res.GetView();
    BeginViewportFrame(Targets);

    if (!bLoggedGameFrameTargets)
    {
        UE_LOG("[GameRender] Game frame targets ready. Size=%ux%u Color=%d Light=%d Fog=%d FXAA=%d DepthSRV=%d, Sandervistan=%d, PostProcess=%d",
               Width, Height,
               Targets.SceneColorRTV != nullptr,
               Targets.SceneLightRTV != nullptr,
               Targets.SceneFogRTV != nullptr,
               Targets.SceneFXAARTV != nullptr,
               Targets.SceneDepthSRV != nullptr,
			   Targets.SceneSandervistanSRV != nullptr,
			   Targets.ScenePostProcessSRV != nullptr);
        bLoggedGameFrameTargets = true;
    }

    return Targets;
}

void FRenderer::UseBackBufferRenderTargets()
{
	CurrentRenderTargets = Device.GetBackBufferRenderTargets();

	if (CurrentRenderTargets.IsValid())
	{
        ID3D11RenderTargetView* RTV =
                CurrentRenderTargets.SceneColorRTV; // Back Buffer 의 경우 SceneColorRTV 가 FinalRTV 역할
        SceneFinalRTV = RTV;
        
		Device.GetDeviceContext()->OMSetRenderTargets(1, &RTV, CurrentRenderTargets.DepthStencilView);
		Device.SetSubViewport(0, 0,
			static_cast<int32>(CurrentRenderTargets.Width),
			static_cast<int32>(CurrentRenderTargets.Height));
	}
}

void FRenderer::UseViewportRenderTargets(FRenderTargetSet InRenderTargetSet)
{
    CurrentRenderTargets = InRenderTargetSet;

    if (!CurrentRenderTargets.IsValid())
    {
        InvalidateSceneFinalTargets();
		// Back Buffer 아마 쓰이면 안될텐데 여기 중단점 찍히면 확인
		// 기존 단일 Viewport 구조에서 쓰이던 내용
        UseBackBufferRenderTargets();
        return;
    }

    Device.SetSubViewport(0, 0,
                          static_cast<int32>(CurrentRenderTargets.Width),
                          static_cast<int32>(CurrentRenderTargets.Height));
}

void FRenderer::InvalidateSceneFinalTargets()
{
	SceneFinalRTV.Reset();
	SceneFinalSRV.Reset();
	CurrentRenderTargets = {};
}

//	RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행 (GPU)
void FRenderer::Render(const FRenderBus& InRenderBus)
{
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	UpdateUberBuffer(Context, InRenderBus);
    UpdateFrameBuffer(Context, InRenderBus);

	/** Opaque 만 테스트 */
    
	RenderPassContext->Device = Device.GetDevice();
    RenderPassContext->DeviceContext = Device.GetDeviceContext();
    RenderPassContext->RenderState = &PassRenderStates[(uint32)ERenderPass::Opaque];
    RenderPassContext->RenderBus = &InRenderBus;
    RenderPassContext->RenderTargets = &CurrentRenderTargets;
    RenderPassContext->RenderResources = &Resources;
    RenderPassContext->FontBatcher = &FontBatcher;
    RenderPassContext->SubUVBatcher = &SubUVBatcher;
    RenderPassContext->GridLineBatcher = &GridLineBatcher;
    RenderPassContext->EditorLineBatcher = &EditorLineBatcher;
    RenderPassContext->EditorOverlayLineBatcher = &EditorOverlayLineBatcher;
	RenderPipeline.Render(RenderPassContext.get());
	
	SceneFinalSRV = RenderPipeline.GetOutSRV();
    SceneFinalRTV = RenderPipeline.GetOutRTV();
}

void FRenderer::RenderEditorIdPickBuffer(const FRenderBus& InRenderBus, FViewportRenderResource& Resource, TArray<AActor*>& OutActors)
{
    OutActors.clear();

    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    if (!Context || !Resource.EditorIdPickRTV || !Resource.EditorIdPickSRV || !Resource.EditorIdPickDebugRTV)
    {
        return;
    }

    UpdateFrameBuffer(Context, InRenderBus);

    const float ClearId[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    Context->ClearRenderTargetView(Resource.EditorIdPickRTV.Get(), ClearId);
    Context->ClearDepthStencilView(Resource.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    ID3D11RenderTargetView* IdRTV = Resource.EditorIdPickRTV.Get();
    Context->OMSetRenderTargets(1, &IdRTV, Resource.DepthStencilView.Get());
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
    ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::Opaque);
    ID3D11RasterizerState* RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidNoCull);
    ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
    Context->OMSetDepthStencilState(DepthState, 0);
    Context->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
    Context->RSSetState(RasterizerState);
    Context->PSSetSamplers(0, 1, &Sampler);

    ID3D11ShaderResourceView* DefaultSRV = FResourceManager::Get().GetDefaultWhiteSRV();
    std::unordered_map<AActor*, uint32> ActorToId;
    ActorToId.reserve(128);

    static constexpr ERenderPass PickPasses[] =
    {
        ERenderPass::Opaque,
        ERenderPass::Translucent,
        ERenderPass::SubUV
    };

    for (ERenderPass Pass : PickPasses)
    {
        const TArray<FRenderCommand>& Commands = InRenderBus.GetCommands(Pass);
        for (const FRenderCommand& Command : Commands)
        {
            UPrimitiveComponent* Primitive = Command.SourcePrimitive;
            AActor* Actor = Primitive ? Primitive->GetOwner() : nullptr;
            if (!Primitive || !Primitive->IsVisible() || !Actor || !Actor->IsVisible() || !Actor->GetRootComponent())
            {
                continue;
            }

            const uint32 PickingId = GetOrAssignEditorPickId(Actor, ActorToId, OutActors);
            if (PickingId == 0 || !Command.MeshBuffer || Command.SectionIndexCount == 0)
            {
                continue;
            }

            FEditorPickingConstants PickingConstants = {};
            PickingConstants.PickingId = PickingId;
            PickingConstants.AlphaCutoff = 0.01f;
            PickingConstants.UVScale = FVector2(1.0f, 1.0f);

            ID3D11ShaderResourceView* TextureSRV = DefaultSRV;
            uint32 ShaderKey = 0;
            if (Command.Type == ERenderCommandType::StaticMesh || Command.Type == ERenderCommandType::SkeletalMesh)
            {
                ShaderKey = Command.VertexFactoryType == EVertexFactoryType::SkeletalMesh ? 3 : 1;
                ID3D11ShaderResourceView* DiffuseSRV = GetDiffuseSRV(Command.Material);
                TextureSRV = DiffuseSRV ? DiffuseSRV : DefaultSRV;
                PickingConstants.bUseAlphaTest = DiffuseSRV ? 1u : 0u;
            }
            else if (Command.Type == ERenderCommandType::Billboard)
            {
                ShaderKey = 2;
                UTexture* Texture = Command.Constants.Billboard.Texture;
                TextureSRV = Texture && Texture->GetSRV() ? Texture->GetSRV() : DefaultSRV;
                PickingConstants.bUseAlphaTest = TextureSRV ? 1u : 0u;
            }
            else if (Command.Type == ERenderCommandType::SubUV)
            {
                ShaderKey = 2;
                const FParticleResource* Particle = Command.Constants.SubUV.Particle;
                TextureSRV = Particle && Particle->Texture && Particle->Texture->GetSRV()
                    ? Particle->Texture->GetSRV()
                    : DefaultSRV;
                PickingConstants.bUseAlphaTest = TextureSRV ? 1u : 0u;
                if (Particle && Particle->Columns > 0 && Particle->Rows > 0)
                {
                    const uint32 Columns = Particle->Columns;
                    const uint32 Rows = Particle->Rows;
                    const uint32 FrameIndex = Command.Constants.SubUV.FrameIndex;
                    const uint32 Col = FrameIndex % Columns;
                    const uint32 Row = FrameIndex / Columns;
                    PickingConstants.UVOffset = FVector2(
                        static_cast<float>(Col) / static_cast<float>(Columns),
                        static_cast<float>(Row) / static_cast<float>(Rows));
                    PickingConstants.UVScale = FVector2(
                        1.0f / static_cast<float>(Columns),
                        1.0f / static_cast<float>(Rows));
                }
            }

            FPerObjectConstants PerObjectConstants = Command.PerObjectConstants;
            if (Command.Type == ERenderCommandType::Billboard || Command.Type == ERenderCommandType::SubUV)
            {
                if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Primitive))
                {
                    PerObjectConstants = FPerObjectConstants(
                        MakeEditorIdPickBillboardMatrix(Billboard, InRenderBus),
                        Command.PerObjectConstants.Color);
                }
            }

            Resources.PerObjectConstantBuffer.Update(Context, &PerObjectConstants, sizeof(FPerObjectConstants));
            ID3D11Buffer* PerObjectBuffer = Resources.PerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(1, 1, &PerObjectBuffer);
            Context->PSSetConstantBuffers(1, 1, &PerObjectBuffer);

            Resources.EditorPickingConstantBuffer.Update(Context, &PickingConstants, sizeof(FEditorPickingConstants));
            ID3D11Buffer* PickingBuffer = Resources.EditorPickingConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(12, 1, &PickingBuffer);
            Context->PSSetConstantBuffers(12, 1, &PickingBuffer);

            Context->PSSetShaderResources(0, 1, &TextureSRV);
            FShaderProgram* PickProgram = GetEditorIdPickProgram(ShaderKey);
            if (!PickProgram)
            {
                continue;
            }
            PickProgram->Bind(Context);
            DrawIdPickCommand(Context, Command);
        }
    }

    if (Resource.EditorIdPickDebugRTV && Resource.EditorIdPickSRV)
    {
        ID3D11RenderTargetView* DebugRTV = Resource.EditorIdPickDebugRTV.Get();
        Context->OMSetRenderTargets(1, &DebugRTV, nullptr);
        Context->IASetInputLayout(nullptr);
        Context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ID3D11ShaderResourceView* IdSRV = Resource.EditorIdPickSRV.Get();
        Context->PSSetShaderResources(0, 1, &IdSRV);
        FShaderProgram* DebugProgram = GetFullscreenProgram(FShaderPaths::EditorIDPickDebug, "VS", "PS");
        if (!DebugProgram)
        {
            return;
        }
        DebugProgram->Bind(Context);
        Context->Draw(3, 0);
    }

    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context->PSSetShaderResources(0, 1, &NullSRV);
}

void FRenderer::CompositeCurrentSceneToBackBuffer()
{
    ID3D11ShaderResourceView* SourceSRV = SceneFinalSRV.Get();
    ID3D11RenderTargetView* BackBufferRTV = Device.GetFrameBufferRTV();
    ID3D11DeviceContext* Context = Device.GetDeviceContext();

    if (!SourceSRV || !BackBufferRTV || !Context)
    {
        static bool bLoggedMissingCompositeSource = false;
        if (!bLoggedMissingCompositeSource)
        {
            UE_LOG_WARNING("[GameRender] Backbuffer composite skipped. SourceSRV=%d BackBufferRTV=%d Context=%d",
                   SourceSRV != nullptr, BackBufferRTV != nullptr, Context != nullptr);
            bLoggedMissingCompositeSource = true;
        }
        UseBackBufferRenderTargets();
        return;
    }

    ID3D11RenderTargetView* RTVs[1] = { BackBufferRTV };
    Context->OMSetRenderTargets(1, RTVs, nullptr);
    Device.SetSubViewport(
        0,
        0,
        static_cast<int32>(Device.GetViewportWidth()),
        static_cast<int32>(Device.GetViewportHeight()));

    FFXAAConstants Constants = {};
    Constants.InvResolution[0] = (Device.GetViewportWidth() > 0.0f) ? (1.0f / Device.GetViewportWidth()) : 0.0f;
    Constants.InvResolution[1] = (Device.GetViewportHeight() > 0.0f) ? (1.0f / Device.GetViewportHeight()) : 0.0f;
    Constants.bEnabled = 0;
    Resources.FXAAConstantBuffer.Update(Context, &Constants, sizeof(Constants));
    ID3D11Buffer* FXAACB = Resources.FXAAConstantBuffer.GetBuffer();
    Context->PSSetConstantBuffers(10, 1, &FXAACB);

    FShaderProgram* BlitProgram = GetFullscreenProgram(FShaderPaths::PostProcessFXAA);
    if (!BlitProgram)
    {
        static bool bLoggedMissingBlitShader = false;
        if (!bLoggedMissingBlitShader)
        {
            UE_LOG_ERROR("[GameRender] Backbuffer composite failed. FXAA blit shader is missing.");
            bLoggedMissingBlitShader = true;
        }
        UseBackBufferRenderTargets();
        return;
    }

    ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
    Context->PSSetSamplers(0, 1, &Sampler);
    Context->PSSetShaderResources(0, 1, &SourceSRV);

    BlitProgram->Bind(Context);
    Context->IASetInputLayout(nullptr);
    Context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->Draw(3, 0);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    Context->PSSetShaderResources(0, 1, &NullSRV);
    UseBackBufferRenderTargets();
}

void FRenderer::RenderScreenOverlays(const FRenderBus& InRenderBus, bool bTargetBackBuffer)
{
    const float Width = bTargetBackBuffer ? Device.GetViewportWidth() : CurrentRenderTargets.Width;
    const float Height = bTargetBackBuffer ? Device.GetViewportHeight() : CurrentRenderTargets.Height;
    if (Width <= 0.0f || Height <= 0.0f)
    {
        return;
    }

    ID3D11RenderTargetView* TargetRTV = bTargetBackBuffer
        ? Device.GetFrameBufferRTV()
        : SceneFinalRTV.Get();
    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    FShaderProgram* OverlayProgram = GetFullscreenProgram(FShaderPaths::UIScreenOverlay);
    if (!TargetRTV || !Context || !OverlayProgram)
    {
        return;
    }

    ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
    ID3D11RasterizerState* RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidNoCull);
    Context->OMSetRenderTargets(1, &TargetRTV, nullptr);
    Context->OMSetDepthStencilState(nullptr, 0);
    Context->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
    Context->RSSetState(RasterizerState);

    OverlayProgram->Bind(Context);
    Context->IASetInputLayout(nullptr);
    Context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    auto DrawRect = [&](int32 X, int32 Y, int32 RectWidth, int32 RectHeight, const FVector4& Color)
    {
        if (RectWidth <= 0 || RectHeight <= 0 || Color.W <= 0.001f)
        {
            return;
        }

        FScreenOverlayConstants Constants = {};
        Constants.Color[0] = Color.X;
        Constants.Color[1] = Color.Y;
        Constants.Color[2] = Color.Z;
        Constants.Color[3] = Color.W;
        Resources.ScreenOverlayCB.Update(Context, &Constants, sizeof(Constants));
        ID3D11Buffer* OverlayCB = Resources.ScreenOverlayCB.GetBuffer();
        Context->PSSetConstantBuffers(13, 1, &OverlayCB);

        Device.SetSubViewport(X, Y, RectWidth, RectHeight);
        Context->Draw(3, 0);
    };

    const int32 TargetWidth = static_cast<int32>(Width);
    const int32 TargetHeight = static_cast<int32>(Height);
    const float LetterboxAspect = InRenderBus.GetLetterboxTargetAspect();
    const float LetterboxAmount = std::clamp(InRenderBus.GetLetterboxAmount(), 0.0f, 1.0f);
    if (LetterboxAspect > 0.001f && LetterboxAmount > 0.001f)
    {
        const float CurrentAspect = Width / Height;
        const FVector4 Black(0.0f, 0.0f, 0.0f, 1.0f);
        if (CurrentAspect > LetterboxAspect)
        {
            const int32 ContentWidth = std::max(static_cast<int32>(Height * LetterboxAspect), 1);
            const int32 FullBarWidth = std::max((TargetWidth - ContentWidth) / 2, 0);
            const int32 BarWidth = static_cast<int32>(FullBarWidth * LetterboxAmount);
            DrawRect(0, 0, BarWidth, TargetHeight, Black);
            DrawRect(TargetWidth - BarWidth, 0, BarWidth, TargetHeight, Black);
        }
        else
        {
            const int32 ContentHeight = std::max(static_cast<int32>(Width / LetterboxAspect), 1);
            const int32 FullBarHeight = std::max((TargetHeight - ContentHeight) / 2, 0);
            const int32 BarHeight = static_cast<int32>(FullBarHeight * LetterboxAmount);
            DrawRect(0, 0, TargetWidth, BarHeight, Black);
            DrawRect(0, TargetHeight - BarHeight, TargetWidth, BarHeight, Black);
        }
    }

    FVector4 FadeColor = InRenderBus.GetCameraFadeColor();
    FadeColor.W *= std::clamp(InRenderBus.GetCameraFadeAlpha(), 0.0f, 1.0f);
    DrawRect(0, 0, TargetWidth, TargetHeight, FadeColor);

    //  Viewport의 사이즈 자체를 조절함
    Device.SetSubViewport(0, 0, TargetWidth, TargetHeight);
}

FViewportRenderResource& FRenderer::AcquireViewportResource(uint32 Width, uint32 Height, int32 Index)
{
    assert(Index < 4 && "Index Out of Bound");

    FViewportRenderResource& Res = ViewportResources[Index];

    if (Device.GetDevice() == nullptr || Width == 0 || Height == 0)
    {
        ReleaseViewportResource(Index);
        return Res;
    }

    const bool bSameSize =
        (Res.Width == Width) &&
        (Res.Height == Height);

    const bool bResourcesValid =
        (Res.ColorRTV != nullptr) &&
        (Res.SelectionMaskRTV != nullptr) &&
        (Res.DepthStencilView != nullptr);

    if (bSameSize && bResourcesValid)
    {
        return Res;
    }

    // 재생성
    ReleaseViewportResource(Index);
    InitializeViewportResource(Width, Height, Index);

    return Res;
}

FViewportRenderResource& FRenderer::AcquireViewerViewportResource(uint32 W, uint32 H)
{
    if (Device.GetDevice() == nullptr || W == 0 || H == 0)
    {
        ReleaseRenderResource(ViewerViewportResource);
        return ViewerViewportResource;
    }

    const bool bSameSize =
        (ViewerViewportResource.Width == W) &&
        (ViewerViewportResource.Height == H);

    const bool bResourcesValid =
        (ViewerViewportResource.ColorRTV != nullptr) &&
        (ViewerViewportResource.SelectionMaskRTV != nullptr) &&
        (ViewerViewportResource.DepthStencilView != nullptr);

    if (bSameSize && bResourcesValid)
    {
        return ViewerViewportResource;
    }

    ReleaseRenderResource(ViewerViewportResource);
    InitializeRenderResource(ViewerViewportResource, W, H);
    InitializeEditorIdPickResource(ViewerViewportResource, W, H);

    return ViewerViewportResource;
}

void FRenderer::InitializeViewportResource(uint32 Width, uint32 Height, int32 Index)
{
    FViewportRenderResource& Res = ViewportResources[Index];
	InitializeRenderResource(Res, Width, Height);
    InitializeEditorIdPickResource(Res, Width, Height);
}

void FRenderer::ReleaseViewportResource(int32 Index)
{
    assert(Index < 4 && "Index Out of Bound");

    FViewportRenderResource& Res = ViewportResources[Index];
	ReleaseRenderResource(Res);
}

FViewportRenderResource& FRenderer::AcquirePreviewResource(uint32 Width, uint32 Height)
{
	if (Device.GetDevice() == nullptr || Width == 0 || Height == 0)
	{
		ReleasePreviewResource();
		return PreviewResource;
	}

	const bool bSameSize = (PreviewResource.Width == Width) && (PreviewResource.Height == Height);
	const bool bResourcesValid =
		(PreviewResource.ColorRTV != nullptr) &&
		(PreviewResource.SelectionMaskRTV != nullptr) &&
		(PreviewResource.DepthStencilView != nullptr);

	if (bSameSize && bResourcesValid)
	{
		return PreviewResource;
	}

	ReleasePreviewResource();
	InitializeRenderResource(PreviewResource, Width, Height);
	return PreviewResource;
}

void FRenderer::ReleasePreviewResource()
{
	ReleaseRenderResource(PreviewResource);
}

void FRenderer::InitializeRenderResource(FViewportRenderResource& Res, uint32 Width, uint32 Height)
{
    FRenderTarget RT;

    Res.Width = Width;
    Res.Height = Height;

    RT = FRenderTargetFactory::CreateSceneColor(Device.GetDevice(), Width, Height);
    Res.ColorTex = RT.Texture;
    Res.ColorRTV = RT.RTV;
    Res.ColorSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSceneNormal(Device.GetDevice(), Width, Height);
    Res.NormalTex = RT.Texture;
    Res.NormalRTV = RT.RTV;
    Res.NormalSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSelectionMask(Device.GetDevice(), Width, Height);
    Res.SelectionMaskTex = RT.Texture;
    Res.SelectionMaskRTV = RT.RTV;
    Res.SelectionMaskSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSceneLight(Device.GetDevice(), Width, Height);
    Res.LightTex = RT.Texture;
    Res.LightRTV = RT.RTV;
    Res.LightSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSceneFog(Device.GetDevice(), Width, Height);
    Res.FogTex = RT.Texture;
    Res.FogRTV = RT.RTV;
    Res.FogSRV = RT.SRV;

	RT = FRenderTargetFactory::CreateSceneSandervistan(Device.GetDevice(), Width, Height);
    Res.SandervistanTex = RT.Texture;
    Res.SandervistanRTV = RT.RTV;
    Res.SandervistanSRV = RT.SRV;
	
	RT = FRenderTargetFactory::CreateScenePostProcess(Device.GetDevice(), Width, Height);
    Res.PostProcessTex = RT.Texture;
    Res.PostProcessRTV = RT.RTV;
    Res.PostProcessSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSceneWorldPos(Device.GetDevice(), Width, Height);
    Res.WorldPosTex = RT.Texture;
    Res.WorldPosRTV = RT.RTV;
    Res.WorldPosSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateSceneFXAA(Device.GetDevice(), Width, Height);
    Res.FXAATex = RT.Texture;
    Res.FXAARTV = RT.RTV;
    Res.FXAASRV = RT.SRV;

    // Depth
    FDepthStencilResource DSR =
        FDepthStencilFactory::CreateDepthStencilView(Device.GetDevice(), Width, Height);

    Res.DepthTex = DSR.Texture;
    Res.DepthStencilView = DSR.DSV;
    Res.DepthStencilSRV = DSR.SRV;
}

void FRenderer::InitializeEditorIdPickResource(FViewportRenderResource& Res, uint32 Width, uint32 Height)
{
    FRenderTarget RT = FRenderTargetFactory::CreateEditorIdPick(Device.GetDevice(), Width, Height);
    Res.EditorIdPickTex = RT.Texture;
    Res.EditorIdPickRTV = RT.RTV;
    Res.EditorIdPickSRV = RT.SRV;

    RT = FRenderTargetFactory::CreateEditorIdPickDebug(Device.GetDevice(), Width, Height);
    Res.EditorIdPickDebugTex = RT.Texture;
    Res.EditorIdPickDebugRTV = RT.RTV;
    Res.EditorIdPickDebugSRV = RT.SRV;

    D3D11_TEXTURE2D_DESC ReadbackDesc = {};
    ReadbackDesc.Width = 1;
    ReadbackDesc.Height = 1;
    ReadbackDesc.MipLevels = 1;
    ReadbackDesc.ArraySize = 1;
    ReadbackDesc.Format = DXGI_FORMAT_R32_UINT;
    ReadbackDesc.SampleDesc.Count = 1;
    ReadbackDesc.Usage = D3D11_USAGE_STAGING;
    ReadbackDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Device.GetDevice()->CreateTexture2D(&ReadbackDesc, nullptr, Res.EditorIdPickReadbackTex.ReleaseAndGetAddressOf());
}

void FRenderer::ReleaseRenderResource(FViewportRenderResource& Res)
{
    ReleaseEditorIdPickResource(Res);

    Res.SelectionMaskSRV.Reset();
    Res.SelectionMaskRTV.Reset();
    Res.SelectionMaskTex.Reset();

    Res.ColorSRV.Reset();
    Res.ColorRTV.Reset();
    Res.ColorTex.Reset();

    Res.NormalRTV.Reset();
    Res.NormalSRV.Reset();
    Res.NormalTex.Reset();

    Res.LightRTV.Reset();
    Res.LightSRV.Reset();
    Res.LightTex.Reset();

    Res.DepthStencilView.Reset();
    Res.DepthTex.Reset();
    Res.DepthStencilSRV.Reset();

	Res.VSMDepthStencilSRV.Reset();
	Res.VSMDepthStencilView.Reset();
	Res.VSMDepthTexture.Reset();

    Res.FogTex.Reset();
    Res.FogRTV.Reset();
    Res.FogSRV.Reset();

    Res.WorldPosRTV.Reset();
    Res.WorldPosSRV.Reset();
    Res.WorldPosTex.Reset();

    Res.FXAARTV.Reset();
    Res.FXAASRV.Reset();
    Res.FXAATex.Reset();

    Res.SandervistanRTV.Reset();
    Res.SandervistanSRV.Reset();
    Res.SandervistanTex.Reset();

    Res.PostProcessRTV.Reset();
    Res.PostProcessSRV.Reset();
    Res.PostProcessTex.Reset();

    Res.RenderTargetSet = FRenderTargetSet();
    Res.Width = 0;
    Res.Height = 0;
}

void FRenderer::ReleaseEditorIdPickResource(FViewportRenderResource& Res)
{
    Res.EditorIdPickDebugSRV.Reset();
    Res.EditorIdPickDebugRTV.Reset();
    Res.EditorIdPickDebugTex.Reset();
    Res.EditorIdPickReadbackTex.Reset();
    Res.EditorIdPickSRV.Reset();
    Res.EditorIdPickRTV.Reset();
    Res.EditorIdPickTex.Reset();
}

// ============================================================
// 패스별 기본 렌더 상태 테이블 초기화
// ============================================================
void FRenderer::InitializePassRenderStates()
{
	using E = ERenderPass;
	auto& S = PassRenderStates;

	S[(uint32)E::Opaque] = { false };
	S[(uint32)E::Decal] = { false };
	S[(uint32)E::Light] = { false };
	S[(uint32)E::Translucent] = { false };
	S[(uint32)E::Fog] = { false };
    S[(uint32)E::FXAA] = { false };
	S[(uint32)E::SelectionMask] = { false };
	S[(uint32)E::Editor] = { true };
	S[(uint32)E::EditorOverlay] = { true };
	S[(uint32)E::Grid] = { false };
	S[(uint32)E::DepthLess] = { false };
	S[(uint32)E::Font] = { true };
	S[(uint32)E::SubUV] = { true };
    S[(uint32)E::PostProcessOutline] = { false };

}

// ============================================================
// Pass Batcher 바인딩 초기화
// ============================================================
void FRenderer::InitializePassBatchers()
{
	// --- Editor 패스: AABB 디버그 박스 → EditorLineBatcher ---
	PassBatchers[(uint32)ERenderPass::Editor] = {
		/*.Clear   =*/ [this]() { EditorLineBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus&) {
			if (Cmd.Type == ERenderCommandType::DebugBox)
			{
				EditorLineBatcher.AddAABB(FBoundingBox{ Cmd.Constants.AABB.Min, Cmd.Constants.AABB.Max }, Cmd.Constants.AABB.Color);
			}
			else if (Cmd.Type == ERenderCommandType::DebugOBB)
			{
				EditorLineBatcher.AddOBB(FOBB{ Cmd.Constants.OBB.Center, Cmd.Constants.OBB.Extents, Cmd.Constants.OBB.Rotation }, Cmd.Constants.OBB.Color);
			}
			else if (Cmd.Type == ERenderCommandType::DebugDirectionalLight)
			{
				const auto& D = Cmd.Constants.DirectionalLight;
				EditorLineBatcher.AddDirectionalLight(D.Position, D.Direction, 1.5f, D.Color);
			}
			else if (Cmd.Type == ERenderCommandType::DebugPointLight)
			{
				const auto& P = Cmd.Constants.PointLight;
				EditorLineBatcher.AddPointLight(P.Position, P.Range, P.Color);
			}
			else if (Cmd.Type == ERenderCommandType::DebugSpotlight)
			{
				const auto& S = Cmd.Constants.SpotLight;
				EditorLineBatcher.AddSpotLight(S.Position, S.Direction, S.Range, S.InnerAngle, S.OuterAngle, S.Color);
			}
			else if (Cmd.Type == ERenderCommandType::DebugLine)
			{
				const auto& L = Cmd.Constants.Line;
				EditorLineBatcher.AddLine(L.Start, L.End, L.Color);
			}
		},
		/*.Flush   =*/ [this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			FlushLineBatcher(EditorLineBatcher, Pass, Bus, Ctx);
		}
	};

	// --- EditorOverlay 패스: 항상 위에 보이는 디버그 와이어 (본 등) → EditorOverlayLineBatcher ---
	PassBatchers[(uint32)ERenderPass::EditorOverlay] = {
		/*.Clear   =*/ [this]() { EditorOverlayLineBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus&) {
			if (Cmd.Type == ERenderCommandType::DebugLine)
			{
				const auto& L = Cmd.Constants.Line;
				EditorOverlayLineBatcher.AddLine(L.Start, L.End, L.Color);
			}
			else if (Cmd.Type == ERenderCommandType::DebugBone)
			{
				const auto& B = Cmd.Constants.Bone;
				EditorOverlayLineBatcher.AddBoneOctahedron(B.Start, B.End, B.Color, B.WidthRatio);

				// 양 끝점 sphere — 본 길이에 비례한 작은 와이어 구.
				const float Length = (B.End - B.Start).Size();
				const float SphereRadius = Length * B.EndpointRadiusRatio;
				EditorOverlayLineBatcher.AddWireSphere(B.Start, SphereRadius, B.Color, 12);
				EditorOverlayLineBatcher.AddWireSphere(B.End,   SphereRadius, B.Color, 12);
			}
		},
		/*.Flush   =*/ [this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			FlushLineBatcher(EditorOverlayLineBatcher, Pass, Bus, Ctx);
		}
	};

	// --- Grid 패스: 월드 그리드 + 축 → GridLineBatcher ---
	PassBatchers[(uint32)ERenderPass::Grid] = {
		/*.Clear   =*/ [this]() { GridLineBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::Grid)
			{
				const FVector CameraPos = Bus.GetView().GetInverse().GetOrigin();
				const FVector CameraFwd = Bus.GetCameraForward();

				GridLineBatcher.AddWorldHelpers(
					Bus.GetShowFlags(),
					Cmd.Constants.Grid.GridSpacing,
					Cmd.Constants.Grid.GridHalfLineCount,
					CameraPos, CameraFwd,
					Cmd.Constants.Grid.bOrthographic);
			}
		},
		/*.Flush   =*/ [this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			FlushLineBatcher(GridLineBatcher, Pass, Bus, Ctx);
		}
	};

	// --- Font 패스: 텍스트 → FontBatcher ---
	PassBatchers[(uint32)ERenderPass::Font] = {
		/*.Clear   =*/ [this]() { FontBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::Font && Cmd.Constants.Font.Text && !Cmd.Constants.Font.Text->empty())
			{
				FontBatcher.AddText(
					*Cmd.Constants.Font.Text,
					Cmd.PerObjectConstants.Model,
					Cmd.Constants.Font.Scale
				);
			}
		},
		/*.Flush   =*/ [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.Flush(Ctx, FontRes);
		}
	};

	// --- SubUV 패스: 스프라이트 → SubUVBatcher ---
	// Collect 시 첫 번째 유효한 SRV를 캡처하여 Flush에서 재순회 방지
	PassBatchers[(uint32)ERenderPass::SubUV] = {
		/*.Clear   =*/ [this]() {
			SubUVBatcher.Clear();
			SubUVCachedTexture = nullptr;
		},
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::SubUV && Cmd.Constants.SubUV.Particle)
			{
				const auto& SubUV = Cmd.Constants.SubUV;
				if (!SubUVCachedTexture && SubUV.Particle->IsLoaded())
				{
					SubUVCachedTexture = SubUV.Particle->Texture;
				}

				SubUVBatcher.AddSprite(
					SubUV.Particle->Texture,
					Cmd.PerObjectConstants.Model.GetOrigin(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Cmd.PerObjectConstants.Model.GetScaleVector(),
					SubUV.FrameIndex,
					SubUV.Particle->Columns,
					SubUV.Particle->Rows,
					SubUV.Width,
					SubUV.Height
				);
			}
			// 기존 SubUV 분기 아래에
			else if (Cmd.Type == ERenderCommandType::Billboard && Cmd.Constants.Billboard.Texture)
			{
				SubUVBatcher.AddSprite(
					Cmd.Constants.Billboard.Texture,
					Cmd.PerObjectConstants.Model.GetOrigin(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Cmd.PerObjectConstants.Model.GetScaleVector(),
					0,   // FrameIndex 고정
					1,   // Columns 고정
					1,   // Rows 고정
					Cmd.Constants.Billboard.Width,
					Cmd.Constants.Billboard.Height,
					Cmd.Constants.Billboard.Color
				);
			}
		},
		/*.Flush   =*/ [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			SubUVBatcher.Flush(Ctx);
		}
	};
}

// ============================================================
// LineBatcher Flush 공통
// ============================================================
void FRenderer::FlushLineBatcher(FLineBatcher& Batcher, ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
	if (Batcher.GetLineCount() == 0) return;

	ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

	Batcher.Flush(Context);
}

// ============================================================
// 기본 패스 실행기
// ============================================================
void FRenderer::ExecuteDefaultPass(ERenderPass Pass, const TArray<FRenderCommand>& Commands, const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
	//ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

	//ERenderCommandType LastCommandType = static_cast<ERenderCommandType>(-1);
	//for (const auto& Cmd : Commands)
	//{
	//	BindShaderByType(Cmd, Context, LastCommandType);
	//	if (Cmd.Type == ERenderCommandType::PostProcessOutline)
	//	{
	//		DrawPostProcessOutline(Context);
	//		continue;
	//	}
	//	DrawCommand(Context, Cmd);
	//}
}

void FRenderer::ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context, EViewMode CurViewMode)
{
    ID3D11RenderTargetView* RTVs[MaxRTVCount] = {nullptr, nullptr};
    ID3D11DepthStencilView* DSV = nullptr;

	/** Pass 별 RTV 설정 */
	switch (Pass)
	{
		/**
		* TODO: Final 로 쓰이는 경로가 Light, Fog 만 있어서 현재는 해당 패스들만 Final 기록 (추후 확장 필요)
		* 
		*/
        case ERenderPass::Opaque:
			RTVs[0] = CurrentRenderTargets.SceneColorRTV;
            RTVs[1] = CurrentRenderTargets.SceneNormalRTV;
            RTVs[2] = CurrentRenderTargets.SceneWorldPosRTV;
			break;
		case ERenderPass::Decal:
            RTVs[0] = CurrentRenderTargets.SceneColorRTV;
            RTVs[1] = CurrentRenderTargets.SceneNormalRTV;
            RTVs[2] = CurrentRenderTargets.SceneWorldPosRTV;
            break;
        case ERenderPass::Light:
			RTVs[0] = CurrentRenderTargets.SceneLightRTV;
            SceneFinalRTV = CurrentRenderTargets.SceneLightRTV;
            SceneFinalSRV = CurrentRenderTargets.SceneLightSRV;
            break;
        case ERenderPass::Fog:
            RTVs[0] = CurrentRenderTargets.SceneFogRTV;
            SceneFinalRTV = CurrentRenderTargets.SceneFogRTV;
            SceneFinalSRV = CurrentRenderTargets.SceneFogSRV;
            break;
        case ERenderPass::Sandervistan:
            RTVs[0] = CurrentRenderTargets.SceneSandervistanRTV;
            SceneFinalRTV = CurrentRenderTargets.SceneSandervistanRTV;
            SceneFinalSRV = CurrentRenderTargets.SceneSandervistanSRV;
            break;
        case ERenderPass::SelectionMask:
            RTVs[0] = CurrentRenderTargets.SelectionMaskRTV;
            break;
        case ERenderPass::FXAA:
            RTVs[0] = CurrentRenderTargets.SceneFXAARTV; // FXAA 결과도 최종 출력이므로 SceneFinalRTV 사용
            SceneFinalRTV = CurrentRenderTargets.SceneFXAARTV;
            SceneFinalSRV = CurrentRenderTargets.SceneFXAASRV;
            break;
        default:
			// 나머지 Pass (UI, ...) 들은 하나의 RTV 에 그린다 가정
            RTVs[0] = SceneFinalRTV.Get();
            break;
	}

	/** Pass 별 DSV 설정 */
	switch (Pass)
	{
        case ERenderPass::Light:
            DSV = nullptr;
			break;
        case ERenderPass::Fog:
            DSV = nullptr;
            break;
        case ERenderPass::FXAA:
			DSV = nullptr;
            break;
        default:
            DSV = CurrentRenderTargets.DepthStencilView;
            break;
	}

	Context->OMSetRenderTargets(MaxRTVCount, RTVs, DSV);

	const FPassRenderState& State = PassRenderStates[(uint32)Pass];

	ERasterizerType Rasterizer = ERasterizerType::SolidBackCull;
	if (State.bWireframeAware && CurViewMode == EViewMode::Wireframe)
	{
		Rasterizer = ERasterizerType::WireFrame;
	}

	//Device.SetDepthStencilState(State.DepthStencil);
	//Device.SetBlendState(State.Blend);
	//Device.SetRasterizerState(Rasterizer);
	Context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

}

void FRenderer::BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context, ERenderCommandType& LastCommandType)
{
    bool bTypeChanged = (LastCommandType != InCmd.Type);

    // 객체별 Transform Data는 항상 업데이트해야 한다.
    Resources.PerObjectConstantBuffer.Update(Context, &InCmd.PerObjectConstants, sizeof(FPerObjectConstants));
	ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
	Context->VSSetConstantBuffers(1, 1, &cb1);
	Context->PSSetConstantBuffers(1, 1, &cb1);

	// 데이터 Update는 항상 수행하지만, 셰이더/상수 버퍼 바인딩은 타입이 변경된 경우에만 수행
    switch (InCmd.Type)
    {
	case ERenderCommandType::PostProcessOutline:
	{
		UMaterial* OutlineMaterial = Cast<UMaterial>(InCmd.Material);
		FShaderProgram* Program = GetOutlineProgram(OutlineMaterial);
		if (Program)
		{
			Program->Bind(Context);
			OutlineMaterial->BindRenderStates(Context);
			OutlineMaterial->BindParameters(Context, Program->PS);
		}
		break;
	}
	}

    LastCommandType = InCmd.Type;
}

void FRenderer::DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr || !InCommand.MeshBuffer->IsValid())
	{
		return;
	}

	uint32 offset = 0;
	ID3D11Buffer* vertexBuffer = InCommand.MeshBuffer->GetVertexBuffer().GetBuffer();
	if (vertexBuffer == nullptr)
	{
		return;
	}

	uint32 vertexCount = InCommand.MeshBuffer->GetVertexBuffer().GetVertexCount();
	uint32 stride = InCommand.MeshBuffer->GetVertexBuffer().GetStride();
	if (vertexCount == 0 || stride == 0)
	{
		return;
	}

	if (InCommand.Material)
	{
		FShaderProgram* Program = GetProgramForMaterialCommand(InCommand);
		if (Program)
		{
			Program->Bind(InDeviceContext);
			InCommand.Material->BindRenderStates(InDeviceContext);
			InCommand.Material->BindParameters(InDeviceContext, Program->PS);
			BindVertexFactoryResources(InDeviceContext, InCommand.VertexFactoryType, InCommand);
		}
	}

	InDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetIndexBuffer().GetBuffer();
	if (indexBuffer != nullptr)
	{
		uint32 indexStart = InCommand.SectionIndexStart;
		uint32 indexCount = InCommand.SectionIndexCount;
		InDeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		InDeviceContext->DrawIndexed(indexCount, indexStart, 0);
	}
	else
	{
		InDeviceContext->Draw(vertexCount, 0);
	}
}

void FRenderer::DrawPostProcessOutline(ID3D11DeviceContext* InDeviceContext)
{
	ID3D11RenderTargetView* RTV = SceneFinalRTV.Get();
	InDeviceContext->OMSetRenderTargets(1, &RTV, nullptr);
	InDeviceContext->OMSetDepthStencilState(nullptr, 0);

	ID3D11ShaderResourceView* maskSRV = CurrentRenderTargets.SelectionMaskSRV;
	InDeviceContext->PSSetShaderResources(7, 1, &maskSRV);

	auto DepthStencilState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
	auto BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
    auto RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidBackCull);
    
	InDeviceContext->OMSetDepthStencilState(DepthStencilState, 0);
	InDeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
	InDeviceContext->RSSetState(RasterizerState);

	InDeviceContext->Draw(3, 0);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	InDeviceContext->PSSetShaderResources(7, 1, &nullSRV);
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
#if STATS
	FGPUProfiler::Get().EndFrame();
#endif
	Device.EndFrame();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
	FFrameConstants frameConstantData;
	frameConstantData.View = InRenderBus.GetView();
	frameConstantData.Projection = InRenderBus.GetProj();
	frameConstantData.CameraPosition = InRenderBus.GetCameraPosition();
	frameConstantData.bIsOrthographic = InRenderBus.IsOrthographic();
	frameConstantData.WireframeColor = InRenderBus.GetWireframeColor();
	frameConstantData.bIsWireframe = (InRenderBus.GetViewMode() == EViewMode::Wireframe);
    frameConstantData.ViewportSize = InRenderBus.GetViewportSize();
    frameConstantData.NearPlane = InRenderBus.GetNearPlane();
    frameConstantData.FarPlane = InRenderBus.GetFarPlane();

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(0, 1, &b0);
	Context->PSSetConstantBuffers(0, 1, &b0);
}

void FRenderer::UpdateUberBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
	// Update Decal struct buf and textures ? one entry per unique decal
    TArray<UTexture*> DecalTextures;
    TArray<FDecalInfo> DecalConstantArray;
    const auto& Cmds = InRenderBus.GetCommands(ERenderPass::Decal);
    for (const auto& Cmd : Cmds)
    {
        bool bAlreadyAdded = false;
        for (const auto& Existing : DecalConstantArray)
        {
            if (memcmp(&Existing.InvDecalWorld, &Cmd.Constants.Decal.InvDecalWorld, sizeof(FMatrix)) == 0)
            {
                bAlreadyAdded = true;
                break;
            }
        }
        if (bAlreadyAdded) continue;

        FDecalInfo Constant = {};
        FMaterialParamValue TexValue;
        UTexture* Tex = nullptr;
        if (Cmd.Material && Cmd.Material->GetParam("DiffuseMap", TexValue))
            Tex = std::get<UTexture*>(TexValue.Value);
        DecalTextures.push_back(Tex);
        Constant.TextureIndex = (uint32)DecalConstantArray.size();
        Constant.InvDecalWorld = Cmd.Constants.Decal.InvDecalWorld;
        Constant.ColorTint = Cmd.Constants.Decal.ColorTint;
        DecalConstantArray.push_back(Constant);
    }

    if (!DecalConstantArray.empty())
    {
        Resources.DecalStructuredBuffer.Update(Context, DecalConstantArray.data(), (uint32)DecalConstantArray.size());
        ID3D11ShaderResourceView* DecalSRV = Resources.DecalStructuredBuffer.GetSRV();
        Context->PSSetShaderResources(8, 1, &DecalSRV);
    }

    if (DecalTextures != CachedDecalTextures)
    {
        DecalTextureArray.Reset();
        DecalTextureArraySRV.Reset();

        // Read dimensions/format from first valid texture
        ID3D11Texture2D* FirstTex2D = nullptr;
        for (auto* Tex : DecalTextures)
        {
            if (!Tex || !Tex->GetSRV())
                continue;
            ID3D11Resource* Res = nullptr;
            Tex->GetSRV()->GetResource(&Res);
            Res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&FirstTex2D));
            Res->Release();
            if (FirstTex2D)
                break;
        }

        if (FirstTex2D)
        {
            D3D11_TEXTURE2D_DESC SrcDesc = {};
            FirstTex2D->GetDesc(&SrcDesc);
            FirstTex2D->Release();

            D3D11_TEXTURE2D_DESC Desc = {};
            Desc.Width = SrcDesc.Width;
            Desc.Height = SrcDesc.Height;
            Desc.MipLevels = 1;
            Desc.ArraySize = (uint32)DecalTextures.size();
            Desc.Format = SrcDesc.Format;
            Desc.SampleDesc.Count = 1;
            Desc.Usage = D3D11_USAGE_DEFAULT;
            Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            Device.GetDevice()->CreateTexture2D(&Desc, nullptr, &DecalTextureArray);

            for (uint32 i = 0; i < (uint32)DecalTextures.size(); i++)
            {
                auto* Tex = DecalTextures[i];
                if (!Tex || !Tex->GetSRV())
                    continue;
                ID3D11Resource* Res = nullptr;
                Tex->GetSRV()->GetResource(&Res);
                Context->CopySubresourceRegion(
                    DecalTextureArray.Get(), D3D11CalcSubresource(0, i, 1),
                    0, 0, 0,
                    Res, 0, nullptr);
                Res->Release();
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.Format = Desc.Format;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            SRVDesc.Texture2DArray.MostDetailedMip = 0;
            SRVDesc.Texture2DArray.MipLevels = 1;
            SRVDesc.Texture2DArray.FirstArraySlice = 0;
            SRVDesc.Texture2DArray.ArraySize = Desc.ArraySize;

            Device.GetDevice()->CreateShaderResourceView(DecalTextureArray.Get(), &SRVDesc, &DecalTextureArraySRV);
        }

        CachedDecalTextures = DecalTextures;
    }

    if (DecalTextureArraySRV)
    {
        ID3D11ShaderResourceView* ArraySRV = DecalTextureArraySRV.Get();
        Context->VSSetShaderResources(9, 1, &ArraySRV);
        Context->PSSetShaderResources(9, 1, &ArraySRV);
    }

	FUberConstants lightConstantData;
    lightConstantData.AmbientLight = InRenderBus.AmbientLightInfo;
    lightConstantData.DirectionalLight = InRenderBus.DirectionalLightInfo;
    lightConstantData.LightCount = (uint32)InRenderBus.LightInfos.size();
    lightConstantData.DecalCount = (uint32)DecalConstantArray.size();

    Resources.LightBuffer.Update(Context, &lightConstantData, sizeof(FUberConstants));
    ID3D11Buffer* b3 = Resources.LightBuffer.GetBuffer();
    Context->VSSetConstantBuffers(3, 1, &b3);
    Context->PSSetConstantBuffers(3, 1, &b3);
    Context->CSSetConstantBuffers(3, 1, &b3);

	ID3D11Buffer* b4 = Resources.ShadowBuffer.GetBuffer();
	Context->VSSetConstantBuffers(4, 1, &b4);
	Context->PSSetConstantBuffers(4, 1, &b4);

    Resources.LightStructuredBuffer.Update(Context, InRenderBus.LightInfos.data(), (uint32)InRenderBus.LightInfos.size());
    ID3D11ShaderResourceView* SRVs[] = {
        Resources.LightStructuredBuffer.GetSRV(),
    };
    Context->VSSetShaderResources(4, 1, SRVs);
    Context->PSSetShaderResources(4, 1, SRVs);
}
