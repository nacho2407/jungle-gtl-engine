#include "SkeletalMeshEditorViewerWidget.h"

#include "Component/GizmoComponent.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewer/SkeletalMeshEditorViewer.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformProxy.h"
#include "Core/ResourceManager.h"
#include "GameFramework/PrimitiveActors.h"
#include "imgui.h"

#include <algorithm>

namespace
{
	FSkeletalMeshEditorViewer* AsSkeletalMeshViewer(FEditorViewer* Viewer);
	const FSkeletalMeshEditorViewer* AsSkeletalMeshViewer(const FEditorViewer* Viewer);

	constexpr uint64 MeshEditHashOffset = 14695981039346656037ull;
	constexpr uint64 MeshEditHashPrime = 1099511628211ull;

	uint64 HashBytes(uint64 Seed, const void* Data, size_t Size);
	template <typename T> uint64 HashValue(uint64 Seed, const T& Value);
	uint64 HashString(uint64 Seed, const FString& Value);
	uint64 HashMatrix(uint64 Seed, const FMatrix& Matrix);
}

void FSkeletalMeshEditorViewerWidget::RenderContent(float DeltaTime)
{
	(void)DeltaTime;

	if (!Viewer)
	{
		return;
	}

	FSceneViewport& SceneViewport = Viewer->GetViewport();
	ID3D11ShaderResourceView* SRV = SceneViewport.GetOutSRV();
	if (!SRV)
	{
		ImGui::TextDisabled("Viewer render target is not ready.");
		return;
	}

	ImVec2 FullSize = ImGui::GetContentRegionAvail();
	const float CenterWidth = std::max(
		160.0f,
		FullSize.x - LeftPanelWidth - RightPanelWidth - (ImGui::GetStyle().ItemSpacing.x * 2.0f));

	ASkeletalMeshActor* ViewTarget = Viewer->GetViewTarget();
	USkeletalMeshComponent* SkelMeshComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
	USkeletalMesh* SkeletalMesh = SkelMeshComp ? SkelMeshComp->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* MeshData = SkeletalMesh ? SkeletalMesh->GetMeshData() : nullptr;
	CachedSkComp = SkelMeshComp;

	RenderSkeletonLeftPanel(SkelMeshComp, MeshData);

	ImGui::SameLine();
	ImGui::Button("##left_splitter", ImVec2(2.0f, -1.0f));
	if (ImGui::IsItemActive())
	{
		LeftPanelWidth += ImGui::GetIO().MouseDelta.x;
		LeftPanelWidth = std::clamp(LeftPanelWidth, 100.0f, FullSize.x * 0.4f);
	}
	ImGui::SameLine();

	RenderViewportPanel(SceneViewport, SRV, ImVec2(CenterWidth, 0.0f));

	ImGui::SameLine();
	ImGui::Button("##right_splitter", ImVec2(2.0f, -1.0f));
	if (ImGui::IsItemActive())
	{
		RightPanelWidth -= ImGui::GetIO().MouseDelta.x;
		RightPanelWidth = std::clamp(RightPanelWidth, 100.0f, FullSize.x * 0.4f);
	}
	ImGui::SameLine();

	RenderBoneRightPanel(SkelMeshComp);
}



























bool FSkeletalMeshEditorViewerWidget::CanSaveMesh() const
{
	if (!AsSkeletalMeshViewer(Viewer))
	{
		return false;
	}

	ASkeletalMeshActor* ViewTarget = Viewer->GetViewTarget();
	USkeletalMeshComponent* SkelComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
	return SkelComp && SkelComp->GetSkeletalMesh() && HasMeshAssetEdits();
}

bool FSkeletalMeshEditorViewerWidget::IsMeshDirty() const
{
	return AsSkeletalMeshViewer(Viewer) && HasMeshAssetEdits();
}

void FSkeletalMeshEditorViewerWidget::RequestSaveMesh()
{
	if (!AsSkeletalMeshViewer(Viewer))
	{
		return;
	}

	ASkeletalMeshActor* ViewTarget = Viewer->GetViewTarget();
	USkeletalMeshComponent* SkelComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
	USkeletalMesh* Mesh = SkelComp ? SkelComp->GetSkeletalMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	if (FResourceManager::Get().SaveSkeletalMesh(Mesh))
	{
		ResetMeshDirtyBaseline();
	}
}

