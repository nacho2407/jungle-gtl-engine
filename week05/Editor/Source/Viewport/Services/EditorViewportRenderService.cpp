#include "Viewport/Services/EditorViewportRenderService.h"

#include "EditorEngine.h"
#include "Viewport/EditorViewportRegistry.h"
#include "Actor/Actor.h"
#include "Core/Engine.h"
#include "Gizmo/Gizmo.h"
#include "Math/Frustum.h"
#include "Renderer/Material.h"
#include "Renderer/Renderer.h"
#include "Level/Level.h"
#include "UI/EditorUI.h"
#include "Viewport/Viewport.h"
#include "Component/PrimitiveComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/SkyComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Slate/Widget/Painter.h"

namespace
{
	void BuildGridVectors(const FRenderCommandQueue& Queue, const FViewportLocalState& LocalState, FVector& OutGridAxisU, FVector& OutGridAxisV, FVector& OutViewForward)
	{
		const FMatrix ViewInverse = Queue.ViewMatrix.GetInverse();
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

	TArray<FOutlineRenderItem> BuildSelectionOutlineItems(FEditorEngine* EditorEngine)
	{
		TArray<FOutlineRenderItem> OutlineItems;
		if (!EditorEngine)
		{
			return OutlineItems;
		}

		AActor* Selected = EditorEngine->GetSelectedActor();
		if (!Selected || Selected->IsPendingDestroy() || !Selected->IsVisible())
		{
			return OutlineItems;
		}

		if (Selected->GetComponentByClass<USkyComponent>() != nullptr)
		{
			return OutlineItems;
		}

		for (UActorComponent* Component : Selected->GetComponents())
		{
			if (!Component || !Component->IsA(UPrimitiveComponent::StaticClass()))
			{
				continue;
			}

			if (Component->IsA(UTextRenderComponent::StaticClass()) ||
				Component->IsA(USubUVComponent::StaticClass()) ||
				Component->IsA(UBillboardComponent::StaticClass()))
			{
				continue;
			}

			UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
			FRenderMesh* RenderMesh = PrimitiveComponent->GetRenderMesh();
			if (!RenderMesh)
			{
				continue;
			}

			FOutlineRenderItem& Item = OutlineItems.emplace_back();
			Item.Mesh = RenderMesh;
			Item.WorldMatrix = PrimitiveComponent->GetWorldTransform();
		}

		return OutlineItems;
	}
}


void FEditorViewportRenderService::RenderAll(
	FEngine* Engine,
	FRenderer* Renderer,
	FEditorEngine* EditorEngine,
	FEditorViewportRegistry& ViewportRegistry,
	FEditorUI& EditorUI,
	FGizmo& Gizmo,
	const std::shared_ptr<FMaterial>& WireFrameMaterial,
	FRenderMesh* GridMesh,
	FMaterial* GridMaterials[MAX_VIEWPORTS],
	const FBuildSceneRenderPacket& BuildSceneRenderPacket) const
{
	if (!Engine || !Renderer || !EditorEngine)
	{
		return;
	}

	ID3D11Device* Device = Renderer->GetDevice();
	if (!Device || !Renderer->GetDeviceContext())
	{
		return;
	}

	ULevel* Scene = Engine->GetScene();
	if (!Scene)
	{
		return;
	}

	FEditorFrameRequest FrameRequest;
	const TArray<FViewportEntry>& Entries = ViewportRegistry.GetEntries();
	const TArray<FOutlineRenderItem> SelectionOutlineItems = BuildSelectionOutlineItems(EditorEngine);

	int32 EntryIndex = 0;
	for (const FViewportEntry& Entry : Entries)
	{
		const int32 CurrentEntryIndex = EntryIndex++;
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

		const float AspectRatio = static_cast<float>(Rect.Width) / static_cast<float>(Rect.Height);
		FSceneRenderPacket ScenePacket;
		// 씬 패킷과 별도로, 그리드/기즈모 같은 추가 씬 커맨드는 별도 큐로 유지한다.
		FRenderCommandQueue AdditionalQueue;
		AdditionalQueue.Reserve(Renderer->GetPrevCommandCount());
		AdditionalQueue.ProjectionMatrix = Entry.LocalState.BuildProjMatrix(AspectRatio);
		AdditionalQueue.ViewMatrix = Entry.LocalState.BuildViewMatrix();

		FFrustum Frustum;
		Frustum.ExtractFromVP(AdditionalQueue.ViewMatrix * AdditionalQueue.ProjectionMatrix);
		const FVector CameraPosition = AdditionalQueue.ViewMatrix.GetInverse().GetTranslation();
		BuildSceneRenderPacket(Engine, Scene, Frustum, Entry.LocalState.ShowFlags, ScenePacket);

		AActor* GizmoTarget = EditorEngine->GetSelectedActor();
		if (GizmoTarget && GizmoTarget->GetComponentByClass<USkyComponent>() == nullptr)
		{
			Gizmo.BuildRenderCommands(GizmoTarget, &Entry, AdditionalQueue);
		}

		FMaterial* EntryGridMaterial = (CurrentEntryIndex < MAX_VIEWPORTS) ? GridMaterials[CurrentEntryIndex] : nullptr;
		if (Entry.LocalState.bShowGrid && GridMesh && EntryGridMaterial)
		{
			FVector GridAxisU = FVector::ForwardVector;
			FVector GridAxisV = FVector::RightVector;
			FVector ViewForward = FVector::ForwardVector;
			BuildGridVectors(AdditionalQueue, Entry.LocalState, GridAxisU, GridAxisV, ViewForward);

			EntryGridMaterial->SetParameterData("GridSize", &Entry.LocalState.GridSize, 4);
			EntryGridMaterial->SetParameterData("LineThickness", &Entry.LocalState.LineThickness, 4);
			EntryGridMaterial->SetParameterData("GridAxisU", &GridAxisU, sizeof(FVector));
			EntryGridMaterial->SetParameterData("GridAxisV", &GridAxisV, sizeof(FVector));
			EntryGridMaterial->SetParameterData("ViewForward", &ViewForward, sizeof(FVector));

			FRenderCommand GridCommand;
			GridCommand.RenderMesh = GridMesh;
			GridCommand.Material = EntryGridMaterial;
			GridCommand.WorldMatrix = FMatrix::Identity;
			GridCommand.RenderLayer = ERenderLayer::Default;
			AdditionalQueue.AddCommand(GridCommand);
		}

		FViewportScenePassRequest ScenePass;
		ScenePass.RenderTargetView = RTV;
		ScenePass.DepthStencilView = DSV;
		ScenePass.Viewport = Viewport;
		ScenePass.ScenePacket = std::move(ScenePacket);
		ScenePass.SceneView.ViewMatrix = AdditionalQueue.ViewMatrix;
		ScenePass.SceneView.ProjectionMatrix = AdditionalQueue.ProjectionMatrix;
		ScenePass.SceneView.CameraPosition = CameraPosition;
		ScenePass.SceneView.TotalTimeSeconds = Engine ? static_cast<float>(Engine->GetTimer().GetTotalTime()) : 0.0f;
		ScenePass.AdditionalCommands = std::move(AdditionalQueue);
		ScenePass.bForceWireframe = (Entry.LocalState.ViewMode == ERenderMode::Wireframe && WireFrameMaterial != nullptr);
		ScenePass.WireframeMaterial = WireFrameMaterial.get();
		ScenePass.OutlineRequest.bEnabled = Entry.LocalState.ShowFlags.HasFlag(EEngineShowFlags::SF_Primitives);
		if (ScenePass.OutlineRequest.bEnabled)
		{
			ScenePass.OutlineRequest.Items = SelectionOutlineItems;
		}
		EditorEngine->BuildDebugLineRenderRequest(Entry.LocalState.ShowFlags, ScenePass.DebugLineRequest);

		FrameRequest.ScenePasses.push_back(std::move(ScenePass));
	}

	FrameRequest.CompositeItems.reserve(Entries.size());
	for (const FViewportEntry& Entry : Entries)
	{
		if (!Entry.Viewport)
		{
			continue;
		}

		const FRect& Rect = Entry.Viewport->GetRect();
		FViewportCompositeItem Item;
		Item.SceneColorSRV = Entry.Viewport->GetSRV();
		Item.Rect.X = Rect.X;
		Item.Rect.Y = Rect.Y;
		Item.Rect.Width = Rect.Width;
		Item.Rect.Height = Rect.Height;
		Item.bVisible = Entry.bActive;
		FrameRequest.CompositeItems.push_back(Item);
	}

	if (FSlateApplication* Slate = EditorEngine->GetSlateApplication())
	{
		FSlatePaintContext PaintContext;

		RECT rc{};
		::GetClientRect(Renderer->GetHwnd(), &rc);
		PaintContext.SetScreenSize(rc.right - rc.left, rc.bottom - rc.top);
		Slate->BuildDrawList(PaintContext);

		FrameRequest.ScreenDrawList = PaintContext.ConsumeDrawList();
	}

	Renderer->RenderEditorFrame(FrameRequest);
	EditorEngine->ClearDebugDrawForFrame();
	EditorUI.Render();
}
