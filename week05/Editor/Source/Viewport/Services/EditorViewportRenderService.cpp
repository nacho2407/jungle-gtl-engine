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
#include "Component/PrimitiveComponent.h"
#include "Component/SkyComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Asset/ObjManager.h"
#include "Slate/Widget/Painter.h"
#include <algorithm>
#include <utility>

namespace
{
#ifndef WITH_EDITOR_SINGLE_VIEWPORT_DIRECT_RENDER
#define WITH_EDITOR_SINGLE_VIEWPORT_DIRECT_RENDER 1
#endif

	size_t GSceneCommandReserveHint = 2048;

	int32 CountActiveViewportEntries(const TArray<FViewportEntry>& Entries)
	{
		int32 ActiveCount = 0;
		for (const FViewportEntry& Entry : Entries)
		{
			if (Entry.bActive && Entry.Viewport)
			{
				++ActiveCount;
			}
		}

		return ActiveCount;
	}

	bool BuildSceneViewport(const FRect& Rect, bool bDirectToBackBuffer, D3D11_VIEWPORT& OutViewport)
	{
		if (!Rect.IsValid())
		{
			return false;
		}

		OutViewport = {};
		OutViewport.TopLeftX = bDirectToBackBuffer ? static_cast<float>(Rect.X) : 0.0f;
		OutViewport.TopLeftY = bDirectToBackBuffer ? static_cast<float>(Rect.Y) : 0.0f;
		OutViewport.Width = static_cast<float>(Rect.Width);
		OutViewport.Height = static_cast<float>(Rect.Height);
		OutViewport.MinDepth = 0.0f;
		OutViewport.MaxDepth = 1.0f;
		return true;
	}

	void BuildGridVectors(const FMatrix& ViewInverse, const FViewportLocalState& LocalState, FVector& OutGridAxisU, FVector& OutGridAxisV, FVector& OutViewForward)
	{
		OutViewForward = ViewInverse.GetForwardVector().GetSafeNormal();

		if (LocalState.ProjectionType == EViewportType::Perspective)
		{
			OutGridAxisU = FVector::ForwardVector;
			OutGridAxisV = FVector::RightVector;
			return;
		}

		OutGridAxisU = ViewInverse.GetRightVector().GetSafeNormal();
		OutGridAxisV = ViewInverse.GetUpVector().GetSafeNormal();
	}

	void BuildOutlineItemsForViewport(FEditorEngine* EditorEngine, const FViewportEntry& Entry, FRenderCommandQueue& Queue)
	{
		if (!EditorEngine || !Entry.LocalState.ShowFlags.HasFlag(EEngineShowFlags::SF_Primitives))
		{
			return;
		}

		AActor* SelectedActor = EditorEngine->GetSelectedActor();
		if (!SelectedActor || SelectedActor->IsPendingDestroy() || !SelectedActor->IsVisible())
		{
			return;
		}

		if (SelectedActor->GetComponentByClass<USkyComponent>() != nullptr)
		{
			return;
		}

		const TArray<UActorComponent*>& Components = SelectedActor->GetComponents();
		Queue.OutlineItems.reserve(Queue.OutlineItems.size() + Components.size());

		for (UActorComponent* Component : Components)
		{
			if (!Component || !Component->IsA(UPrimitiveComponent::StaticClass()))
			{
				continue;
			}
			if (Component->IsA(UTextComponent::StaticClass()) || Component->IsA(USubUVComponent::StaticClass()))
			{
				continue;
			}

			UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
			FRenderMesh* RenderMesh = PrimitiveComponent->GetRenderMesh();
			if (!RenderMesh)
			{
				continue;
			}

			FOutlineRenderItem Item = {};
			Item.Mesh = RenderMesh;
			Item.WorldMatrix = PrimitiveComponent->GetWorldTransform();
			Queue.OutlineItems.push_back(Item);
		}
	}
}


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
#if WITH_EDITOR_SINGLE_VIEWPORT_DIRECT_RENDER
	const bool bUseDirectSingleViewportPath = (CountActiveViewportEntries(Entries) == 1);