FSkeletalMesh* FSkeletalMeshEditorViewerWidget::ResolveCurrentMeshData() const
{
	if (CachedMesh)
	{
		return CachedMesh;
	}

	if (!Viewer)
	{
		return nullptr;
	}

	ASkeletalMeshActor* ViewTarget = Viewer->GetViewTarget();
	USkeletalMeshComponent* SkelComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
	USkeletalMesh* Mesh = SkelComp ? SkelComp->GetSkeletalMesh() : nullptr;
	return Mesh ? Mesh->GetMeshData() : nullptr;
}

uint64 FSkeletalMeshEditorViewerWidget::ComputeEditableMeshSignature(const FSkeletalMesh* MeshData) const
{
	if (!MeshData)
	{
		return 0;
	}

	uint64 Hash = MeshEditHashOffset;

	Hash = HashValue(Hash, static_cast<uint64>(MeshData->Bones.size()));
	for (const FBoneInfo& Bone : MeshData->Bones)
	{
		Hash = HashString(Hash, Bone.Name);
		Hash = HashValue(Hash, Bone.ParentIndex);
		Hash = HashMatrix(Hash, Bone.LocalBindTransform);
		Hash = HashMatrix(Hash, Bone.GlobalBindTransform);
		Hash = HashMatrix(Hash, Bone.InverseBindPose);
	}

	Hash = HashValue(Hash, static_cast<uint64>(MeshData->Sockets.size()));
	for (const FSkeletalMeshSocket& Socket : MeshData->Sockets)
	{
		Hash = HashString(Hash, Socket.Name.ToString());
		Hash = HashValue(Hash, Socket.BoneIndex);
		Hash = HashValue(Hash, Socket.RelativeLocation.X);
		Hash = HashValue(Hash, Socket.RelativeLocation.Y);
		Hash = HashValue(Hash, Socket.RelativeLocation.Z);
		Hash = HashValue(Hash, Socket.RelativeRotation.Pitch);
		Hash = HashValue(Hash, Socket.RelativeRotation.Yaw);
		Hash = HashValue(Hash, Socket.RelativeRotation.Roll);
		Hash = HashValue(Hash, Socket.RelativeScale.X);
		Hash = HashValue(Hash, Socket.RelativeScale.Y);
		Hash = HashValue(Hash, Socket.RelativeScale.Z);
	}

	return Hash;
}

void FSkeletalMeshEditorViewerWidget::ResetMeshDirtyBaseline()
{
	FSkeletalMesh* MeshData = ResolveCurrentMeshData();
	if (!MeshData)
	{
		CleanMeshEditSignature = 0;
		bHasCleanMeshEditSignature = false;
		bMeshDirty = false;
		return;
	}

	CleanMeshEditSignature = ComputeEditableMeshSignature(MeshData);
	bHasCleanMeshEditSignature = true;
	bMeshDirty = false;
}

bool FSkeletalMeshEditorViewerWidget::HasMeshAssetEdits() const
{
	FSkeletalMesh* MeshData = ResolveCurrentMeshData();
	if (!MeshData)
	{
		return false;
	}

	if (bMeshDirty)
	{
		return true;
	}

	return bHasCleanMeshEditSignature && ComputeEditableMeshSignature(MeshData) != CleanMeshEditSignature;
}

