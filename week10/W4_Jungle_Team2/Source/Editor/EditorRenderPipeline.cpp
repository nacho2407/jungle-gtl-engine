#include "EditorRenderPipeline.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Render/Renderer/Renderer.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"

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
	const FViewportCamera* Camera = Editor->GetCamera();
	if (Camera)
	{
		const auto& Settings = Editor->GetSettings();
		const FShowFlags& ShowFlags = Settings.ShowFlags;
		EViewMode ViewMode = Settings.ViewMode;

		Bus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix());
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