#else
	const bool bUseDirectSingleViewportPath = false;
#endif

	for (const FViewportEntry& Entry : Entries)
	{
		if (!Entry.bActive || !Entry.Viewport)
		{
			continue;
		}

		const FRect& Rect = Entry.Viewport->GetRect();
		D3D11_VIEWPORT Viewport = {};
		if (!BuildSceneViewport(Rect, bUseDirectSingleViewportPath, Viewport))
		{
			continue;
		}

		ID3D11RenderTargetView* RTV = nullptr;
		ID3D11DepthStencilView* DSV = nullptr;

		if (bUseDirectSingleViewportPath)
		{
			RTV = Renderer->GetRenderTargetView();
			DSV = Renderer->GetDepthStencilView();
		}
		else
		{
			Entry.Viewport->EnsureResources(Device);
			RTV = Entry.Viewport->GetRTV();
			DSV = Entry.Viewport->GetDSV();

			if (RTV && DSV)
			{
				Context->ClearRenderTargetView(RTV, ClearColor);
				Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
			}
		}

		if (!RTV || !DSV)
		{
			continue;
		}

		Renderer->BeginScenePass(RTV, DSV, Viewport);

		const float AspectRatio = static_cast<float>(Rect.Width) / static_cast<float>(Rect.Height);
		FRenderCommandQueue Queue;
		const size_t ReserveHint = (std::max)(Renderer->GetPrevCommandCount(), GSceneCommandReserveHint);
		Queue.Reserve(ReserveHint);
		Queue.ProjectionMatrix = Entry.LocalState.BuildProjMatrix(AspectRatio);
		Queue.ViewMatrix = Entry.LocalState.BuildViewMatrix();

		FFrustum Frustum;
		Frustum.ExtractFromVP(Queue.ViewMatrix * Queue.ProjectionMatrix);
		const FMatrix ViewInverse = Queue.ViewMatrix.GetInverse();
		const FVector CameraPosition = ViewInverse.GetTranslation();
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
			FVector GridAxisU = FVector::ForwardVector;
			FVector GridAxisV = FVector::RightVector;
			FVector ViewForward = FVector::ForwardVector;
			BuildGridVectors(ViewInverse, Entry.LocalState, GridAxisU, GridAxisV, ViewForward);

			GridMaterial->SetParameterData("GridSize", &Entry.LocalState.GridSize, 4);
			GridMaterial->SetParameterData("LineThickness", &Entry.LocalState.LineThickness, 4);
			GridMaterial->SetParameterData("GridAxisU", &GridAxisU, sizeof(FVector));
			GridMaterial->SetParameterData("GridAxisV", &GridAxisV, sizeof(FVector));
			GridMaterial->SetParameterData("ViewForward", &ViewForward, sizeof(FVector));

			FRenderCommand GridCommand;
			GridCommand.RenderMesh = GridMesh;
			GridCommand.Material = GridMaterial;
			GridCommand.WorldMatrix = FMatrix::Identity;
			GridCommand.RenderLayer = ERenderLayer::Default;
			Queue.AddCommand(GridCommand);
		}

		BuildOutlineItemsForViewport(EditorEngine, Entry, Queue);

		const size_t QueueSize = Queue.Commands.size();
		if (QueueSize > GSceneCommandReserveHint)
		{
			GSceneCommandReserveHint = QueueSize;
		}
		else
		{
			GSceneCommandReserveHint = (std::max)(QueueSize, GSceneCommandReserveHint * 7 / 8);
		}

		Renderer->SubmitCommands(std::move(Queue));
		Renderer->ExecuteCommands();
		EditorEngine->FlushDebugDrawForViewport(Renderer, Entry.LocalState.ShowFlags, false);
		Renderer->EndScenePass();
	}
	EditorEngine->ClearDebugDrawForFrame();

	Renderer->BindSwapChainRTV();
	if (!bUseDirectSingleViewportPath)
	{
		BlitRenderer.BlitAll(Context, Entries);
		Renderer->BindSwapChainRTV();
	}

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