void FSkeletalMeshEditorViewerWidget::RenderSkeletonLeftPanel(USkeletalMeshComponent* SkelMeshComp, FSkeletalMesh* MeshData)
{
	ImGui::BeginChild("SkeletonPanel", ImVec2(LeftPanelWidth, 0), true);
	ImGui::Text("Skeleton");

	if (!MeshData)
	{
		CachedMesh = nullptr;
		Children.clear();
		BoneToSocketIndices.clear();
		if (Viewer)
		{
			AsSkeletalMeshViewer(Viewer)->ClearSelection();
		}
		ResetMeshDirtyBaseline();
		ImGui::TextDisabled("No skeletal mesh");
	}
	else if (CachedMesh != MeshData)
	{
		CachedMesh = MeshData;
		if (Viewer)
		{
			AsSkeletalMeshViewer(Viewer)->ClearSelection();
		}

		RebuildBoneTreeCaches(MeshData);
		ResetMeshDirtyBaseline();
	}

	if (MeshData)
	{
		ApplyPendingBoneTreeOpenState(MeshData);
		for (int32 j = 0; j < MeshData->Bones.size(); ++j)
		{
			if (MeshData->Bones[j].ParentIndex == -1)
			{
				DrawBoneNode(j, MeshData->Bones, Children);
			}
		}
	}

	if (PendingPreviewPickerSocketIdx >= 0 && !ImGui::IsPopupOpen("PickStaticMesh"))
	{
		ImGui::OpenPopup("PickStaticMesh");
	}
	DrawPreviewPickerModal();

	if (RenameSocketIdx >= 0 && !ImGui::IsPopupOpen("RenameSocket"))
	{
		ImGui::OpenPopup("RenameSocket");
	}
	DrawRenameModal();

	ImGui::Separator();
	DrawSocketInspector();

	ImGui::EndChild();
}

void FSkeletalMeshEditorViewerWidget::RenderBoneRightPanel(USkeletalMeshComponent* SkelMeshComp)
{
	ImGui::BeginChild("BoneDetailsPanel", ImVec2(RightPanelWidth, 0), true);
	ImGui::Text("Details");
	ImGui::Separator();
	if (AsSkeletalMeshViewer(Viewer)->GetSelectedBoneIndex() != -1 && SkelMeshComp)
	{
		RenderBoneDetails(SkelMeshComp);
	}
	else if (AsSkeletalMeshViewer(Viewer)->GetSelectedSocketIndex() != -1 && SkelMeshComp)
	{
		if (CachedMesh && AsSkeletalMeshViewer(Viewer)->GetSelectedSocketIndex() < (int32)CachedMesh->Sockets.size())
		{
			ImGui::Text("Socket: %s", CachedMesh->Sockets[AsSkeletalMeshViewer(Viewer)->GetSelectedSocketIndex()].Name.ToString().c_str());
			ImGui::Separator();
			ImGui::Text("Selected Socket for transformation.");
		}
	}
	else
	{
		ImGui::TextDisabled("No bone or socket selected.");
	}
	ImGui::EndChild();
}

void FSkeletalMeshEditorViewerWidget::RenderBoneDetails(USkeletalMeshComponent* SkelComp)
{
	const int32 SelectedBoneIndex = Viewer ? AsSkeletalMeshViewer(Viewer)->GetSelectedBoneIndex() : -1;
	if (!SkelComp || SelectedBoneIndex == -1) return;

	const FBoneInfo& Bone = SkelComp->GetSkeletalMesh()->GetMeshData()->Bones[SelectedBoneIndex];
	ImGui::Text("Bone: %s (Index: %d)", Bone.Name.c_str(), SelectedBoneIndex);
	ImGui::Spacing();

	FMatrix LocalTransform = SkelComp->GetBoneLocalTransform(SelectedBoneIndex);
	FVector Location, Scale;
	FMatrix RotationMatrix;
	LocalTransform.Decompose(Location, RotationMatrix, Scale);

	// 외부(기즈모 등)에서 회전이 변경되었는지 확인
	FVector CurrentEuler = RotationMatrix.GetEuler();
	FVector& CachedRotation = AsSkeletalMeshViewer(Viewer)->GetCachedBoneRotation();

	if ((CurrentEuler - FMatrix::MakeRotationEuler(CachedRotation).GetEuler()).Size() > 0.01f)
	{
		CachedRotation = CurrentEuler;
	}

	bool bEdited = false;

	auto DrawTransformField = [&](const char* Label, FVector& Value, float Speed) {
		float Arr[3] = { Value.X, Value.Y, Value.Z };
		if (ImGui::DragFloat3(Label, Arr, Speed))
		{
			Value = FVector(Arr[0], Arr[1], Arr[2]);
			return true;
		}
		return false;
	};

	ImGui::Text("Transform (Local)");
	if (DrawTransformField("Location", Location, 0.1f)) bEdited = true;
	if (DrawTransformField("Rotation", CachedRotation, 0.1f)) bEdited = true;
	if (DrawTransformField("Scale", Scale, 0.01f)) bEdited = true;

	if (bEdited)
	{
		FMatrix NewLocal = FMatrix::MakeTRS(Location, FMatrix::MakeRotationEuler(CachedRotation), Scale);
		SkelComp->SetBoneLocalTransform(SelectedBoneIndex, NewLocal);

		// Gizmo 위치 업데이트
		FViewportClient* BaseClient = Viewer->GetViewport().GetClient();
		FEditorViewportClient* EditorClient = static_cast<FEditorViewportClient*>(BaseClient);
		if (UGizmoComponent* Gizmo = EditorClient->GetGizmo())
		{
			Gizmo->UpdateGizmoTransform();
		}
	}
}

