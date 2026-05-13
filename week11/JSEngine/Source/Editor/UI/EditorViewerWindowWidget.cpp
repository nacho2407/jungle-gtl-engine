#include "EditorViewerWindowWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewer/EditorViewer.h"
#include "Viewport/ViewportLayout.h"
#include "GameFramework/PrimitiveActors.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/ResourceManager.h"
#include "Component/GizmoComponent.h"
#include "Component/TransformProxy.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace
{
void SetOpaqueBlendStateCallback(const ImDrawList*, const ImDrawCmd* Cmd)
{
    ID3D11DeviceContext* DeviceContext = static_cast<ID3D11DeviceContext*>(Cmd->UserCallbackData);
    if (!DeviceContext)
        return;

    const float BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xffffffff);
}

bool UsesAbsoluteImGuiCoordinates()
{
    return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
}

POINT ImGuiScreenToClientPoint(FWindowsWindow* Window, const ImVec2& Point)
{
    POINT Result =
    {
        static_cast<LONG>(std::lround(Point.x)),
        static_cast<LONG>(std::lround(Point.y))
    };
    if (Window && Window->GetHWND() && UsesAbsoluteImGuiCoordinates())
    {
        ::ScreenToClient(Window->GetHWND(), &Result);
    }
    return Result;
}

FString GetBaseFileNameWithoutExtension(const FString& Path)
{
    if (Path.empty())
    {
        return "Viewer";
    }

    const size_t SlashPos = Path.find_last_of("/\\");
    const size_t NameBegin = SlashPos == FString::npos ? 0 : SlashPos + 1;
    FString Name = Path.substr(NameBegin);

    const size_t DotPos = Name.find_last_of('.');
    if (DotPos != FString::npos && DotPos > 0)
    {
        Name = Name.substr(0, DotPos);
    }

    return Name.empty() ? "Viewer" : Name;
}

FString GetViewerAssetLabel(FEditorViewer* Viewer)
{
    return Viewer ? GetBaseFileNameWithoutExtension(Viewer->GetFileName()) : FString("Viewer");
}
}

void FEditorViewerWindowWidget::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorWidget::Initialize(InEditorEngine);
}

void FEditorViewerWindowWidget::Shutdown()
{
    Children.clear();
    BoneToSocketIndices.clear();
    CachedMesh = nullptr;
    CachedSkComp = nullptr; 

    PendingPreviewPickerSocketIdx = -1; 
    RenameSocketIdx = -1;               
    bMeshDirty = false; 

    Viewer = nullptr;
    bOpen = false;
}

FString FEditorViewerWindowWidget::GetWindowName() const
{
    char WindowName[64];
    sprintf_s(WindowName, "###ViewerWindow_%p", Viewer);
    return GetViewerAssetLabel(Viewer) + WindowName;
}

bool FEditorViewerWindowWidget::CanSaveMesh() const
{
	if (!Viewer)
	{
		return false;
	}

	ASkeletalMeshActor* ViewTarget = Viewer->GetViewTarget();
	USkeletalMeshComponent* SkelComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
	return SkelComp && SkelComp->GetSkeletalMesh();
}

void FEditorViewerWindowWidget::RequestSaveMesh()
{
	if (!Viewer)
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
		bMeshDirty = false;
	}
}

