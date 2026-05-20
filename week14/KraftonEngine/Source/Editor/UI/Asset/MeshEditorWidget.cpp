#include "MeshEditorWidget.h"

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

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
#include "Render/Shader/ShaderManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimDataModel.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationManager.h"
#include "Asset/AssetRegistry.h"
#include "UI/Asset/AnimationTransportBar.h"
#include "UI/Asset/AnimationTimelinePanel.h"
#include "UI/Asset/AnimSequencePropertyPanel.h"
#include "UI/Asset/AnimMontagePropertyPanel.h"
#include "UI/EditorFileUtils.h"
#include "Editor/UI/EditorTextureManager.h"
#include "Platform/Paths.h"
#include "Object/Object.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

// Paths.h가 끌어오는 Windows.h는 GetCurrentTime을 GetTickCount로 치환한다.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace
{
	ID3D11ShaderResourceView* LoadTabIcon(const wchar_t* FileName)
	{
		const FString Path = FPaths::ToUtf8(
			FPaths::Combine(FPaths::AssetDir(), L"Editor/ToolIcons/", FileName));
		return FEditorTextureManager::Get().GetOrLoadIcon(Path);
	}

	FString FormatMeshStatCount(size_t Value)
	{
		FString Result = std::to_string(Value);
		for (int32 InsertPos = static_cast<int32>(Result.length()) - 3; InsertPos > 0; InsertPos -= 3)
		{
			Result.insert(static_cast<size_t>(InsertPos), ",");
		}
		return Result;
	}

	FString FormatMeshStatSeconds(double Seconds)
	{
		char Buffer[64] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%.3f sec", Seconds);
		return FString(Buffer);
	}

	bool IsSameSkeletonBindingForAnimationList(const FSkeletonBinding& A, const FSkeletonBinding& B)
	{
		return A.SkeletonPath == B.SkeletonPath
			&& A.SkeletonAssetGuid == B.SkeletonAssetGuid
			&& A.CompatibilitySignature == B.CompatibilitySignature;
	}

	TMap<FString, double> GMeshImportDurationsByAssetPath;

	double GetRecordedImportDurationSeconds(const USkeletalMesh* Mesh)
	{
		if (!Mesh)
		{
			return -1.0;
		}

		const FString& AssetPath = Mesh->GetAssetPathFileName();
		if (AssetPath.empty() || AssetPath == "None")
		{
			return -1.0;
		}

		auto It = GMeshImportDurationsByAssetPath.find(AssetPath);
		return It != GMeshImportDurationsByAssetPath.end() ? It->second : -1.0;
	}

	FMorphTargetCurve& FindOrAddMorphCurve(UAnimSequence* Seq, const FString& MorphTargetName)
	{
		TArray<FMorphTargetCurve>& Curves = Seq->GetMutableMorphTargetCurves();
		for (FMorphTargetCurve& Curve : Curves)
		{
			if (Curve.MorphTargetName == MorphTargetName)
			{
				return Curve;
			}
		}
		FMorphTargetCurve NewCurve;
		NewCurve.MorphTargetName = MorphTargetName;
		Curves.push_back(std::move(NewCurve));
		return Curves.back();
	}

	void AddOrUpdateMorphCurveKey(FMorphTargetCurve& Curve, float TimeSeconds, float Value)
	{
		constexpr float TimeTolerance = 1.0e-4f;
		for (FRawFloatCurveKey& Key : Curve.Curve.Keys)
		{
			if (std::fabs(Key.TimeSeconds - TimeSeconds) <= TimeTolerance)
			{
				Key.Value = Value;
				return;
			}
		}
		FRawFloatCurveKey NewKey;
		NewKey.TimeSeconds   = TimeSeconds;
		NewKey.Value         = Value;
		NewKey.Interpolation = 2;
		Curve.Curve.Keys.push_back(NewKey);
		std::sort(
			Curve.Curve.Keys.begin(),
			Curve.Curve.Keys.end(),
			[](const FRawFloatCurveKey& A, const FRawFloatCurveKey& B)
			{
				return A.TimeSeconds < B.TimeSeconds;
			}
		);
	}

	EUberLitDefines::ELightingModel GetLightingModelForViewMode(EViewMode ViewMode)
	{
		switch (ViewMode)
		{
		case EViewMode::Unlit:       return EUberLitDefines::ELightingModel::Unlit;
		case EViewMode::Lit_Gouraud: return EUberLitDefines::ELightingModel::Gouraud;
		case EViewMode::Lit_Lambert: return EUberLitDefines::ELightingModel::Lambert;
		case EViewMode::Lit_Phong:
		case EViewMode::LightCulling:
		default:                     return EUberLitDefines::ELightingModel::Phong;
		}
	}
}