void FSkeletalMeshEditorViewerWidget::DrawBoneNode(int32 BoneIndex, const TArray<FBoneInfo>& Bones, const TArray<TArray<int32>>& Children)
{
	const FBoneInfo& Bone = Bones[BoneIndex];

	// socket까지 자식으로 그리므로 "자식 없음"은 bone-children + socket-children 모두 비어야 성립.
	const bool bHasBoneChildren   = Children[BoneIndex].size() > 0;
	const bool bHasSocketChildren = BoneIndex < static_cast<int32>(BoneToSocketIndices.size())
									&& BoneToSocketIndices[BoneIndex].size() > 0;

	ImGuiTreeNodeFlags Flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanAvailWidth;

	if (!bHasBoneChildren && !bHasSocketChildren)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf;
	}

	if (AsSkeletalMeshViewer(Viewer)->GetSelectedBoneIndex() == BoneIndex)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	bool bOpen = ImGui::TreeNodeEx(
		(void*)(intptr_t)BoneIndex,
		Flags,
		"%s",
		Bone.Name.c_str());

	// 클릭 → bone 선택. socket 선택은 해제 (상호 배타).
	if (ImGui::IsItemClicked())
	{
		AsSkeletalMeshViewer(Viewer)->SelectBone(BoneIndex);
	}

	// 우클릭 컨텍스트
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Add Socket"))
		{
			AddSocketOnBone(BoneIndex);
		}
		ImGui::Separator();

		const bool bCanToggleChildren = bHasBoneChildren || bHasSocketChildren;
		if (ImGui::MenuItem("Expand Children", nullptr, false, bCanToggleChildren))
		{
			QueueBoneSubtreeOpenState(BoneIndex, true);
		}
		if (ImGui::MenuItem("Collapse Children", nullptr, false, bCanToggleChildren))
		{
			QueueBoneSubtreeOpenState(BoneIndex, false);
		}
		ImGui::EndPopup();
	}

	if (bOpen)
	{
		// (1) 자식 bone들
		for (int32 ChildIndex : Children[BoneIndex])
		{
			DrawBoneNode(ChildIndex, Bones, Children);
		}

		// (2) 이 bone에 매달린 socket들 (자식 bone 다음에 표시)
		if (bHasSocketChildren)
		{
			for (int32 SocketIdx : BoneToSocketIndices[BoneIndex])
			{
				DrawSocketNode(SocketIdx);
			}
		}

		ImGui::TreePop();
	}
}

void FSkeletalMeshEditorViewerWidget::QueueBoneSubtreeOpenState(int32 BoneIdx, bool bOpen)
{
	PendingBoneTreeOpenStateRoot = BoneIdx;
	bPendingBoneTreeOpenStateValue = bOpen;
}

void FSkeletalMeshEditorViewerWidget::ApplyPendingBoneTreeOpenState(const FSkeletalMesh* MeshData)
{
	if (!MeshData || PendingBoneTreeOpenStateRoot < 0)
	{
		return;
	}

	SetBoneSubtreeOpenState(PendingBoneTreeOpenStateRoot, Children, bPendingBoneTreeOpenStateValue);
	PendingBoneTreeOpenStateRoot = -1;
}

