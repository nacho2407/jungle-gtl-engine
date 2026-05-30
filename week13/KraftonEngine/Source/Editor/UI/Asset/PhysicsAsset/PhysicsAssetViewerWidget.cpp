#include "PhysicsAssetViewerWidget.h"

#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsAssetManager.h"
#include "Component/Debug/PhysicsAssetDebugComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Editor/UI/Util/InlinePropertyRenderer.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/World.h"
#include "Core/Logging/Log.h"
#include "Runtime/Engine.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <algorithm>

namespace
{
uint32 GNextPhysicsAssetViewerInstanceId = 1;

FString BuildPhysicsBodyTreeLabel(const FString& BoneName, int32 BodyIndex, const UBodySetup* BodySetup)
{
	FString Label = BoneName;
	const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
	Label += "  [Body ";
	Label += std::to_string(BodyIndex);
	Label += "] S:";
	Label += std::to_string(AggGeom.SphereElems.size());
	Label += " B:";
	Label += std::to_string(AggGeom.BoxElems.size());
	Label += " C:";
	Label += std::to_string(AggGeom.SphylElems.size());
	Label += " X:";
	Label += std::to_string(AggGeom.ConvexElems.size());
	return Label;
}

bool HasVectorChanged(const FVector& Before, const FVector& After)
{
	return FVector::Distance(Before, After) > 1.0e-4f;
}

bool RenderConstraintInitDescDetails(
	UPhysicsAsset* PhysicsAsset,
	int32 ConstraintIndex,
	UPhysicsAssetDebugComponent* DebugComponent)
{
	if (!PhysicsAsset || ConstraintIndex < 0)
	{
		return false;
	}

	TArray<FConstraintInstanceInitDesc>& ConstraintDescs = PhysicsAsset->GetConstraintInitDescsMutable();
	if (ConstraintIndex >= static_cast<int32>(ConstraintDescs.size()))
	{
		return false;
	}

	FConstraintInstanceInitDesc* ConstraintDesc = &ConstraintDescs[ConstraintIndex];
	ImGui::TextUnformatted("Constraint");

	const FVector PreviousParentLocation = ConstraintDesc->ParentFrame.Location;
	const FVector PreviousChildLocation = ConstraintDesc->ChildFrame.Location;
	const bool bChanged = FInlinePropertyRenderer::RenderStructProperties(
		FConstraintInstanceInitDesc::StaticStruct(),
		ConstraintDesc,
		PhysicsAsset,
		"##PhysicsAssetViewerConstraintProps");
	if (!bChanged)
	{
		return false;
	}

	const bool bParentChanged = HasVectorChanged(PreviousParentLocation, ConstraintDesc->ParentFrame.Location);
	const bool bChildChanged = HasVectorChanged(PreviousChildLocation, ConstraintDesc->ChildFrame.Location);
	if (DebugComponent && (bParentChanged || bChildChanged))
	{
		DebugComponent->SyncConstraintFrameLocation(
			*ConstraintDesc,
			bParentChanged
				? EPhysicsAssetConstraintFrameSide::Parent
				: EPhysicsAssetConstraintFrameSide::Child);
	}

	return true;
}

bool RenderBodyPhysicsInfoDetails(UPhysicsAsset* PhysicsAsset, int32 BodyIndex)
{
	if (!PhysicsAsset || BodyIndex < 0)
	{
		return false;
	}

	TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetupsMutable();
	if (BodyIndex >= static_cast<int32>(Bodies.size()) || !Bodies[BodyIndex])
	{
		return false;
	}

	UBodySetup* BodySetup = Bodies[BodyIndex];
	ImGui::TextUnformatted("Body Physics");
	ImGui::Text("Calculated Mass: %.4f kg", BodySetup->CalculateMass());

	return FInlinePropertyRenderer::RenderStructProperties(
		FBodySetupPhysicsInfo::StaticStruct(),
		&BodySetup->GetPhysicsInfo(),
		PhysicsAsset,
		"##PhysicsAssetViewerBodyPhysicsProps");
}

bool HasPhysicsBodyInSubtree(const FSkeletalMesh* Asset, UPhysicsAsset* PhysicsAsset, int32 BoneIndex)
{
	if (!Asset || !PhysicsAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Asset->Bones.size()))
	{
		return false;
	}

	const FBone& Bone = Asset->Bones[BoneIndex];
	if (PhysicsAsset->FindBodyIndexByBoneName(FName(Bone.Name)) != -1)
	{
		return true;
	}

