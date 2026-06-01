#include "Editor/UI/Asset/Physics/PhysicsAssetEditorWidget.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Editor/Slate/SlateApplication.h"
#include "Editor/UI/Toolbar/ViewportToolbar.h"
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
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <cstdio>

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

	bool IsValidAssetPath(const FString& Path)
	{
		return !Path.empty() && Path != "None";
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

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
	ViewportClient.Initialize(Device, static_cast<uint32>(ViewportSize.x), static_cast<uint32>(ViewportSize.y));
	ViewportClient.SetPreviewScene(WorldContext.World, PhysicsAsset, PreviewMeshComponent);
	ViewportClient.ResetCameraToPreviewBounds();

	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FPhysicsAssetEditorWidget::Close()
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
	ImGui::Separator();

	const float TotalHeight = ImGui::GetContentRegionAvail().y;
	ImGui::BeginChild("SkeletonTree", ImVec2(HierarchyWidth, TotalHeight), true);
	RenderSkeletonTreePanel(PhysicsAsset);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
	ImGui::Button("##PhysicsAssetTreeSplitter", ImVec2(4.0f, TotalHeight));
	if (ImGui::IsItemActive())
	{
		HierarchyWidth += ImGui::GetIO().MouseDelta.x;
		float MaxHierarchyWidth = ImGui::GetWindowWidth() - DetailsWidth - 320.0f;
		if (MaxHierarchyWidth < 180.0f) MaxHierarchyWidth = 180.0f;
		HierarchyWidth = Clamp(HierarchyWidth, 180.0f, MaxHierarchyWidth);
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();

	const float ViewportWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x - 4.0f;
	RenderViewportPanel(ImVec2(ViewportWidth, TotalHeight));

	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
	ImGui::Button("##PhysicsAssetDetailsSplitter", ImVec2(4.0f, TotalHeight));
	if (ImGui::IsItemActive())
	{
		DetailsWidth -= ImGui::GetIO().MouseDelta.x;
		float MaxDetailsWidth = ImGui::GetWindowWidth() - HierarchyWidth - 320.0f;
		if (MaxDetailsWidth < 240.0f) MaxDetailsWidth = 240.0f;
		DetailsWidth = Clamp(DetailsWidth, 240.0f, MaxDetailsWidth);
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();

	ImGui::BeginChild("Details", ImVec2(DetailsWidth, TotalHeight), true);
	RenderDetailsPanel(PhysicsAsset);
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
	constexpr float BarHeight = 32.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 BarPos = ImGui::GetCursorScreenPos();
	const float BarWidth = ImGui::GetContentRegionAvail().x;
	DrawList->AddRectFilled(BarPos, ImVec2(BarPos.x + BarWidth, BarPos.y + BarHeight), IM_COL32(38, 38, 38, 255));

	auto ModeButton = [&](const char* Label, EPhysicsAssetEditorMode Mode)
	{
		const bool bActive = ActiveMode == Mode;
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		const float Width = TextSize.x + 28.0f;

		ImGui::InvisibleButton(Label, ImVec2(Width, BarHeight));
		if (ImGui::IsItemClicked())
		{
			ActiveMode = Mode;
		}

		if (bActive || ImGui::IsItemHovered())
		{
			DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + BarHeight),
				bActive ? IM_COL32(44, 44, 44, 255) : IM_COL32(255, 255, 255, 20));
		}

		DrawList->AddText(ImVec2(Pos.x + 14.0f, Pos.y + (BarHeight - TextSize.y) * 0.5f),
			bActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(190, 190, 190, 255), Label);
		if (bActive)
		{
			DrawList->AddRectFilled(ImVec2(Pos.x, Pos.y + BarHeight - 2.0f),
				ImVec2(Pos.x + Width, Pos.y + BarHeight), IM_COL32(64, 132, 224, 255));
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

	ImGui::SetCursorScreenPos(ImVec2(BarPos.x, BarPos.y + BarHeight));
}

void FPhysicsAssetEditorWidget::RenderViewportPanel(ImVec2 Size)
{
	if (Size.x <= 1.0f || Size.y <= 1.0f)
	{
		return;
	}

	ImGui::BeginChild("Viewport", Size, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

	if (FViewport* VP = ViewportClient.GetViewport())
	{
		VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));
		if (VP->GetSRV())
		{
			ImGui::Image((ImTextureID)VP->GetSRV(), Size);
			FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
		}
	}

	constexpr float ToolbarHeight = 28.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
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

	ImGui::EndChild();
}

void FPhysicsAssetEditorWidget::RenderSkeletonTreePanel(UPhysicsAsset* Asset)
{
	ImGui::TextUnformatted("Skeleton Tree");
	ImGui::Separator();

	if (!Asset)
	{
		return;
	}

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##PhysicsAssetTreeFilter", "Search Skeleton, Bodies, Constraints", TreeFilter, sizeof(TreeFilter));
	const FString Filter = TreeFilter;

	if (PreviewMesh && PreviewMesh->GetSkeletalMeshAsset())
	{
		const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
		if (ImGui::TreeNodeEx("Skeleton", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MeshAsset->Bones.size()); ++BoneIndex)
			{
				if (MeshAsset->Bones[BoneIndex].ParentIndex == -1)
				{
					RenderBoneTreeNode(MeshAsset, Asset, BoneIndex);
				}
			}
			ImGui::TreePop();
		}
	}

	const TArray<UBodySetup*>& Bodies = Asset->GetBodySetups();
	if (ImGui::TreeNodeEx("Bodies", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
	{
		for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(Bodies.size()); ++BodyIndex)
		{
			UBodySetup* Body = Bodies[BodyIndex];
			if (!Body)
			{
				continue;
			}

			ImGui::PushID(BodyIndex);
			const bool bBodySelected = Selection.Type == EPhysicsAssetEditorSelectionType::Body && Selection.BodyIndex == BodyIndex;
			const FString BodyLabel = Body->GetBoneName().ToString();
			if (!Filter.empty() && BodyLabel.find(Filter) == FString::npos)
			{
				ImGui::PopID();
				continue;
			}
			ImGuiTreeNodeFlags BodyFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
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
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Constraints", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
	{
		const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset->GetConstraintTemplates();
		for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(Constraints.size()); ++ConstraintIndex)
		{
			const UPhysicsConstraintTemplate* Constraint = Constraints[ConstraintIndex];
			if (!Constraint)
			{
				continue;
			}

			const bool bSelected = Selection.Type == EPhysicsAssetEditorSelectionType::Constraint
				&& Selection.ConstraintIndex == ConstraintIndex;
			FString Label = Constraint->GetParentBoneName().ToString();
			Label += " -> ";
			Label += Constraint->GetChildBoneName().ToString();
			if (!Filter.empty() && Label.find(Filter) == FString::npos)
			{
				continue;
			}
			ImGui::PushID(ConstraintIndex);
			if (ImGui::Selectable(Label.c_str(), bSelected))
			{
				SelectConstraint(ConstraintIndex);
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
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
			if (MeshAsset->Bones[ChildIndex].ParentIndex == BoneIndex)
			{
				RenderBoneTreeNode(MeshAsset, Asset, ChildIndex);
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
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

void FPhysicsAssetEditorWidget::SelectBody(int32 BodyIndex)
{
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Body;
	Selection.BodyIndex = BodyIndex;
}

void FPhysicsAssetEditorWidget::SelectShape(int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex)
{
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Shape;
	Selection.BodyIndex = BodyIndex;
	Selection.ShapeType = ShapeType;
	Selection.ShapeIndex = ShapeIndex;
}

void FPhysicsAssetEditorWidget::SelectConstraint(int32 ConstraintIndex)
{
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Constraint;
	Selection.ConstraintIndex = ConstraintIndex;
}

void FPhysicsAssetEditorWidget::SelectBone(int32 BoneIndex)
{
	Selection.Clear();
	Selection.Type = EPhysicsAssetEditorSelectionType::Bone;
	Selection.BoneIndex = BoneIndex;
}

void FPhysicsAssetEditorWidget::ClearSelection()
{
	Selection.Clear();
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