void FSkeletalMeshEditorViewerWidget::SetBoneSubtreeOpenState(
	int32 BoneIdx,
	const TArray<TArray<int32>>& InChildren,
	bool bOpen)
{
	if (BoneIdx < 0 || BoneIdx >= static_cast<int32>(InChildren.size()))
	{
		return;
	}

	ImGuiStorage* Storage = ImGui::GetStateStorage();
	if (!Storage)
	{
		return;
	}

	const void* NodePtr = reinterpret_cast<void*>(static_cast<intptr_t>(BoneIdx));
	const ImGuiID NodeId = ImGui::GetID(NodePtr);

	// Expand는 부모가 먼저 열려야 화면에서 즉시 전체 subtree가 보인다.
	if (bOpen)
	{
		Storage->SetInt(NodeId, 1);
	}

	ImGui::PushID(NodePtr);
	for (int32 ChildIndex : InChildren[BoneIdx])
	{
		SetBoneSubtreeOpenState(ChildIndex, InChildren, bOpen);
	}
	ImGui::PopID();

	// Collapse는 자식부터 닫고 마지막에 부모를 닫아야,
	// 부모를 다시 열었을 때 이전에 열려 있던 하위 노드가 되살아나지 않는다.
	if (!bOpen)
	{
		Storage->SetInt(NodeId, 0);
	}
}

void FSkeletalMeshEditorViewerWidget::DrawSocketNode(int32 SocketIdx)
{
	if (!CachedMesh) return;
	if (SocketIdx < 0 || SocketIdx >= static_cast<int32>(CachedMesh->Sockets.size())) return;

	const FSkeletalMeshSocket& Socket = CachedMesh->Sockets[SocketIdx];

	ImGuiTreeNodeFlags Flags =
		ImGuiTreeNodeFlags_Leaf |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_NoTreePushOnOpen;   // leaf니까 자식 push 불필요

	if (Viewer && AsSkeletalMeshViewer(Viewer)->GetSelectedSocketIndex() == SocketIdx)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	// bone ID 공간(int32 직접)과 충돌하지 않게 high-bit 네임스페이스.
	const void* NodeId = reinterpret_cast<const void*>(
		static_cast<uintptr_t>(0x80000000u | static_cast<uint32>(SocketIdx)));

	// socket을 시각적으로 구분 — cyan-ish, "◇" prefix
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 1.0f, 1.0f));
	ImGui::TreeNodeEx(NodeId, Flags, "\xe2\x97\x87 %s", Socket.Name.ToString().c_str());   // ◇
	ImGui::PopStyleColor();

	// 클릭 → socket 선택. bone 선택은 해제.
	if (ImGui::IsItemClicked())
	{
		if (Viewer)
		{
			AsSkeletalMeshViewer(Viewer)->SelectSocket(SocketIdx);
		}
	}

	// 우클릭 컨텍스트
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Add Preview Mesh..."))
		{
			// 모달은 popup 바깥에서 OpenPopup해야 안정적 — 여기선 트리거 idx만 기록.
			PendingPreviewPickerSocketIdx = SocketIdx;
		}

		const bool bHasPreview = HasPreview(Socket.Name);
		if (ImGui::MenuItem("Remove Preview Mesh", nullptr, false, bHasPreview))
		{
			if (EditorEngine)
			{
				AsSkeletalMeshViewer(Viewer)->ClearSocketPreview(Socket.Name);
			}
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Rename"))
		{
			RenameSocketIdx = SocketIdx;
			std::snprintf(RenameBuffer, sizeof(RenameBuffer), "%s",
						  Socket.Name.ToString().c_str());
		}

		if (ImGui::MenuItem("Delete Socket"))
		{
			DeleteSocket(SocketIdx);
		}

		ImGui::EndPopup();
	}
}

void FSkeletalMeshEditorViewerWidget::RebuildBoneTreeCaches(const FSkeletalMesh* MeshData)
{
	Children.clear();
	BoneToSocketIndices.clear();
	if (!MeshData) return;

	const int32 BoneCount = static_cast<int32>(MeshData->Bones.size());
	Children.resize(BoneCount);

	for (int32 i = 0; i < BoneCount; ++i)
	{
		const int32 Parent = MeshData->Bones[i].ParentIndex;
		if (Parent >= 0)
		{
			Children[Parent].push_back(i);
		}
	}

	RebuildBoneToSocketIndices(MeshData);
}

void FSkeletalMeshEditorViewerWidget::RebuildBoneToSocketIndices(const FSkeletalMesh* MeshData)
{
	BoneToSocketIndices.clear();
	if (!MeshData) return;

	const int32 BoneCount = static_cast<int32>(MeshData->Bones.size());
	BoneToSocketIndices.resize(BoneCount);

	for (int32 i = 0; i < static_cast<int32>(MeshData->Sockets.size()); ++i)
	{
		const int32 B = MeshData->Sockets[i].BoneIndex;
		if (B >= 0 && B < BoneCount)
		{
			BoneToSocketIndices[B].push_back(i);
		}
	}
}

