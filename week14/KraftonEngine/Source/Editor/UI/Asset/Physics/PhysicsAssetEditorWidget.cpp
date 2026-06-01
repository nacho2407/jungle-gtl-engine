#include "Editor/UI/Asset/Physics/PhysicsAssetEditorWidget.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Editor/Subsystem/PhysicsAssetGenerator.h"
#include "Editor/Slate/SlateApplication.h"
#include "Editor/UI/Toolbar/ViewportToolbar.h"
#include "Core/Types/CollisionTypes.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Math/Rotator.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsAssetManager.h"
#include "Physics/PhysicsConstraintTemplate.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
	uint32 GNextPhysicsAssetEditorInstanceId = 0;

	const char* ShapeTypeLabel(EPhysicsAssetShapeType ShapeType)
	{
		switch (ShapeType)
		{
		case EPhysicsAssetShapeType::Sphere: return "Sphere";
		case EPhysicsAssetShapeType::Box:    return "Box";
		case EPhysicsAssetShapeType::Sphyl:  return "Capsule";
		case EPhysicsAssetShapeType::Convex: return "Convex";
		default:                             return "Shape";
		}
	}

	const char* AngularModeLabel(EAngularConstraintMode Mode)
	{
		switch (Mode)
		{
		case EAngularConstraintMode::Free:    return "Free";
		case EAngularConstraintMode::Locked:  return "Locked";
		case EAngularConstraintMode::Limited:
		default:                              return "Limited";
		}
	}

	template<typename TEnum>
	bool EnumCombo(const char* Label, TEnum& Value, const char* const* Items, int32 Count)
	{
		int32 Current = static_cast<int32>(Value);
		if (ImGui::Combo(Label, &Current, Items, Count))
		{
			Value = static_cast<TEnum>(Current);
			return true;
		}
		return false;
	}

	bool IsValidAssetPath(const FString& Path)
	{
		return !Path.empty() && Path != "None";
	}

	FVector BoneLocalToWorld(const FMatrix& BoneMatrix, const FVector& LocalPoint)
	{
		return BoneMatrix.TransformPositionWithW(LocalPoint);
	}

	FVector ShapeLocalToBoneLocal(const FVector& Center, const FQuat& Rotation, const FVector& ShapeLocalPoint)
	{
		return Center + Rotation.RotateVector(ShapeLocalPoint);
	}

	bool BoneSubtreeMatchesFilter(const FSkeletalMesh* MeshAsset, int32 BoneIndex, const FString& Filter)
	{
		if (Filter.empty())
		{
			return true;
		}
		if (!MeshAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
		{
			return false;
		}

		const FBone& Bone = MeshAsset->Bones[BoneIndex];
		if (Bone.Name.find(Filter) != FString::npos)
		{
			return true;
		}

		for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(MeshAsset->Bones.size()); ++ChildIndex)
		{
			if (MeshAsset->Bones[ChildIndex].ParentIndex == BoneIndex
				&& BoneSubtreeMatchesFilter(MeshAsset, ChildIndex, Filter))
			{
				return true;
			}
		}
		return false;
	}

	void DrawPhysicsPanelHeader(const char* Label)
	{
		constexpr float HeaderHeight = 25.0f;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + HeaderHeight), IM_COL32(36, 36, 36, 255));
		DrawList->AddText(ImVec2(Pos.x + 8.0f, Pos.y + 5.0f), IM_COL32(220, 220, 220, 255), Label);
		ImGui::Dummy(ImVec2(Width, HeaderHeight + 5.0f));
	}

	bool DrawPhysicsSplitter(const char* Id, const ImVec2& Size, bool bVertical)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
		ImGui::Button(Id, Size);
		const bool bActive = ImGui::IsItemActive();
		if (ImGui::IsItemHovered() || bActive)
		{
			ImGui::SetMouseCursor(bVertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
		}
		ImGui::PopStyleColor(3);
		return bActive;
	}
}

FPhysicsAssetEditorWidget::FPhysicsAssetEditorWidget()
	: InstanceId(GNextPhysicsAssetEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("PhysicsAssetEditorPreview_" + Id);
	WindowIdSuffix = "###PhysicsAssetEditor_" + Id;
}

bool FPhysicsAssetEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UPhysicsAsset>();
}

bool FPhysicsAssetEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	const UPhysicsAsset* CurrentAsset = Cast<UPhysicsAsset>(EditedObject);
	const UPhysicsAsset* RequestedAsset = Cast<UPhysicsAsset>(Object);
	return IsOpen()
		&& CurrentAsset
		&& RequestedAsset
		&& IsValidAssetPath(CurrentAsset->GetSourcePath())
		&& CurrentAsset->GetSourcePath() == RequestedAsset->GetSourcePath();
}

void FPhysicsAssetEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);
	ClearSelection();

	UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(EditedObject);
	PreviewMesh = nullptr;
	PreviewMeshComponent = nullptr;
	PreviewActor = nullptr;

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (PhysicsAsset && IsValidAssetPath(PhysicsAsset->GetPreviewSkeletalMeshPath()))
	{
		PreviewMesh = FMeshManager::LoadSkeletalMesh(PhysicsAsset->GetPreviewSkeletalMeshPath(), Device);
	}

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();
	WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	PreviewActor = WorldContext.World->SpawnActor<AActor>();
	if (PreviewMesh)
	{
		PreviewMeshComponent = PreviewActor->AddComponent<USkeletalMeshComponent>();
		PreviewMeshComponent->SetSkeletalMesh(PreviewMesh);
		PreviewMeshComponent->SetPhysicsAssetOverride(PhysicsAsset);
		PreviewActor->SetRootComponent(PreviewMeshComponent);
	}
	PreviewActor->SetActorLocation(FVector::ZeroVector);

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	if (UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>())
	{
		LightComp->SetShadowBias(0.0f);
		LightComp->PushToScene();
	}

	AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	FloorActor->InitDefaultComponents("Content/Data/BasicShape/Cube.OBJ");
	FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
	FloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.02f));

	AActor* FloorCollisionActor = WorldContext.World->SpawnActor<AActor>();
	UBoxComponent* FloorCollision = FloorCollisionActor ? FloorCollisionActor->AddComponent<UBoxComponent>() : nullptr;
	if (FloorCollisionActor && FloorCollision)
	{
		FloorCollisionActor->SetRootComponent(FloorCollision);
		FloorCollision->SetBoxExtent(FVector(10.0f, 10.0f, 0.1f));
		FloorCollisionActor->SetActorLocation(FVector(0.0f, 0.0f, -0.1f));
		FloorCollision->SetVisibility(false);
		FloorCollision->SetCollisionObjectType(ECollisionChannel::WorldStatic);
		FloorCollision->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
		FloorCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
	ViewportClient.Initialize(Device, static_cast<uint32>(ViewportSize.x), static_cast<uint32>(ViewportSize.y));
	ViewportClient.SetPreviewScene(WorldContext.World, PhysicsAsset, PreviewMeshComponent);
	ViewportClient.SetSimulatePhysics(false);
	ViewportClient.ResetCameraToPreviewBounds();
	bPreviewSimulationActive = false;

	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FPhysicsAssetEditorWidget::Close()
{
	FAssetEditorWidget::Close();
	StopPreviewSimulation();

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

	PreviewMesh = nullptr;
	PreviewMeshComponent = nullptr;
	PreviewActor = nullptr;
	ClearSelection();
}

void FPhysicsAssetEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}
	TickPreviewSimulation(DeltaTime);
}

void FPhysicsAssetEditorWidget::TickPreviewSimulation(float DeltaTime)
{
	const bool bShouldSimulate = ViewportClient.IsSimulatingPhysics();
	UWorld* PreviewWorld = ViewportClient.GetPreviewWorld();

	if (!PreviewWorld || !PreviewMeshComponent)
	{
		bPreviewSimulationActive = false;
		ViewportClient.SetSimulatePhysics(false);
		return;
	}

	if (!bShouldSimulate)
	{
		StopPreviewSimulation();
		return;
	}

	if (!bPreviewSimulationActive)
	{
		ViewportClient.SetShowBodies(true);
		CapturePreviewSimulationStartPose();
		PreviewMeshComponent->SetVisibility(false);
		PreviewMeshComponent->SetSimulatePhysics(true);
		bPreviewSimulationActive = PreviewMeshComponent->IsSimulatingPhysics();

		if (!bPreviewSimulationActive)
		{
			PreviewSimulationStartLocalPose.clear();
			PreviewMeshComponent->SetVisibility(ViewportClient.IsShowPreviewMesh());
			ViewportClient.SetSimulatePhysics(false);
			return;
		}
	}

	PreviewMeshComponent->SetVisibility(false);
	const float SimDeltaTime = (std::max)(0.0f, DeltaTime);
	PreviewWorld->Tick(SimDeltaTime, ELevelTick::LEVELTICK_All);
	PreviewMeshComponent->SyncSimulatedPhysics();
}

