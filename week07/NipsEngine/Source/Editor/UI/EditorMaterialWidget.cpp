#include "Editor/UI/EditorMaterialWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"

#include "Component/PrimitiveComponent.h"
#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "GameFramework/AActor.h"
#include "Core/ResourceManager.h"
#include <algorithm>

#include "ImGui/imgui.h"

#define MAT_SEPARATOR() ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

void FEditorMaterialWidget::ResetSelection()
{
	SelectedComponent = nullptr;
	SelectedSectionIndex = -1;
	SelectedMaterialPtr = nullptr;
}

// -----------------------------------------------------------------------
// Render (진입점)
// -----------------------------------------------------------------------
void FEditorMaterialWidget::Render(float DeltaTime)
{
    ImGui::SetNextWindowSize(ImVec2(500.0f, 400.0f), ImGuiCond_Once);
    ImGui::Begin("Material Editor");

	FEditorPropertyWidget& PropWidget = EditorEngine->GetMainPanel().GetPropertyWidget();
	
	UActorComponent* ActorComp = PropWidget.GetSelectedComponent();
	
	if (ActorComp == nullptr)
	{
		ImGui::End();
		return;
	}

	USceneComponent* CurrentComp = Cast<USceneComponent>(ActorComp);

	// 만약 액터가 선택되어 있고 루트 컴포넌트가 있다면 그것을 기본으로 사용
	if (PropWidget.IsActorSelected())
	{
		AActor* PrimaryActor = EditorEngine->GetSelectionManager().GetPrimarySelection();
		CurrentComp = PrimaryActor ? PrimaryActor->GetRootComponent() : nullptr;
	}

	if (CurrentComp && CurrentComp != SelectedComponent)
	{
		SelectedComponent = CurrentComp;
		SelectedSectionIndex = -1;
		SelectedMaterialPtr = nullptr;
	}

	if (!SelectedComponent)
	{
		ImGui::TextDisabled("Select an actor with PrimitiveComponent to edit materials.");
	}
	else 
	{	
		if (UPrimitiveComponent* PrimitiveComp = Cast<UPrimitiveComponent>(SelectedComponent))
		{
			RenderMaterialEditor(PrimitiveComp);
		}
		else
		{
			ImGui::TextDisabled("Selected component is not a PrimitiveComponent.");
		}
	}
	
	ImGui::End();
}

void FEditorMaterialWidget::RenderMaterialEditor(UPrimitiveComponent* PrimitiveComp)
{
	int32 NumMaterials = PrimitiveComp->GetNumMaterials();
	if (NumMaterials <= 0)
	{
		ImGui::TextDisabled("No material slots found.");
		return;
	}

	// 최초 진입 시 첫 번째 섹션 자동 선택
	if (SelectedSectionIndex < 0 || SelectedSectionIndex >= NumMaterials)
	{
		SelectedSectionIndex = 0;
		UMaterialInstance* Inst = Cast<UMaterialInstance>(PrimitiveComp->GetMaterial(0));

		if (!Inst)
		{
			UMaterial* Mat = Cast<UMaterial>(PrimitiveComp->GetMaterial(0));
			if (Mat)
			{
				Inst = UMaterialInstance::Create(Mat);
				PrimitiveComp->SetMaterial(0, Inst);
			}
		}

		SelectedMaterialPtr = Inst;
	}

	const float SectionPanelWidth = 160.0f;

	// 왼쪽: 섹션 목록
	ImGui::BeginChild("##SectionList", ImVec2(SectionPanelWidth, 0), true);
	RenderSectionList(PrimitiveComp);
	ImGui::EndChild();

	ImGui::SameLine();

	// 오른쪽: 선택 섹션의 머테리얼 복사본 편집
	ImGui::BeginChild("##MaterialDetails", ImVec2(0, 0), true);
	RenderMaterialDetails(PrimitiveComp);
	ImGui::EndChild();
}