void FSkeletalMeshEditorViewerWidget::AddSocketOnBone(int32 BoneIdx)
{
	if (!CachedMesh) return;
	if (BoneIdx < 0 || BoneIdx >= static_cast<int32>(CachedMesh->Bones.size())) return;

	FSkeletalMeshSocket NewSocket;
	NewSocket.Name = FName(GenerateUniqueSocketName());
	NewSocket.BoneIndex = BoneIdx;
	// Loc/Rot/Scale은 기본값(0, identity, 1)

	CachedMesh->Sockets.push_back(NewSocket);
	const int32 NewIdx = static_cast<int32>(CachedMesh->Sockets.size()) - 1;

	RebuildBoneToSocketIndices(CachedMesh);

	if (Viewer)
	{
		AsSkeletalMeshViewer(Viewer)->SelectSocket(NewIdx);
	}
	bMeshDirty = true;

	// socket-attached children의 transform이 새로 계산되도록 본 자세 dirty 전파 트리거.
	if (CachedSkComp)
	{
		CachedSkComp->MarkSkinningDirty();
	}
}

FString FSkeletalMeshEditorViewerWidget::GenerateUniqueSocketName(const char* Base) const
{
	if (!CachedMesh) return FString(Base);

	auto Exists = [&](const FString& Candidate) -> bool {
		const FName CandidateName(Candidate);
		for (const FSkeletalMeshSocket& S : CachedMesh->Sockets)
		{
			if (S.Name == CandidateName) return true;
		}
		return false;
	};

	FString Candidate = Base;
	if (!Exists(Candidate)) return Candidate;

	for (int32 i = 1; i < 10000; ++i)
	{
		Candidate = FString(Base) + "_" + std::to_string(i);
		if (!Exists(Candidate)) return Candidate;
	}
	return Candidate;   // 폴백 — 거의 도달 불가
}

void FSkeletalMeshEditorViewerWidget::DeleteSocket(int32 SocketIdx)
{
	if (!CachedMesh) return;
	if (SocketIdx < 0 || SocketIdx >= static_cast<int32>(CachedMesh->Sockets.size())) return;

	// (1) 해당 socket에 매달린 preview mesh 먼저 정리
	const FName SocketName = CachedMesh->Sockets[SocketIdx].Name;
	if (EditorEngine && Viewer)
	{
		AsSkeletalMeshViewer(Viewer)->ClearSocketPreview(SocketName);
	}

	// (2) Sockets 배열에서 erase. 다른 socket들의 인덱스가 시프트됨.
	CachedMesh->Sockets.erase(CachedMesh->Sockets.begin() + SocketIdx);

	// (3) BoneToSocketIndices 통째 재빌드 (시프트된 인덱스 반영)
	RebuildBoneToSocketIndices(CachedMesh);

	// (4) 선택 상태 정리
	if (Viewer)
	{
		AsSkeletalMeshViewer(Viewer)->NotifySocketDeleted(SocketIdx);
	}

	bMeshDirty = true;

	if (CachedSkComp)
	{
		CachedSkComp->MarkSkinningDirty();
	}
}

bool FSkeletalMeshEditorViewerWidget::HasPreview(const FName& SocketName) const
{
	if (!EditorEngine || !Viewer) return false;
	return AsSkeletalMeshViewer(Viewer)->FindPreviewMesh(SocketName) != nullptr;
}

