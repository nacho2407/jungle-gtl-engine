#include "StaticMeshEditorWidget.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "Core/Logging/Log.h"
#include "Math/MathUtils.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"
#include "Mesh/MeshManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "UI/Util/InlinePropertyRenderer.h"
#include "Viewport/Viewport.h"

#include <imgui.h>

namespace
{
	FString FormatStaticMeshStatCount(size_t Value)
	{
		FString Result = std::to_string(Value);
		for (int32 InsertPos = static_cast<int32>(Result.length()) - 3; InsertPos > 0; InsertPos -= 3)
		{
			Result.insert(static_cast<size_t>(InsertPos), ",");
		}
		return Result;
	}
}

static uint32 GNextStaticMeshEditorInstanceId = 0;

FStaticMeshEditorWidget::FStaticMeshEditorWidget()
	: InstanceId(GNextStaticMeshEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("StaticMeshEditorPreview_" + Id);
	WindowIdSuffix = "###StaticMeshEditor_" + Id;
}

bool FStaticMeshEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UStaticMesh>();
}

bool FStaticMeshEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	const UStaticMesh* CurrentMesh = Cast<UStaticMesh>(EditedObject);
	const UStaticMesh* RequestedMesh = Cast<UStaticMesh>(Object);
	if (!IsOpen() || !CurrentMesh || !RequestedMesh)
	{
		return false;
	}

	const FString& CurrentPath = CurrentMesh->GetAssetPathFileName();
	return !CurrentPath.empty()
		&& CurrentPath != "None"
		&& CurrentPath == RequestedMesh->GetAssetPathFileName();
}

void FStaticMeshEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(EditedObject))
	{
		UStaticMeshComponent* Comp = Actor->AddComponent<UStaticMeshComponent>();
		Comp->SetStaticMesh(Mesh);
		Actor->SetRootComponent(Comp);
	}
	Actor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>();
	LightComp->SetShadowBias(0.0f);
	LightComp->PushToScene();

	AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	FloorActor->InitDefaultComponents("Content/Data/BasicShape/Cube.OBJ");
	FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
	FloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.02f));

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), static_cast<uint32>(ViewportSize.x), static_cast<uint32>(ViewportSize.y));
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(Actor->GetComponentByClass<UStaticMeshComponent>());
	ViewportClient.CreatePreviewGizmo();
	ViewportClient.ResetCameraToPreviewBounds();
	ViewportClient.SetOnBodySetupShapePicked([this](FBodySetupShapeSelection Selection)
	{
		SetSelectedShape(Selection);
	});
	ViewportClient.SetOnBodySetupShapeEdited([this]()
	{
		OnBodySetupShapeEdited();
	});

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FStaticMeshEditorWidget::Close()
{
	FAssetEditorWidget::Close();

	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
}

void FStaticMeshEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}
}

void FStaticMeshEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FStaticMeshEditorViewportClient*>(&ViewportClient));
	}
}

void FStaticMeshEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	static float DetailsWidth = 300.0f;
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(EditedObject);

	bool bWindowOpen = true;
	FString VisibleTitle = "Static Mesh Editor";
	const FString AssetPath = StaticMesh ? StaticMesh->GetAssetPathFileName() : FString();
	if (!AssetPath.empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += AssetPath;
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	RenderTabBar();
	ImGui::Separator();
	if (ActiveTab == EStaticMeshEditorTab::AggregateGeometry)
	{
		RenderAggregateGeometryTab(StaticMesh);
	}
	else
	{
		RenderViewTab(StaticMesh, DetailsWidth);
	}

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FStaticMeshEditorWidget::RenderTabBar()
{
	if (ImGui::BeginTabBar("StaticMeshEditorTabs"))
	{
		if (ImGui::BeginTabItem("View"))
		{
			ActiveTab = EStaticMeshEditorTab::View;
			ViewportClient.SetBodySetupEditingEnabled(false);
			ViewportClient.GetRenderOptions().ShowFlags.bPhysicsBody = false;
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Aggregate Geometry"))
		{
			ActiveTab = EStaticMeshEditorTab::AggregateGeometry;
			ViewportClient.SetBodySetupEditingEnabled(true);
			ViewportClient.GetRenderOptions().ShowFlags.bPhysicsBody = true;
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void FStaticMeshEditorWidget::RenderViewTab(UStaticMesh* StaticMesh, float DetailsWidth)
{
	ImGui::BeginGroup();
	const float AvailableWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x;
	RenderViewportPanel(ImVec2(AvailableWidth, ImGui::GetContentRegionAvail().y), false);
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("Details", ImVec2(DetailsWidth, 0), true);
	ImGui::Text("Static Mesh Details");
	ImGui::Separator();
	RenderDetailsPanel(StaticMesh ? StaticMesh->GetStaticMeshAsset() : nullptr);
	ImGui::EndChild();
}

void FStaticMeshEditorWidget::RenderAggregateGeometryTab(UStaticMesh* StaticMesh)
{
	constexpr float ShapeListWidth = 220.0f;
	constexpr float ShapeDetailsWidth = 340.0f;

	ImGui::BeginChild("AggregateGeometryShapes", ImVec2(ShapeListWidth, 0), true);
	RenderAggregateShapeList(StaticMesh);
	ImGui::EndChild();

	ImGui::SameLine();

	const float ViewportWidth = FMath::Max(
		160.0f,
		ImGui::GetContentRegionAvail().x - ShapeDetailsWidth - ImGui::GetStyle().ItemSpacing.x);
	ImGui::BeginGroup();
	RenderViewportPanel(ImVec2(ViewportWidth, ImGui::GetContentRegionAvail().y), true);
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("AggregateGeometryShapeDetails", ImVec2(ShapeDetailsWidth, 0), true);
	RenderAggregateShapeDetails(StaticMesh);
	ImGui::EndChild();
}

void FStaticMeshEditorWidget::RenderViewportPanel(ImVec2 Size, bool bShowGizmoControls)
{
	ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

	FViewport* VP = ViewportClient.GetViewport();
	if (!VP || Size.x <= 0.0f || Size.y <= 0.0f)
	{
		ImGui::Dummy(Size);
		return;
	}

	VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));

	if (VP->GetSRV())
	{
		ImGui::Image((ImTextureID)VP->GetSRV(), Size);
		FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
	}
	else
	{
		ImGui::Dummy(Size);
	}

	constexpr float ToolbarHeight = 28.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(ViewportPos,
		ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight),
		IM_COL32(40, 40, 40, 255));

	FViewportToolbarContext Context;
	Context.Renderer = &GEngine->GetRenderer();
	Context.Gizmo = bShowGizmoControls ? ViewportClient.GetGizmo() : nullptr;
	Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
	Context.RenderOptions = &ViewportClient.GetRenderOptions();
	Context.ToolbarLeft = ViewportPos.x;
	Context.ToolbarTop = ViewportPos.y;
	Context.ToolbarWidth = Size.x;
	Context.bReservePlayStopSpace = false;
	Context.bShowAddActor = false;
	Context.bShowGizmoControls = bShowGizmoControls;
	Context.OnCoordSystemToggled = [&]()
	{
		FGizmoToolSettings& Settings = FEditorSettings::Get().MeshEditorViewportSettings.Gizmo;
		Settings.CoordSystem = Settings.CoordSystem == EEditorCoordSystem::World
			? EEditorCoordSystem::Local
			: EEditorCoordSystem::World;
		ViewportClient.ApplyTransformSettingsToGizmo();
	};
	Context.OnSettingsChanged = [&]()
	{
		ViewportClient.ApplyTransformSettingsToGizmo();
	};

	FViewportToolbar::Render(Context);
	RenderMeshStatsOverlay(DrawList, ViewportPos);
}

void FStaticMeshEditorWidget::RenderAggregateShapeList(UStaticMesh* StaticMesh)
{
	ImGui::TextUnformatted("Aggregate Geometry");
	ImGui::Separator();

	UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	if (!BodySetup)
	{
		ImGui::TextDisabled("No BodySetup.");
	}
	else
	{
		const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
		for (int32 Index = 0; Index < static_cast<int32>(AggGeom.SphereElems.size()); ++Index)
		{
			RenderShapeSelectable("Sphere", { EAggCollisionShape::Sphere, Index });
		}
		for (int32 Index = 0; Index < static_cast<int32>(AggGeom.BoxElems.size()); ++Index)
		{
			RenderShapeSelectable("Box", { EAggCollisionShape::Box, Index });
		}
		for (int32 Index = 0; Index < static_cast<int32>(AggGeom.SphylElems.size()); ++Index)
		{
			RenderShapeSelectable("Capsule", { EAggCollisionShape::Sphyl, Index });
		}
		for (int32 Index = 0; Index < static_cast<int32>(AggGeom.ConvexElems.size()); ++Index)
		{
			RenderShapeSelectable("Convex", { EAggCollisionShape::Convex, Index });
		}
	}

	if (ImGui::BeginPopupContextWindow("##StaticMeshAggregateGeometryContext", ImGuiPopupFlags_MouseButtonRight))
	{
		RenderAddShapeContextMenu(StaticMesh);
		ImGui::EndPopup();
	}
}

void FStaticMeshEditorWidget::RenderAggregateShapeDetails(UStaticMesh* StaticMesh)
{
	FKShapeElem* Shape = GetSelectedShape(StaticMesh);
	if (!Shape)
	{
		ImGui::TextDisabled("Select a shape.");
		return;
	}

	ImGui::TextUnformatted("Shape Details");
	ImGui::Separator();

	UStruct* StructType = nullptr;
	switch (SelectedShape.Type)
	{
	case EAggCollisionShape::Sphere:
		StructType = FKSphereElem::StaticStruct();
		break;
	case EAggCollisionShape::Box:
		StructType = FKBoxElem::StaticStruct();
		break;
	case EAggCollisionShape::Sphyl:
		StructType = FKSphylElem::StaticStruct();
		break;
	case EAggCollisionShape::Convex:
		StructType = FKConvexElem::StaticStruct();
		break;
	default:
		break;
	}

	if (StructType && FInlinePropertyRenderer::RenderStructProperties(StructType, Shape, StaticMesh, "##StaticMeshAggregateShapeProps"))
	{
		SaveStaticMeshChange("StaticMesh AggregateGeom edit warning");
		MarkDirty();
		ViewportClient.MarkBodySetupDebugDirty();
	}
}

bool FStaticMeshEditorWidget::RenderAddShapeContextMenu(UStaticMesh* StaticMesh)
{
	bool bAdded = false;
	if (ImGui::MenuItem("Add Sphere"))
	{
		AddAggregateShape(StaticMesh, EAggCollisionShape::Sphere);
		bAdded = true;
	}
	if (ImGui::MenuItem("Add Box"))
	{
		AddAggregateShape(StaticMesh, EAggCollisionShape::Box);
		bAdded = true;
	}
	if (ImGui::MenuItem("Add Capsule"))
	{
		AddAggregateShape(StaticMesh, EAggCollisionShape::Sphyl);
		bAdded = true;
	}
	if (ImGui::MenuItem("Add Convex"))
	{
		AddAggregateShape(StaticMesh, EAggCollisionShape::Convex);
		bAdded = true;
	}
	return bAdded;
}

bool FStaticMeshEditorWidget::RenderShapeSelectable(const char* TypeLabel, FBodySetupShapeSelection Selection)
{
	FString Label = TypeLabel;
	Label += " [";
	Label += std::to_string(Selection.Index);
	Label += "]##StaticMeshAggShape";
	Label += std::to_string(static_cast<int32>(Selection.Type));
	Label += "_";
	Label += std::to_string(Selection.Index);

	const bool bSelected = SelectedShape == Selection;
	if (ImGui::Selectable(Label.c_str(), bSelected))
	{
		SetSelectedShape(Selection);
	}
	return bSelected;
}

void FStaticMeshEditorWidget::AddAggregateShape(UStaticMesh* StaticMesh, EAggCollisionShape Type)
{
	if (!StaticMesh)
	{
		return;
	}

	UBodySetup* BodySetup = StaticMesh->CreateBodySetupIfMissing();
	if (!BodySetup)
	{
		return;
	}

	FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
	FBodySetupShapeSelection NewSelection { Type, 0 };
	switch (Type)
	{
	case EAggCollisionShape::Sphere:
		AggGeom.SphereElems.push_back(FKSphereElem());
		NewSelection.Index = static_cast<int32>(AggGeom.SphereElems.size()) - 1;
		break;
	case EAggCollisionShape::Box:
		AggGeom.BoxElems.push_back(FKBoxElem());
		NewSelection.Index = static_cast<int32>(AggGeom.BoxElems.size()) - 1;
		break;
	case EAggCollisionShape::Sphyl:
		AggGeom.SphylElems.push_back(FKSphylElem());
		NewSelection.Index = static_cast<int32>(AggGeom.SphylElems.size()) - 1;
		break;
	case EAggCollisionShape::Convex:
	{
		FKConvexElem ConvexElem;
		ConvexElem.VertexData = {
			FVector(-0.5f, -0.5f, -0.5f),
			FVector(0.5f, -0.5f, -0.5f),
			FVector(0.5f, 0.5f, -0.5f),
			FVector(-0.5f, 0.5f, -0.5f),
			FVector(-0.5f, -0.5f, 0.5f),
			FVector(0.5f, -0.5f, 0.5f),
			FVector(0.5f, 0.5f, 0.5f),
			FVector(-0.5f, 0.5f, 0.5f),
		};
		ConvexElem.IndexData = {
			0, 2, 1, 0, 3, 2,
			4, 5, 6, 4, 6, 7,
			0, 1, 5, 0, 5, 4,
			1, 2, 6, 1, 6, 5,
			2, 3, 7, 2, 7, 6,
			3, 0, 4, 3, 4, 7,
		};
		ConvexElem.UpdateElemBox();
		AggGeom.ConvexElems.push_back(ConvexElem);
		NewSelection.Index = static_cast<int32>(AggGeom.ConvexElems.size()) - 1;
		break;
	}
	default:
		return;
	}

	SetSelectedShape(NewSelection);
	SaveStaticMeshChange("StaticMesh AggregateGeom add warning");
	MarkDirty();
	ViewportClient.MarkBodySetupDebugDirty();
}

FKShapeElem* FStaticMeshEditorWidget::GetSelectedShape(UStaticMesh* StaticMesh) const
{
	UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	return BodySetup && SelectedShape.IsValid()
		? BodySetup->GetAggGeom().GetElement(SelectedShape.Type, SelectedShape.Index)
		: nullptr;
}

void FStaticMeshEditorWidget::SetSelectedShape(FBodySetupShapeSelection Selection)
{
	SelectedShape = Selection;
	ViewportClient.SetSelectedBodySetupShape(SelectedShape);
}

void FStaticMeshEditorWidget::SaveStaticMeshChange(const char* LogPrefix)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(EditedObject);
	if (!StaticMesh)
	{
		return;
	}

	const FString StaticMeshPath = StaticMesh->GetAssetPathFileName();
	if (!FMeshManager::SaveStaticMesh(StaticMesh, StaticMeshPath))
	{
		UE_LOG("%s: failed to persist StaticMesh change. StaticMesh=%s", LogPrefix, StaticMeshPath.c_str());
	}
}

void FStaticMeshEditorWidget::OnBodySetupShapeEdited()
{
	SaveStaticMeshChange("StaticMesh AggregateGeom gizmo edit warning");
	MarkDirty();
	ViewportClient.MarkBodySetupDebugDirty();
}

void FStaticMeshEditorWidget::RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const
{
	if (!DrawList || !EditedObject)
	{
		return;
	}

	size_t VertexCount = 0;
	size_t TriangleCount = 0;

	if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(EditedObject))
	{
		if (const FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset())
		{
			VertexCount = Asset->Vertices.size();
			TriangleCount = Asset->Indices.size() / 3;
		}
	}

	const FString Text =
		"Triangles: " + FormatStaticMeshStatCount(TriangleCount) + "\n" +
		"Vertices: " + FormatStaticMeshStatCount(VertexCount);

	const ImVec2 TextPos(ViewportPos.x + 8.0f, ViewportPos.y + 36.0f);
	DrawList->AddText(ImVec2(TextPos.x + 1.0f, TextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), Text.c_str());
	DrawList->AddText(TextPos, IM_COL32(235, 238, 242, 255), Text.c_str());
}

void FStaticMeshEditorWidget::RenderDetailsPanel(FStaticMesh* Asset) const
{
	if (!Asset)
	{
		ImGui::TextDisabled("No static mesh data.");
		return;
	}

	ImGui::Text("Vertices: %s", FormatStaticMeshStatCount(Asset->Vertices.size()).c_str());
	ImGui::Text("Indices: %s", FormatStaticMeshStatCount(Asset->Indices.size()).c_str());
	ImGui::Text("Triangles: %s", FormatStaticMeshStatCount(Asset->Indices.size() / 3).c_str());
	ImGui::Text("Sections: %s", FormatStaticMeshStatCount(Asset->Sections.size()).c_str());
}