static uint32 GNextMeshEditorInstanceId = 0;

void FMeshEditorWidget::RecordImportDurationForAsset(const FString& AssetPath, double Seconds)
{
	if (AssetPath.empty() || AssetPath == "None" || Seconds < 0.0)
	{
		return;
	}

	GMeshImportDurationsByAssetPath[AssetPath] = Seconds;
}

void FMeshEditorWidget::ClearImportDurationForAsset(const FString& AssetPath)
{
	if (AssetPath.empty() || AssetPath == "None")
	{
		return;
	}

	GMeshImportDurationsByAssetPath.erase(AssetPath);
}

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

	FSlateApplication::Get().RegisterViewport(&ViewportClient);

	// 디스크의 기존 AnimSequence .uasset 들을 목록에 채워 둔다(런타임 Load/Save 만으론 안 잡힘).
	FAnimationManager::Get().RefreshAvailableAnimations();

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

		NodeInst->EvaluatePose(Out);
		ApplyMorphPreviewOverrides(Out.MorphWeights);

		Comp->SetAnimationPose(Out.Pose, Out.MorphWeights);
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
		// 접힌 동안엔 hover 를 보고하지 않음
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
	// 언리얼 Persona 모드 툴바: 평평한 버튼 + 선택 시 액센트 밑줄.
	constexpr float BarHeight = 30.0f;
	ImDrawList*     DrawList  = ImGui::GetWindowDrawList();
	const ImVec2    BarPos    = ImGui::GetCursorScreenPos();
	const float     BarWidth  = ImGui::GetContentRegionAvail().x;
	DrawList->AddRectFilled(BarPos, ImVec2(BarPos.x + BarWidth, BarPos.y + BarHeight),
	                        IM_COL32(38, 38, 38, 255));

	auto TabButton = [&](const char* Label, const wchar_t* IconFile, EMeshEditorTab Tab)
	{
		const bool      bActive = (ActiveTab == Tab);
		constexpr float IconSz  = 18.0f;
		constexpr float PadX    = 14.0f;
		constexpr float Gap     = 8.0f;

		const ImVec2 Pos    = ImGui::GetCursorScreenPos();
		const ImVec2 TextSz = ImGui::CalcTextSize(Label);
		const float  Width  = PadX + IconSz + Gap + TextSz.x + PadX;

		ImGui::InvisibleButton(Label, ImVec2(Width, BarHeight));
		const bool bHovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked())
		{
			const EMeshEditorTab PreviousTab = ActiveTab;
			ActiveTab = Tab;
			if (PreviousTab != ActiveTab && ActiveTab == EMeshEditorTab::Skeleton)
			{
				if (USkeletalMeshComponent* Comp = ViewportClient.GetPreviewMeshComponent())
				{
					Comp->ApplyBoneEditBasePose();
				}
			}
		}

		if (bActive || bHovered)
		{
			DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + BarHeight),
				bActive ? IM_COL32(41, 41, 41, 255) : IM_COL32(255, 255, 255, 20));
		}

		const float IconY = Pos.y + (BarHeight - IconSz) * 0.5f;
		if (ID3D11ShaderResourceView* Icon = LoadTabIcon(IconFile))
		{
			DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon),
			                   ImVec2(Pos.x + PadX, IconY),
			                   ImVec2(Pos.x + PadX + IconSz, IconY + IconSz));
		}

		DrawList->AddText(ImVec2(Pos.x + PadX + IconSz + Gap, Pos.y + (BarHeight - TextSz.y) * 0.5f),
		                  bActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(190, 190, 190, 255),
		                  Label);

		if (bActive)
		{
			DrawList->AddRectFilled(ImVec2(Pos.x, Pos.y + BarHeight - 2.0f),
			                        ImVec2(Pos.x + Width, Pos.y + BarHeight),
			                        IM_COL32(64, 132, 224, 255));
		}
		ImGui::SameLine(0.0f, 0.0f);
	};

	TabButton("Skeleton", L"Skeleton.png", EMeshEditorTab::Skeleton);
	TabButton("Mesh", L"SkeletalMesh.png", EMeshEditorTab::Mesh);
	TabButton("Animation", L"Animation.png", EMeshEditorTab::Animation);

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

	if (VP->GetSRV())
	{
		ImGui::Image((ImTextureID)VP->GetSRV(), Size);
	}
	else
	{
		ImGui::Dummy(Size);
	}

	// ImGui가 계산한 hover(다른 창에 가려지면 false)를 입력 소유권 중재에 보고.
	FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());

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

		FViewportRenderOptions& RenderOptions = ViewportClient.GetRenderOptions();
		bool bWeightBoneHeatMap = RenderOptions.bWeightBoneHeatMap;
		if (ImGui::Checkbox("Weight Bone HeatMap", &bWeightBoneHeatMap))
		{
			RenderOptions.bWeightBoneHeatMap = bWeightBoneHeatMap;
			RenderOptions.WeightBoneHeatMapBoneIndex = SelectedBoneIndex;
			if (bWeightBoneHeatMap)
			{
				FShaderManager::Get().GetOrCreateUberLitPermutation(
					GetLightingModelForViewMode(RenderOptions.ViewMode),
					EUberLitDefines::EVertexFactory::SkeletalMesh,
					EShaderErrorMode::Notification,
					true);
			}
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
		FTransform LocalTransform = PreviewMeshComponent
			? PreviewMeshComponent->GetBoneEditBaseLocalTransformByIndex(SelectedBoneIndex)
			: FTransform(Bone.GetReferenceLocalPose());

		FVector Location = LocalTransform.Location;
		if (ImGui::DragFloat3("Location", &Location.X, 0.1f))
		{
			LocalTransform.Location = Location;
			if (PreviewMeshComponent)
				PreviewMeshComponent->SetBoneEditBaseLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			else
			{
				Bone.ReferenceLocalPose = LocalTransform.ToMatrix();
				Bone.SyncLegacyPoseDataFromSeparated();
			}
		}

		FVector Rotation = LocalTransform.GetRotator().ToVector();
		if (ImGui::DragFloat3("Rotation", &Rotation.X, 0.1f))
		{
			LocalTransform.SetRotation(FRotator(Rotation));
			if (PreviewMeshComponent)
				PreviewMeshComponent->SetBoneEditBaseLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			else
			{
				Bone.ReferenceLocalPose = LocalTransform.ToMatrix();
				Bone.SyncLegacyPoseDataFromSeparated();
			}
		}

		FVector Scale = LocalTransform.Scale;
		if (ImGui::DragFloat3("Scale", &Scale.X, 0.1f, 0.01f))
		{
			LocalTransform.Scale = Scale;
			if (PreviewMeshComponent)
				PreviewMeshComponent->SetBoneEditBaseLocalTransformByIndex(SelectedBoneIndex, LocalTransform);
			else
			{
				Bone.ReferenceLocalPose = LocalTransform.ToMatrix();
				Bone.SyncLegacyPoseDataFromSeparated();
			}
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
			ImGui::Text("Morphs:    %zu", Asset->MorphTargets.size());
			USkeletalMeshComponent* PreviewMeshComponent = ViewportClient.GetPreviewMeshComponent();
			if (!Asset->MorphTargets.empty() && PreviewMeshComponent)
			{
				ImGui::Dummy(ImVec2(0, 8));
				ImGui::Separator();
				ImGui::TextUnformatted("Morph Preview");
				if (ImGui::SmallButton("Reset Morphs"))
				{
					PreviewMeshComponent->ClearMorphTargetWeights();
				}
				for (int32 MorphIndex = 0; MorphIndex < static_cast<int32>(Asset->MorphTargets.size()); ++MorphIndex)
				{
					const FMorphTarget& MorphTarget = Asset->MorphTargets[MorphIndex];
					float               Weight      = PreviewMeshComponent->GetMorphTargetWeightByIndex(MorphIndex);
					ImGui::PushID(MorphIndex);
					const char* Label = MorphTarget.Name.empty() ? "Unnamed" : MorphTarget.Name.c_str();
					if (ImGui::SliderFloat(Label, &Weight, -1.0f, 1.0f, "%.3f"))
					{
						PreviewMeshComponent->SetMorphTargetWeightByIndex(MorphIndex, Weight);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%zu vertex deltas", MorphTarget.Deltas.size());
					}
					ImGui::PopID();
				}
			}
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
	ResetMorphPreviewOverrides();
}

void FMeshEditorWidget::EnsureMorphPreviewOverrideSize()
{
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject);
	FSkeletalMesh* MeshAsset    = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	const size_t   MorphCount   = MeshAsset ? MeshAsset->MorphTargets.size() : 0;
	if (AnimTabState.MorphPreviewWeights.size() != MorphCount)
	{
		AnimTabState.MorphPreviewWeights.assign(MorphCount, 0.0f);
	}
	if (AnimTabState.MorphPreviewOverrideMask.size() != MorphCount)
	{
		AnimTabState.MorphPreviewOverrideMask.assign(MorphCount, 0);
	}
}

