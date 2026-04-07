#include "Editor/UI/EditorMaterialWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"

#include "Component/StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "GameFramework/AActor.h"
#include "Core/ResourceManager.h"
#include <algorithm>

#include "ImGui/imgui.h"

#define MAT_SEPARATOR() ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

// -----------------------------------------------------------------------
// Render (진입점)
// -----------------------------------------------------------------------
void FEditorMaterialWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(500.0f, 400.0f), ImGuiCond_Once);
    ImGui::Begin("Material Editor");

    UStaticMeshComponent* MeshComp = GetSelectedMeshComponent();
    if (!MeshComp || !MeshComp->GetStaticMesh())
    {
        ImGui::TextDisabled("Select a StaticMesh actor to edit materials.");
        ImGui::End();
        return;
    }

	UStaticMesh* MeshAsset = MeshComp->GetStaticMesh();
    // 선택된 컴포넌트 또는 메시 에셋이 바뀌면 상태 초기화
    if (MeshComp != LastMeshComp || MeshAsset != LastMeshAsset)
    {
		LastMeshComp = MeshComp;
		LastMeshAsset = MeshAsset;
        SelectedSectionIndex = -1;
		SelectedMaterialPtr = nullptr;
    }

    const TArray<FStaticMeshSection>& Sections = MeshComp->GetStaticMesh()->GetSections();
    if (Sections.empty())
    {
        ImGui::TextDisabled("No sections.");
        ImGui::End();
        return;
    }

    // 최초 진입 시 첫 번째 섹션 자동 선택
	if (SelectedSectionIndex < 0)
	{
		SelectedSectionIndex = 0;
		SelectedMaterialPtr = MeshComp->GetMaterial(0);
	}

    const float SectionPanelWidth = 160.0f;

    // 왼쪽: 섹션 목록
    ImGui::BeginChild("##SectionList", ImVec2(SectionPanelWidth, 0), true);
    RenderSectionList(MeshComp);
    ImGui::EndChild();

    ImGui::SameLine();

    // 오른쪽: 선택 섹션의 머테리얼 복사본 편집
    ImGui::BeginChild("##MaterialDetails", ImVec2(0, 0), true);
    RenderMaterialDetails(MeshComp);
    ImGui::EndChild();

    ImGui::End();
}

// -----------------------------------------------------------------------
// 왼쪽: 섹션 목록
// -----------------------------------------------------------------------
void FEditorMaterialWidget::RenderSectionList(UStaticMeshComponent* MeshComp)
{
	const TArray<FMaterial*>& OverrideMaterial = MeshComp->GetOverrideMaterial();
	const TArray<FStaticMeshSection>& Sections = MeshComp->GetStaticMesh()->GetSections();
	const TArray<FStaticMeshMaterialSlot>& MatSlots = MeshComp->GetStaticMesh()->GetMaterialSlots();

    ImGui::Text("Sections (%d)", static_cast<int32>(OverrideMaterial.size()));
    ImGui::Separator();

    for (int32 i = 0; i < static_cast<int32>(OverrideMaterial.size()); ++i)
	{
		// 슬롯 이름 가져오기
		FString SlotName = "Unknown";
		if (i < static_cast<int32>(Sections.size()))
		{
			int32 SlotIdx = Sections[i].MaterialSlotIndex;
			if (SlotIdx >= 0 && SlotIdx < static_cast<int32>(MatSlots.size()))
				SlotName = MatSlots[SlotIdx].SlotName;
		}

		bool bMissing = (OverrideMaterial[i] == nullptr);

        char Label[128];
        snprintf(Label, sizeof(Label), "[%d] %s%s", i, SlotName.c_str(), bMissing ? " (!)" : "");

        bool bSelected = (SelectedSectionIndex == i);
		if (bMissing)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));

        if (ImGui::Selectable(Label, bSelected, 0, ImVec2(0, 20)))
        {
			if (!bSelected)
			{
				SelectedSectionIndex = i;
				SelectedMaterialPtr = MeshComp->GetMaterial(i);
			}
        }

		if (bMissing)
			ImGui::PopStyleColor();
    }
}

