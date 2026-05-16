#include "MeshEditorWidget.h"

#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Runtime/Engine.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Viewport/Viewport.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "Settings/EditorSettings.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "Slate/SlateApplication.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationManager.h"
#include "UI/EditorFileUtils.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace
{
	FString FormatMeshStatCount(size_t Value)
	{
		FString Result = std::to_string(Value);
		for (int32 InsertPos = static_cast<int32>(Result.length()) - 3; InsertPos > 0; InsertPos -= 3)
		{
			Result.insert(static_cast<size_t>(InsertPos), ",");
		}
		return Result;
	}
}

static uint32 GNextMeshEditorInstanceId = 0;

FMeshEditorWidget::FMeshEditorWidget()
	: InstanceId(GNextMeshEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("MeshEditorPreview_" + Id);
	WindowIdSuffix = "###MeshEditor_" + Id;
}

bool FMeshEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<USkeletalMesh>();
}

bool FMeshEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	const USkeletalMesh* CurrentMesh = Cast<USkeletalMesh>(EditedObject);
	const USkeletalMesh* RequestedMesh = Cast<USkeletalMesh>(Object);
	if (!IsOpen() || !CurrentMesh || !RequestedMesh)
	{
		return false;
	}

	const FString& CurrentPath = CurrentMesh->GetAssetPathFileName();
	return !CurrentPath.empty()
		&& CurrentPath != "None"
		&& CurrentPath == RequestedMesh->GetAssetPathFileName();
}

void FMeshEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(EditedObject))
	{
		USkeletalMeshComponent* Comp = Actor->AddComponent<USkeletalMeshComponent>();
		Comp->SetSkeletalMesh(Mesh);
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
	FloorActor->InitDefaultComponents("Data/BasicShape/Cube.OBJ");
	FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
	FloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.02f));

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), static_cast<uint32>(ViewportSize.x), static_cast<uint32>(ViewportSize.y));
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(Actor->GetComponentByClass<USkeletalMeshComponent>());

	ViewportClient.CreatePreviewGizmo();
	ViewportClient.CreateBoneDebugComponent();
	ViewportClient.ResetCameraToPreviousBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	ViewportClient.SetSelectedBone(Cast<USkeletalMesh>(EditedObject), -1);

	FSlateApplication::Get().RegisterViewport(&MeshViewportWindow, &ViewportClient);

	ActiveTab         = EMeshEditorTab::Skeleton;
	AnimTabState      = FAnimationTabState {};
	SelectedBoneIndex = -1;
}

void FMeshEditorWidget::Close()
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

void FMeshEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}

	if (ActiveTab == EMeshEditorTab::Animation)
	{
		USkeletalMeshComponent* Comp = ViewportClient.GetPreviewMeshComponent();
		if (!Comp) return;
		UAnimSingleNodeInstance* NodeInst = Comp->GetAnimNodeInstance(FName::None);
		if (!NodeInst) return;

		NodeInst->UpdateAnimation(DeltaTime);

		USkeletalMesh* Mesh = Comp->GetSkeletalMesh();
		if (!Mesh) return;
		FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
		if (!Asset || Asset->Bones.empty()) return;

		FPoseContext Out;
		Out.SkeletalMesh = Mesh;
		Out.Pose.resize(Asset->Bones.size());
		Out.ResetToRefPose();

		// Root bone을 bind pose에 고정하기 위해 ref pose값을 미리 보관.
		// AnimViewer는 in-place 재생이 기본이며, root motion을 적용하면
		// 캐릭터가 뷰포트 밖으로 이동/회전해 버린다.
		const FTransform RootRefPose = Out.Pose.empty() ? FTransform {} : Out.Pose[0];

		NodeInst->EvaluatePose(Out);

		// root motion 억제: 루트 본을 ref pose로 복원
		if (!Out.Pose.empty())
		{
			Out.Pose[0] = RootRefPose;
		}

		Comp->SetBoneLocalTransforms(Out.Pose);
	}
}

void FMeshEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FMeshEditorViewportClient*>(&ViewportClient));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Render entry point
// ─────────────────────────────────────────────────────────────────────────────