void FMeshEditorWidget::ResetMorphPreviewOverrides()
{
	AnimTabState.MorphPreviewWeights.clear();
	AnimTabState.MorphPreviewOverrideMask.clear();
	AnimTabState.bMorphPreviewOverrideEnabled = false;
}

void FMeshEditorWidget::ApplyMorphPreviewOverrides(TArray<float>& InOutMorphWeights) const
{
	if (!AnimTabState.bMorphPreviewOverrideEnabled)
	{
		return;
	}
	const size_t Count = AnimTabState.MorphPreviewWeights.size();
	if (Count == 0 || AnimTabState.MorphPreviewOverrideMask.size() != Count)
	{
		return;
	}
	if (InOutMorphWeights.size() < Count)
	{
		InOutMorphWeights.resize(Count, 0.0f);
	}
	for (size_t Index = 0; Index < Count; ++Index)
	{
		if (AnimTabState.MorphPreviewOverrideMask[Index] != 0)
		{
			InOutMorphWeights[Index] = AnimTabState.MorphPreviewWeights[Index];
		}
	}
}

void FMeshEditorWidget::RefreshAnimationPreviewPose()
{
	USkeletalMeshComponent* Comp = ViewportClient.GetPreviewMeshComponent();
	if (!Comp) return;
	UAnimSingleNodeInstance* NodeInst = Comp->GetAnimNodeInstance(FName::None);
	if (!NodeInst) return;
	USkeletalMesh* Mesh = Comp->GetSkeletalMesh();
	if (!Mesh) return;
	FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->Bones.empty()) return;

	FPoseContext Out;
	Out.SkeletalMesh = Mesh;
	Out.Pose.resize(Asset->Bones.size());
	Out.ResetToRefPose();
	NodeInst->EvaluatePose(Out);
	ApplyMorphPreviewOverrides(Out.MorphWeights);
	Comp->SetAnimationPose(Out.Pose, Out.MorphWeights);
}