void FPhysicsAssetEditorWidget::CapturePreviewSimulationStartPose()
{
	PreviewSimulationStartLocalPose.clear();

	if (!PreviewMeshComponent)
	{
		return;
	}

	USkeletalMesh* Mesh = PreviewMeshComponent->GetSkeletalMesh();
	FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset)
	{
		return;
	}

	const int32 NumBones = static_cast<int32>(MeshAsset->Bones.size());
	PreviewSimulationStartLocalPose.reserve(NumBones);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		PreviewSimulationStartLocalPose.push_back(PreviewMeshComponent->GetBoneLocalTransformByIndex(BoneIndex));
	}
}

void FPhysicsAssetEditorWidget::RestorePreviewSimulationStartPose()
{
	if (!PreviewMeshComponent || PreviewSimulationStartLocalPose.empty())
	{
		PreviewSimulationStartLocalPose.clear();
		return;
	}

	USkeletalMesh* Mesh = PreviewMeshComponent->GetSkeletalMesh();
	FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const int32 NumBones = MeshAsset ? static_cast<int32>(MeshAsset->Bones.size()) : 0;
	if (NumBones <= 0 || static_cast<int32>(PreviewSimulationStartLocalPose.size()) != NumBones)
	{
		PreviewSimulationStartLocalPose.clear();
		return;
	}

	PreviewMeshComponent->SetBoneLocalTransforms(PreviewSimulationStartLocalPose);
	PreviewSimulationStartLocalPose.clear();
}

void FPhysicsAssetEditorWidget::StopPreviewSimulation()
{
	if (PreviewMeshComponent && PreviewMeshComponent->IsSimulatingPhysics())
	{
		PreviewMeshComponent->SetSimulatePhysics(false);
	}
	RestorePreviewSimulationStartPose();

	bPreviewSimulationActive = false;
	ViewportClient.SetSimulatePhysics(false);
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetVisibility(ViewportClient.IsShowPreviewMesh());
	}
}

void FPhysicsAssetEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FPhysicsAssetEditorViewportClient*>(&ViewportClient));
	}
}

void FPhysicsAssetEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(EditedObject);
	if (!PhysicsAsset)
	{
		return;
	}
	ValidateSelection(PhysicsAsset);

	FString VisibleTitle = "Physics Asset Editor";
	if (IsValidAssetPath(PhysicsAsset->GetSourcePath()))
	{
		VisibleTitle += " - ";
		VisibleTitle += PhysicsAsset->GetSourcePath();
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

	bool bWindowOpen = true;
	const FString WindowTitle = VisibleTitle + WindowIdSuffix;
	ImGui::SetNextWindowSize(ImVec2(1320.0f, 760.0f), ImGuiCond_Once);
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

	RenderModeToolbar(PhysicsAsset);

	const ImGuiStyle& Style = ImGui::GetStyle();
	constexpr float SplitterThickness = 4.0f;
	const float TotalHeight = ImGui::GetContentRegionAvail().y;
	const float TotalWidth = ImGui::GetContentRegionAvail().x;

	float MaxHierarchyWidth = TotalWidth - DetailsWidth - 480.0f;
	if (MaxHierarchyWidth < 240.0f) MaxHierarchyWidth = 240.0f;
	HierarchyWidth = Clamp(HierarchyWidth, 240.0f, MaxHierarchyWidth);

	float MaxDetailsWidth = TotalWidth - HierarchyWidth - 480.0f;
	if (MaxDetailsWidth < 280.0f) MaxDetailsWidth = 280.0f;
	DetailsWidth = Clamp(DetailsWidth, 280.0f, MaxDetailsWidth);

	ImGui::BeginChild("##PhysicsAssetLeftColumn", ImVec2(HierarchyWidth, TotalHeight), false);
	float MaxGraphHeight = TotalHeight - 260.0f;
	if (MaxGraphHeight < 120.0f) MaxGraphHeight = 120.0f;
	GraphHeight = Clamp(GraphHeight, 120.0f, MaxGraphHeight);
	const float SkeletonTreeHeight = TotalHeight - GraphHeight - SplitterThickness - Style.ItemSpacing.y;
	ImGui::BeginChild("##PhysicsAssetSkeletonTree", ImVec2(0.0f, SkeletonTreeHeight), true);
	RenderSkeletonTreePanel(PhysicsAsset);
	ImGui::EndChild();
	if (DrawPhysicsSplitter("##PhysicsAssetLeftHorizontalSplitter", ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness), false))
	{
		GraphHeight -= ImGui::GetIO().MouseDelta.y;
		GraphHeight = Clamp(GraphHeight, 120.0f, MaxGraphHeight);
	}
	RenderGraphPanel(PhysicsAsset, ImVec2(0.0f, GraphHeight));
	ImGui::EndChild();

	ImGui::SameLine();

	if (DrawPhysicsSplitter("##PhysicsAssetTreeSplitter", ImVec2(SplitterThickness, TotalHeight), true))
	{
		HierarchyWidth += ImGui::GetIO().MouseDelta.x;
		float MaxWidth = ImGui::GetWindowWidth() - DetailsWidth - 480.0f;
		if (MaxWidth < 240.0f) MaxWidth = 240.0f;
		HierarchyWidth = Clamp(HierarchyWidth, 240.0f, MaxWidth);
	}

	ImGui::SameLine();

	float CenterWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - Style.ItemSpacing.x - SplitterThickness;
	if (CenterWidth < 320.0f) CenterWidth = 320.0f;
	ImGui::BeginChild("##PhysicsAssetCenterColumn", ImVec2(CenterWidth, TotalHeight), false);
	const float CenterHeight = ImGui::GetContentRegionAvail().y;
	const float CenterContentWidth = ImGui::GetContentRegionAvail().x;
	float MaxListHeight = CenterHeight - 260.0f;
	if (MaxListHeight < 150.0f) MaxListHeight = 150.0f;
	ViewportListHeight = Clamp(ViewportListHeight, 150.0f, MaxListHeight);
	const float ViewportHeight = CenterHeight - ViewportListHeight - SplitterThickness - Style.ItemSpacing.y;
	RenderViewportPanel(PhysicsAsset, ImVec2(CenterContentWidth, ViewportHeight));
	if (DrawPhysicsSplitter("##PhysicsAssetViewportListSplitter", ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness), false))
	{
		ViewportListHeight -= ImGui::GetIO().MouseDelta.y;
		ViewportListHeight = Clamp(ViewportListHeight, 150.0f, MaxListHeight);
	}
	RenderPhysicsListPanel(PhysicsAsset, ImVec2(CenterContentWidth, ViewportListHeight));
	ImGui::EndChild();

	ImGui::SameLine();

	if (DrawPhysicsSplitter("##PhysicsAssetDetailsSplitter", ImVec2(SplitterThickness, TotalHeight), true))
	{
		DetailsWidth -= ImGui::GetIO().MouseDelta.x;
		float MaxWidth = ImGui::GetWindowWidth() - HierarchyWidth - 480.0f;
		if (MaxWidth < 280.0f) MaxWidth = 280.0f;
		DetailsWidth = Clamp(DetailsWidth, 280.0f, MaxWidth);
	}

	ImGui::SameLine();

	ImGui::BeginChild("##PhysicsAssetRightColumn", ImVec2(DetailsWidth, TotalHeight), false);
	float MaxToolsHeight = TotalHeight - 260.0f;
	if (MaxToolsHeight < 160.0f) MaxToolsHeight = 160.0f;
	ToolsHeight = Clamp(ToolsHeight, 160.0f, MaxToolsHeight);
	const float DetailsPanelHeight = TotalHeight - ToolsHeight - SplitterThickness - Style.ItemSpacing.y;
	ImGui::BeginChild("##PhysicsAssetDetails", ImVec2(0.0f, DetailsPanelHeight), true);
	RenderDetailsPanel(PhysicsAsset);
	ImGui::EndChild();
	if (DrawPhysicsSplitter("##PhysicsAssetDetailsToolsSplitter", ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness), false))
	{
		ToolsHeight -= ImGui::GetIO().MouseDelta.y;
		ToolsHeight = Clamp(ToolsHeight, 160.0f, MaxToolsHeight);
	}
	RenderToolsPanel(PhysicsAsset, ImVec2(0.0f, ToolsHeight));
	ImGui::EndChild();

	ImGui::End();

	if (!bWindowOpen || bPendingClose)
	{
		bPendingClose = false;
		Close();
	}
}