	for (int32 Index = BoneIndex + 1; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
	{
		if (Asset->Bones[Index].ParentIndex == BoneIndex &&
			HasPhysicsBodyInSubtree(Asset, PhysicsAsset, Index))
		{
			return true;
		}
	}

	return false;
}
}

FPhysicsAssetViewerWidget::FPhysicsAssetViewerWidget()
	: InstanceId(GNextPhysicsAssetViewerInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("PhysicsAssetViewerPreview_" + Id);
	WindowIdSuffix = "###PhysicsAssetViewer_" + Id;
}

bool FPhysicsAssetViewerWidget::CanEdit(UObject* Object) const
{
	return dynamic_cast<UPhysicsAsset*>(Object) != nullptr;
}

void FPhysicsAssetViewerWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);
	const UPhysicsAsset* PhysicsAsset = dynamic_cast<UPhysicsAsset*>(Object);
	SourceSkeletalMesh = PhysicsAsset ? PhysicsAsset->GetTypedOuter<USkeletalMesh>() : nullptr;
	SelectedBodyIndex = -1;
	SelectedConstraintIndex = -1;
	CreatePreviewWorld();
}

void FPhysicsAssetViewerWidget::Close()
{
	if (UPhysicsAsset* PhysicsAsset = dynamic_cast<UPhysicsAsset*>(EditedObject))
	{
		ViewportClient.SyncPhysicsAssetDebugComponent(PhysicsAsset, -1, -1);
	}
	ViewportClient.SetPhysicsAssetPickingEnabled(false);
	ViewportClient.SetOnPhysicsAssetBodyPicked(nullptr);
	ViewportClient.SetOnPhysicsAssetConstraintPicked(nullptr);
	ViewportClient.SetOnPhysicsAssetShapeEdited(nullptr);
	ViewportClient.SetOnPhysicsAssetConstraintEdited(nullptr);
	DestroyPreviewWorld();
	FAssetEditorWidget::Close();
	SourceSkeletalMesh = nullptr;
	SelectedBodyIndex = -1;
	SelectedConstraintIndex = -1;
}

void FPhysicsAssetViewerWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}
}

void FPhysicsAssetViewerWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (ViewportClient.IsRenderable())
	{
		OutClients.push_back(const_cast<FMeshEditorViewportClient*>(&ViewportClient));
	}
}

void FPhysicsAssetViewerWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAssetEditorWidget::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(SourceSkeletalMesh);
	ViewportClient.AddReferencedObjects(Collector);
}

void FPhysicsAssetViewerWidget::CreatePreviewWorld()
{
	if (!SourceSkeletalMesh || ViewportClient.IsRenderable())
	{
		return;
	}

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	USkeletalMeshComponent* MeshComponent = Actor->AddComponent<USkeletalMeshComponent>();
	MeshComponent->SetSkeletalMesh(SourceSkeletalMesh);
	Actor->SetRootComponent(MeshComponent);
	Actor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>();
	if (LightComp)
	{
		LightComp->SetShadowBias(0.0f);
		LightComp->PushToScene();
	}

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), 512, 512);
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(MeshComponent);
	ViewportClient.CreatePhysicsAssetDebugComponent();
	ViewportClient.SetPhysicsAssetPickingEnabled(true);
	ViewportClient.SetOnPhysicsAssetBodyPicked([this](int32 BodyIndex)
	{
		SelectedBodyIndex = BodyIndex;
		SelectedConstraintIndex = -1;
		UPhysicsAsset* PhysicsAsset = dynamic_cast<UPhysicsAsset*>(EditedObject);
		ViewportClient.SyncPhysicsAssetDebugComponent(
			PhysicsAsset,
			SelectedBodyIndex,
			SelectedConstraintIndex);
	});
	ViewportClient.SetOnPhysicsAssetConstraintPicked([this](int32 ConstraintIndex)
	{
		SelectedBodyIndex = -1;
		SelectedConstraintIndex = ConstraintIndex;
		UPhysicsAsset* PhysicsAsset = dynamic_cast<UPhysicsAsset*>(EditedObject);
		ViewportClient.SyncPhysicsAssetDebugComponent(
			PhysicsAsset,
			SelectedBodyIndex,
			SelectedConstraintIndex);
	});
	ViewportClient.SetOnPhysicsAssetShapeEdited([this]()
	{
		UPhysicsAsset* PhysicsAsset = dynamic_cast<UPhysicsAsset*>(EditedObject);
		if (SavePhysicsAsset(PhysicsAsset))
		{
			ClearDirty();
		}
		else if (PhysicsAsset)
		{
			UE_LOG("PhysicsAsset shape edit warning: failed to persist shape edit. PhysicsAsset=%s", PhysicsAsset->GetAssetPathFileName().c_str());
			MarkDirty();
		}
	});
	ViewportClient.SetOnPhysicsAssetConstraintEdited([this]()
	{
		UPhysicsAsset* PhysicsAsset = dynamic_cast<UPhysicsAsset*>(EditedObject);
		if (SavePhysicsAsset(PhysicsAsset))
		{
			ClearDirty();
		}
		else if (PhysicsAsset)
		{
			UE_LOG("PhysicsAsset constraint gizmo warning: failed to persist constraint edit. PhysicsAsset=%s", PhysicsAsset->GetAssetPathFileName().c_str());
			MarkDirty();
		}
	});
	ViewportClient.GetRenderOptions().ShowFlags.bDebugPhysicsAsset = true;
	ViewportClient.ResetCameraToPreviousBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FPhysicsAssetViewerWidget::DestroyPreviewWorld()
{
	ViewportClient.SetPhysicsAssetPickingEnabled(false);
	ViewportClient.SetOnPhysicsAssetBodyPicked(nullptr);
	ViewportClient.SetOnPhysicsAssetConstraintPicked(nullptr);
	ViewportClient.SetOnPhysicsAssetShapeEdited(nullptr);
	ViewportClient.SetOnPhysicsAssetConstraintEdited(nullptr);

	if (ViewportClient.IsRenderable())
	{
		FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	}

	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	ViewportClient.Release();
}