void FSkeletalMeshEditorViewerWidget::DrawSocketInspector()
{
	// Save 상태는 socket 선택 여부와 무관하게 항상 보이는 게 편함.
	auto DrawSaveButton = [&]() {
		const bool bCanSave = CanSaveMesh();
		if (!bCanSave) ImGui::BeginDisabled();
		const char* Label = IsMeshDirty() ? "Save Mesh *" : "Save Mesh";
		if (ImGui::Button(Label))
		{
			TriggerSaveMesh();
		}
		if (!bCanSave) ImGui::EndDisabled();
	};

	const int32 SelectedSocketIndex = Viewer ? AsSkeletalMeshViewer(Viewer)->GetSelectedSocketIndex() : -1;
	if (!CachedMesh || SelectedSocketIndex < 0 ||
		SelectedSocketIndex >= static_cast<int32>(CachedMesh->Sockets.size()))
	{
		ImGui::TextDisabled("(no socket selected)");
		DrawSaveButton();
		return;
	}

	FSkeletalMeshSocket& Socket = CachedMesh->Sockets[SelectedSocketIndex];

	ImGui::Text("Socket: %s", Socket.Name.ToString().c_str());

	// Bone 콤보
	const TArray<FBoneInfo>& Bones = CachedMesh->Bones;
	const char* CurrentBoneName = (Socket.BoneIndex >= 0 && Socket.BoneIndex < (int32)Bones.size())
		? Bones[Socket.BoneIndex].Name.c_str()
		: "<invalid>";

	if (ImGui::BeginCombo("Bone", CurrentBoneName))
	{
		for (int32 i = 0; i < static_cast<int32>(Bones.size()); ++i)
		{
			const bool bSelected = (Socket.BoneIndex == i);
			if (ImGui::Selectable(Bones[i].Name.c_str(), bSelected))
			{
				if (Socket.BoneIndex != i)
				{
					Socket.BoneIndex = i;
					RebuildBoneToSocketIndices(CachedMesh);   // 트리에서 새 본 밑으로 이동
					bMeshDirty = true;
					if (CachedSkComp) CachedSkComp->MarkSkinningDirty();
				}
			}
			if (bSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	// Location / Rotation / Scale
	// FVector / FRotator 모두 contiguous 3 float — &X / &Pitch로 DragFloat3에 전달.
	bool bChanged = false;
	bChanged |= ImGui::DragFloat3("Location", &Socket.RelativeLocation.X, 0.5f);
	bChanged |= ImGui::DragFloat3("Rotation (P/Y/R)", &Socket.RelativeRotation.Pitch, 0.5f);
	bChanged |= ImGui::DragFloat3("Scale", &Socket.RelativeScale.X, 0.01f, 0.001f, 100.0f);

	if (bChanged)
	{
		bMeshDirty = true;
		if (CachedSkComp) CachedSkComp->MarkSkinningDirty();
	}

	ImGui::Separator();
	DrawSaveButton();
}

void FSkeletalMeshEditorViewerWidget::TriggerSaveMesh()
{
	RequestSaveMesh();
}

bool FSkeletalMeshEditorViewerWidget::IsSocketNameUnique(const FString& Candidate, int32 IgnoreIdx) const
{
	if (!CachedMesh) return false;
	const FName CandidateName(Candidate);
	for (int32 i = 0; i < static_cast<int32>(CachedMesh->Sockets.size()); ++i)
	{
		if (i == IgnoreIdx) continue;
		if (CachedMesh->Sockets[i].Name == CandidateName) return false;
	}
	return true;
}

void FSkeletalMeshEditorViewerWidget::DrawRenameModal()
{
	if (!ImGui::BeginPopupModal("RenameSocket", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	// 무효한 상태 — 즉시 닫기
	if (!CachedMesh || RenameSocketIdx < 0 ||
		RenameSocketIdx >= static_cast<int32>(CachedMesh->Sockets.size()))
	{
		RenameSocketIdx = -1;
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	ImGui::Text("Rename socket:");
	ImGui::InputText("##rename", RenameBuffer, sizeof(RenameBuffer));

	const FString Candidate(RenameBuffer);
	const bool bEmpty  = Candidate.empty();
	const bool bUnique = !bEmpty && IsSocketNameUnique(Candidate, RenameSocketIdx);
	const bool bValid  = !bEmpty && bUnique;

	if (bEmpty)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Name cannot be empty");
	}
	else if (!bUnique)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Name already in use");
	}

	if (!bValid) ImGui::BeginDisabled();
	if (ImGui::Button("OK"))
	{
		// Preview mesh가 이 socket에 attach되어 있다면 key가 socket name이므로,
		// 이름 변경 시 preview를 깔끔히 재attach해야 함.
		const FName OldName = CachedMesh->Sockets[RenameSocketIdx].Name;
		const FName NewName(Candidate);

		FString PreviewPath;
		if (EditorEngine && Viewer)
		{
			UStaticMeshComponent* Preview = AsSkeletalMeshViewer(Viewer)->FindPreviewMesh(OldName);
			if (Preview && Preview->GetStaticMesh())
			{
				PreviewPath = Preview->GetStaticMesh()->GetAssetPathFileName();
				AsSkeletalMeshViewer(Viewer)->ClearSocketPreview(OldName);
			}
		}

		CachedMesh->Sockets[RenameSocketIdx].Name = NewName;

		if (!PreviewPath.empty() && EditorEngine && Viewer)
		{
			AsSkeletalMeshViewer(Viewer)->SetSocketPreviewMesh(NewName, PreviewPath);
		}

		if (Viewer && AsSkeletalMeshViewer(Viewer)->GetSelectedSocketIndex() == RenameSocketIdx)
		{
			AsSkeletalMeshViewer(Viewer)->SelectSocket(RenameSocketIdx);
		}

		bMeshDirty = true;
		if (CachedSkComp) CachedSkComp->MarkSkinningDirty();
		RenameSocketIdx = -1;
		ImGui::CloseCurrentPopup();
	}
	if (!bValid) ImGui::EndDisabled();

	ImGui::SameLine();

	if (ImGui::Button("Cancel"))
	{
		RenameSocketIdx = -1;
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void FSkeletalMeshEditorViewerWidget::DrawPreviewPickerModal()
{
	if (!ImGui::BeginPopupModal("PickStaticMesh", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	static char Filter[256] = "";
	ImGui::InputText("Filter", Filter, sizeof(Filter));
	ImGui::Separator();

	const TArray<FString>& Paths = FResourceManager::Get().GetStaticMeshPaths();

	ImGui::BeginChild("PickList", ImVec2(420.0f, 300.0f), true);
	for (const FString& Path : Paths)
	{
		if (Filter[0] != '\0' && Path.find(Filter) == FString::npos)
		{
			continue;
		}

		if (ImGui::Selectable(Path.c_str()))
		{
			if (CachedMesh && EditorEngine && Viewer &&
				PendingPreviewPickerSocketIdx >= 0 &&
				PendingPreviewPickerSocketIdx < static_cast<int32>(CachedMesh->Sockets.size()))
			{
				const FName SocketName = CachedMesh->Sockets[PendingPreviewPickerSocketIdx].Name;
				AsSkeletalMeshViewer(Viewer)->SetSocketPreviewMesh(SocketName, Path);
			}
			PendingPreviewPickerSocketIdx = -1;
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::EndChild();

	if (ImGui::Button("Cancel"))
	{
		PendingPreviewPickerSocketIdx = -1;
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

namespace
{
	FSkeletalMeshEditorViewer* AsSkeletalMeshViewer(FEditorViewer* Viewer)
	{
		return Viewer && Viewer->GetTabKind() == EEditorTabKind::SkeletalMeshViewer
			? static_cast<FSkeletalMeshEditorViewer*>(Viewer)
			: nullptr;
	}

	const FSkeletalMeshEditorViewer* AsSkeletalMeshViewer(const FEditorViewer* Viewer)
	{
		return Viewer && Viewer->GetTabKind() == EEditorTabKind::SkeletalMeshViewer
			? static_cast<const FSkeletalMeshEditorViewer*>(Viewer)
			: nullptr;
	}

	uint64 HashBytes(uint64 Seed, const void* Data, size_t Size)
	{
		const unsigned char* Bytes = static_cast<const unsigned char*>(Data);
		for (size_t Index = 0; Index < Size; ++Index)
		{
			Seed ^= static_cast<uint64>(Bytes[Index]);
			Seed *= MeshEditHashPrime;
		}
		return Seed;
	}

	template <typename T>
	uint64 HashValue(uint64 Seed, const T& Value)
	{
		return HashBytes(Seed, &Value, sizeof(T));
	}

	uint64 HashString(uint64 Seed, const FString& Value)
	{
		const uint64 Length = static_cast<uint64>(Value.size());
		Seed = HashValue(Seed, Length);
		return Value.empty() ? Seed : HashBytes(Seed, Value.data(), Value.size());
	}

	uint64 HashMatrix(uint64 Seed, const FMatrix& Matrix)
	{
		return HashBytes(Seed, Matrix.M, sizeof(Matrix.M));
	}
}