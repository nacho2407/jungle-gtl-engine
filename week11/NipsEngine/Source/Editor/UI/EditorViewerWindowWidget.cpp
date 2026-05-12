#include "EditorViewerWindowWidget.h"
#include "Editor/EditorEngine.h"
#include "Viewport/ViewportLayout.h"
#include "GameFramework/PrimitiveActors.h"
#include "Component/SkeletalMeshComponent.h"
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

    auto& SceneViewport = EditorEngine->GetViewer().GetViewport();

    ID3D11ShaderResourceView* SRV = SceneViewport.GetOutSRV();
    if (!SRV)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("Viewer");

    ImVec2 FullSize = ImGui::GetContentRegionAvail();

    float LeftWidth = FullSize.x * 0.25f;
    float RightWidth = FullSize.x - LeftWidth;

    // =====================================================
    // LEFT: Skeleton Tree
    // =====================================================
    ImGui::BeginChild("SkeletonPanel", ImVec2(LeftWidth, 0), true);

    ImGui::Text("Skeleton");

	ASkeletalMeshActor* ViewTarget = EditorEngine->GetViewer().GetViewTarget();
    USkeletalMeshComponent* SkelMeshComp = ViewTarget->GetSkeletalMeshComponent();
    FSkeletalMesh* MeshData = SkelMeshComp->GetSkeletalMesh()->GetMeshData();

	if (CachedMesh != MeshData)
	{
        CachedMesh = MeshData;

		Children.clear();
        Children.resize(MeshData->Bones.size());

        for (int32 i = 0; i < MeshData->Bones.size(); ++i)
        {
            int32 Parent = MeshData->Bones[i].ParentIndex;
            if (Parent >= 0)
            {
                Children[Parent].push_back(i);
            }
        }
	}

	for (int32 i = 0; i < MeshData->Bones.size(); ++i)
    {
        if (MeshData->Bones[i].ParentIndex == -1)
        {
            DrawBoneNode(i, MeshData->Bones, Children);
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // =====================================================
    // RIGHT: Viewport
    // =====================================================
    ImGui::BeginChild("ViewportPanel", ImVec2(RightWidth, 0), false);

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

    ImGui::End();
    ImGui::PopStyleVar();
}

void FEditorViewerWindowWidget::DrawBoneNode(int32 BoneIndex, const TArray<FBoneInfo>& Bones, const TArray<TArray<int32>>& Children)
{
    const FBoneInfo& Bone = Bones[BoneIndex];

    ImGuiTreeNodeFlags Flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (Children[BoneIndex].size() == 0)
    {
        Flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (SelectedBoneIndex == BoneIndex)
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
        SelectedBoneIndex = BoneIndex;
    }

    if (bOpen)
    {
        for (int32 ChildIndex : Children[BoneIndex])
        {
            DrawBoneNode(ChildIndex, Bones, Children);
        }

        ImGui::TreePop();
    }
}
