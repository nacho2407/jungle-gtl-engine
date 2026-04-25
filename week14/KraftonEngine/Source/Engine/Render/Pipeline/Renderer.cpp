#include "Renderer.h"

#include "Render/Types/RenderTypes.h"
#include "Render/Resource/ShaderManager.h"
#include "Core/Log.h"
#include "Render/Proxy/FScene.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Materials/MaterialManager.h"


void FRenderer::Create(HWND hWindow)
{
	Device.Create(hWindow);

	if (Device.GetDevice() == nullptr)
	{
		UE_LOG("Failed to create D3D Device.");
	}

	FShaderManager::Get().Initialize(Device.GetDevice());
	Resources.Create(Device.GetDevice());

	TileBasedCulling.Initialize(Device.GetDevice());
	ClusteredLightCuller.Initialize(Device.GetDevice(), Device.GetDeviceContext());

	Pipeline.Initialize();

	Builder.Create(Device.GetDevice(), Device.GetDeviceContext(), &Pipeline.GetStateTable());

	// GPU Profiler 초기화
	FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
	FGPUProfiler::Get().Shutdown();

	Builder.Release();

	Resources.Release();
	TileBasedCulling.Release();
	ClusteredLightCuller.Release();
	FShaderManager::Get().Release();
	FMaterialManager::Get().Release();
	Device.Release();
}

//	스왑체인 백버퍼 복귀 — ImGui 합성 직전에 호출
void FRenderer::BeginFrame()
{
	Device.BeginFrame();
}

// ============================================================
// Render — 정렬 + GPU 제출
// BeginCollect + Collector + BuildDynamicCommands 이후에 호출.
// ============================================================
void FRenderer::Render(const FFrameContext& Frame, FScene& Scene)
{
	FDrawCallStats::Reset();

	{
		SCOPE_STAT_CAT("UpdateFrameBuffer", "4_ExecutePass");
		Resources.UpdateFrameBuffer(Device, Frame);
	}
	{
		SCOPE_STAT_CAT("UpdateLightBuffer", "4_ExecutePass");

		FClusterCullingState& ClusterState = ClusteredLightCuller.GetCullingState();
		ClusterState.NearZ = Frame.NearClip;
		ClusterState.FarZ = Frame.FarClip;
		ClusterState.ScreenWidth = static_cast<uint32>(Frame.ViewportWidth);
		ClusterState.ScreenHeight = static_cast<uint32>(Frame.ViewportHeight);

		Resources.UpdateLightBuffer(Device, Scene, Frame, &ClusterState);
	}

	// 시스템 샘플러 영구 바인딩 (s0-s2)
	Resources.BindSystemSamplers(Device);

	FDrawCommandList& CommandList = Builder.GetCommandList();

	// 커맨드 정렬 + 패스별 오프셋 빌드
	CommandList.Sort();

	// 단일 StateCache — 패스 간 상태 유지 (DSV Read-Only 전환 등)
	FStateCache Cache;
	Cache.Reset();
	Cache.RTV = Frame.ViewportRTV;
	Cache.DSV = Frame.ViewportDSV;

	FPassContext PassCtx{ Device, Frame, Cache, this };
	Pipeline.Execute(PassCtx, CommandList, Device, Resources);

	CleanupPassState(Cache);
}

// ============================================================
// CleanupPassState — 패스 루프 종료 후 시스템 텍스처 언바인딩 + 캐시 정리
// ============================================================
void FRenderer::CleanupPassState(FStateCache& Cache)
{
	Resources.UnbindSystemTextures(Device);
	Resources.UnbindTileCullingBuffers(Device);
	UnbindClusterCullingResources();

	Cache.Cleanup(Device.GetDeviceContext());
	Builder.GetCommandList().Reset();
}

void FRenderer::DispatchClusterCullingResources()
{
	if (!ClusteredLightCuller.IsInitialized())
	{
		return;
	}

	Resources.UnbindTileCullingBuffers(Device);
	UnbindClusterCullingResources();

	/*{
		GPU_SCOPE_STAT_CAT("ClutserCulling AABB Creation", "AABBCreation");
		ClusteredLightCuller.DispatchViewSpaceAABB();
	}*/
	{
		GPU_SCOPE_STAT_CAT("Cluster Culling Dispatch", "Culling Dispatch");
		ClusteredLightCuller.DispatchLightCullingCS(Resources.ForwardLights.LightBufferSRV);
	}

	BindClusterCullingResources();
}

void FRenderer::BindClusterCullingResources()
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	ID3D11ShaderResourceView* LightIndexList = ClusteredLightCuller.GetLightIndexListSRV();
	ID3D11ShaderResourceView* LightGridList = ClusteredLightCuller.GetLightGridSRV();
	Ctx->VSSetShaderResources(ELightTexSlot::ClusterLightIndexList, 1, &LightIndexList);
	Ctx->VSSetShaderResources(ELightTexSlot::ClusterLightGrid, 1, &LightGridList);
	Ctx->PSSetShaderResources(ELightTexSlot::ClusterLightIndexList, 1, &LightIndexList);
	Ctx->PSSetShaderResources(ELightTexSlot::ClusterLightGrid, 1, &LightGridList);
}

void FRenderer::UnbindClusterCullingResources()
{
	ID3D11DeviceContext* Ctx = Device.GetDeviceContext();
	ID3D11ShaderResourceView* NullSRVs[2] = {};
	Ctx->VSSetShaderResources(ELightTexSlot::ClusterLightIndexList, 2, NullSRVs);
	Ctx->PSSetShaderResources(ELightTexSlot::ClusterLightIndexList, 2, NullSRVs);
	Ctx->CSSetShaderResources(ELightTexSlot::ClusterLightIndexList, 2, NullSRVs);
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
	Device.Present();
}
