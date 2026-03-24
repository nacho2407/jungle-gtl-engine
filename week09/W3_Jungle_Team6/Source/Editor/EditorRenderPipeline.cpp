#include "EditorRenderPipeline.h"

#include "Editor/EditorEngine.h"
#include "Render/Renderer/Renderer.h"
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
		const auto& Settings = Editor->GetSettings();
		const FShowFlags& ShowFlags = Settings.ShowFlags;
		EViewMode ViewMode = Settings.ViewMode;

		Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix(),
			Camera->GetRightVector(), Camera->GetUpVector());
		Bus.SetRenderSettings(ViewMode, ShowFlags);

		Collector.CollectWorld(World, ShowFlags, ViewMode, Bus);
		Collector.CollectGrid(Settings.GridSpacing, Settings.GridHalfLineCount, Bus);
		Collector.CollectGizmo(Editor->GetGizmo(), ShowFlags, Bus);
		Collector.CollectSelection(
			Editor->GetSelectionManager().GetSelectedActors(),
			ShowFlags, ViewMode, Bus);
	}

	Renderer.PrepareBatchers(Bus);
	Renderer.BeginFrame();
	Renderer.Render(Bus);
	Editor->RenderUI(DeltaTime);
	Renderer.EndFrame();
}