// -----------------------------------------------------------------------
// 왼쪽: 섹션 목록
// -----------------------------------------------------------------------
void FEditorMaterialWidget::RenderSectionList(UPrimitiveComponent* PrimitiveComp)
{
	int32 NumMaterials = PrimitiveComp->GetNumMaterials();
    ImGui::Text("Materials (%d)", NumMaterials);
    ImGui::Separator();

	UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(PrimitiveComp);
	UStaticMesh* MeshAsset = MeshComp ? MeshComp->GetStaticMesh() : nullptr;

    for (int32 i = 0; i < NumMaterials; ++i)
	{
		// 슬롯 이름 가져오기
		FString SlotName = "Slot";
		if (MeshAsset)
		{
			const TArray<FStaticMeshSection>& Sections = MeshAsset->GetSections();
			const TArray<FStaticMeshMaterialSlot>& MatSlots = MeshAsset->GetMaterialSlots();
			if (i < static_cast<int32>(Sections.size()))
			{
				int32 SlotIdx = Sections[i].MaterialSlotIndex;
				if (SlotIdx >= 0 && SlotIdx < static_cast<int32>(MatSlots.size()))
					SlotName = MatSlots[SlotIdx].SlotName;
			}
		}

		UMaterialInterface* Material = PrimitiveComp->GetMaterial(i);
		bool bMissing = (Material == nullptr);

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
				UMaterialInstance* Mat = Cast<UMaterialInstance>(PrimitiveComp->GetMaterial(i));
				SelectedMaterialPtr = Mat;
			}
        }

		if (bMissing)
			ImGui::PopStyleColor();
    }
}