void FEditorViewerWindowWidget::Render(float DeltaTime)
{
    if (!bOpen)
        return;

    if (!EditorEngine)
        return;

	if (!Viewer)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	FString WindowName = GetWindowName();
	bool bDockRequested = false;
	bool bCloseRequested = false;

	// Make the viewer window reasonably large on first creation.
	ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
	constexpr ImGuiWindowFlags WindowFlags =
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoCollapse;
	if (ImGui::Begin(WindowName.c_str(), &bOpen, WindowFlags))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Save Mesh", "Ctrl+S"))
				{
					RequestSaveMesh();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Close"))
				{
					bCloseRequested = true;
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
				ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, false);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Asset"))
			{
				if (ImGui::MenuItem("Save Mesh"))
				{
					RequestSaveMesh();
				}
				ImGui::MenuItem("Reimport Mesh", nullptr, false, false);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window"))
			{
				if (ImGui::MenuItem("Dock Back"))
				{
					bDockRequested = true;
				}
				if (ImGui::MenuItem("Close"))
				{
					bCloseRequested = true;
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Tools"))
			{
				FSkeletalViewerShowFlags& ShowFlags = Viewer->GetClient().GetShowFlags();
				ImGui::MenuItem("Bones", nullptr, &ShowFlags.bShowBones);
				ImGui::MenuItem("Bounding Box", nullptr, &ShowFlags.bShowBoundingBox);
				ImGui::MenuItem("Outline", nullptr, &ShowFlags.bShowOutline);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help"))
			{
				ImGui::TextDisabled("Skeletal Mesh Previewer");
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		constexpr ImGuiWindowFlags StripFlags =
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse;
		ImGui::BeginChild("##DetachedViewerTabStrip", ImVec2(0.0f, 32.0f), false, StripFlags);
		ImGui::SetCursorPos(ImVec2(10.0f, 5.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.12f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.20f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.17f, 0.22f, 1.0f));
		const FString AssetLabel = GetViewerAssetLabel(Viewer);
		char TabLabel[256];
		snprintf(TabLabel, sizeof(TabLabel), "  Mesh  %s  x  ##DetachedViewerAssetTab", AssetLabel.c_str());
		if (ImGui::Button(TabLabel, ImVec2(std::min(280.0f, ImGui::CalcTextSize(TabLabel).x + 22.0f), 24.0f)))
		{
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", Viewer->GetFileName().c_str());
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
		{
			bCloseRequested = true;
		}
		const ImVec2 TabMin = ImGui::GetItemRectMin();
		const ImVec2 TabMax = ImGui::GetItemRectMax();
		const ImVec2 CloseMin(TabMax.x - 21.0f, TabMin.y + 3.0f);
		const ImVec2 CloseMax(TabMax.x - 5.0f, TabMin.y + 19.0f);
		if (ImGui::IsMouseHoveringRect(CloseMin, CloseMax) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			bCloseRequested = true;
		}
		ImGui::PopStyleColor(3);
		ImGui::EndChild();

		constexpr ImGuiWindowFlags ToolbarFlags =
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse;
		ImGui::BeginChild("##DetachedViewerToolbar", ImVec2(0.0f, 40.0f), false, ToolbarFlags);
		ImGui::SetCursorPos(ImVec2(8.0f, 6.0f));
		if (ImGui::Button("Save"))
		{
			RequestSaveMesh();
		}
		ImGui::SameLine();
		if (ImGui::Button("Dock"))
		{
			bDockRequested = true;
		}
		ImGui::SameLine(0.0f, 12.0f);
		EditorEngine->GetMainPanel().RenderViewerToolbarControls(Viewer);
		ImGui::EndChild();

		RenderContent(DeltaTime);
	}
	ImGui::End();

    ImGui::PopStyleVar();

	if (bDockRequested)
	{
		EditorEngine->GetMainPanel().RequestDockViewer(Viewer);
		return;
	}
	if (bCloseRequested)
	{
		bOpen = false;
	}

	if (!bOpen)
    {
        EditorEngine->RemoveViewer(Viewer);
        Shutdown();
    }
}

void FEditorViewerWindowWidget::RenderEmbedded(float DeltaTime)
{
	if (!bOpen || !EditorEngine || !Viewer)
	{
		return;
	}

	RenderContent(DeltaTime);
}

void FEditorViewerWindowWidget::RenderContent(float DeltaTime)
{
	(void)DeltaTime;

	FSceneViewport& SceneViewport = Viewer->GetViewport();

	ID3D11ShaderResourceView* SRV = SceneViewport.GetOutSRV();

    if (!SRV)
	{
		ImGui::TextDisabled("Viewer render target is not ready.");
        return;
	}

	ImVec2 FullSize = ImGui::GetContentRegionAvail();

		float CenterWidth = std::max(160.0f, FullSize.x - LeftPanelWidth - RightPanelWidth - (ImGui::GetStyle().ItemSpacing.x * 2.0f));

		// =====================================================
		// LEFT: Skeleton Tree
		// =====================================================
		ImGui::BeginChild("SkeletonPanel", ImVec2(LeftPanelWidth, 0), true);

		ImGui::Text("Skeleton");

		ASkeletalMeshActor* ViewTarget = Viewer->GetViewTarget();
		USkeletalMeshComponent* SkelMeshComp = ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
		USkeletalMesh* SkeletalMesh = SkelMeshComp ? SkelMeshComp->GetSkeletalMesh() : nullptr;
		FSkeletalMesh* MeshData = SkeletalMesh ? SkeletalMesh->GetMeshData() : nullptr;

		// 헬퍼들이 참조할 transient 캐시 (Render 호출 범위에서만 유효)
		CachedSkComp = SkelMeshComp;

		if (!MeshData)
		{
			CachedMesh = nullptr;
			Children.clear();
			BoneToSocketIndices.clear();
			if (Viewer)
			{
				Viewer->ClearSelection();
			}
			bMeshDirty = false;
			ImGui::TextDisabled("No skeletal mesh");
		}
		else if (CachedMesh != MeshData)
		{
			CachedMesh = MeshData;
			if (Viewer)
			{
				Viewer->ClearSelection();
			}
			bMeshDirty = false;

			RebuildBoneTreeCaches(MeshData);
		}

		if (MeshData)
		{
			for (int32 j = 0; j < MeshData->Bones.size(); ++j)
			{
				if (MeshData->Bones[j].ParentIndex == -1)
				{
					DrawBoneNode(j, MeshData->Bones, Children);
				}
			}
		}

		// 컨텍스트 메뉴에서 PendingPreviewPickerSocketIdx가 셋되면 모달 오픈.
		// (BeginPopupContextItem 안에서 OpenPopup하지 않고 *바깥*에서 하는 것이 안전 — popup 스택 충돌 방지)
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

		// 선택된 socket의 properties 편집기 + Save 버튼 (좌측 패널 하단)
		ImGui::Separator();
		DrawSocketInspector();

		ImGui::EndChild();

        // Left Splitter
        ImGui::SameLine();
        ImGui::Button("##left_splitter", ImVec2(2.0f, -1.0f));
        if (ImGui::IsItemActive())
        {
            LeftPanelWidth += ImGui::GetIO().MouseDelta.x;
            LeftPanelWidth = std::clamp(LeftPanelWidth, 100.0f, FullSize.x * 0.4f);
        }
        ImGui::SameLine();

		// =====================================================
		// CENTER: Viewport
		// =====================================================
		ImGui::BeginChild("ViewportPanel", ImVec2(CenterWidth, 0), false);

		ImVec2 Size = ImGui::GetContentRegionAvail();

		Size.x = std::max(Size.x, 1.0f);
		Size.y = std::max(Size.y, 1.0f);

		ImGui::Dummy(Size);
        ImVec2 Min = ImGui::GetItemRectMin();
        ImVec2 Max = ImGui::GetItemRectMax();
		const POINT ClientMin = ImGuiScreenToClientPoint(EditorEngine ? EditorEngine->GetWindow() : nullptr, Min);
		const bool bViewportHovered = ImGui::IsItemHovered();
		const bool bViewportClicked =
			bViewportHovered &&
			(ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
			 ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
			 ImGui::IsMouseClicked(ImGuiMouseButton_Middle));

		FViewportRect NewRect;
        NewRect.X = (int32)ClientMin.x;
        NewRect.Y = (int32)ClientMin.y;
        NewRect.Width = (int32)(Max.x - Min.x);
        NewRect.Height = (int32)(Max.y - Min.y);

		SceneViewport.SetRect(NewRect);

		if (auto* Client = SceneViewport.GetClient())
		{
			Client->SetViewportSize((float)NewRect.Width, (float)NewRect.Height);
		}
		if (bViewportClicked)
		{
			EditorEngine->FocusViewportInput(&SceneViewport);
		}

		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		ID3D11DeviceContext* DC =
			EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();

		DrawList->AddCallback(SetOpaqueBlendStateCallback, DC);

		// Render viewport
        DrawList->AddImage((ImTextureID)SRV, Min, Max);

		DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

		ImGui::EndChild();

        // Right Splitter
        ImGui::SameLine();
        ImGui::Button("##right_splitter", ImVec2(2.0f, -1.0f));
        if (ImGui::IsItemActive())
        {
            RightPanelWidth -= ImGui::GetIO().MouseDelta.x;
            RightPanelWidth = std::clamp(RightPanelWidth, 100.0f, FullSize.x * 0.4f);
        }
        ImGui::SameLine();

		// =====================================================
		// RIGHT: Bone Details
		// =====================================================
		ImGui::BeginChild("BoneDetailsPanel", ImVec2(RightPanelWidth, 0), true);
		ImGui::Text("Details");
		ImGui::Separator();
		if (Viewer->GetSelectedBoneIndex() != -1 && SkelMeshComp)
		{
			RenderBoneDetails(SkelMeshComp);
		}
        else if (Viewer->GetSelectedSocketIndex() != -1 && SkelMeshComp)
        {
            // Socket details (Location, Rotation, Scale already in Left Panel, but showing something here is good)
            if (CachedMesh && Viewer->GetSelectedSocketIndex() < (int32)CachedMesh->Sockets.size())
            {
                ImGui::Text("Socket: %s", CachedMesh->Sockets[Viewer->GetSelectedSocketIndex()].Name.ToString().c_str());
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

void FEditorViewerWindowWidget::RenderBoneDetails(USkeletalMeshComponent* SkelComp)
{
    const int32 SelectedBoneIndex = Viewer ? Viewer->GetSelectedBoneIndex() : -1;
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
    FVector& CachedRotation = Viewer->GetCachedBoneRotation();

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

void FEditorViewerWindowWidget::DrawBoneNode(int32 BoneIndex, const TArray<FBoneInfo>& Bones, const TArray<TArray<int32>>& Children)
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

    if (Viewer->GetSelectedBoneIndex() == BoneIndex)
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
        Viewer->SelectBone(BoneIndex);
    }

    // 우클릭 컨텍스트
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Add Socket"))
        {
            AddSocketOnBone(BoneIndex);
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

void FEditorViewerWindowWidget::DrawSocketNode(int32 SocketIdx)
{
    if (!CachedMesh) return;
    if (SocketIdx < 0 || SocketIdx >= static_cast<int32>(CachedMesh->Sockets.size())) return;

    const FSkeletalMeshSocket& Socket = CachedMesh->Sockets[SocketIdx];

    ImGuiTreeNodeFlags Flags =
        ImGuiTreeNodeFlags_Leaf |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_NoTreePushOnOpen;   // leaf니까 자식 push 불필요

    if (Viewer && Viewer->GetSelectedSocketIndex() == SocketIdx)
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
            Viewer->SelectSocket(SocketIdx);
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
                Viewer->ClearSocketPreview(Socket.Name);
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

void FEditorViewerWindowWidget::RebuildBoneTreeCaches(const FSkeletalMesh* MeshData)
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

void FEditorViewerWindowWidget::RebuildBoneToSocketIndices(const FSkeletalMesh* MeshData)
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

void FEditorViewerWindowWidget::AddSocketOnBone(int32 BoneIdx)
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
        Viewer->SelectSocket(NewIdx);
    }
    bMeshDirty = true;

    // socket-attached children의 transform이 새로 계산되도록 본 자세 dirty 전파 트리거.
    if (CachedSkComp)
    {
        CachedSkComp->MarkSkinningDirty();
    }
}

FString FEditorViewerWindowWidget::GenerateUniqueSocketName(const char* Base) const
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

void FEditorViewerWindowWidget::DeleteSocket(int32 SocketIdx)
{
    if (!CachedMesh) return;
    if (SocketIdx < 0 || SocketIdx >= static_cast<int32>(CachedMesh->Sockets.size())) return;

    // (1) 해당 socket에 매달린 preview mesh 먼저 정리
    const FName SocketName = CachedMesh->Sockets[SocketIdx].Name;
    if (EditorEngine && Viewer)
    {
        Viewer->ClearSocketPreview(SocketName);
    }

    // (2) Sockets 배열에서 erase. 다른 socket들의 인덱스가 시프트됨.
    CachedMesh->Sockets.erase(CachedMesh->Sockets.begin() + SocketIdx);

    // (3) BoneToSocketIndices 통째 재빌드 (시프트된 인덱스 반영)
    RebuildBoneToSocketIndices(CachedMesh);

    // (4) 선택 상태 정리
    if (Viewer)
    {
        Viewer->NotifySocketDeleted(SocketIdx);
    }

    bMeshDirty = true;

    if (CachedSkComp)
    {
        CachedSkComp->MarkSkinningDirty();
    }
}

bool FEditorViewerWindowWidget::HasPreview(const FName& SocketName) const
{
    if (!EditorEngine || !Viewer) return false;
    return Viewer->FindPreviewMesh(SocketName) != nullptr;
}

void FEditorViewerWindowWidget::DrawSocketInspector()
{
    // Save 상태는 socket 선택 여부와 무관하게 항상 보이는 게 편함.
    auto DrawSaveButton = [&]() {
        const bool bCanSave = bMeshDirty && CachedSkComp && CachedSkComp->GetSkeletalMesh();
        if (!bCanSave) ImGui::BeginDisabled();
        const char* Label = bMeshDirty ? "Save Mesh *" : "Save Mesh";
        if (ImGui::Button(Label))
        {
            TriggerSaveMesh();
        }
        if (!bCanSave) ImGui::EndDisabled();
    };

    const int32 SelectedSocketIndex = Viewer ? Viewer->GetSelectedSocketIndex() : -1;
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

void FEditorViewerWindowWidget::TriggerSaveMesh()
{
	RequestSaveMesh();
}

bool FEditorViewerWindowWidget::IsSocketNameUnique(const FString& Candidate, int32 IgnoreIdx) const
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

void FEditorViewerWindowWidget::DrawRenameModal()
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
            UStaticMeshComponent* Preview = Viewer->FindPreviewMesh(OldName);
            if (Preview && Preview->GetStaticMesh())
            {
                PreviewPath = Preview->GetStaticMesh()->GetAssetPathFileName();
                Viewer->ClearSocketPreview(OldName);
            }
        }

        CachedMesh->Sockets[RenameSocketIdx].Name = NewName;

        if (!PreviewPath.empty() && EditorEngine && Viewer)
        {
            Viewer->SetSocketPreviewMesh(NewName, PreviewPath);
        }

        if (Viewer && Viewer->GetSelectedSocketIndex() == RenameSocketIdx)
        {
            Viewer->SelectSocket(RenameSocketIdx);
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

void FEditorViewerWindowWidget::DrawPreviewPickerModal()
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
                Viewer->SetSocketPreviewMesh(SocketName, Path);
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
