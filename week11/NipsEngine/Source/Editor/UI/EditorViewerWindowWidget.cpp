#include "EditorViewerWindowWidget.h"
#include "Editor/EditorEngine.h"
#include "Viewport/ViewportLayout.h"
#include "GameFramework/PrimitiveActors.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/TransformProxy.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "imgui.h"

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
}

void FEditorViewerWindowWidget::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorWidget::Initialize(InEditorEngine);
}

void FEditorViewerWindowWidget::Render(float DeltaTime)
{
    if (!EditorEngine)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	auto& Viewers = EditorEngine->GetViewers();
	for (int32 i = 0; i < (int32)Viewers.size(); ++i)
	{
		auto& Viewer = *Viewers[i];
		FSceneViewport& SceneViewport = Viewer.GetViewport();

		ID3D11ShaderResourceView* SRV = SceneViewport.GetOutSRV();
		if (!SRV)
			continue;

		char WindowName[64];
		sprintf_s(WindowName, "Viewer##%p", &Viewer);

		bool bOpen = true;
		// Make the viewer window reasonably large on first creation
		ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(WindowName, &bOpen))
		{
			ImVec2 FullSize = ImGui::GetContentRegionAvail();

			float LeftWidth = FullSize.x * 0.2f;
			float RightWidth = FullSize.x * 0.2f;
			float CenterWidth = FullSize.x - LeftWidth - RightWidth;

			// =====================================================
			// LEFT: Skeleton Tree
			// =====================================================
			ImGui::BeginChild("SkeletonPanel", ImVec2(LeftWidth, 0), true);

			ImGui::Text("Skeleton");

			ASkeletalMeshActor* ViewTarget = Viewer.GetViewTarget();
			USkeletalMeshComponent* SkelMeshComp = ViewTarget->GetSkeletalMeshComponent();
			FSkeletalMesh* MeshData = SkelMeshComp->GetSkeletalMesh()->GetMeshData();

			if (CachedMesh != MeshData)
			{
				CachedMesh = MeshData;

				Children.clear();
				Children.resize(MeshData->Bones.size());

				for (int32 j = 0; j < (int32)MeshData->Bones.size(); ++j)
				{
					int32 Parent = MeshData->Bones[j].ParentIndex;
					if (Parent >= 0)
					{
						Children[Parent].push_back(j);
					}
				}
			}

			for (int32 j = 0; j < (int32)MeshData->Bones.size(); ++j)
			{
				if (MeshData->Bones[j].ParentIndex == -1)
				{
					DrawBoneNode(Viewer, j, MeshData->Bones, Children);
				}
			}

			ImGui::EndChild();

			ImGui::SameLine();

			// =====================================================
			// CENTER: Viewport
			// =====================================================
			ImGui::BeginChild("ViewportPanel", ImVec2(CenterWidth, 0), false);

			ImVec2 ScreenPos = ImGui::GetCursorScreenPos();
			ImVec2 Size = ImGui::GetContentRegionAvail();

			Size.x = std::max(Size.x, 1.0f);
			Size.y = std::max(Size.y, 1.0f);

			POINT pt = { (LONG)ScreenPos.x, (LONG)ScreenPos.y };

			FViewportRect NewRect;
			NewRect.X = (int32)pt.x;
			NewRect.Y = (int32)pt.y;
			NewRect.Width = (int32)Size.x;
			NewRect.Height = (int32)Size.y;

			SceneViewport.SetRect(NewRect);

			if (auto* Client = SceneViewport.GetClient())
			{
				Client->SetViewportSize((float)NewRect.Width, (float)NewRect.Height);
			}

			ImDrawList* DrawList = ImGui::GetWindowDrawList();

			ID3D11DeviceContext* DC =
				EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();

			DrawList->AddCallback(SetOpaqueBlendStateCallback, DC);

			// Render viewport
			ImGui::Image((ImTextureID)SRV, Size);

			DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

			ImGui::EndChild();

			ImGui::SameLine();

			// =====================================================
			// RIGHT: Bone Details
			// =====================================================
			ImGui::BeginChild("BoneDetailsPanel", ImVec2(RightWidth, 0), true);
			ImGui::Text("Bone Details");
			ImGui::Separator();
			if (Viewer.SelectedBoneIndex != -1 && SkelMeshComp)
			{
				RenderBoneDetails(Viewer, SkelMeshComp);
			}
			else
			{
				ImGui::TextDisabled("No bone selected.");
			}
			ImGui::EndChild();
		}
		ImGui::End();

		if (!bOpen)
		{
			EditorEngine->RemoveViewer(&Viewer);
			break; // List modified, stop for this frame
		}
	}

    ImGui::PopStyleVar();
}