void FPhysicsAssetViewerWidget::RenderViewportPanel(ImVec2 Size)
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
	}
	else
	{
		ImGui::Dummy(Size);
	}

	FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
}

bool FPhysicsAssetViewerWidget::SavePhysicsAsset(UPhysicsAsset* PhysicsAsset)
{
	if (!PhysicsAsset)
	{
		return false;
	}

	if (SourceSkeletalMesh)
	{
		return FPhysicsAssetManager::Get().SaveForSkeletalMesh(
			SourceSkeletalMesh,
			SourceSkeletalMesh->GetAssetPathFileName());
	}

	const FString& PhysicsAssetPath = PhysicsAsset->GetAssetPathFileName();
	if (PhysicsAssetPath.empty() || PhysicsAssetPath == "None")
	{
		return false;
	}

	return FPhysicsAssetManager::Get().Save(
		PhysicsAsset,
		PhysicsAssetPath,
		PhysicsAsset->GetSourceSkeletalMeshPath());
}

void FPhysicsAssetViewerWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	UPhysicsAsset* PhysicsAsset = dynamic_cast<UPhysicsAsset*>(EditedObject);
	if (!PhysicsAsset)
	{
		return;
	}

	bool bWindowOpen = true;
	FString Title = "Physics Asset Viewer";
	if (IsDirty())
	{
		Title += " *";
	}

	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(Title.c_str(), &bWindowOpen))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	ViewportClient.SyncPhysicsAssetDebugComponent(PhysicsAsset, SelectedBodyIndex, SelectedConstraintIndex);

	constexpr float BodyListWidth = 280.0f;
	constexpr float BodyDetailsWidth = 320.0f;
	ImGui::BeginChild("PhysicsAssetViewerBodies", ImVec2(BodyListWidth, 0), true);
	RenderBodyList(PhysicsAsset);
	ImGui::EndChild();

	ImGui::SameLine();

	const float ViewportWidth = std::max(
		160.0f,
		ImGui::GetContentRegionAvail().x - BodyDetailsWidth - ImGui::GetStyle().ItemSpacing.x);
	ImGui::BeginGroup();
	RenderViewportPanel(ImVec2(ViewportWidth, ImGui::GetContentRegionAvail().y));
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("PhysicsAssetViewerBodyDetails", ImVec2(BodyDetailsWidth, 0), true);
	RenderBodyDetails(PhysicsAsset);
	ImGui::EndChild();

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FPhysicsAssetViewerWidget::RenderBodyList(UPhysicsAsset* PhysicsAsset)
{
	ImGui::TextUnformatted("Bodies");
	ImGui::Separator();

	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	if (SelectedBodyIndex >= static_cast<int32>(Bodies.size()))
	{
		SelectedBodyIndex = -1;
		SelectedConstraintIndex = -1;
	}
	if (SelectedConstraintIndex >= static_cast<int32>(PhysicsAsset->GetConstraintInitDescs().size()))
	{
		SelectedConstraintIndex = -1;
	}
	ViewportClient.SyncPhysicsAssetDebugComponent(PhysicsAsset, SelectedBodyIndex, SelectedConstraintIndex);

	const FSkeletalMesh* Asset = SourceSkeletalMesh ? SourceSkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset)
	{
		ImGui::TextDisabled("No source skeletal mesh.");
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
	{
		if (Asset->Bones[Index].ParentIndex == -1)
		{
			RenderBodyTree(Asset, PhysicsAsset, Index);
		}
	}
}