// -----------------------------------------------------------------------
// 오른쪽: 머테리얼 상세 (복사본 편집)
// -----------------------------------------------------------------------
void FEditorMaterialWidget::RenderMaterialDetails(UStaticMeshComponent* MeshComp)
{
    if (SelectedSectionIndex < 0)
    {
        ImGui::TextDisabled("Select a section to edit.");
        return;
    }

	// 슬롯 이름 표시
	const TArray<FStaticMeshSection>& Sections = MeshComp->GetStaticMesh()->GetSections();
	const TArray<FStaticMeshMaterialSlot>& MatSlots = MeshComp->GetStaticMesh()->GetMaterialSlots();
	FString SlotName = "Unknown";
	if (SelectedSectionIndex < static_cast<int32>(Sections.size()))
	{
		int32 SlotIdx = Sections[SelectedSectionIndex].MaterialSlotIndex;
		if (SlotIdx >= 0 && SlotIdx < static_cast<int32>(MatSlots.size()))
			SlotName = MatSlots[SlotIdx].SlotName;
	}
	ImGui::Text("Section [%d]  |  Slot: %s", SelectedSectionIndex, SlotName.c_str());

	// MTL 못 읽어 머테리얼 없는 경우 경고
	if (!SelectedMaterialPtr)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Material not loaded. Assign one below.");
		ImGui::Spacing();
	}

    // ---- 머테리얼 교체 콤보박스 (항상 표시) ----
    const TArray<FString> MatNames = FResourceManager::Get().GetMaterialNames();

    int32 CurrentIdx = -1;
	if (SelectedMaterialPtr)
	{
		for (int32 i = 0; i < static_cast<int32>(MatNames.size()); ++i)
		{
			if (MatNames[i] == SelectedMaterialPtr->Name)
			{
				CurrentIdx = i;
				break;
			}
		}
	}

    const char* PreviewLabel = (CurrentIdx >= 0) ? MatNames[CurrentIdx].c_str() : "(none)";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##MaterialCombo", PreviewLabel))
    {
        for (int32 i = 0; i < static_cast<int32>(MatNames.size()); ++i)
        {
            bool bIsSelected = (i == CurrentIdx);
            if (ImGui::Selectable(MatNames[i].c_str(), bIsSelected))
            {
                FMaterial* NewMat = FResourceManager::Get().FindMaterial(MatNames[i]);
                if (NewMat)
                {
                    MeshComp->SetMaterial(SelectedSectionIndex, NewMat);
                    SelectedMaterialPtr = NewMat;
                }
            }
            if (bIsSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

	// 머테리얼이 없으면 색상/텍스처 편집 불가
	if (!SelectedMaterialPtr)
		return;

    MAT_SEPARATOR();
    RenderColorSection(*SelectedMaterialPtr);

    MAT_SEPARATOR();
    RenderScalarSection(*SelectedMaterialPtr);

    MAT_SEPARATOR();
    RenderTextureSection(*SelectedMaterialPtr);
}

// -----------------------------------------------------------------------
// Colors 섹션
// -----------------------------------------------------------------------
void FEditorMaterialWidget::RenderColorSection(FMaterial& Mat)
{
    ImGui::Text("Colors");
    ImGui::Separator();
    ImGui::Spacing();

	ImGui::ColorButton("Diffuse (Kd)", ImVec4(Mat.DiffuseColor.X, Mat.DiffuseColor.Y, Mat.DiffuseColor.Z, 1.0f));
	ImGui::SameLine();
	ImGui::ColorButton("Ambient (Ka)", ImVec4(Mat.AmbientColor.X, Mat.AmbientColor.Y, Mat.AmbientColor.Z, 1.0f));
	ImGui::SameLine();
	ImGui::ColorButton("Specular (Ks)", ImVec4(Mat.SpecularColor.X, Mat.SpecularColor.Y, Mat.SpecularColor.Z, 1.0f));
	ImGui::SameLine();
	ImGui::ColorButton("Emissive (Ke)", ImVec4(Mat.EmissiveColor.X, Mat.EmissiveColor.Y, Mat.EmissiveColor.Z, 1.0f));
}

// -----------------------------------------------------------------------
// Scalars 섹션
// -----------------------------------------------------------------------
void FEditorMaterialWidget::RenderScalarSection(FMaterial& Mat)
{
    ImGui::Text("Scalars");
    ImGui::Separator();
    ImGui::Spacing();

	ImGui::BeginDisabled(true);
	ImGui::DragFloat("Shininess (Ns)", &Mat.Shininess, 0.5f, 0.0f, 1000.0f);
	ImGui::DragFloat("Opacity (d)", &Mat.Opacity, 0.01f, 0.0f, 1.0f);
	ImGui::EndDisabled();
}

// -----------------------------------------------------------------------
// Textures 섹션
// -----------------------------------------------------------------------
void FEditorMaterialWidget::RenderTextureSection(FMaterial& Mat)
{
    ImGui::Text("Textures");
    ImGui::Separator();
    ImGui::Spacing();

    constexpr float ThumbSize = 64.0f;
    const ImVec4    EmptyColor(0.2f, 0.2f, 0.2f, 1.0f);

    auto ExtractFilename = [](const FString& Path) -> FString
    {
        size_t Pos = Path.find_last_of("/\\");
        return (Pos != FString::npos) ? Path.substr(Pos + 1) : Path;
    };

    // SRV는 ResourceManager에서 경로로 직접 조회
    auto ResolveSRV = [](const FString& Path) -> ID3D11ShaderResourceView*
    {
        if (Path.empty()) return nullptr;
        FMaterialResource* Res = FResourceManager::Get().FindTexture(Path);
        return (Res && Res->SRV) ? Res->SRV.Get() : nullptr;
    };

    auto TextureRow = [&](const char* MapLabel,
                          FString& Path, bool& bHasTexture,
                          ID3D11ShaderResourceView* SRV)
    {
        ImGui::PushID(MapLabel);

        // ---- 왼쪽: 썸네일 ----
        if (SRV && bHasTexture)
        {
            ImGui::Image((ImTextureID)SRV, ImVec2(ThumbSize, ThumbSize));
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Image((ImTextureID)SRV, ImVec2(128.0f, 128.0f));
                ImGui::EndTooltip();
            }
        }
        else
        {
            ImVec2 Pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                Pos,
                ImVec2(Pos.x + ThumbSize, Pos.y + ThumbSize),
                ImGui::ColorConvertFloat4ToU32(EmptyColor)
            );
            ImGui::GetWindowDrawList()->AddRect(
                Pos,
                ImVec2(Pos.x + ThumbSize, Pos.y + ThumbSize),
                IM_COL32(100, 100, 100, 255)
            );
            ImGui::Dummy(ImVec2(ThumbSize, ThumbSize));
        }

        ImGui::SameLine();


        ImGui::BeginGroup();
        {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "%s", MapLabel);

            FString Filename = bHasTexture ? ExtractFilename(Path) : FString("(none)");
            ImGui::TextDisabled("%s", Filename.c_str());

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::BeginDisabled(true);
            char Buf[512];
            strncpy_s(Buf, sizeof(Buf), Path.c_str(), _TRUNCATE);
            if (ImGui::InputText("##path", Buf, sizeof(Buf)))
            {
                Path       = Buf;
                bHasTexture = !Path.empty();
            }
            if (ImGui::IsItemHovered() && !Path.empty())
                ImGui::SetTooltip("%s", Path.c_str());
            ImGui::PopItemWidth();
			ImGui::EndDisabled();
        }
        ImGui::EndGroup();

        ImGui::Spacing();
        ImGui::PopID();
    };
    TextureRow("Diffuse Map  (map_Kd)",   Mat.DiffuseTexPath,  Mat.bHasDiffuseTexture,  ResolveSRV(Mat.DiffuseTexPath));
    TextureRow("Ambient Map  (map_Ka)",   Mat.AmbientTexPath,  Mat.bHasAmbientTexture,  ResolveSRV(Mat.AmbientTexPath));
    TextureRow("Specular Map (map_Ks)",   Mat.SpecularTexPath, Mat.bHasSpecularTexture, ResolveSRV(Mat.SpecularTexPath));
    TextureRow("Bump Map     (map_bump)", Mat.BumpTexPath,     Mat.bHasBumpTexture,     ResolveSRV(Mat.BumpTexPath));
}

// -----------------------------------------------------------------------
// 헬퍼: 선택된 액터에서 StaticMeshComponent 가져오기
// -----------------------------------------------------------------------
UStaticMeshComponent* FEditorMaterialWidget::GetSelectedMeshComponent() const
{
    AActor* Actor = EditorEngine->GetSelectionManager().GetPrimarySelection();
    if (!Actor) return nullptr;

    USceneComponent* Root = Actor->GetRootComponent();
    if (Root && Root->IsA<UStaticMeshComponent>())
        return static_cast<UStaticMeshComponent*>(Root);

    return nullptr;
}
