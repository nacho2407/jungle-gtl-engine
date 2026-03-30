#include "Viewport/Services/EditorViewportRenderService.h"

#include "EditorEngine.h"
#include "Viewport/EditorViewportRegistry.h"
#include "Actor/Actor.h"
#include "Core/Engine.h"
#include "Gizmo/Gizmo.h"
#include "Math/Frustum.h"
#include "Renderer/Material.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"
#include "UI/EditorUI.h"
#include "Viewport/BlitRenderer.h"
#include "Viewport/Viewport.h"
#include "Component/SkyComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Asset/ObjManager.h"
#include "Slate/Painter.h"


void FEditorViewportRenderService::RenderAll(
	FEngine* Engine,
	FRenderer* Renderer,
	FEditorEngine* EditorEngine,
	FEditorViewportRegistry& ViewportRegistry,
	FEditorUI& EditorUI,
	FGizmo& Gizmo,
	FBlitRenderer& BlitRenderer,
	const std::shared_ptr<FMaterial>& WireFrameMaterial,
	FRenderMesh* GridMesh,
	FMaterial* GridMaterial,
	const FBuildRenderCommands& BuildRenderCommands) const
{
	if (!Engine || !Renderer || !EditorEngine)
	{
		return;
	}

	ID3D11Device* Device = Renderer->GetDevice();
	ID3D11DeviceContext* Context = Renderer->GetDeviceContext();
	if (!Device || !Context)
	{
		return;
	}

	UScene* Scene = Engine->GetScene();
	if (!Scene)
	{
		return;
	}

	constexpr float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	const TArray<FViewportEntry>& Entries = ViewportRegistry.GetEntries();

	for (const FViewportEntry& Entry : Entries)
	{
		if (!Entry.bActive || !Entry.Viewport)
		{
			continue;
		}

		Entry.Viewport->EnsureResources(Device);

		ID3D11RenderTargetView* RTV = Entry.Viewport->GetRTV();
		ID3D11DepthStencilView* DSV = Entry.Viewport->GetDSV();
		if (!RTV || !DSV)
		{
			continue;
		}

		const FRect& Rect = Entry.Viewport->GetRect();
		D3D11_VIEWPORT Viewport = {};
		Viewport.TopLeftX = 0.0f;
		Viewport.TopLeftY = 0.0f;
		Viewport.Width = static_cast<float>(Rect.Width);
		Viewport.Height = static_cast<float>(Rect.Height);
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;

		Context->ClearRenderTargetView(RTV, ClearColor);
		Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		Renderer->BeginScenePass(RTV, DSV, Viewport);

		const float AspectRatio = static_cast<float>(Rect.Width) / static_cast<float>(Rect.Height);
		FRenderCommandQueue Queue;
		Queue.Reserve(Renderer->GetPrevCommandCount());
		Queue.ProjectionMatrix = Entry.LocalState.BuildProjMatrix(AspectRatio);
		Queue.ViewMatrix = Entry.LocalState.BuildViewMatrix();

		FFrustum Frustum;
		Frustum.ExtractFromVP(Queue.ViewMatrix * Queue.ProjectionMatrix);
		const FVector CameraPosition = Queue.ViewMatrix.GetInverse().GetTranslation();
		BuildRenderCommands(Engine, Scene, Frustum, Entry.LocalState.ShowFlags, CameraPosition, Queue);

		AActor* GizmoTarget = EditorEngine->GetSelectedActor();
		if (GizmoTarget && GizmoTarget->GetComponentByClass<USkyComponent>() == nullptr)
		{
			Gizmo.BuildRenderCommands(GizmoTarget, &Entry, Queue);
		}

		if (Entry.LocalState.ViewMode == ERenderMode::Wireframe && WireFrameMaterial)
		{
			ApplyWireframe(Queue, WireFrameMaterial.get());
		}

		if (Entry.LocalState.bShowGrid && GridMesh && GridMaterial)
		{
			GridMaterial->SetParameterData("GridSize", &Entry.LocalState.GridSize, 4);
			GridMaterial->SetParameterData("LineThickness", &Entry.LocalState.LineThickness, 4);

			FRenderCommand GridCommand;
			GridCommand.RenderMesh = GridMesh;
			GridCommand.Material = GridMaterial;
			GridCommand.WorldMatrix = FMatrix::Identity;
			GridCommand.RenderLayer = ERenderLayer::Default;
			Queue.AddCommand(GridCommand);
		}

		Renderer->SubmitCommands(Queue);
		Renderer->ExecuteCommands();
		EditorEngine->FlushDebugDrawForViewport(Renderer, Entry.LocalState.ShowFlags, false);
		Renderer->EndScenePass();
	}
	EditorEngine->ClearDebugDrawForFrame();

	Renderer->BindSwapChainRTV();
	BlitRenderer.BlitAll(Context, Entries);

	Renderer->BindSwapChainRTV();
	if (FSlateApplication* Slate = EditorEngine->GetSlateApplication())
	{
		FPainter Painter(Renderer);

		RECT rc{};
		::GetClientRect(Renderer->GetHwnd(), &rc);
		Painter.SetScreenSize(rc.right - rc.left, rc.bottom - rc.top);
		Slate->Paint(Painter);
		Painter.Flush();
	}

	EditorUI.Render();
}

void FEditorViewportRenderService::ApplyWireframe(FRenderCommandQueue& Queue, FMaterial* WireMaterial)
{
	for (FRenderCommand& Command : Queue.Commands)
	{
		if (Command.RenderLayer != ERenderLayer::Overlay)
		{
			Command.Material = WireMaterial;
		}
	}
}