void FPhysicsAssetEditorWidget::RenderModeToolbar(UPhysicsAsset* Asset)
{
	constexpr float RowHeight = 32.0f;
	constexpr float BarHeight = RowHeight * 2.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 BarPos = ImGui::GetCursorScreenPos();
	const float BarWidth = ImGui::GetContentRegionAvail().x;
	DrawList->AddRectFilled(BarPos, ImVec2(BarPos.x + BarWidth, BarPos.y + BarHeight), IM_COL32(38, 38, 38, 255));
	DrawList->AddRectFilled(ImVec2(BarPos.x, BarPos.y + RowHeight), ImVec2(BarPos.x + BarWidth, BarPos.y + BarHeight), IM_COL32(32, 32, 32, 255));
	DrawList->AddLine(ImVec2(BarPos.x, BarPos.y + RowHeight), ImVec2(BarPos.x + BarWidth, BarPos.y + RowHeight), IM_COL32(18, 18, 18, 255));

	const char* Title = "Physics Asset";
	const ImVec2 TitleSize = ImGui::CalcTextSize(Title);
	DrawList->AddText(ImVec2(BarPos.x + 10.0f, BarPos.y + (RowHeight - TitleSize.y) * 0.5f),
		IM_COL32(235, 235, 235, 255), Title);
	ImGui::SetCursorScreenPos(ImVec2(BarPos.x + 118.0f, BarPos.y));

	auto ModeButton = [&](const char* Label, EPhysicsAssetEditorMode Mode)
	{
		const bool bActive = ActiveMode == Mode;
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		const float Width = TextSize.x + 28.0f;

		ImGui::InvisibleButton(Label, ImVec2(Width, RowHeight));
		if (ImGui::IsItemClicked())
		{
			ActiveMode = Mode;
		}

		if (bActive || ImGui::IsItemHovered())
		{
			DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + RowHeight),
				bActive ? IM_COL32(44, 44, 44, 255) : IM_COL32(255, 255, 255, 20));
		}

		DrawList->AddText(ImVec2(Pos.x + 14.0f, Pos.y + (RowHeight - TextSize.y) * 0.5f),
			bActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(190, 190, 190, 255), Label);
		if (bActive)
		{
			DrawList->AddRectFilled(ImVec2(Pos.x, Pos.y + RowHeight - 2.0f),
				ImVec2(Pos.x + Width, Pos.y + RowHeight), IM_COL32(64, 132, 224, 255));
		}
		ImGui::SameLine(0.0f, 0.0f);
	};

	ModeButton("Body Mode", EPhysicsAssetEditorMode::Body);
	ModeButton("Constraint Mode", EPhysicsAssetEditorMode::Constraint);
	ModeButton("Preview", EPhysicsAssetEditorMode::Preview);

	const float RightStart = BarPos.x + BarWidth - 228.0f;
	ImGui::SetCursorScreenPos(ImVec2(RightStart, BarPos.y + 4.0f));
	if (ImGui::Button("Save", ImVec2(64.0f, 24.0f)))
	{
		if (FPhysicsAssetManager::Get().Save(Asset))
		{
			ClearDirty();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete", ImVec2(70.0f, 24.0f)))
	{
		StopPreviewSimulation();
		if (DeleteSelection(Asset))
		{
			MarkDirty();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Focus", ImVec2(64.0f, 24.0f)))
	{
		ViewportClient.ResetCameraToPreviewBounds();
	}

	ImGui::SetCursorScreenPos(ImVec2(BarPos.x + 10.0f, BarPos.y + RowHeight + 4.0f));
	bool bShowPreviewMesh = ViewportClient.IsShowPreviewMesh();
	if (ImGui::Checkbox("Preview Mesh", &bShowPreviewMesh))
	{
		ViewportClient.SetShowPreviewMesh(bShowPreviewMesh);
	}
	ImGui::SameLine();
	bool bShowBodies = ViewportClient.IsShowBodies();
	if (ImGui::Checkbox("Bodies", &bShowBodies))
	{
		ViewportClient.SetShowBodies(bShowBodies);
	}
	ImGui::SameLine();
	bool bShowConstraints = ViewportClient.IsShowConstraints();
	if (ImGui::Checkbox("Constraints", &bShowConstraints))
	{
		ViewportClient.SetShowConstraints(bShowConstraints);
	}
	ImGui::SameLine();
	bool bSimulatePhysics = ViewportClient.IsSimulatingPhysics();
	if (ImGui::Checkbox("Simulate", &bSimulatePhysics))
	{
		ViewportClient.SetSimulatePhysics(bSimulatePhysics);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reference Pose", ImVec2(108.0f, 24.0f)) && PreviewMeshComponent)
	{
		StopPreviewSimulation();
		PreviewMeshComponent->ResetBoneEditPose();
	}
	ImGui::SameLine();
	if (ImGui::Button("Create Asset", ImVec2(96.0f, 24.0f)))
	{
		StopPreviewSimulation();
		RegenerateBodies(Asset);
	}

	ImGui::SetCursorScreenPos(ImVec2(BarPos.x, BarPos.y + BarHeight));
}

void FPhysicsAssetEditorWidget::RenderAssetDetailsPanel(UPhysicsAsset* Asset)
{
	DrawPhysicsPanelHeader("Asset Details");

	if (!Asset)
	{
		return;
	}

	ImGui::TextUnformatted("Physics Asset");
	ImGui::TextWrapped("%s", IsValidAssetPath(Asset->GetSourcePath()) ? Asset->GetSourcePath().c_str() : "Unsaved");
	ImGui::Spacing();
	ImGui::TextUnformatted("Preview Mesh");
	ImGui::TextWrapped("%s", IsValidAssetPath(Asset->GetPreviewSkeletalMeshPath())
		? Asset->GetPreviewSkeletalMeshPath().c_str()
		: "None");
	ImGui::Spacing();
	ImGui::Text("Bodies: %llu", static_cast<unsigned long long>(Asset->GetBodySetups().size()));
	ImGui::SameLine();
	ImGui::Text("Constraints: %llu", static_cast<unsigned long long>(Asset->GetConstraintTemplates().size()));
}

void FPhysicsAssetEditorWidget::RenderViewportPanel(UPhysicsAsset* Asset, ImVec2 Size)
{
	if (Size.x <= 1.0f || Size.y <= 1.0f)
	{
		return;
	}

	ImGui::BeginChild("Viewport", Size, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

	constexpr float ToolbarHeight = 28.0f;
	bool bViewportImageClicked = false;
	if (FViewport* VP = ViewportClient.GetViewport())
	{
		VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));
		if (VP->GetSRV())
		{
			ImGui::Image((ImTextureID)VP->GetSRV(), Size);
			FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
			const ImVec2 MousePos = ImGui::GetIO().MousePos;
			bViewportImageClicked = ImGui::IsItemHovered()
				&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)
				&& MousePos.y > ViewportPos.y + ToolbarHeight;
		}
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	RenderSolidBodiesOverlay(DrawList, ViewportPos, Size, Asset);
	DrawList->AddRectFilled(ViewportPos, ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight), IM_COL32(40, 40, 40, 255));

	FViewportToolbarContext Context;
	Context.Renderer = &GEngine->GetRenderer();
	Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
	Context.RenderOptions = &ViewportClient.GetRenderOptions();
	Context.ToolbarLeft = ViewportPos.x;
	Context.ToolbarTop = ViewportPos.y;
	Context.ToolbarWidth = Size.x;
	Context.bReservePlayStopSpace = false;
	Context.bShowAddActor = false;
	Context.bShowGizmoControls = false;
	Context.bShowCameraControls = true;
	Context.bShowViewMode = true;
	Context.bShowShowFlags = true;
	Context.OnRenderViewModeExtras = [&]()
	{
		bool bShowPreviewMesh = ViewportClient.IsShowPreviewMesh();
		if (ImGui::Checkbox("Skeletal Mesh", &bShowPreviewMesh))
		{
			ViewportClient.SetShowPreviewMesh(bShowPreviewMesh);
		}

		bool bShowBodies = ViewportClient.IsShowBodies();
		if (ImGui::Checkbox("Bodies", &bShowBodies))
		{
			ViewportClient.SetShowBodies(bShowBodies);
		}

		bool bShowConstraints = ViewportClient.IsShowConstraints();
		if (ImGui::Checkbox("Constraints", &bShowConstraints))
		{
			ViewportClient.SetShowConstraints(bShowConstraints);
		}
	};
	FViewportToolbar::Render(Context);

	if (bViewportImageClicked)
	{
		HandleViewportSelectionClick();
	}

	ImGui::EndChild();
}

void FPhysicsAssetEditorWidget::RenderSolidBodiesOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos, const ImVec2& ViewportSize, UPhysicsAsset* Asset) const
{
	if (!DrawList || !Asset || !PreviewMeshComponent || !ViewportClient.IsShowBodies())
	{
		return;
	}
	if (ViewportSize.x <= 1.0f || ViewportSize.y <= 1.0f)
	{
		return;
	}

	FMinimalViewInfo POV;
	if (!ViewportClient.GetCameraView(POV))
	{
		return;
	}
	POV.AspectRatio = ViewportSize.x / ViewportSize.y;

	const FMatrix View = POV.CalculateViewMatrix();
	const FMatrix ViewProjection = POV.CalculateViewProjectionMatrix();

	struct FProjectedVertex
	{
		ImVec2 Screen;
		float Depth = 0.0f;
	};

	struct FProjectedTriangle
	{
		ImVec2 A;
		ImVec2 B;
		ImVec2 C;
		float Depth = 0.0f;
		ImU32 Color = 0;
	};

	std::vector<FProjectedTriangle> Triangles;
	Triangles.reserve(2048);

	auto ProjectWorld = [&](const FVector& World, FProjectedVertex& Out) -> bool
	{
		const FVector ViewPos = View.TransformPositionWithW(World);
		if (ViewPos.Z <= POV.NearClip)
		{
			return false;
		}

		const FVector Clip = ViewProjection.TransformPositionWithW(World);
		if (!std::isfinite(Clip.X) || !std::isfinite(Clip.Y) || !std::isfinite(Clip.Z))
		{
			return false;
		}

		Out.Screen = ImVec2(
			ViewportPos.x + (Clip.X * 0.5f + 0.5f) * ViewportSize.x,
			ViewportPos.y + (1.0f - (Clip.Y * 0.5f + 0.5f)) * ViewportSize.y);
		Out.Depth = ViewPos.Z;
		return true;
	};

	auto AddTriangle = [&](const FVector& A, const FVector& B, const FVector& C, ImU32 Color)
	{
		constexpr size_t MaxSolidOverlayTriangles = 24000;
		if (Triangles.size() >= MaxSolidOverlayTriangles)
		{
			return;
		}

		FProjectedVertex PA, PB, PC;
		if (!ProjectWorld(A, PA) || !ProjectWorld(B, PB) || !ProjectWorld(C, PC))
		{
			return;
		}

		FProjectedTriangle Triangle;
		Triangle.A = PA.Screen;
		Triangle.B = PB.Screen;
		Triangle.C = PC.Screen;
		Triangle.Depth = (PA.Depth + PB.Depth + PC.Depth) / 3.0f;
		Triangle.Color = Color;
		Triangles.push_back(Triangle);
	};

	auto IsSelectedShape = [&](int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex) -> bool
	{
		if (Selection.Type == EPhysicsAssetEditorSelectionType::Body)
		{
			return Selection.BodyIndex == BodyIndex;
		}
		if (Selection.Type != EPhysicsAssetEditorSelectionType::Shape)
		{
			return false;
		}
		return Selection.BodyIndex == BodyIndex
			&& Selection.ShapeType == ShapeType
			&& Selection.ShapeIndex == ShapeIndex;
	};

	auto ShapeColor = [&](int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex) -> ImU32
	{
		return IsSelectedShape(BodyIndex, ShapeType, ShapeIndex)
			? IM_COL32(255, 176, 0, 112)
			: IM_COL32(0, 162, 255, 58);
	};

	const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
	const int32 BodyCount = static_cast<int32>(Bodies.size());
	const bool bLargeAsset = BodyCount > 48;
	const int32 SphereSlices = bLargeAsset ? 8 : 16;
	const int32 SphereStacks = bLargeAsset ? 4 : 8;
	const int32 CapsuleSlices = bLargeAsset ? 8 : 16;
	const int32 CapsuleHemisphereStacks = bLargeAsset ? 2 : 4;

	auto SpherePoint = [](float Radius, float Theta, float Phi) -> FVector
	{
		const float SinTheta = std::sin(Theta);
		return FVector(
			Radius * SinTheta * std::cos(Phi),
			Radius * SinTheta * std::sin(Phi),
			Radius * std::cos(Theta));
	};

	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(Bodies.size()); ++BodyIndex)
	{
		const UBodySetup* Body = Bodies[BodyIndex];
		if (!Body)
		{
			continue;
		}

		FMatrix BoneMatrix;
		if (!PreviewMeshComponent->GetBoneWorldMatrixByName(Body->GetBoneName().ToString(), BoneMatrix))
		{
			continue;
		}

		const FKAggregateGeom& Geom = Body->GetAggGeom();

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.SphereElems.size()); ++ShapeIndex)
		{
			const FKSphereElem& Sphere = Geom.SphereElems[ShapeIndex];
			if (Sphere.Radius <= 0.0f)
			{
				continue;
			}

			const ImU32 Color = ShapeColor(BodyIndex, EPhysicsAssetShapeType::Sphere, ShapeIndex);
			for (int32 Stack = 0; Stack < SphereStacks; ++Stack)
			{
				const float Theta0 = FMath::Pi * static_cast<float>(Stack) / static_cast<float>(SphereStacks);
				const float Theta1 = FMath::Pi * static_cast<float>(Stack + 1) / static_cast<float>(SphereStacks);
				for (int32 Slice = 0; Slice < SphereSlices; ++Slice)
				{
					const float Phi0 = 2.0f * FMath::Pi * static_cast<float>(Slice) / static_cast<float>(SphereSlices);
					const float Phi1 = 2.0f * FMath::Pi * static_cast<float>(Slice + 1) / static_cast<float>(SphereSlices);

					const FVector P00 = BoneLocalToWorld(BoneMatrix, Sphere.Center + SpherePoint(Sphere.Radius, Theta0, Phi0));
					const FVector P01 = BoneLocalToWorld(BoneMatrix, Sphere.Center + SpherePoint(Sphere.Radius, Theta0, Phi1));
					const FVector P10 = BoneLocalToWorld(BoneMatrix, Sphere.Center + SpherePoint(Sphere.Radius, Theta1, Phi0));
					const FVector P11 = BoneLocalToWorld(BoneMatrix, Sphere.Center + SpherePoint(Sphere.Radius, Theta1, Phi1));

					AddTriangle(P00, P10, P11, Color);
					AddTriangle(P00, P11, P01, Color);
				}
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.BoxElems.size()); ++ShapeIndex)
		{
			const FKBoxElem& Box = Geom.BoxElems[ShapeIndex];
			if (Box.Extents.X <= 0.0f || Box.Extents.Y <= 0.0f || Box.Extents.Z <= 0.0f)
			{
				continue;
			}

			const FVector Signs[8] = {
				FVector(-1.0f, -1.0f, -1.0f), FVector( 1.0f, -1.0f, -1.0f),
				FVector( 1.0f,  1.0f, -1.0f), FVector(-1.0f,  1.0f, -1.0f),
				FVector(-1.0f, -1.0f,  1.0f), FVector( 1.0f, -1.0f,  1.0f),
				FVector( 1.0f,  1.0f,  1.0f), FVector(-1.0f,  1.0f,  1.0f)
			};

			FVector W[8];
			for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
			{
				const FVector LocalCorner(
					Signs[CornerIndex].X * Box.Extents.X,
					Signs[CornerIndex].Y * Box.Extents.Y,
					Signs[CornerIndex].Z * Box.Extents.Z);
				W[CornerIndex] = BoneLocalToWorld(BoneMatrix, ShapeLocalToBoneLocal(Box.Center, Box.Rotation, LocalCorner));
			}

			const ImU32 Color = ShapeColor(BodyIndex, EPhysicsAssetShapeType::Box, ShapeIndex);
			const int32 FaceTris[12][3] = {
				{0, 1, 2}, {0, 2, 3},
				{4, 6, 5}, {4, 7, 6},
				{0, 4, 5}, {0, 5, 1},
				{1, 5, 6}, {1, 6, 2},
				{2, 6, 7}, {2, 7, 3},
				{3, 7, 4}, {3, 4, 0}
			};
			for (const int32* Tri : FaceTris)
			{
				AddTriangle(W[Tri[0]], W[Tri[1]], W[Tri[2]], Color);
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.SphylElems.size()); ++ShapeIndex)
		{
			const FKSphylElem& Capsule = Geom.SphylElems[ShapeIndex];
			if (Capsule.Radius <= 0.0f || Capsule.Length <= 0.0f)
			{
				continue;
			}

			const ImU32 Color = ShapeColor(BodyIndex, EPhysicsAssetShapeType::Sphyl, ShapeIndex);
			const float HalfLength = Capsule.Length * 0.5f;

			auto CapsulePoint = [&](float Radius, float Z, float Phi) -> FVector
			{
				const FVector ShapeLocal(Radius * std::cos(Phi), Radius * std::sin(Phi), Z);
				return BoneLocalToWorld(BoneMatrix, ShapeLocalToBoneLocal(Capsule.Center, Capsule.Rotation, ShapeLocal));
			};

			for (int32 Slice = 0; Slice < CapsuleSlices; ++Slice)
			{
				const float Phi0 = 2.0f * FMath::Pi * static_cast<float>(Slice) / static_cast<float>(CapsuleSlices);
				const float Phi1 = 2.0f * FMath::Pi * static_cast<float>(Slice + 1) / static_cast<float>(CapsuleSlices);

				const FVector B0 = CapsulePoint(Capsule.Radius, -HalfLength, Phi0);
				const FVector B1 = CapsulePoint(Capsule.Radius, -HalfLength, Phi1);
				const FVector T0 = CapsulePoint(Capsule.Radius,  HalfLength, Phi0);
				const FVector T1 = CapsulePoint(Capsule.Radius,  HalfLength, Phi1);
				AddTriangle(B0, T0, T1, Color);
				AddTriangle(B0, T1, B1, Color);
			}

			for (int32 Stack = 0; Stack < CapsuleHemisphereStacks; ++Stack)
			{
				const float Theta0 = (FMath::Pi * 0.5f) * static_cast<float>(Stack) / static_cast<float>(CapsuleHemisphereStacks);
				const float Theta1 = (FMath::Pi * 0.5f) * static_cast<float>(Stack + 1) / static_cast<float>(CapsuleHemisphereStacks);
				const float TopR0 = Capsule.Radius * std::cos(Theta0);
				const float TopR1 = Capsule.Radius * std::cos(Theta1);
				const float TopZ0 = HalfLength + Capsule.Radius * std::sin(Theta0);
				const float TopZ1 = HalfLength + Capsule.Radius * std::sin(Theta1);
				const float BotR0 = Capsule.Radius * std::cos(Theta0);
				const float BotR1 = Capsule.Radius * std::cos(Theta1);
				const float BotZ0 = -HalfLength - Capsule.Radius * std::sin(Theta0);
				const float BotZ1 = -HalfLength - Capsule.Radius * std::sin(Theta1);

				for (int32 Slice = 0; Slice < CapsuleSlices; ++Slice)
				{
					const float Phi0 = 2.0f * FMath::Pi * static_cast<float>(Slice) / static_cast<float>(CapsuleSlices);
					const float Phi1 = 2.0f * FMath::Pi * static_cast<float>(Slice + 1) / static_cast<float>(CapsuleSlices);

					const FVector Top00 = CapsulePoint(TopR0, TopZ0, Phi0);
					const FVector Top01 = CapsulePoint(TopR0, TopZ0, Phi1);
					const FVector Top10 = CapsulePoint(TopR1, TopZ1, Phi0);
					const FVector Top11 = CapsulePoint(TopR1, TopZ1, Phi1);
					AddTriangle(Top00, Top10, Top11, Color);
					AddTriangle(Top00, Top11, Top01, Color);

					const FVector Bot00 = CapsulePoint(BotR0, BotZ0, Phi0);
					const FVector Bot01 = CapsulePoint(BotR0, BotZ0, Phi1);
					const FVector Bot10 = CapsulePoint(BotR1, BotZ1, Phi0);
					const FVector Bot11 = CapsulePoint(BotR1, BotZ1, Phi1);
					AddTriangle(Bot00, Bot11, Bot10, Color);
					AddTriangle(Bot00, Bot01, Bot11, Color);
				}
			}
		}
	}

	std::sort(Triangles.begin(), Triangles.end(),
		[](const FProjectedTriangle& A, const FProjectedTriangle& B)
		{
			return A.Depth > B.Depth;
		});

	DrawList->PushClipRect(ViewportPos, ImVec2(ViewportPos.x + ViewportSize.x, ViewportPos.y + ViewportSize.y), true);
	const ImDrawListFlags OldDrawListFlags = DrawList->Flags;
	DrawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;
	for (const FProjectedTriangle& Triangle : Triangles)
	{
		DrawList->AddTriangleFilled(Triangle.A, Triangle.B, Triangle.C, Triangle.Color);
	}
	DrawList->Flags = OldDrawListFlags;
	DrawList->PopClipRect();
}