void FMeshEditorWidget::Render(float DeltaTime)
{
	// 1프레임 지연 close (SRV lifetime issue)
	if (bPendingClose)
	{
		Close();
		bPendingClose = false;
		return;
	}
	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject);

	bool bWindowOpen = true;
	FString VisibleTitle = "Mesh Editor";
	const FString AssetPath = SkeletalMesh ? SkeletalMesh->GetAssetPathFileName() : FString();
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

	const float AvailableHeight = ImGui::GetContentRegionAvail().y;

	switch (ActiveTab)
	{
	case EMeshEditorTab::Skeleton:
		RenderSkeletonLayout();
		break;
	case EMeshEditorTab::Mesh:
		RenderMeshLayout();
		break;
	case EMeshEditorTab::Animation:
		RenderAnimationLayout(AvailableHeight);
		break;
	}

	ImGui::End();

	if (!bWindowOpen)
	{
		bPendingClose = true;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab bar
// ─────────────────────────────────────────────────────────────────────────────

void FMeshEditorWidget::RenderTabBar()
{
	auto TabButton = [this](const char* Label, EMeshEditorTab Tab)
	{
		const bool bActive = (ActiveTab == Tab);
		if (bActive)
		{
			ImVec4 ActiveColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
			ImGui::PushStyleColor(ImGuiCol_Button, ActiveColor);
		}
		if (ImGui::Button(Label))
		{
			ActiveTab = Tab;
		}
		if (bActive)
		{
			ImGui::PopStyleColor();
		}
		ImGui::SameLine();
	};

	TabButton("Skeleton", EMeshEditorTab::Skeleton);
	TabButton("Mesh", EMeshEditorTab::Mesh);
	TabButton("Animation", EMeshEditorTab::Animation);

	ImGui::NewLine();
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared: viewport panel
// ─────────────────────────────────────────────────────────────────────────────

void FMeshEditorWidget::RenderViewportPanel(ImVec2 Size)
{
	ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

	FViewport* VP = ViewportClient.GetViewport();
	if (!VP || Size.x <= 0 || Size.y <= 0)
	{
		ImGui::Dummy(Size);
		return;
	}

	VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));
	MeshViewportWindow.SetRect(FRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y));

	if (VP->GetSRV())
	{
		ImGui::Image((ImTextureID)VP->GetSRV(), Size);
	}
	else
	{
		ImGui::Dummy(Size);
	}

	constexpr float ToolbarHeight = 28.0f;
	ImDrawList*     DrawList      = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(ViewportPos, ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight), IM_COL32(40, 40, 40, 255));

	FViewportToolbarContext Context;
	Context.Renderer              = &GEngine->GetRenderer();
	Context.Gizmo                 = ViewportClient.GetGizmo();
	Context.Settings              = &FEditorSettings::Get().MeshEditorViewportSettings;
	Context.RenderOptions         = &ViewportClient.GetRenderOptions();
	Context.ToolbarLeft           = ViewportPos.x;
	Context.ToolbarTop            = ViewportPos.y;
	Context.ToolbarWidth          = Size.x;
	Context.bReservePlayStopSpace = false;
	Context.bShowAddActor         = false;
	Context.OnCoordSystemToggled  = [&]()
	{
		FGizmoToolSettings& Settings = FEditorSettings::Get().MeshEditorViewportSettings.Gizmo;
		Settings.CoordSystem         = (Settings.CoordSystem == EEditorCoordSystem::World) ? EEditorCoordSystem::Local : EEditorCoordSystem::World;
		ViewportClient.ApplyTransformSettingsToGizmo();
	};
	Context.OnSettingsChanged = [&]()
	{
		ViewportClient.ApplyTransformSettingsToGizmo();
	};
	Context.OnRenderViewModeExtras = [&]()
	{
		const EBoneDebugDrawMode CurrentBoneDrawMode = ViewportClient.GetBoneDebugDrawMode();
		int32                    BoneDrawMode        = static_cast<int32>(CurrentBoneDrawMode);
		ImGui::Text("Bone Display");
		ImGui::RadioButton("Selected Bone", &BoneDrawMode, static_cast<int32>(EBoneDebugDrawMode::SelectedOnly));
		ImGui::RadioButton("All Bones", &BoneDrawMode, static_cast<int32>(EBoneDebugDrawMode::AllBones));
		if (BoneDrawMode != static_cast<int32>(CurrentBoneDrawMode))
		{
			ViewportClient.SetBoneDebugDrawMode(static_cast<EBoneDebugDrawMode>(BoneDrawMode));
		}
	};

	FViewportToolbar::Render(Context);
	RenderMeshStatsOverlay(DrawList, ViewportPos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Skeleton tab
// ─────────────────────────────────────────────────────────────────────────────

void FMeshEditorWidget::RenderSkeletonLayout()
{
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject);

	// Left: bone hierarchy
	ImGui::BeginChild("BoneHierarchy", ImVec2(HierarchyWidth, 0), true);
	ImGui::Text("Bone Hierarchy");
	ImGui::Separator();
	if (SkeletalMesh)
	{
		const FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
		if (Asset)
		{
			for (int32 i = 0; i < static_cast<int32>(Asset->Bones.size()); ++i)
			{
				if (Asset->Bones[i].ParentIndex == -1)
				{
					RenderBoneTree(Asset, i);
				}
			}
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Splitter
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
	ImGui::Button("##skelSplitter", ImVec2(4.0f, -1.0f));
	if (ImGui::IsItemActive())
	{
		HierarchyWidth += ImGui::GetIO().MouseDelta.x;
		HierarchyWidth = std::max(100.0f, std::min(HierarchyWidth, ImGui::GetWindowWidth() - DetailsWidth - 100.0f));
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	ImGui::PopStyleColor(3);

	ImGui::SameLine();

	// Center: viewport
	ImGui::BeginGroup();
	{
		float  ViewportWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x;
		ImVec2 Size          = ImVec2(ViewportWidth, ImGui::GetContentRegionAvail().y);
		RenderViewportPanel(Size);
	}
	ImGui::EndGroup();

	ImGui::SameLine();

	// Right: bone details
	ImGui::BeginChild("BoneDetails", ImVec2(DetailsWidth, 0), true);
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
		FTransform LocalTransform = PreviewMeshComponent ? PreviewMeshComponent->GetBoneLocalTransformByIndex(SelectedBoneIndex) : FTransform(Bone.LocalMatrix);

		FVector Location = LocalTransform.Location;
		if (ImGui::DragFloat3("Location", &Location.X, 0.1f))
		{
			LocalTransform.Location = Location;
			if (PreviewMeshComponent)
				PreviewMeshComponent->SetBoneLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			else
				Bone.LocalMatrix = LocalTransform.ToMatrix();
		}

		FVector Rotation = LocalTransform.GetRotator().ToVector();
		if (ImGui::DragFloat3("Rotation", &Rotation.X, 0.1f))
		{
			LocalTransform.SetRotation(FRotator(Rotation));
			if (PreviewMeshComponent)
				PreviewMeshComponent->SetBoneLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			else
				Bone.LocalMatrix = LocalTransform.ToMatrix();
		}

		FVector Scale = LocalTransform.Scale;
		if (ImGui::DragFloat3("Scale", &Scale.X, 0.1f, 0.01f))
		{
			LocalTransform.Scale = Scale;
			if (PreviewMeshComponent)
				PreviewMeshComponent->SetBoneLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			else
				Bone.LocalMatrix = LocalTransform.ToMatrix();
		}
	}
	else
	{
		ImGui::TextDisabled("Select a bone to edit.");
	}

	ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
// Mesh tab
// ─────────────────────────────────────────────────────────────────────────────

void FMeshEditorWidget::RenderMeshLayout()
{
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject);

	// Left: mesh info
	const float StatsWidth = 220.0f;
	ImGui::BeginChild("MeshInfo", ImVec2(StatsWidth, 0), true);
	ImGui::Text("Mesh Info");
	ImGui::Separator();
	if (SkeletalMesh)
	{
		const FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
		if (Asset)
		{
			ImGui::Text("Vertices:  %s", FormatMeshStatCount(Asset->Vertices.size()).c_str());
			ImGui::Text("Triangles: %s", FormatMeshStatCount(Asset->Indices.size() / 3).c_str());
			ImGui::Text("Bones:     %zu", Asset->Bones.size());
			ImGui::Dummy(ImVec2(0, 8));
			const FString& Path = SkeletalMesh->GetAssetPathFileName();
			if (!Path.empty() && Path != "None")
			{
				ImGui::TextWrapped("Path:\n%s", Path.c_str());
			}
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Center: viewport (full remaining width)
	ImGui::BeginGroup();
	{
		ImVec2 Size = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
		RenderViewportPanel(Size);
	}
	ImGui::EndGroup();
}

// ─────────────────────────────────────────────────────────────────────────────
// Animation tab
// ─────────────────────────────────────────────────────────────────────────────

void FMeshEditorWidget::ApplyAnimationToComponent()
{
	USkeletalMeshComponent* Comp = ViewportClient.GetPreviewMeshComponent();
	if (!Comp || !AnimTabState.CurrentSequence)
	{
		return;
	}
	Comp->PlayAnimation(AnimTabState.CurrentSequence, /*bLooping=*/true);
	Comp->SetPlaying(false);
	Comp->SetPlayRate(1.0f);
}

void FMeshEditorWidget::RenderAnimationLayout(float TotalHeight)
{
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject);

	constexpr float TimelineHeight = 100.0f;
	const float     ContentHeight  = TotalHeight - TimelineHeight - ImGui::GetStyle().ItemSpacing.y * 3.0f;

	// ─── Top 3-panel area ───

	// Left: animation list
	ImGui::BeginChild("AnimList", ImVec2(AnimTabState.AnimListWidth, ContentHeight), true);
	ImGui::Text("Animations");
	ImGui::Separator();

	if (ImGui::Button("Load..."))
	{
		FEditorFileDialogOptions Opts;
		Opts.Filter                       = L"Animation Files (*.uasset)\0*.uasset\0All Files (*.*)\0*.*\0";
		Opts.Title                        = L"Load Animation";
		Opts.bReturnRelativeToProjectRoot = true;
		FString Path                      = FEditorFileUtils::OpenFileDialog(Opts);
		if (!Path.empty())
		{
			UAnimSequence* Seq = FAnimationManager::Get().LoadAnimation(Path);
			if (Seq && Seq->IsCompatibleWith(SkeletalMesh))
			{
				AnimTabState.CurrentSequence   = Seq;
				AnimTabState.SelectedAnimIndex = -1;
				ApplyAnimationToComponent();
			}
		}
	}

	ImGui::Separator();

	const TArray<FAssetListItem>& AnimFiles = FAnimationManager::Get().GetAvailableAnimationFiles();
	for (int32 i = 0; i < static_cast<int32>(AnimFiles.size()); ++i)
	{
		const FAssetListItem& Item      = AnimFiles[i];
		const bool            bSelected = (AnimTabState.SelectedAnimIndex == i);
		if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
		{
			if (AnimTabState.SelectedAnimIndex != i)
			{
				AnimTabState.SelectedAnimIndex = i;
				UAnimSequence* Seq             = FAnimationManager::Get().LoadAnimation(Item.FullPath);
				if (Seq && Seq->IsCompatibleWith(SkeletalMesh))
				{
					AnimTabState.CurrentSequence = Seq;
					ApplyAnimationToComponent();
				}
			}
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", Item.FullPath.c_str());
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Center: viewport
	ImGui::BeginGroup();
	{
		float  ViewportWidth = ImGui::GetContentRegionAvail().x - AnimTabState.AnimDetailsWidth - ImGui::GetStyle().ItemSpacing.x;
		ImVec2 Size          = ImVec2(ViewportWidth, ContentHeight);
		RenderViewportPanel(Size);
	}
	ImGui::EndGroup();

	ImGui::SameLine();

	// Right: sequence properties
	ImGui::BeginChild("AnimDetails", ImVec2(AnimTabState.AnimDetailsWidth, ContentHeight), true);
	ImGui::Text("Details");
	ImGui::Separator();
	if (AnimTabState.CurrentSequence)
	{
		UAnimSequence* Seq = AnimTabState.CurrentSequence;
		ImGui::Text("Name:   %s", Seq->GetName().c_str());
		ImGui::Text("Length: %.3f s", Seq->GetPlayLength());
		ImGui::Text("FPS:    %.1f", Seq->GetFrameRate());
		ImGui::Text("Frames: %d", Seq->GetNumberOfFrames());
		ImGui::Dummy(ImVec2(0, 6));
		const FString& Path = Seq->GetAssetPathFileName();
		if (!Path.empty() && Path != "None")
		{
			ImGui::TextWrapped("Path:\n%s", Path.c_str());
		}
	}
	else
	{
		ImGui::TextDisabled("No animation selected.");
	}
	ImGui::EndChild();

	// ─── Bottom: timeline ───
	ImGui::BeginChild("AnimTimeline", ImVec2(0, TimelineHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	{
		UAnimSingleNodeInstance* NodeInst = nullptr;
		USkeletalMeshComponent*  Comp     = ViewportClient.GetPreviewMeshComponent();
		if (Comp && AnimTabState.CurrentSequence)
		{
			NodeInst = Comp->GetAnimNodeInstance(FName::None);
		}

		const float TotalLength = AnimTabState.CurrentSequence ? AnimTabState.CurrentSequence->GetPlayLength() : 0.0f;
		const int32 TotalFrames = AnimTabState.CurrentSequence ? AnimTabState.CurrentSequence->GetNumberOfFrames() : 0;
		RenderAnimationTimeline(NodeInst, TotalLength, TotalFrames);
	}
	ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
// Animation timeline
// ─────────────────────────────────────────────────────────────────────────────

void FMeshEditorWidget::RenderAnimationTimeline(UAnimSingleNodeInstance* NodeInst, float TotalLength, int32 TotalFrames)
{
	if (!AnimTabState.CurrentSequence || TotalLength <= 0.0f)
	{
		ImGui::TextDisabled("No animation selected.");
		return;
	}

	USkeletalMeshComponent* Comp = ViewportClient.GetPreviewMeshComponent();

	const float CurrentTime = NodeInst ? NodeInst->GetCurrentTime() : 0.0f;
	const bool  bIsPlaying  = NodeInst && NodeInst->IsPlaying();
	const bool  bIsLooping  = NodeInst ? NodeInst->IsLooping() : true;
	const float PlayRate    = NodeInst ? NodeInst->GetPlayRate() : 1.0f;

	const float FrameDelta = (TotalFrames > 1) ? (TotalLength / static_cast<float>(TotalFrames - 1)) : 0.0f;

	// ─── Transport controls ───
	if (ImGui::Button("|<"))
	{
		if (NodeInst) NodeInst->SetCurrentTime(0.0f);
	}
	ImGui::SameLine();

	if (ImGui::Button("<"))
	{
		if (NodeInst) NodeInst->SetCurrentTime(std::max(CurrentTime - FrameDelta, 0.0f));
	}
	ImGui::SameLine();

	if (bIsPlaying)
	{
		if (ImGui::Button(" || "))
		{
			if (Comp) Comp->SetPlaying(false);
		}
	}
	else
	{
		if (ImGui::Button(" >  "))
		{
			if (Comp) Comp->SetPlaying(true);
		}
	}
	ImGui::SameLine();

	if (ImGui::Button(">"))
	{
		if (NodeInst) NodeInst->SetCurrentTime(std::min(CurrentTime + FrameDelta, TotalLength));
	}
	ImGui::SameLine();

	if (ImGui::Button(">|"))
	{
		if (NodeInst) NodeInst->SetCurrentTime(TotalLength);
	}
	ImGui::SameLine();

	ImGui::Text("  %.3f / %.3f s", CurrentTime, TotalLength);
	ImGui::SameLine();

	bool bLoop = bIsLooping;
	if (ImGui::Checkbox("Loop", &bLoop))
	{
		if (Comp) Comp->SetLooping(bLoop);
	}
	ImGui::SameLine();

	float Rate = PlayRate;
	ImGui::SetNextItemWidth(60.0f);
	if (ImGui::DragFloat("Rate", &Rate, 0.01f, -3.0f, 3.0f, "%.2fx"))
	{
		if (Comp) Comp->SetPlayRate(Rate);
	}

	// ─── Timeline bar ───
	ImDrawList*     DrawList          = ImGui::GetWindowDrawList();
	ImVec2          TimelinePos       = ImGui::GetCursorScreenPos();
	const float     TimelineWidth     = ImGui::GetContentRegionAvail().x;
	constexpr float BarHeight         = 26.0f;
	constexpr float NotifyTrackHeight = 18.0f;
	constexpr float TrackGap          = 2.0f;

	// Interaction via invisible button
	ImGui::InvisibleButton("##timeline", ImVec2(TimelineWidth, BarHeight));
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive  = ImGui::IsItemActive();

	if ((bHovered || bActive) && ImGui::IsMouseDown(0) && NodeInst && Comp)
	{
		const float MouseX = ImGui::GetIO().MousePos.x - TimelinePos.x;
		const float T      = FMath::Clamp(MouseX / TimelineWidth, 0.0f, 1.0f) * TotalLength;
		NodeInst->SetCurrentTime(T);
		if (bActive && ImGui::IsMouseDragging(0, 1.0f))
		{
			Comp->SetPlaying(false);
		}
	}

	// Background
	DrawList->AddRectFilled(TimelinePos, ImVec2(TimelinePos.x + TimelineWidth, TimelinePos.y + BarHeight), IM_COL32(22, 22, 22, 255));

	// Tick marks + labels
	float TickInterval = 0.5f;
	if (TotalLength <= 0.5f) TickInterval = 0.05f;
	else if (TotalLength <= 1.0f) TickInterval = 0.1f;
	else if (TotalLength <= 5.0f) TickInterval = 0.5f;
	else if (TotalLength <= 20.0f) TickInterval = 1.0f;
	else TickInterval                           = 5.0f;

	for (float T = 0.0f; T <= TotalLength + 1e-4f; T += TickInterval)
	{
		const float X = TimelinePos.x + (T / TotalLength) * TimelineWidth;
		// alternating major/minor based on even multiples
		const bool  bMajor = (static_cast<int32>(std::roundf(T / TickInterval)) % 2 == 0);
		const float TickH  = bMajor ? BarHeight * 0.55f : BarHeight * 0.3f;
		DrawList->AddLine(ImVec2(X, TimelinePos.y + BarHeight - TickH), ImVec2(X, TimelinePos.y + BarHeight), IM_COL32(100, 100, 100, 255));
		if (bMajor)
		{
			char Buf[16];
			snprintf(Buf, sizeof(Buf), "%.2gs", static_cast<double>(T));
			DrawList->AddText(ImVec2(X + 2.0f, TimelinePos.y + 2.0f), IM_COL32(160, 160, 160, 255), Buf);
		}
	}

	// Current time cursor
	const float CursorX = TimelinePos.x + (CurrentTime / TotalLength) * TimelineWidth;
	DrawList->AddLine(ImVec2(CursorX, TimelinePos.y), ImVec2(CursorX, TimelinePos.y + BarHeight), IM_COL32(255, 200, 50, 255), 2.0f);

	// Cursor triangle (top)
	constexpr float TriSize = 5.0f;
	DrawList->AddTriangleFilled(
		ImVec2(CursorX - TriSize, TimelinePos.y),
		ImVec2(CursorX + TriSize, TimelinePos.y),
		ImVec2(CursorX, TimelinePos.y + TriSize),
		IM_COL32(255, 200, 50, 255)
	);

	// Border
	DrawList->AddRect(TimelinePos, ImVec2(TimelinePos.x + TimelineWidth, TimelinePos.y + BarHeight), IM_COL32(55, 55, 55, 255));

	// ─── Notify track (TODO) ───
	const ImVec2 NotifyPos(TimelinePos.x, TimelinePos.y + BarHeight + TrackGap);
	DrawList->AddRectFilled(NotifyPos, ImVec2(NotifyPos.x + TimelineWidth, NotifyPos.y + NotifyTrackHeight), IM_COL32(30, 30, 30, 255));
	DrawList->AddText(ImVec2(NotifyPos.x + 4.0f, NotifyPos.y + 3.0f), IM_COL32(90, 90, 90, 255), "Notifies  (TODO)");
	DrawList->AddRect(NotifyPos, ImVec2(NotifyPos.x + TimelineWidth, NotifyPos.y + NotifyTrackHeight), IM_COL32(55, 55, 55, 255));

	ImGui::Dummy(ImVec2(TimelineWidth, NotifyTrackHeight + TrackGap));
}

// ─────────────────────────────────────────────────────────────────────────────
// Mesh stats overlay
// ─────────────────────────────────────────────────────────────────────────────

void FMeshEditorWidget::RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const
{
	if (!DrawList || !EditedObject)
	{
		return;
	}

	size_t VertexCount   = 0;
	size_t TriangleCount = 0;

	if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject))
	{
		if (const FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset())
		{
			VertexCount   = Asset->Vertices.size();
			TriangleCount = Asset->Indices.size() / 3;
		}
	}

	const FString Text =
		"Triangles: " + FormatMeshStatCount(TriangleCount) + "\n" + "Vertices: " + FormatMeshStatCount(VertexCount);

	const ImVec2 TextPos(ViewportPos.x + 8.0f, ViewportPos.y + 36.0f);
	DrawList->AddText(ImVec2(TextPos.x + 1.0f, TextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), Text.c_str());
	DrawList->AddText(TextPos, IM_COL32(235, 238, 242, 255), Text.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
// Bone tree (Skeleton tab)
// ─────────────────────────────────────────────────────────────────────────────

void FMeshEditorWidget::RenderBoneTree(const FSkeletalMesh* Asset, int32 Index)
{
	const FBone& Bone = Asset->Bones[Index];

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

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