// -----------------------------------------------------------------------
// 오른쪽: 머테리얼 상세 (복사본 편집)
// -----------------------------------------------------------------------
void FEditorMaterialWidget::RenderMaterialDetails(UPrimitiveComponent* PrimitiveComp)
{
    if (SelectedSectionIndex < 0 || SelectedSectionIndex >= PrimitiveComp->GetNumMaterials())
    {
        ImGui::TextDisabled("Select a slot to edit.");
        return;
    }

	// 슬롯 이름 표시
	UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(PrimitiveComp);
	UStaticMesh* MeshAsset = MeshComp ? MeshComp->GetStaticMesh() : nullptr;
	FString SlotName = "Slot";
	if (MeshAsset) 
	{
		const TArray<FStaticMeshSection>& Sections = MeshAsset->GetSections();
		const TArray<FStaticMeshMaterialSlot>& MatSlots = MeshAsset->GetMaterialSlots();
		if (SelectedSectionIndex < static_cast<int32>(Sections.size())) 
		{
			int32 SlotIdx = Sections[SelectedSectionIndex].MaterialSlotIndex;
			if (SlotIdx >= 0 && SlotIdx < static_cast<int32>(MatSlots.size()))
				SlotName = MatSlots[SlotIdx].SlotName;
		}
	}
	ImGui::Text("Slot [%d]  |  Name: %s", SelectedSectionIndex, SlotName.c_str());

	// MTL 못 읽어 머테리얼 없는 경우 경고
	if (!SelectedMaterialPtr || !SelectedMaterialPtr->Parent)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Material not loaded. Assign one below.");
		ImGui::Spacing();
	}

    // ---- 머테리얼 교체 콤보박스 (항상 표시) ----
    const TArray<FString> MatNames = FResourceManager::Get().GetMaterialNames();

    int32 CurrentIdx = -1;
	if (SelectedMaterialPtr && SelectedMaterialPtr->Parent)
	{
		for (int32 i = 0; i < static_cast<int32>(MatNames.size()); ++i)
		{
			if (MatNames[i] == SelectedMaterialPtr->Parent->Name)
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
                UMaterial* NewMat = Cast<UMaterial>(FResourceManager::Get().GetMaterial(MatNames[i]));
                if (NewMat)
                {
					UMaterialInstance* Inst = UMaterialInstance::Create(NewMat);
					PrimitiveComp->SetMaterial(SelectedSectionIndex, Inst);
					SelectedMaterialPtr = Inst;
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
	RenderMaterialProperties();
}

void FEditorMaterialWidget::RenderMaterialDetails(FMaterial* Mat, std::function<void(FMaterial*)> OnMaterialChanged)
{
	// Legacy helper for specific cases if needed
	if (!SelectedMaterialPtr)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Material not loaded. Assign one below.");
		ImGui::Spacing();
	}

	const TArray<FString> MatNames = FResourceManager::Get().GetMaterialNames();

	int32 CurrentIdx = -1;
	if (SelectedMaterialPtr)
	{
		for (int32 i = 0; i < static_cast<int32>(MatNames.size()); ++i)
		{
			if (MatNames[i] == SelectedMaterialPtr->Parent->Name)
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
				UMaterial* NewMat = Cast<UMaterial>(FResourceManager::Get().GetMaterial(MatNames[i]));
				if (NewMat)
				{
					UMaterialInstance* Inst = UMaterialInstance::Create(NewMat);
					OnMaterialChanged(&Inst->Parent->MaterialData);
					SelectedMaterialPtr = Inst;
				}
			}
			if (bIsSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (!SelectedMaterialPtr)
		return;

	MAT_SEPARATOR();
	RenderMaterialProperties();
}

void FEditorMaterialWidget::RenderMaterialProperties()
{
	TMap<FString, FMaterialParamValue> DisplayParams;

	SelectedMaterialPtr->GatherAllParams(DisplayParams);

	for (auto& [ParamName, ParamValue] : DisplayParams)
	{
		switch (ParamValue.Type)
		{
		case EMaterialParamType::Bool:
			if (ImGui::Checkbox(ParamName.c_str(), &std::get<bool>(ParamValue.Value)))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
			}
			break;
		case EMaterialParamType::Int:
			if (ImGui::DragInt(ParamName.c_str(), &std::get<int32>(ParamValue.Value)))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
			}
			break;
		case EMaterialParamType::UInt:
			if (ImGui::DragInt(ParamName.c_str(), reinterpret_cast<int32*>(&std::get<uint32>(ParamValue.Value))))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
			}
			break;
		case EMaterialParamType::Float:
			if (ImGui::DragFloat(ParamName.c_str(), &std::get<float>(ParamValue.Value), 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
			}
			break;
		case EMaterialParamType::Vector2:
			if (ImGui::DragFloat2(ParamName.c_str(), &std::get<FVector2>(ParamValue.Value).X, 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
			}
			break;
		case EMaterialParamType::Vector3:
			if (ImGui::DragFloat3(ParamName.c_str(), &std::get<FVector>(ParamValue.Value).X, 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
			}
			break;
		case EMaterialParamType::Vector4:
			if (ImGui::DragFloat4(ParamName.c_str(), &std::get<FVector4>(ParamValue.Value).X, 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
			}
			break;
		case EMaterialParamType::Texture:
		{
			UTexture* CurrentTex = std::get<UTexture*>(ParamValue.Value);
			ID3D11ShaderResourceView* SRV = CurrentTex ? CurrentTex->GetSRV() : nullptr;

			if (ImGui::ImageButton(ParamName.c_str(), (void*)SRV, ImVec2(64, 64)))
			{
				// 이미지 버튼 클릭 시 동작 (필요 시 팝업 등 추가)
			}
			ImGui::SameLine();
			
			ImGui::BeginGroup();
			ImGui::Text("%s", ParamName.c_str());
			
			const TArray<FString>& TextureList = FResourceManager::Get().GetTextureFilePath();
			FString CurrentPath = CurrentTex ? CurrentTex->GetName() : "None";

			ImGui::SetNextItemWidth(200.0f);
			FString ComboId = "##Combo_" + ParamName;
			if (ImGui::BeginCombo(ComboId.c_str(), CurrentPath.c_str()))
			{
				for (const FString& TexPath : TextureList)
				{
					bool bSelected = (TexPath == CurrentPath);
					if (ImGui::Selectable(TexPath.c_str(), bSelected))
					{
						UTexture* NewTex = FResourceManager::Get().GetTexture(TexPath);
						if (NewTex)
						{
							ParamValue.Value = NewTex;
							SelectedMaterialPtr->SetParam(ParamName, ParamValue);
						}
					}
					if (bSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::EndGroup();

			break;
		}
		}
	}
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
        UTexture* Texture = FResourceManager::Get().GetTexture(Path);
		return Texture ? Texture->GetSRV() : nullptr;
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
// 헬퍼: 선택된 액터에서 PrimitiveComponent 가져오기
// -----------------------------------------------------------------------
UPrimitiveComponent* FEditorMaterialWidget::GetSelectedPrimitiveComponent() const
{
    AActor* Actor = EditorEngine->GetSelectionManager().GetPrimarySelection();
    if (!Actor) return nullptr;

    USceneComponent* Root = Actor->GetRootComponent();
    if (Root && Root->IsA<UPrimitiveComponent>())
        return static_cast<UPrimitiveComponent*>(Root);

    return nullptr;
}