void FPhysicsAssetEditorWidget::RenderPhysicsListPanel(UPhysicsAsset* Asset, ImVec2 Size)
{
	if (Size.x <= 1.0f || Size.y <= 1.0f)
	{
		return;
	}

	ImGui::BeginChild("##PhysicsAssetListPanel", Size, true);
	DrawPhysicsPanelHeader("Physics Bodies / Constraints");

	if (!Asset)
	{
		ImGui::EndChild();
		return;
	}

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##PhysicsAssetListFilter", "Search Bodies, Shapes, Constraints", ListFilter, sizeof(ListFilter));
	const FString Filter = ListFilter;

	if (ImGui::BeginTabBar("##PhysicsAssetListTabs"))
	{
		if (ImGui::BeginTabItem("Bodies"))
		{
			const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
			for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(Bodies.size()); ++BodyIndex)
			{
				UBodySetup* Body = Bodies[BodyIndex];
				if (!Body)
				{
					continue;
				}

				const FString BodyLabel = Body->GetBoneName().ToString();
				if (!Filter.empty() && BodyLabel.find(Filter) == FString::npos)
				{
					continue;
				}

				ImGui::PushID(BodyIndex);
				const bool bBodySelected = Selection.Type == EPhysicsAssetEditorSelectionType::Body && Selection.BodyIndex == BodyIndex;
				ImGuiTreeNodeFlags BodyFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (bBodySelected)
				{
					BodyFlags |= ImGuiTreeNodeFlags_Selected;
				}

				const bool bOpen = ImGui::TreeNodeEx("Body", BodyFlags, "%s", BodyLabel.c_str());
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				{
					SelectBody(BodyIndex);
				}

				if (bOpen)
				{
					const EPhysicsAssetShapeType ShapeTypes[] = {
						EPhysicsAssetShapeType::Sphere,
						EPhysicsAssetShapeType::Box,
						EPhysicsAssetShapeType::Sphyl,
						EPhysicsAssetShapeType::Convex
					};

					for (EPhysicsAssetShapeType ShapeType : ShapeTypes)
					{
						const int32 ShapeCount = Body->GetShapeCount(ShapeType);
						for (int32 ShapeIndex = 0; ShapeIndex < ShapeCount; ++ShapeIndex)
						{
							ImGui::PushID(static_cast<int32>(ShapeType) * 1000 + ShapeIndex);
							const bool bShapeSelected = Selection.Type == EPhysicsAssetEditorSelectionType::Shape
								&& Selection.BodyIndex == BodyIndex
								&& Selection.ShapeType == ShapeType
								&& Selection.ShapeIndex == ShapeIndex;
							char Label[64];
							std::snprintf(Label, sizeof(Label), "%s %d", ShapeTypeLabel(ShapeType), ShapeIndex);
							if (ImGui::Selectable(Label, bShapeSelected))
							{
								SelectShape(BodyIndex, ShapeType, ShapeIndex);
							}
							ImGui::PopID();
						}
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Constraints"))
		{
			const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
			for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(Constraints.size()); ++ConstraintIndex)
			{
				const UPhysicsConstraintTemplate* Constraint = Constraints[ConstraintIndex];
				if (!Constraint)
				{
					continue;
				}

				FString Label = Constraint->GetParentBoneName().ToString();
				Label += " -> ";
				Label += Constraint->GetChildBoneName().ToString();
				if (!Filter.empty() && Label.find(Filter) == FString::npos)
				{
					continue;
				}

				ImGui::PushID(ConstraintIndex);
				const bool bSelected = Selection.Type == EPhysicsAssetEditorSelectionType::Constraint
					&& Selection.ConstraintIndex == ConstraintIndex;
				if (ImGui::Selectable(Label.c_str(), bSelected))
				{
					SelectConstraint(ConstraintIndex);
				}
				ImGui::PopID();
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Profiles"))
		{
			ImGui::TextDisabled("Collision and constraint profiles are not implemented yet.");
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::EndChild();
}

void FPhysicsAssetEditorWidget::RenderSkeletonTreePanel(UPhysicsAsset* Asset)
{
	DrawPhysicsPanelHeader("Skeleton Tree");

	if (!Asset)
	{
		return;
	}

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##PhysicsAssetTreeFilter", "Search Skeleton", TreeFilter, sizeof(TreeFilter));
	const FString Filter = TreeFilter;

	if (PreviewMesh && PreviewMesh->GetSkeletalMeshAsset())
	{
		const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
		if (ImGui::TreeNodeEx("Skeleton", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MeshAsset->Bones.size()); ++BoneIndex)
			{
				if (MeshAsset->Bones[BoneIndex].ParentIndex == -1
					&& BoneSubtreeMatchesFilter(MeshAsset, BoneIndex, Filter))
				{
					RenderBoneTreeNode(MeshAsset, Asset, BoneIndex);
				}
			}
			ImGui::TreePop();
		}
	}
}

void FPhysicsAssetEditorWidget::RenderBoneTreeNode(const FSkeletalMesh* MeshAsset, UPhysicsAsset* Asset, int32 BoneIndex)
{
	if (!MeshAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
	{
		return;
	}

	const FBone& Bone = MeshAsset->Bones[BoneIndex];
	const bool bHasBody = Asset && Asset->FindBodyIndex(FName(Bone.Name)) >= 0;
	const bool bSelected = Selection.Type == EPhysicsAssetEditorSelectionType::Bone && Selection.BoneIndex == BoneIndex;

	bool bHasChild = false;
	for (const FBone& Candidate : MeshAsset->Bones)
	{
		if (Candidate.ParentIndex == BoneIndex)
		{
			bHasChild = true;
			break;
		}
	}

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!bHasChild)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (bSelected)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	FString Label = Bone.Name;
	if (bHasBody)
	{
		Label += "  [Body]";
	}

	ImGui::PushID(BoneIndex);
	const bool bOpen = ImGui::TreeNodeEx("Bone", Flags, "%s", Label.c_str());
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		SelectBone(BoneIndex);
	}

	if (bHasChild && bOpen)
	{
		for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(MeshAsset->Bones.size()); ++ChildIndex)
		{
			if (MeshAsset->Bones[ChildIndex].ParentIndex == BoneIndex
				&& BoneSubtreeMatchesFilter(MeshAsset, ChildIndex, TreeFilter))
			{
				RenderBoneTreeNode(MeshAsset, Asset, ChildIndex);
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void FPhysicsAssetEditorWidget::RenderGraphPanel(UPhysicsAsset* Asset, ImVec2 Size)
{
	if (Size.x <= 1.0f || Size.y <= 1.0f)
	{
		return;
	}

	ImGui::BeginChild("##PhysicsAssetGraphPanel", Size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	DrawPhysicsPanelHeader("Graph");

	const ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), IM_COL32(24, 24, 24, 255));

	const float GridStep = 24.0f;
	for (float X = CanvasPos.x; X < CanvasPos.x + CanvasSize.x; X += GridStep)
	{
		DrawList->AddLine(ImVec2(X, CanvasPos.y), ImVec2(X, CanvasPos.y + CanvasSize.y), IM_COL32(42, 42, 42, 180));
	}
	for (float Y = CanvasPos.y; Y < CanvasPos.y + CanvasSize.y; Y += GridStep)
	{
		DrawList->AddLine(ImVec2(CanvasPos.x, Y), ImVec2(CanvasPos.x + CanvasSize.x, Y), IM_COL32(42, 42, 42, 180));
	}

	const char* Watermark = "PHYSICS";
	const ImVec2 WatermarkSize = ImGui::CalcTextSize(Watermark);
	DrawList->AddText(ImVec2(CanvasPos.x + CanvasSize.x - WatermarkSize.x - 10.0f, CanvasPos.y + CanvasSize.y - WatermarkSize.y - 8.0f),
		IM_COL32(120, 120, 120, 70), Watermark);

	if (!Asset)
	{
		ImGui::EndChild();
		return;
	}

	const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
	std::vector<ImVec2> BodyPins(Bodies.size(), ImVec2(0.0f, 0.0f));

	const float BodyNodeWidth = Clamp(CanvasSize.x * 0.42f, 84.0f, 132.0f);
	const float BodyNodeHeight = 26.0f;
	const float LeftX = CanvasPos.x + 12.0f;
	const float RightX = CanvasPos.x + CanvasSize.x - BodyNodeWidth - 12.0f;
	const float StartY = CanvasPos.y + 14.0f;
	const float StepY = 36.0f;
	const int32 MaxVisibleBodies = static_cast<int32>(Clamp((CanvasSize.y - 24.0f) / StepY, 1.0f, 256.0f));

	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(Bodies.size()) && BodyIndex < MaxVisibleBodies; ++BodyIndex)
	{
		const UBodySetup* Body = Bodies[BodyIndex];
		if (!Body)
		{
			continue;
		}

		const bool bSelected = Selection.Type == EPhysicsAssetEditorSelectionType::Body && Selection.BodyIndex == BodyIndex;
		const float X = (BodyIndex % 2 == 0) ? LeftX : RightX;
		const float Y = StartY + static_cast<float>(BodyIndex / 2) * StepY;
		const ImVec2 Min(X, Y);
		const ImVec2 Max(X + BodyNodeWidth, Y + BodyNodeHeight);
		const ImU32 Color = bSelected ? IM_COL32(142, 196, 104, 255) : IM_COL32(104, 154, 84, 255);
		DrawList->AddRectFilled(Min, Max, Color, 2.0f);
		DrawList->AddRect(Min, Max, IM_COL32(210, 235, 180, 220), 2.0f);

		FString Label = Body->GetBoneName().ToString();
		if (Label.size() > 18)
		{
			Label = Label.substr(0, 15) + "...";
		}
		DrawList->AddText(ImVec2(Min.x + 7.0f, Min.y + 5.0f), IM_COL32(20, 20, 20, 255), Label.c_str());
		BodyPins[BodyIndex] = ImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);

		ImGui::SetCursorScreenPos(Min);
		ImGui::PushID(BodyIndex);
		ImGui::InvisibleButton("##BodyGraphNode", ImVec2(BodyNodeWidth, BodyNodeHeight));
		if (ImGui::IsItemClicked())
		{
			SelectBody(BodyIndex);
		}
		ImGui::PopID();
	}

	const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
	for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(Constraints.size()); ++ConstraintIndex)
	{
		const UPhysicsConstraintTemplate* Constraint = Constraints[ConstraintIndex];
		if (!Constraint)
		{
			continue;
		}

		const int32 ParentIndex = Asset->FindBodyIndex(Constraint->GetParentBoneName());
		const int32 ChildIndex = Asset->FindBodyIndex(Constraint->GetChildBoneName());
		if (ParentIndex < 0 || ChildIndex < 0
			|| ParentIndex >= static_cast<int32>(BodyPins.size())
			|| ChildIndex >= static_cast<int32>(BodyPins.size())
			|| ParentIndex >= MaxVisibleBodies
			|| ChildIndex >= MaxVisibleBodies)
		{
			continue;
		}

		const ImU32 LineColor = Selection.Type == EPhysicsAssetEditorSelectionType::Constraint && Selection.ConstraintIndex == ConstraintIndex
			? IM_COL32(255, 210, 96, 255)
			: IM_COL32(210, 210, 160, 190);
		DrawList->AddLine(BodyPins[ParentIndex], BodyPins[ChildIndex], LineColor, 2.0f);
	}

	if (static_cast<int32>(Bodies.size()) > MaxVisibleBodies)
	{
		ImGui::SetCursorScreenPos(ImVec2(CanvasPos.x + 10.0f, CanvasPos.y + CanvasSize.y - 22.0f));
		ImGui::TextDisabled("+ %d more bodies", static_cast<int32>(Bodies.size()) - MaxVisibleBodies);
	}

	ImGui::EndChild();
}

void FPhysicsAssetEditorWidget::RenderDetailsPanel(UPhysicsAsset* Asset)
{
	ImGui::TextUnformatted("Details");
	ImGui::Separator();

	if (!Asset)
	{
		return;
	}

	if (ImGui::CollapsingHeader("Asset", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Preview Mesh");
		ImGui::TextWrapped("%s", Asset->GetPreviewSkeletalMeshPath().c_str());
		if (!PreviewMesh)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "Preview mesh is not resolved.");
		}

		ImGui::Text("Bodies: %llu", static_cast<unsigned long long>(Asset->GetBodySetups().size()));
		ImGui::Text("Constraints: %llu", static_cast<unsigned long long>(Asset->GetConstraintTemplates().size()));
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::None)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("Select a bone, body, shape, or constraint.");
		return;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Bone)
	{
		if (!PreviewMesh || !PreviewMesh->GetSkeletalMeshAsset())
		{
			return;
		}

		const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
		if (Selection.BoneIndex < 0 || Selection.BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
		{
			return;
		}

		const FBone& Bone = MeshAsset->Bones[Selection.BoneIndex];
		const FName BoneName(Bone.Name);
		const int32 ExistingBodyIndex = Asset->FindBodyIndex(BoneName);

		if (ImGui::CollapsingHeader("Bone", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Name: %s", Bone.Name.c_str());
			ImGui::Text("Index: %d", Selection.BoneIndex);
			ImGui::Text("Parent: %d", Bone.ParentIndex);

			if (ExistingBodyIndex >= 0)
			{
				if (ImGui::Button("Select Body"))
				{
					SelectBody(ExistingBodyIndex);
				}
			}
			else if (ImGui::Button("Create Body"))
			{
				UBodySetup* Body = Asset->GetOrCreateBodySetup(BoneName);
				if (Body)
				{
					const FVector BoneWorld = Bone.GetReferenceGlobalPose().GetLocation();
					float BoneLength = 0.35f;
					for (const FBone& Candidate : MeshAsset->Bones)
					{
						if (Candidate.ParentIndex == Selection.BoneIndex)
						{
							BoneLength = Clamp((Candidate.GetReferenceGlobalPose().GetLocation() - BoneWorld).Length(), 0.1f, 1000.0f);
							break;
						}
					}

					Body->AddSphyl(FVector::ZeroVector, FQuat::Identity, BoneLength * 0.2f, BoneLength);
					SelectBody(Asset->FindBodyIndex(BoneName));
					MarkDirty();
				}
			}
		}
		return;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Body)
	{
		const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
		if (Selection.BodyIndex >= 0 && Selection.BodyIndex < static_cast<int32>(Bodies.size()) && Bodies[Selection.BodyIndex])
		{
			UBodySetup* Body = Bodies[Selection.BodyIndex];
			if (ImGui::CollapsingHeader("Body Setup", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Bone: %s", Body->GetBoneName().ToString().c_str());
				ImGui::Text("Spheres: %d", Body->GetShapeCount(EPhysicsAssetShapeType::Sphere));
				ImGui::Text("Boxes: %d", Body->GetShapeCount(EPhysicsAssetShapeType::Box));
				ImGui::Text("Capsules: %d", Body->GetShapeCount(EPhysicsAssetShapeType::Sphyl));
				ImGui::Text("Convex: %d", Body->GetShapeCount(EPhysicsAssetShapeType::Convex));

				if (ImGui::Button("Add Sphere"))
				{
					Body->AddSphere(FVector::ZeroVector, 0.2f);
					SelectShape(Selection.BodyIndex, EPhysicsAssetShapeType::Sphere, Body->GetShapeCount(EPhysicsAssetShapeType::Sphere) - 1);
					MarkDirty();
				}
				ImGui::SameLine();
				if (ImGui::Button("Add Box"))
				{
					Body->AddBox(FVector::ZeroVector, FQuat::Identity, FVector(0.2f, 0.2f, 0.2f));
					SelectShape(Selection.BodyIndex, EPhysicsAssetShapeType::Box, Body->GetShapeCount(EPhysicsAssetShapeType::Box) - 1);
					MarkDirty();
				}
				if (ImGui::Button("Add Capsule"))
				{
					Body->AddSphyl(FVector::ZeroVector, FQuat::Identity, 0.15f, 0.5f);
					SelectShape(Selection.BodyIndex, EPhysicsAssetShapeType::Sphyl, Body->GetShapeCount(EPhysicsAssetShapeType::Sphyl) - 1);
					MarkDirty();
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete Body"))
				{
					if (Asset->RemoveBodySetupAt(Selection.BodyIndex))
					{
						ClearSelection();
						MarkDirty();
					}
				}
			}
		}
		return;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Shape)
	{
		const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
		if (Selection.BodyIndex >= 0 && Selection.BodyIndex < static_cast<int32>(Bodies.size()) && Bodies[Selection.BodyIndex])
		{
			RenderShapeDetails(Asset, Bodies[Selection.BodyIndex]);
		}
		return;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Constraint)
	{
		RenderConstraintDetails(Asset);
	}
}

void FPhysicsAssetEditorWidget::RenderShapeDetails(UPhysicsAsset* Asset, UBodySetup* Body)
{
	if (!Asset || !Body)
	{
		return;
	}

	FKAggregateGeom& Geom = Body->GetAggGeom();
	bool bChanged = false;

	if (ImGui::CollapsingHeader("Primitive", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Body: %s", Body->GetBoneName().ToString().c_str());
		ImGui::Text("Shape: %s %d", ShapeTypeLabel(Selection.ShapeType), Selection.ShapeIndex);

		if (Selection.ShapeType == EPhysicsAssetShapeType::Sphere)
		{
			if (Selection.ShapeIndex >= 0 && Selection.ShapeIndex < static_cast<int32>(Geom.SphereElems.size()))
			{
				FKSphereElem& Sphere = Geom.SphereElems[Selection.ShapeIndex];
				bChanged |= ImGui::DragFloat3("Center", &Sphere.Center.X, 0.01f);
				bChanged |= ImGui::DragFloat("Radius", &Sphere.Radius, 0.01f, 0.001f, 1000.0f);
				Sphere.Radius = Clamp(Sphere.Radius, 0.001f, 1000.0f);
			}
		}
		else if (Selection.ShapeType == EPhysicsAssetShapeType::Box)
		{
			if (Selection.ShapeIndex >= 0 && Selection.ShapeIndex < static_cast<int32>(Geom.BoxElems.size()))
			{
				FKBoxElem& Box = Geom.BoxElems[Selection.ShapeIndex];
				FVector Euler = FRotator::FromQuaternion(Box.Rotation).ToVector();
				bChanged |= ImGui::DragFloat3("Center", &Box.Center.X, 0.01f);
				if (ImGui::DragFloat3("Rotation", &Euler.X, 0.25f))
				{
					Box.Rotation = FRotator(Euler).ToQuaternion();
					bChanged = true;
				}
				bChanged |= ImGui::DragFloat3("Extents", &Box.Extents.X, 0.01f, 0.001f, 1000.0f);
				Box.Extents.X = Clamp(Box.Extents.X, 0.001f, 1000.0f);
				Box.Extents.Y = Clamp(Box.Extents.Y, 0.001f, 1000.0f);
				Box.Extents.Z = Clamp(Box.Extents.Z, 0.001f, 1000.0f);
			}
		}
		else if (Selection.ShapeType == EPhysicsAssetShapeType::Sphyl)
		{
			if (Selection.ShapeIndex >= 0 && Selection.ShapeIndex < static_cast<int32>(Geom.SphylElems.size()))
			{
				FKSphylElem& Capsule = Geom.SphylElems[Selection.ShapeIndex];
				FVector Euler = FRotator::FromQuaternion(Capsule.Rotation).ToVector();
				bChanged |= ImGui::DragFloat3("Center", &Capsule.Center.X, 0.01f);
				if (ImGui::DragFloat3("Rotation", &Euler.X, 0.25f))
				{
					Capsule.Rotation = FRotator(Euler).ToQuaternion();
					bChanged = true;
				}
				bChanged |= ImGui::DragFloat("Radius", &Capsule.Radius, 0.01f, 0.001f, 1000.0f);
				bChanged |= ImGui::DragFloat("Length", &Capsule.Length, 0.01f, 0.001f, 1000.0f);
				Capsule.Radius = Clamp(Capsule.Radius, 0.001f, 1000.0f);
				Capsule.Length = Clamp(Capsule.Length, 0.001f, 1000.0f);
			}
		}
		else
		{
			ImGui::TextDisabled("Convex primitive editing is not implemented yet.");
		}

		if (ImGui::Button("Delete Shape"))
		{
			if (Body->RemoveShape(Selection.ShapeType, Selection.ShapeIndex))
			{
				SelectBody(Selection.BodyIndex);
				MarkDirty();
				return;
			}
		}
	}

	if (bChanged)
	{
		MarkDirty();
	}
}

void FPhysicsAssetEditorWidget::RenderConstraintDetails(UPhysicsAsset* Asset)
{
	if (!Asset)
	{
		return;
	}

	const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
	if (Selection.ConstraintIndex < 0
		|| Selection.ConstraintIndex >= static_cast<int32>(Constraints.size())
		|| !Constraints[Selection.ConstraintIndex])
	{
		return;
	}

	UPhysicsConstraintTemplate* Constraint = Constraints[Selection.ConstraintIndex];
	if (ImGui::CollapsingHeader("Constraint", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Parent: %s", Constraint->GetParentBoneName().ToString().c_str());
		ImGui::Text("Child: %s", Constraint->GetChildBoneName().ToString().c_str());

		const char* Modes[] = { "Free", "Locked", "Limited" };
		int32 Mode = static_cast<int32>(Constraint->GetAngularMode());
		if (ImGui::Combo("Angular Mode", &Mode, Modes, 3))
		{
			Constraint->SetAngularMode(static_cast<EAngularConstraintMode>(Mode));
			MarkDirty();
		}

		float Swing1 = Constraint->GetSwing1Limit();
		float Swing2 = Constraint->GetSwing2Limit();
		float Twist = Constraint->GetTwistLimit();
		bool bLimitsChanged = false;
		bLimitsChanged |= ImGui::DragFloat("Swing 1", &Swing1, 0.25f, 0.0f, 180.0f);
		bLimitsChanged |= ImGui::DragFloat("Swing 2", &Swing2, 0.25f, 0.0f, 180.0f);
		bLimitsChanged |= ImGui::DragFloat("Twist", &Twist, 0.25f, 0.0f, 180.0f);
		if (bLimitsChanged)
		{
			Constraint->SetAngularLimits(Swing1, Swing2, Twist);
			MarkDirty();
		}

		FTransform FrameA = Constraint->GetLocalFrameA();
		FTransform FrameB = Constraint->GetLocalFrameB();
		if (ImGui::DragFloat3("Frame A Location", &FrameA.Location.X, 0.01f))
		{
			Constraint->SetLocalFrameA(FrameA);
			MarkDirty();
		}
		if (ImGui::DragFloat3("Frame B Location", &FrameB.Location.X, 0.01f))
		{
			Constraint->SetLocalFrameB(FrameB);
			MarkDirty();
		}

		if (ImGui::Button("Delete Constraint"))
		{
			if (Asset->RemoveConstraintAt(Selection.ConstraintIndex))
			{
				ClearSelection();
				MarkDirty();
			}
		}
	}
}

void FPhysicsAssetEditorWidget::RenderToolsPanel(UPhysicsAsset* Asset, ImVec2 Size)
{
	if (Size.x <= 1.0f || Size.y <= 1.0f)
	{
		return;
	}

	ImGui::BeginChild("##PhysicsAssetToolsPanel", Size, true);
	if (ImGui::BeginTabBar("##PhysicsAssetRightTabs"))
	{
		if (ImGui::BeginTabItem("Tools"))
		{
			DrawPhysicsPanelHeader("Body Creation");

			ImGui::DragFloat("Min Bone Size", &BodyCreationParams.MinBoneSize, 0.5f, 0.0f, 1000.0f);

			static const char* const PrimitiveItems[] = { "Box", "Capsule", "Sphere" };
			EnumCombo("Primitive Type", BodyCreationParams.PrimitiveType, PrimitiveItems, 3);

			static const char* const WeightItems[] = { "Any Weight", "Dominant Weight" };
			EnumCombo("Vertex Weighting Type", BodyCreationParams.VertexWeighting, WeightItems, 2);

			ImGui::Checkbox("Auto Orient to Bone", &BodyCreationParams.bAutoOrientToBone);
			ImGui::Checkbox("Walk Past Small Bones", &BodyCreationParams.bWalkPastSmallBones);
			ImGui::Checkbox("Create Body for All Bones", &BodyCreationParams.bCreateBodyForAllBones);
			if (BodyCreationParams.bCreateBodyForAllBones)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.32f, 1.0f), "All bones includes cloth/accessory chains.");
			}
			ImGui::InputInt("Lod Index", &BodyCreationParams.LodIndex);

			ImGui::Spacing();
			DrawPhysicsPanelHeader("Constraint Creation");
			ImGui::Checkbox("Create Constraints", &BodyCreationParams.bCreateConstraints);

			static const char* const ConstraintItems[] = { "Free", "Limited", "Locked" };
			EnumCombo("Angular Constraint Mode", BodyCreationParams.AngularConstraintMode, ConstraintItems, 3);

			ImGui::Spacing();
			if (ImGui::Button("Re-generate Bodies", ImVec2(-1.0f, 26.0f)))
			{
				RegenerateBodies(Asset);
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Profiles"))
		{
			DrawPhysicsPanelHeader("Profiles");
			ImGui::TextDisabled("Collision and constraint profiles are not implemented yet.");
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
	ImGui::EndChild();
}

bool FPhysicsAssetEditorWidget::RegenerateBodies(UPhysicsAsset* Asset)
{
	if (!Asset || !PreviewMesh || !PreviewMesh->GetSkeletalMeshAsset())
	{
		return false;
	}

	Asset->Clear();
	const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
	GeneratePhysicsAssetBodies(*Asset, *MeshAsset, BodyCreationParams);
	GeneratePhysicsAssetConstraints(*Asset, *MeshAsset, BodyCreationParams);
	ClearSelection();
	MarkDirty();
	return !Asset->GetBodySetups().empty();
}

void FPhysicsAssetEditorWidget::HandleViewportSelectionClick()
{
	if (ActiveMode == EPhysicsAssetEditorMode::Preview)
	{
		return;
	}

	FPhysicsAssetEditorHitResult Hit;
	if (ViewportClient.PickBodyShapeAtMouse(Hit))
	{
		SelectShape(Hit.BodyIndex, Hit.ShapeType, Hit.ShapeIndex);
	}
	else
	{
		ClearSelection();
	}
}

void FPhysicsAssetEditorWidget::SelectBody(int32 BodyIndex)
{
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Body;
	Selection.BodyIndex = BodyIndex;
	SyncViewportHighlight();
}

void FPhysicsAssetEditorWidget::SelectShape(int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex)
{
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Shape;
	Selection.BodyIndex = BodyIndex;
	Selection.ShapeType = ShapeType;
	Selection.ShapeIndex = ShapeIndex;
	SyncViewportHighlight();
}

void FPhysicsAssetEditorWidget::SelectConstraint(int32 ConstraintIndex)
{
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Constraint;
	Selection.ConstraintIndex = ConstraintIndex;
	SyncViewportHighlight();
}

void FPhysicsAssetEditorWidget::SelectBone(int32 BoneIndex)
{
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Bone;
	Selection.BoneIndex = BoneIndex;
	SyncViewportHighlight();
}

void FPhysicsAssetEditorWidget::ClearSelection()
{
	Selection.Clear();
	SyncViewportHighlight();
}

void FPhysicsAssetEditorWidget::ValidateSelection(UPhysicsAsset* Asset)
{
	if (Selection.Type == EPhysicsAssetEditorSelectionType::None)
	{
		return;
	}

	bool bValid = false;
	if (Asset)
	{
		const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
		const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();

		switch (Selection.Type)
		{
		case EPhysicsAssetEditorSelectionType::Body:
			bValid = Selection.BodyIndex >= 0
				&& Selection.BodyIndex < static_cast<int32>(Bodies.size())
				&& Bodies[Selection.BodyIndex] != nullptr;
			break;

		case EPhysicsAssetEditorSelectionType::Shape:
			bValid = Selection.BodyIndex >= 0
				&& Selection.BodyIndex < static_cast<int32>(Bodies.size())
				&& Bodies[Selection.BodyIndex] != nullptr
				&& Selection.ShapeIndex >= 0
				&& Selection.ShapeIndex < Bodies[Selection.BodyIndex]->GetShapeCount(Selection.ShapeType);
			break;

		case EPhysicsAssetEditorSelectionType::Constraint:
			bValid = Selection.ConstraintIndex >= 0
				&& Selection.ConstraintIndex < static_cast<int32>(Constraints.size())
				&& Constraints[Selection.ConstraintIndex] != nullptr;
			break;

		case EPhysicsAssetEditorSelectionType::Bone:
			if (PreviewMesh && PreviewMesh->GetSkeletalMeshAsset())
			{
				const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
				bValid = Selection.BoneIndex >= 0
					&& Selection.BoneIndex < static_cast<int32>(MeshAsset->Bones.size());
			}
			break;

		default:
			break;
		}
	}

	if (!bValid)
	{
		ClearSelection();
	}
}

void FPhysicsAssetEditorWidget::SyncViewportHighlight()
{
	switch (Selection.Type)
	{
	case EPhysicsAssetEditorSelectionType::Body:
		ViewportClient.SetHighlightedBody(Selection.BodyIndex);
		break;
	case EPhysicsAssetEditorSelectionType::Shape:
		ViewportClient.SetHighlightedShape(Selection.BodyIndex, Selection.ShapeType, Selection.ShapeIndex);
		break;
	case EPhysicsAssetEditorSelectionType::Constraint:
		ViewportClient.SetHighlightedConstraint(Selection.ConstraintIndex);
		break;
	default:
		ViewportClient.ClearHighlight();
		break;
	}
}

bool FPhysicsAssetEditorWidget::DeleteSelection(UPhysicsAsset* Asset)
{
	if (!Asset)
	{
		return false;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Body)
	{
		const bool bRemoved = Asset->RemoveBodySetupAt(Selection.BodyIndex);
		if (bRemoved)
		{
			ClearSelection();
		}
		return bRemoved;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Shape)
	{
		const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
		if (Selection.BodyIndex < 0 || Selection.BodyIndex >= static_cast<int32>(Bodies.size()) || !Bodies[Selection.BodyIndex])
		{
			return false;
		}

		const bool bRemoved = Bodies[Selection.BodyIndex]->RemoveShape(Selection.ShapeType, Selection.ShapeIndex);
		if (bRemoved)
		{
			SelectBody(Selection.BodyIndex);
		}
		return bRemoved;
	}

	if (Selection.Type == EPhysicsAssetEditorSelectionType::Constraint)
	{
		const bool bRemoved = Asset->RemoveConstraintAt(Selection.ConstraintIndex);
		if (bRemoved)
		{
			ClearSelection();
		}
		return bRemoved;
	}

	return false;
}
