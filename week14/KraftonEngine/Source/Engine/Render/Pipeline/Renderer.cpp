#include "Renderer.h"

#include "Render/Types/RenderTypes.h"
#include "Render/Shader/ShaderManager.h"
#include "Core/Log.h"
#include "Render/Scene/FScene.h"
#include "GameFramework/World.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Profiling/StartupProfiler.h"
#include "Materials/MaterialManager.h"


void FRenderer::Create(HWND hWindow)
{
	{
		SCOPE_STARTUP_STAT("  D3DDevice::CreateHW");
		Device.Create(hWindow);
	}

	if (Device.GetDevice() == nullptr)
	{
		UE_LOG("Failed to create D3D Device.");
	}

	{
		SCOPE_STARTUP_STAT("  ShaderManager::Init");
		FShaderManager::Get().Initialize(Device.GetDevice());
	}

	{
		SCOPE_STARTUP_STAT("  SystemResources::Create");
		Resources.Create(Device.GetDevice());
	}

	{
		SCOPE_STARTUP_STAT("  TileCulling::Init");
		Resources.TileBasedCulling.Initialize(Device.GetDevice());
	}

	{
		SCOPE_STARTUP_STAT("  ClusteredCuller::Init");
		Resources.ClusteredLightCuller.Initialize(Device.GetDevice(), Device.GetDeviceContext());
	}

	{
		SCOPE_STARTUP_STAT("  RenderPassPipeline::Init");
		Pipeline.Initialize();
	}

	{
		SCOPE_STARTUP_STAT("  DrawCommandBuilder::Create");
		Builder.Create(Device.GetDevice(), Device.GetDeviceContext(), &Pipeline.GetStateTable());
	}

	// GPU Profiler 초기화
	FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
	FGPUProfiler::Get().Shutdown();

	Builder.Release();

	Resources.TileBasedCulling.Release();
	Resources.ClusteredLightCuller.Release();
	Resources.Release();
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
		Resources.UpdateLightBuffer(Device, Scene, Frame);
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

	FPassContext PassCtx{ Device, Frame, Cache, Resources, CommandList, this, &Scene };
	Pipeline.Execute(PassCtx);

	CleanupPassState(Cache);
}

// ============================================================
// CleanupPassState — 패스 루프 종료 후 시스템 텍스처 언바인딩 + 캐시 정리
// ============================================================
void FRenderer::CleanupPassState(FStateCache& Cache)
{
	Resources.UnbindSystemTextures(Device);
	Resources.UnbindTileCullingBuffers(Device);
	Resources.UnbindClusterCullingResources(Device);

	Cache.Cleanup(Device.GetDeviceContext());
	Builder.GetCommandList().Reset();
}

void FRenderer::SubmitCullingDebugLines(UWorld* World)
{
	Resources.SubmitCullingDebugLines(Device.GetDeviceContext(), World);
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
	Device.Present();
}