void FEditorViewerWindowWidget::RenderBoneDetails(FEditorViewer& Viewer, USkeletalMeshComponent* SkelComp)
{
    if (!SkelComp || Viewer.SelectedBoneIndex == -1) return;

    const FBoneInfo& Bone = SkelComp->GetSkeletalMesh()->GetMeshData()->Bones[Viewer.SelectedBoneIndex];
    ImGui::Text("Bone: %s (Index: %d)", Bone.Name.c_str(), Viewer.SelectedBoneIndex);
    ImGui::Spacing();

    FMatrix LocalTransform = SkelComp->GetBoneLocalTransform(Viewer.SelectedBoneIndex);
    FVector Location, Scale;
    FMatrix RotationMatrix;
    LocalTransform.Decompose(Location, RotationMatrix, Scale);

    // 외부(기즈모 등)에서 회전이 변경되었는지 확인
    FVector CurrentEuler = RotationMatrix.GetEuler();

	if ((CurrentEuler - FMatrix::MakeRotationEuler(Viewer.CachedRotation).GetEuler()).Size() > 0.01f)
    {
        Viewer.CachedRotation = CurrentEuler;
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
    if (DrawTransformField("Rotation", Viewer.CachedRotation, 0.1f)) bEdited = true;
    if (DrawTransformField("Scale", Scale, 0.01f)) bEdited = true;

    if (bEdited)
    {
        FMatrix NewLocal = FMatrix::MakeTRS(Location, FMatrix::MakeRotationEuler(Viewer.CachedRotation), Scale);
        SkelComp->SetBoneLocalTransform(Viewer.SelectedBoneIndex, NewLocal);

        // Gizmo 위치 업데이트
        FViewportClient* BaseClient = Viewer.GetViewport().GetClient();
        FEditorViewportClient* EditorClient = static_cast<FEditorViewportClient*>(BaseClient);
        if (UGizmoComponent* Gizmo = EditorClient->GetGizmo())
        {
            Gizmo->UpdateGizmoTransform();
        }
    }
}

void FEditorViewerWindowWidget::DrawBoneNode(FEditorViewer& Viewer, int32 BoneIndex, const TArray<FBoneInfo>& Bones, const TArray<TArray<int32>>& Children)
{
    const FBoneInfo& Bone = Bones[BoneIndex];

    ImGuiTreeNodeFlags Flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (Children[BoneIndex].size() == 0)
    {
        Flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (Viewer.SelectedBoneIndex == BoneIndex)
    {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool bOpen = ImGui::TreeNodeEx(
        (void*)(intptr_t)BoneIndex,
        Flags,
        "%s",
        Bone.Name.c_str());

    // 클릭 처리 (중요)
    if (ImGui::IsItemClicked())
    {
        Viewer.SelectedBoneIndex = BoneIndex;

		if (EditorEngine)
		{
			FViewportClient* BaseClient = Viewer.GetViewport().GetClient();
			FEditorViewportClient* EditorClient = static_cast<FEditorViewportClient*>(BaseClient);

			if (UGizmoComponent* Gizmo = EditorClient->GetGizmo())
			{
				ASkeletalMeshActor* ViewTarget = Viewer.GetViewTarget();
				if (ViewTarget)
				{
					USkeletalMeshComponent* SkelComp = ViewTarget->GetSkeletalMeshComponent();
					Gizmo->SetProxy(std::make_shared<FBoneTransformProxy>(SkelComp, Viewer.SelectedBoneIndex));

                    // 초기 회전값 캐싱 (Euler Jitter 방지)
                    FMatrix LocalTransform = SkelComp->GetBoneLocalTransform(Viewer.SelectedBoneIndex);
                    FVector dummyL, dummyS;
                    FMatrix RotM;
                    LocalTransform.Decompose(dummyL, RotM, dummyS);
                    Viewer.CachedRotation = RotM.GetEuler();
				}
			}
		}
    }

    if (bOpen)
    {
        for (int32 ChildIndex : Children[BoneIndex])
        {
            DrawBoneNode(Viewer, ChildIndex, Bones, Children);
        }

        ImGui::TreePop();
    }
}