void FMeshEditorWidget::MarkAnimationListDirty()
{
	AnimTabState.bAnimationListDirty = true;
}

const TArray<FAssetListItem>& FMeshEditorWidget::GetCachedAnimationFilesForCurrentSkeleton()
{
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject);
	FSkeletonBinding CurrentBinding;

	if (SkeletalMesh)
	{
		CurrentBinding = SkeletalMesh->GetSkeletonBinding();
	}
	else
	{
		CurrentBinding.Reset();
	}

	if (AnimTabState.bAnimationListDirty ||
		!IsSameSkeletonBindingForAnimationList(AnimTabState.CachedAnimationListBinding, CurrentBinding))
	{
		AnimTabState.CachedAnimationFiles.clear();
		AnimTabState.CachedAnimationListBinding = CurrentBinding;

		if (SkeletalMesh)
		{
			AnimTabState.CachedAnimationFiles = FAssetRegistry::ListAnimationsForSkeleton(CurrentBinding, false);
		}

		AnimTabState.bAnimationListDirty = false;
	}

	return AnimTabState.CachedAnimationFiles;
}

void FMeshEditorWidget::RenderAnimationLayout(float TotalHeight)
{
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject);

	constexpr float TimelineHeight = 210.0f;
	const float     ContentHeight  = TotalHeight - TimelineHeight - ImGui::GetStyle().ItemSpacing.y * 3.0f;

	// ─── Top: Asset Details | Viewport | Asset Browser (Persona 배치) ───

	// Left: 시퀀스 / 몽타주 디테일 패널 (선택 종류에 따라 분기)
	ImGui::BeginChild("AssetDetails", ImVec2(AnimTabState.AnimDetailsWidth, ContentHeight), true);
	if (AnimTabState.bMontageSelected && AnimTabState.CurrentMontage)
	{
		USkeletalMeshComponent* Comp = ViewportClient.GetPreviewMeshComponent();
		UAnimInstance* AnimInst = Comp ? Comp->GetAnimInstance() : nullptr;
		FAnimMontagePropertyPanel::Render(AnimTabState.CurrentMontage, Comp, AnimInst);
	}
	else if (AnimTabState.CurrentSequence)
	{
		UAnimSequence* Seq = AnimTabState.CurrentSequence;
		// Notify entry 가 타임라인에서 선택되어 있으면 Notify 의 UPROPERTY 편집 UI 를 표시.
		// 아니면 기존 시퀀스 메타 + Root Motion 패널.
		const int32 NotifyCount = static_cast<int32>(Seq->GetNotifies().size());
		const bool bShowNotifyDetails =
			AnimTabState.SelectedNotifyIndex >= 0 &&
			AnimTabState.SelectedNotifyIndex < NotifyCount;
		const bool bShowMorphDetails = AnimTabState.SelectedMorphCurveIndex >= 0 && AnimTabState.SelectedMorphCurveIndex
		< static_cast<int32>(Seq->GetMorphTargetCurves().size());

		if (bShowNotifyDetails)
		{
			FAnimationTimelinePanel::RenderNotifyDetails(Seq, AnimTabState.SelectedNotifyIndex);
		}
		else if (bShowMorphDetails)
		{
			if (FAnimationTimelinePanel::RenderMorphDetails(
				Seq,
				AnimTabState.SelectedMorphCurveIndex,
				AnimTabState.SelectedMorphKeyIndex
			))
			{
				RefreshAnimationPreviewPose();
			}
		}
		else
		{
			ImGui::TextUnformatted("Asset Details");
			ImGui::Separator();
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

			// AnimSequence property 패널 — root motion 등 편집 가능한 항목.
			ImGui::Dummy(ImVec2(0, 12));
			FAnimSequencePropertyPanel::Render(Seq);

			USkeletalMeshComponent* PreviewMeshComponent = ViewportClient.GetPreviewMeshComponent();
			USkeletalMesh* PreviewMesh = PreviewMeshComponent ? PreviewMeshComponent->GetSkeletalMesh() : SkeletalMesh;
			FSkeletalMesh* MeshAsset = PreviewMesh ? PreviewMesh->GetSkeletalMeshAsset() : nullptr;
			if (MeshAsset && !MeshAsset->MorphTargets.empty())
			{
				ImGui::Dummy(ImVec2(0, 12));
				ImGui::Separator();
				ImGui::TextUnformatted("Morph Preview / Keys");
				EnsureMorphPreviewOverrideSize();
				if (ImGui::SmallButton("Clear Morph Preview"))
				{
					ResetMorphPreviewOverrides();
					RefreshAnimationPreviewPose();
				}
				for (int32 MorphIndex = 0; MorphIndex < static_cast<int32>(MeshAsset->MorphTargets.size()); ++
				     MorphIndex)
				{
					const FMorphTarget& MorphTarget   = MeshAsset->MorphTargets[MorphIndex];
					float               CurrentWeight = 0.0f;
					if (MorphIndex < static_cast<int32>(AnimTabState.MorphPreviewWeights.size()) && AnimTabState.
						MorphPreviewOverrideMask[MorphIndex] != 0)
					{
						CurrentWeight = AnimTabState.MorphPreviewWeights[MorphIndex];
					}
					else if (PreviewMeshComponent)
					{
						CurrentWeight = PreviewMeshComponent->GetMorphTargetWeightByIndex(MorphIndex);
					}

					ImGui::PushID(MorphIndex);
					const char* Label = MorphTarget.Name.empty() ? "Unnamed" : MorphTarget.Name.c_str();
					if (ImGui::SliderFloat(Label, &CurrentWeight, -1.0f, 1.0f, "%.3f"))
					{
						AnimTabState.MorphPreviewWeights[MorphIndex]      = CurrentWeight;
						AnimTabState.MorphPreviewOverrideMask[MorphIndex] = 1;
						AnimTabState.bMorphPreviewOverrideEnabled         = true;
						RefreshAnimationPreviewPose();
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Key"))
					{
						FMorphTargetCurve& Curve = FindOrAddMorphCurve(Seq, MorphTarget.Name);
						AddOrUpdateMorphCurveKey(
							Curve,
							PreviewMeshComponent && PreviewMeshComponent->GetAnimNodeInstance(FName::None)
							? PreviewMeshComponent->GetAnimNodeInstance(FName::None)->GetCurrentTime() : 0.0f,
							CurrentWeight
						);
						AnimTabState.MorphPreviewOverrideMask[MorphIndex] = 0;
						bool bAnyOverride                                 = false;
						for (uint8 Mask : AnimTabState.MorphPreviewOverrideMask)
						{
							if (Mask != 0)
							{
								bAnyOverride = true;
								break;
							}
						}
						AnimTabState.bMorphPreviewOverrideEnabled = bAnyOverride;
						FAnimationManager::Get().SaveAnimationPreservingMetadata(Seq);
						RefreshAnimationPreviewPose();
					}
					ImGui::PopID();
				}
			}
		}
	}
	else
	{
		ImGui::TextUnformatted("Asset Details");
		ImGui::Separator();
		ImGui::TextDisabled("No animation selected.");
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Center: viewport
	ImGui::BeginGroup();
	{
		float  ViewportWidth = ImGui::GetContentRegionAvail().x - AnimTabState.AnimListWidth - ImGui::GetStyle().ItemSpacing.x;
		ImVec2 Size          = ImVec2(ViewportWidth, ContentHeight);
		RenderViewportPanel(Size);
	}
	ImGui::EndGroup();

	ImGui::SameLine();

	// Right: 에셋 브라우저 (애니메이션 목록)
	ImGui::BeginChild("AssetBrowser", ImVec2(AnimTabState.AnimListWidth, ContentHeight), true);
	ImGui::TextUnformatted("Asset Browser");
	ImGui::Separator();

	if (ImGui::Button("Load...", ImVec2(-1.0f, 0.0f)))
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
				AnimTabState.CurrentSequence         = Seq;
				AnimTabState.SelectedAnimIndex       = -1;
				AnimTabState.SelectedNotifyIndex     = -1;
				AnimTabState.SelectedMorphCurveIndex = -1;
				AnimTabState.SelectedMorphKeyIndex   = -1;
				ApplyAnimationToComponent();
			}
		}
	}

	if (ImGui::Button("Import Animation FBX", ImVec2(-1.0f, 0.0f)))
	{
		FEditorFileDialogOptions Opts;
		Opts.Filter                       = L"FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
		Opts.Title                        = L"Import Animation FBX";
		Opts.bReturnRelativeToProjectRoot = true;
		FString Path                      = FEditorFileUtils::OpenFileDialog(Opts);
		if (!Path.empty())
		{
			FFbxImportOptionsDialog::BeginAnimationImport(AnimTabState.AnimationImportDialog, Path);
		}
	}

	if (ImGui::Button("+ New Morph Animation", ImVec2(-1.0f, 0.0f)) && SkeletalMesh)
	{
		UAnimSequence*  Seq       = UObjectManager::Get().CreateObject<UAnimSequence>();
		UAnimDataModel* DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>(Seq);
		DataModel->SetTiming(1.0f, 30.0f, 0);
		Seq->SetDataModel(DataModel);
		Seq->SetSkeletonBinding(SkeletalMesh->GetSkeletonBinding());
		Seq->SetFName(FName("MorphAnimation"));
		const FString AnimPath = FAnimationManager::GetAnimationPathForSkeleton(
			SkeletalMesh->GetAssetPathFileName(),
			"MorphAnimation",
			SkeletalMesh->GetSkeletonBinding().SkeletonPath
		);
		if (FAnimationManager::Get().SaveAnimation(Seq, AnimPath, SkeletalMesh->GetAssetPathFileName()))
		{
			AnimTabState.CurrentSequence         = Seq;
			AnimTabState.SelectedAnimIndex       = -1;
			AnimTabState.SelectedNotifyIndex     = -1;
			AnimTabState.SelectedMorphCurveIndex = -1;
			AnimTabState.SelectedMorphKeyIndex   = -1;
			ApplyAnimationToComponent();
			FAnimationManager::Get().RefreshAvailableAnimations();
			MarkAnimationListDirty();
		}
	}

	FAnimationImportRequest      AnimationImportRequest;
	const EFbxImportDialogResult AnimationImportDialogResult = FFbxImportOptionsDialog::RenderAnimationImportPopup(
		"Import Animation FBX Options",
		AnimTabState.AnimationImportDialog,
		SkeletalMesh ? SkeletalMesh->GetSkeletonBinding().SkeletonPath : FString("None"),
		AnimationImportRequest
	);

	if (AnimationImportDialogResult == EFbxImportDialogResult::Submitted)
	{
		TArray<UAnimSequence*> ImportedSequences;
		FAnimationManager::Get().ImportAnimationForSkeleton(AnimationImportRequest, &ImportedSequences);
		// 임포트 성공/스킵(이미 존재) 무관하게 디스크를 다시 스캔해 목록 갱신.
		FAnimationManager::Get().RefreshAvailableAnimations();
		MarkAnimationListDirty();
		if (!ImportedSequences.empty())
		{
			AnimTabState.CurrentSequence         = ImportedSequences[0];
			AnimTabState.SelectedAnimIndex       = -1;
			AnimTabState.SelectedNotifyIndex     = -1;
			AnimTabState.SelectedMorphCurveIndex = -1;
			AnimTabState.SelectedMorphKeyIndex   = -1;
			ApplyAnimationToComponent();
			FFbxImportOptionsDialog::RequestClose(AnimTabState.AnimationImportDialog);
		}
		else
		{
			AnimTabState.AnimationImportDialog.Error =
			"No animation was imported. Existing assets may have been skipped.";
		}
	}

	ImGui::Separator();

	if (ImGui::SmallButton("Refresh Animation List"))
	{
		FAnimationManager::Get().RefreshAvailableAnimations();
		FAnimationManager::Get().RefreshAvailableMontages();
		MarkAnimationListDirty();
	}

	// 디스크 스캔 — montage 목록 초기화 (최초 1회 + Refresh 시).
	static bool sMontagesScanned = false;
	if (!sMontagesScanned)
	{
		FAnimationManager::Get().RefreshAvailableMontages();
		sMontagesScanned = true;
	}

	const TArray<FAssetListItem>& AnimFiles     = GetCachedAnimationFilesForCurrentSkeleton();
	const TArray<FAssetListItem>& MontageFiles  = FAnimationManager::Get().GetAvailableMontageFiles();

	// asset 경로의 stem (확장자/디렉토리 제거) — 자동 montage 이름의 source 식별자.
	auto ExtractStem = [](const FString& Path) -> FString
	{
		const size_t LastSlash = Path.find_last_of("/\\");
		const size_t Start = (LastSlash == FString::npos) ? 0 : LastSlash + 1;
		const size_t LastDot = Path.find_last_of('.');
		const size_t End = (LastDot == FString::npos || LastDot < Start) ? Path.size() : LastDot;
		return Path.substr(Start, End - Start);
	};

	// + New Montage — 현재 선택된 sequence 가 있으면 source 로 새 montage 생성.
	// 이름은 sequence 의 asset path stem 사용 (UObject::GetName() 의 자동생성 ObjectName 회피).
	const bool bCanCreateMontage = (AnimTabState.CurrentSequence != nullptr) && !AnimTabState.bMontageSelected;
	if (!bCanCreateMontage) ImGui::BeginDisabled();
	if (ImGui::Button("+ New Montage (from selected sequence)", ImVec2(-1.0f, 0.0f)))
	{
		const FString Stem = ExtractStem(AnimTabState.CurrentSequence->GetAssetPathFileName());
		const FString MontageName = Stem + "_Montage";
		const FString PackagePath = FString("Content/Montages/") + MontageName + ".uasset";
		UAnimMontage* Montage = FAnimationManager::Get().CreateMontage(AnimTabState.CurrentSequence, MontageName);
		if (Montage)
		{
			FAnimationManager::Get().SaveMontage(Montage, PackagePath);
			FAnimationManager::Get().RefreshAvailableMontages();
			AnimTabState.CurrentMontage    = Montage;
			AnimTabState.bMontageSelected  = true;

			// 새 montage 의 인덱스 즉시 매핑 — list 의 hilight + 다음 클릭의 일관 동작 보장.
			const TArray<FAssetListItem>& Updated = FAnimationManager::Get().GetAvailableMontageFiles();
			AnimTabState.SelectedMontageIndex = -1;
			for (int32 j = 0; j < static_cast<int32>(Updated.size()); ++j)
			{
				if (Updated[j].FullPath == PackagePath)
				{
					AnimTabState.SelectedMontageIndex = j;
					break;
				}
			}
		}
	}
	if (!bCanCreateMontage) ImGui::EndDisabled();

	// 통합 리스트 — Sequence + Montage 한 selectable. 알파벳 정렬 (Walking_mixamo_com 옆에
	// Walking_mixamo_com_Montage 가 자연스럽게 인접). 시각 구분: Montage 는 노랑 + [M] prefix.
	struct FEntry
	{
		FString  DisplayName;
		FString  FullPath;
		bool     bIsMontage = false;
		int32    OriginalIndex = -1;   // AnimFiles 또는 MontageFiles 의 인덱스
	};
	TArray<FEntry> Entries;
	Entries.reserve(AnimFiles.size() + MontageFiles.size());
	for (int32 i = 0; i < static_cast<int32>(AnimFiles.size());    ++i) Entries.push_back({ AnimFiles[i].DisplayName,    AnimFiles[i].FullPath,    false, i });
	for (int32 i = 0; i < static_cast<int32>(MontageFiles.size()); ++i) Entries.push_back({ MontageFiles[i].DisplayName, MontageFiles[i].FullPath, true,  i });
	std::sort(Entries.begin(), Entries.end(),
		[](const FEntry& A, const FEntry& B) { return A.DisplayName < B.DisplayName; });

	ImGui::TextUnformatted("Animations & Montages");
	for (const FEntry& E : Entries)
	{
		const bool bSelected =
			E.bIsMontage
				? (AnimTabState.bMontageSelected && AnimTabState.SelectedMontageIndex == E.OriginalIndex)
				: (!AnimTabState.bMontageSelected && AnimTabState.SelectedAnimIndex == E.OriginalIndex);

		// 시각 구분 — Montage 는 노랑 톤. Sequence 는 기본 색.
		const ImU32 Color = E.bIsMontage ? IM_COL32(255, 200, 100, 255) : IM_COL32(255, 255, 255, 255);
		ImGui::PushStyleColor(ImGuiCol_Text, Color);

		const FString Label = (E.bIsMontage ? "[M] " : "      ") + E.DisplayName;
		if (ImGui::Selectable(Label.c_str(), bSelected))
		{
			if (E.bIsMontage)
			{
				AnimTabState.SelectedMontageIndex    = E.OriginalIndex;
				AnimTabState.bMontageSelected        = true;
				AnimTabState.SelectedNotifyIndex     = -1;
				AnimTabState.SelectedMorphCurveIndex = -1;
				AnimTabState.SelectedMorphKeyIndex   = -1;
				ResetMorphPreviewOverrides();
				if (UAnimMontage* M = FAnimationManager::Get().LoadMontage(E.FullPath))
				{
					AnimTabState.CurrentMontage = M;
				}
			}
			else
			{
				AnimTabState.SelectedAnimIndex       = E.OriginalIndex;
				AnimTabState.bMontageSelected        = false;
				AnimTabState.SelectedNotifyIndex     = -1;
				AnimTabState.SelectedMorphCurveIndex = -1;
				AnimTabState.SelectedMorphKeyIndex   = -1;
				if (UAnimSequence* Seq = FAnimationManager::Get().LoadAnimation(E.FullPath))
				{
					if (Seq->IsCompatibleWith(SkeletalMesh))
					{
						AnimTabState.CurrentSequence = Seq;
						ApplyAnimationToComponent();
					}
				}
			}
		}
		ImGui::PopStyleColor();

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s\n%s", E.bIsMontage ? "Montage" : "Sequence", E.FullPath.c_str());
		}
	}
	ImGui::EndChild();

	// ─── Bottom: Unreal 시퀀서 패널 ───
	UAnimSingleNodeInstance* NodeInst = nullptr;
	USkeletalMeshComponent*  Comp     = ViewportClient.GetPreviewMeshComponent();
	if (Comp && AnimTabState.CurrentSequence)
	{
		NodeInst = Comp->GetAnimNodeInstance(FName::None);
	}

	// 스페이스바: 재생/정지 토글 (메시 에디터 창 포커스 + 텍스트 입력 중 아닐 때)
	if (Comp && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
	    !ImGui::GetIO().WantTextInput &&
	    ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		const bool bPlaying = NodeInst && NodeInst->IsPlaying();
		Comp->SetPlaying(!bPlaying);
	}

	FAnimationTimelinePanel::Render(NodeInst, Comp, AnimTabState.CurrentSequence, TimelineHeight,
		AnimTabState.SelectedNotifyIndex,
		AnimTabState.SelectedMorphCurveIndex,
		AnimTabState.SelectedMorphKeyIndex
	);
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
	size_t IndexCount    = 0;
	double ImportSeconds = -1.0;

	if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject))
	{
		if (const FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset())
		{
			VertexCount   = Asset->Vertices.size();
			IndexCount    = Asset->Indices.size();
			TriangleCount = Asset->Indices.size() / 3;
		}
		ImportSeconds = GetRecordedImportDurationSeconds(SkeletalMesh);
	}

	FString Text =
		"Triangles: " + FormatMeshStatCount(TriangleCount) + "\n" +
		"Vertices: " + FormatMeshStatCount(VertexCount) + "\n" +
		"Indices: " + FormatMeshStatCount(IndexCount);

	if (ImportSeconds >= 0.0)
	{
		Text += "\nImport Time: " + FormatMeshStatSeconds(ImportSeconds);
	}

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
