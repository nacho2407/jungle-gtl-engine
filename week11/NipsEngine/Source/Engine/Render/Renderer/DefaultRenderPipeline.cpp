#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Camera/ViewportCamera.h"
#include "Core/Logging/Log.h"
#include "GameFramework/World.h"

FDefaultRenderPipeline::FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
	Collector.Initialize(InRenderer.GetFD3DDevice().GetDevice());
}

FDefaultRenderPipeline::~FDefaultRenderPipeline()
{
	Collector.Release();
}

void FDefaultRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	static bool bLoggedFirstRuntimeCollect = false;
	static bool bLoggedRuntimeFallback = false;
	const uint32 FrameWidth = static_cast<uint32>(Renderer.GetFD3DDevice().GetViewportWidth());
	const uint32 FrameHeight = static_cast<uint32>(Renderer.GetFD3DDevice().GetViewportHeight());

	Bus.Clear();

	UWorld* World = Engine->GetWorld();
	FViewportCamera* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FMatrix ViewMat = Camera->GetViewMatrix();
		FMatrix ProjMat = Camera->GetProjectionMatrix();

		FShowFlags ShowFlags;
		EViewMode ViewMode = EViewMode::Lit_BlinnPhong;

		Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix(), Camera->GetNearPlane(), Camera->GetFarPlane());
		Bus.SetRenderSettings(ViewMode, ShowFlags);
		Bus.SetViewportSize(FVector2(static_cast<float>(FrameWidth), static_cast<float>(FrameHeight)));
		Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
		Bus.SetFXAAEnabled(true);

		const FFrustum& ViewFrustum = Camera->GetFrustum();
		Collector.CollectWorld(World, ShowFlags, ViewMode, Bus, &ViewFrustum);

		const auto& CullingStats = Collector.GetLastCullingStats();
		const int32 OpaqueCount = static_cast<int32>(Bus.GetCommands(ERenderPass::Opaque).size());
		if (!bLoggedFirstRuntimeCollect)
		{
			const FVector CameraLocation = Camera->GetLocation();
			const FVector CameraForward = Camera->GetForwardVector();
			UE_LOG("[GameRender] First collect. Actors=%d VisiblePrimitives=%d BVHPassed=%d FallbackPassed=%d OpaqueCommands=%d Camera=(%.2f, %.2f, %.2f) Forward=(%.2f, %.2f, %.2f)",
				World ? static_cast<int32>(World->GetActors().size()) : 0,
				CullingStats.TotalVisiblePrimitiveCount,
				CullingStats.BVHPassedPrimitiveCount,
				CullingStats.FallbackPassedPrimitiveCount,
				OpaqueCount,
				CameraLocation.X, CameraLocation.Y, CameraLocation.Z,
				CameraForward.X, CameraForward.Y, CameraForward.Z);
			bLoggedFirstRuntimeCollect = true;
		}

		if (OpaqueCount == 0 && CullingStats.TotalVisiblePrimitiveCount > 0)
		{
			Bus.Clear();
			Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix(), Camera->GetNearPlane(), Camera->GetFarPlane());
			Bus.SetRenderSettings(ViewMode, ShowFlags);
			Bus.SetViewportSize(FVector2(static_cast<float>(FrameWidth), static_cast<float>(FrameHeight)));
			Bus.SetViewportOrigin(FVector2(0.0f, 0.0f));
			Bus.SetFXAAEnabled(true);
			Collector.CollectWorld(World, ShowFlags, ViewMode, Bus, nullptr);

			if (!bLoggedRuntimeFallback)
			{
				const auto& FallbackStats = Collector.GetLastCullingStats();
				UE_LOG_WARNING("[GameRender] Frustum collect produced no opaque commands. Fallback full-world collect used. VisiblePrimitives=%d OpaqueCommands=%d",
					FallbackStats.TotalVisiblePrimitiveCount,
					static_cast<int32>(Bus.GetCommands(ERenderPass::Opaque).size()));
				bLoggedRuntimeFallback = true;
			}
		}
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.BeginFrame();
	Renderer.BeginGameFrame(FrameWidth, FrameHeight);
	Renderer.Render(Bus);
	Renderer.CompositeCurrentSceneToBackBuffer();
	Engine->RenderRuntimeUI(FRuntimeUIRenderContext{
		ERuntimeUIRenderMode::GameClient,
		FRuntimeUIVector2(0.0f, 0.0f),
		FRuntimeUIVector2(static_cast<float>(FrameWidth), static_cast<float>(FrameHeight)),
		DeltaTime});
	Renderer.EndFrame();
}
