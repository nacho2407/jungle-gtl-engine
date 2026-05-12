#include "MeshEditorWidget.h"

#include "Mesh/StaticMesh.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Runtime/Engine.h"
#include "Component/SkeletalMeshComponent.h"
#include "Viewport/Viewport.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "Settings/EditorSettings.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "Slate/SlateApplication.h"

#include <imgui.h>

bool FMeshEditorWidget::CanEdit(UObject* Object) const
{
	return Object && (Object->IsA<UStaticMesh>() || Object->IsA<USkeletalMesh>());
}

void FMeshEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, FName("MeshEditorPreview"));
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(EditedObject))
	{
		USkeletalMeshComponent* Comp = Actor->AddComponent<USkeletalMeshComponent>();
		Comp->SetSkeletalMesh(Mesh);
		Actor->SetRootComponent(Comp);
	}

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));

	AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	FloorActor->InitDefaultComponents("Data/BasicShape/Cube.OBJ");
	FloorActor->SetActorScale(FVector(10.0f, 10.0f, 1.0f));

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), ViewportSize.x, ViewportSize.y);
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(Actor->GetComponentByClass<USkeletalMeshComponent>());

	ViewportClient.CreatePreviewGizmo();
	ViewportClient.CreateBoneDebugComponent();
	ViewportClient.ResetCameraToPreviousBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	ViewportClient.SetSelectedBone(Cast<USkeletalMesh>(EditedObject), -1);

	FSlateApplication::Get().RegisterViewport(&MeshViewportWindow, &ViewportClient);
}

void FMeshEditorWidget::Close()
{
	FAssetEditorWidget::Close();

	GEngine->DestroyWorldContext(FName("MeshEditorPreview"));

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);

	ViewportClient.Release();
}

