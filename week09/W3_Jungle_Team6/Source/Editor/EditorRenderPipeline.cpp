#include "EditorRenderPipeline.h"

#include "Editor/EditorEngine.h"
#include "Render/Renderer/Renderer.h"
#include "Render/Scene/RenderCollectorContext.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "Core/Stats.h"
#include "Core/GPUProfiler.h"

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer)
	: Editor(InEditor)
{
	Collector.Initialize(InRenderer.GetFD3DDevice().GetDevice());
}

FEditorRenderPipeline::~FEditorRenderPipeline()
{
	Collector.Release();
}

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
#if STATS
	FStatManager::Get().TakeSnapshot();
	FGPUProfiler::Get().TakeSnapshot();
#endif

	Bus.Clear();

	UWorld* World = Editor->GetWorld();
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
	if (Camera)
	{
		FRenderCollectorContext Context;
		Context.Camera = Camera;
		Context.ViewMode = Editor->GetSettings().ViewMode;
		Context.ShowFlags = Editor->GetSettings().ShowFlags;

		const FShowFlags& ShowFlags = Context.ShowFlags;
		EViewMode ViewMode = Context.ViewMode;

		TArray<FCollectPhase> Phases;

		Phases.push_back([&](FRenderBus& B) {
			Collector.CollectWorld(World, ShowFlags, ViewMode, B);
			});

		Phases.push_back([&](FRenderBus& B) {
			Collector.CollectGizmo(Editor->GetGizmo(), ShowFlags, B);
			});

		Phases.push_back([&](FRenderBus& B) {
			Collector.CollectSelection(
				Editor->GetSelectionManager().GetSelectedActors(),
				ShowFlags, ViewMode, B);
			});

		Collector.Collect(Context, Phases, Bus);
	}

	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Editor->RenderUI(DeltaTime);
	Renderer.EndFrame();
}