bool FPhysicsAssetViewerWidget::RenderBodyTree(const FSkeletalMesh* Asset, UPhysicsAsset* PhysicsAsset, int32 BoneIndex)
{
	if (!Asset || !PhysicsAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Asset->Bones.size()))
	{
		return false;
	}

	const FBone& Bone = Asset->Bones[BoneIndex];
	const int32 BodyIndex = PhysicsAsset->FindBodyIndexByBoneName(FName(Bone.Name));
	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	const UBodySetup* Body = (BodyIndex >= 0 && BodyIndex < static_cast<int32>(Bodies.size()))
		? Bodies[BodyIndex]
		: nullptr;

	if (!Body)
	{
		bool bRenderedAny = false;
		for (int32 Index = BoneIndex + 1; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
		{
			if (Asset->Bones[Index].ParentIndex == BoneIndex &&
				RenderBodyTree(Asset, PhysicsAsset, Index))
			{
				bRenderedAny = true;
			}
		}
		return bRenderedAny;
	}

	bool bHasVisibleChildren = false;
	for (int32 Index = BoneIndex + 1; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
	{
		if (Asset->Bones[Index].ParentIndex == BoneIndex &&
			HasPhysicsBodyInSubtree(Asset, PhysicsAsset, Index))
		{
			bHasVisibleChildren = true;
			break;
		}
	}

	ImGuiTreeNodeFlags Flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen;

	if (Body && BodyIndex == SelectedBodyIndex)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (!bHasVisibleChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	FString Label = BuildPhysicsBodyTreeLabel(Bone.Name, BodyIndex, Body);
	Label += "##PhysicsAssetViewerBodyBone";
	Label += std::to_string(BoneIndex);

	const bool bOpen = ImGui::TreeNodeEx(Label.c_str(), Flags);

	if (ImGui::IsItemClicked())
	{
		SelectedBodyIndex = BodyIndex;
		SelectedConstraintIndex = -1;
		ViewportClient.SyncPhysicsAssetDebugComponent(PhysicsAsset, SelectedBodyIndex, SelectedConstraintIndex);
	}

	if (bOpen && bHasVisibleChildren)
	{
		for (int32 Index = BoneIndex + 1; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
		{
			if (Asset->Bones[Index].ParentIndex == BoneIndex)
			{
				RenderBodyTree(Asset, PhysicsAsset, Index);
			}
		}
		ImGui::TreePop();
	}

	return true;
}

void FPhysicsAssetViewerWidget::RenderBodyDetails(UPhysicsAsset* PhysicsAsset)
{
	if (RenderConstraintInitDescDetails(
			PhysicsAsset,
			SelectedConstraintIndex,
			ViewportClient.GetPhysicsAssetDebugComponent()))
	{
		if (SavePhysicsAsset(PhysicsAsset))
		{
			ClearDirty();
		}
		else
		{
			UE_LOG("PhysicsAsset constraint edit warning: failed to persist constraint. PhysicsAsset=%s", PhysicsAsset->GetAssetPathFileName().c_str());
			MarkDirty();
		}
		return;
	}

	if (SelectedConstraintIndex < 0 && RenderBodyPhysicsInfoDetails(PhysicsAsset, SelectedBodyIndex))
	{
		if (SavePhysicsAsset(PhysicsAsset))
		{
			ClearDirty();
		}
		else
		{
			UE_LOG("PhysicsAsset body physics edit warning: failed to persist body physics info. PhysicsAsset=%s", PhysicsAsset->GetAssetPathFileName().c_str());
			MarkDirty();
		}
	}
}