void FMeshEditorWidget::Render(float DeltaTime)
{
	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	static float HierarchyWidth = 250.0f;
	static float DetailsWidth = 300.0f;

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject);

	bool bWindowOpen = true;
	FString VisibleTitle = "Mesh Editor";
	if (!SkeletalMesh->GetAssetPathFileName().empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += SkeletalMesh->GetAssetPathFileName();
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

	FString WindowTitle = VisibleTitle + "###MeshEditor";
	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	ImGui::BeginChild("BoneHierarchy", ImVec2(HierarchyWidth, 0), true);
	ImGui::Text("Bone Hierarchy");
	ImGui::Separator();

	if (SkeletalMesh)
	{
		const FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();

		for (int32 i = 0; i < static_cast<int32>(Asset->Bones.size()); ++i)
		{
			if (Asset->Bones[i].ParentIndex == -1)
			{
				RenderBoneTree(Asset, i);
			}
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

	ImGui::Button("##splitter", ImVec2(4.0f, -1.0f));

	if (ImGui::IsItemActive())
	{
		HierarchyWidth += ImGui::GetIO().MouseDelta.x;

		if (HierarchyWidth < 100.0f) HierarchyWidth = 100.0f;
		if (HierarchyWidth > ImGui::GetWindowWidth() - 100.0f) HierarchyWidth = ImGui::GetWindowWidth() - 100.0f;
	}

	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}

	ImGui::PopStyleColor(3);
	ImGui::SameLine();

	ImGui::BeginGroup();
	{
		float AvailableWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x;
		ImVec2 Size = ImVec2(AvailableWidth, ImGui::GetContentRegionAvail().y);

		ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
		ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

		FViewport* VP = ViewportClient.GetViewport();
		if (VP && Size.x > 0 && Size.y > 0)
		{
			VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));
			MeshViewportWindow.SetRect(FRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y));

			if (VP->GetSRV())
			{
				ImGui::Image((ImTextureID)VP->GetSRV(), Size);
			}

			constexpr float ToolbarHeight = 28.0f;

			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			DrawList->AddRectFilled(ViewportPos,
				ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight),
				IM_COL32(40, 40, 40, 255));

			FViewportToolbarContext Context;
			Context.Renderer = &GEngine->GetRenderer();
			Context.Gizmo = ViewportClient.GetGizmo();
			Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
			Context.RenderOptions = &ViewportClient.GetRenderOptions();
			Context.ToolbarLeft = ViewportPos.x;
			Context.ToolbarTop = ViewportPos.y;
			Context.ToolbarWidth = Size.x;
			Context.bReservePlayStopSpace = false;
			Context.bShowAddActor = false;
			Context.OnCoordSystemToggled = [&]()
			{
				FGizmoToolSettings& Settings = FEditorSettings::Get().MeshEditorViewportSettings.Gizmo;
				Settings.CoordSystem = (Settings.CoordSystem == EEditorCoordSystem::World) ? EEditorCoordSystem::Local : EEditorCoordSystem::World;

				ViewportClient.ApplyTransformSettingsToGizmo();
			};
			Context.OnSettingsChanged = [&]()
			{
				ViewportClient.ApplyTransformSettingsToGizmo();
			};

			FViewportToolbar::Render(Context);
		}
	}
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("Details", ImVec2(DetailsWidth, 0), true);
	ImGui::Text("Bone Details");
	ImGui::Separator();

	if (SkeletalMesh && SelectedBoneIndex != -1)
	{
		FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
		FBone& Bone = Asset->Bones[SelectedBoneIndex];

		ImGui::Text("Name: %s", Bone.Name.c_str());
		ImGui::Text("Index: %d", SelectedBoneIndex);
		ImGui::Dummy(ImVec2(0, 10));

		USkeletalMeshComponent* PreviewMeshComponent = ViewportClient.GetPreviewMeshComponent();
		FTransform LocalTransform = PreviewMeshComponent ? PreviewMeshComponent->GetBoneLocalTransformByIndex(SelectedBoneIndex) : Bone.LocalTransform;

		FVector Location = LocalTransform.Location;
		if (ImGui::DragFloat3("Location", &Location.X, 0.1f))
		{
			LocalTransform.Location = Location;

			if (PreviewMeshComponent)
			{
				PreviewMeshComponent->SetBoneLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			}
			else
			{
				Bone.LocalTransform = LocalTransform;
			}
		}

		FVector Rotation = LocalTransform.GetRotator().ToVector();
		if (ImGui::DragFloat3("Rotation", &Rotation.X, 0.1f))
		{
			LocalTransform.SetRotation(FRotator(Rotation));

			if (PreviewMeshComponent)
			{
				PreviewMeshComponent->SetBoneLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			}
			else
			{
				Bone.LocalTransform = LocalTransform;
			}
		}

		FVector Scale = LocalTransform.Scale;
		if (ImGui::DragFloat3("Scale", &Scale.X, 0.1f, 0.01f))
		{
			LocalTransform.Scale = Scale;
			if (PreviewMeshComponent)
			{
				PreviewMeshComponent->SetBoneLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			}
			else
			{
				Bone.LocalTransform = LocalTransform;
			}
		}
	}
	else
	{
		ImGui::TextDisabled("Select a bone to edit.");
	}

	ImGui::EndChild();

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FMeshEditorWidget::RenderBoneTree(const FSkeletalMesh* Asset, int32 Index)
{
	const FBone& Bone = Asset->Bones[Index];

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen;

	if (Index == SelectedBoneIndex)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	bool bHasChildren = false;
	for (int32 i = Index + 1; i < static_cast<int32>(Asset->Bones.size()); ++i)
	{
		if (Asset->Bones[i].ParentIndex == Index)
		{
			bHasChildren = true;
			break;
		}
	}

	if (!bHasChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	
	bool bOpen = ImGui::TreeNodeEx(Bone.Name.c_str(), Flags);

	if (ImGui::IsItemClicked())
	{
		SelectedBoneIndex = Index;
		ViewportClient.SetSelectedBone(Cast<USkeletalMesh>(EditedObject), Index);
	}

	if (bOpen && bHasChildren)
	{
		for (int32 i = Index + 1; i < static_cast<int32>(Asset->Bones.size()); ++i)
		{
			if (Asset->Bones[i].ParentIndex == Index)
			{
				RenderBoneTree(Asset, i);
			}
		}
		ImGui::TreePop();
	}
}
