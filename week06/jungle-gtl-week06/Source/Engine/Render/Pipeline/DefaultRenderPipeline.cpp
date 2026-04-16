#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"

FDefaultRenderPipeline::FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
}

FDefaultRenderPipeline::~FDefaultRenderPipeline()
{
}

void FDefaultRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	Bus.Clear();

	UWorld* World = Engine->GetWorld();
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FShowFlags ShowFlags;
		EViewMode ViewMode = EViewMode::Lit;
		FD3DDevice& Device = Renderer.GetFD3DDevice();
		const D3D11_VIEWPORT& Viewport = Device.GetViewport();

		Bus.SetCameraInfo(Camera);
		Bus.SetRenderSettings(ViewMode, ShowFlags);
		Bus.SetRenderTargetInfo(
			Viewport.Width,
			Viewport.Height,
			Device.GetFrameBufferRTV(),
			Device.GetDepthStencilView(),
			nullptr,
			Device.GetDepthStencilSRV());

		Collector.CollectWorld(World, Bus);
		Collector.CollectDebugDraw(World->GetDebugDrawQueue(), Bus);
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Renderer.EndFrame();
}
