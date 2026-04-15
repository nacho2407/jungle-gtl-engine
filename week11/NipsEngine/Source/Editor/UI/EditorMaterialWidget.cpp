#include "Editor/UI/EditorMaterialWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"

#include "Component/PrimitiveComponent.h"
#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "GameFramework/AActor.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectIterator.h"
#include <algorithm>
#include <filesystem>

#include "ImGui/imgui.h"

namespace
{
}

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
			if (SelectedMaterialPtr != PrimitiveComp->GetMaterial(SelectedSectionIndex))
			{
				SelectedSectionIndex = -1;
				SelectedMaterialPtr = nullptr;
			}

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
		SelectedMaterialPtr = PrimitiveComp->GetMaterial(0);
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
				SelectedMaterialPtr = PrimitiveComp->GetMaterial(i);
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
	if (!SelectedMaterialPtr)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Material not loaded. Assign one below.");
		ImGui::Spacing();
	}

	if (ImGui::Button("Create Instance"))
	{
		UMaterial* BaseMat = Cast<UMaterial>(SelectedMaterialPtr);
		if (BaseMat)
		{
			std::filesystem::path MatPath = std::filesystem::path(BaseMat->FilePath);
			std::filesystem::path MatDir = MatPath.parent_path();
			FString PureName = MatPath.stem().string();

			int32 Index = 0;
			std::filesystem::path FinalPath;
			do
			{
				FString NewName = PureName + "_Inst_" + std::to_string(Index) + ".matinst";
				FinalPath = MatDir / NewName;
				Index++;
			} while (std::filesystem::exists(FinalPath));

			FString InstancePath = FPaths::Normalize(FinalPath.string());

			UMaterialInstance* NewInstance = FResourceManager::Get().CreateMaterialInstance(InstancePath, BaseMat);
			if (NewInstance)
			{
				PrimitiveComp->SetMaterial(SelectedSectionIndex, NewInstance);
				SelectedMaterialPtr = NewInstance;

				FResourceManager::Get().SerializeMaterialInstance(InstancePath, NewInstance);
			}
		}
	}

    // ---- 머테리얼 교체 콤보박스 (항상 표시) ----
	TArray<UMaterialInterface*> Materials;
	for (TObjectIterator<UMaterialInterface> It; It; ++It)
	{
		UMaterialInterface* Mat = *It;
		if (Mat)
		{
			Materials.push_back(Mat);
		}
	}

    int32 CurrentIdx = -1;
	if (SelectedMaterialPtr)
	{
		for (int32 i = 0; i < static_cast<int32>(Materials.size()); ++i)
		{
			if (Materials[i]->GetName()  == SelectedMaterialPtr->GetName())
			{
				CurrentIdx = i;
				break;
			}
		}
	}

    const char* PreviewLabel = (CurrentIdx >= 0) ? Materials[CurrentIdx]->GetName().c_str() : "(none)";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##MaterialCombo", PreviewLabel))
    {
        for (int32 i = 0; i < static_cast<int32>(Materials.size()); ++i)
        {
            bool bIsSelected = (i == CurrentIdx);
            if (ImGui::Selectable(Materials[i]->GetName().c_str(), bIsSelected))
            {
				PrimitiveComp->SetMaterial(SelectedSectionIndex, Materials[i]);
				SelectedMaterialPtr = Materials[i];
				break;
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

void FEditorMaterialWidget::RenderMaterialProperties()
{
	TMap<FString, FMaterialParamValue> DisplayParams;

	SelectedMaterialPtr->GatherAllParams(DisplayParams);
	bool bIsInstanced = SelectedMaterialPtr->IsA<UMaterialInstance>();

	if (!bIsInstanced)
	{
		ImGui::BeginDisabled();
	}

	for (auto& [ParamName, ParamValue] : DisplayParams)
	{
		switch (ParamValue.Type)
		{
		case EMaterialParamType::Bool:
			if (ImGui::Checkbox(ParamName.c_str(), &std::get<bool>(ParamValue.Value)))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				FResourceManager::Get().SerializeMaterialInstance(SelectedMaterialPtr->GetFilePath(), Cast<UMaterialInstance>(SelectedMaterialPtr));
			}
			break;
		case EMaterialParamType::Int:
			if (ImGui::DragInt(ParamName.c_str(), &std::get<int32>(ParamValue.Value)))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				FResourceManager::Get().SerializeMaterialInstance(SelectedMaterialPtr->GetFilePath(), Cast<UMaterialInstance>(SelectedMaterialPtr));
			}
			break;
		case EMaterialParamType::UInt:
			if (ImGui::DragInt(ParamName.c_str(), reinterpret_cast<int32*>(&std::get<uint32>(ParamValue.Value))))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				FResourceManager::Get().SerializeMaterialInstance(SelectedMaterialPtr->GetFilePath(), Cast<UMaterialInstance>(SelectedMaterialPtr));
			}
			break;
		case EMaterialParamType::Float:
			if (ImGui::DragFloat(ParamName.c_str(), &std::get<float>(ParamValue.Value), 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				FResourceManager::Get().SerializeMaterialInstance(SelectedMaterialPtr->GetFilePath(), Cast<UMaterialInstance>(SelectedMaterialPtr));
			}
			break;
		case EMaterialParamType::Vector2:
			if (ImGui::DragFloat2(ParamName.c_str(), &std::get<FVector2>(ParamValue.Value).X, 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				FResourceManager::Get().SerializeMaterialInstance(SelectedMaterialPtr->GetFilePath(), Cast<UMaterialInstance>(SelectedMaterialPtr));
			}
			break;
		case EMaterialParamType::Vector3:
			if (ImGui::DragFloat3(ParamName.c_str(), &std::get<FVector>(ParamValue.Value).X, 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				FResourceManager::Get().SerializeMaterialInstance(SelectedMaterialPtr->GetFilePath(), Cast<UMaterialInstance>(SelectedMaterialPtr));
			}
			break;
		case EMaterialParamType::Vector4:
			if (ImGui::DragFloat4(ParamName.c_str(), &std::get<FVector4>(ParamValue.Value).X, 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				FResourceManager::Get().SerializeMaterialInstance(SelectedMaterialPtr->GetFilePath(), Cast<UMaterialInstance>(SelectedMaterialPtr));
			}
			break;
		case EMaterialParamType::Texture:
		{
			UTexture* CurrentTex = std::get<UTexture*>(ParamValue.Value);
			ID3D11ShaderResourceView* SRV = CurrentTex ? CurrentTex->GetSRV() : nullptr;

			if (ImGui::ImageButton(ParamName.c_str(), (void*)SRV, ImVec2(64, 64)))
			{
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
				for (TObjectIterator<UTexture> It; It; ++It)
				{
					UTexture* Texture = *It;
					if (Texture)
					{
						const FString& TexPath = Texture->GetFilePath().empty() ? "None" : Texture->GetFilePath();
						bool bSelected = (TexPath == CurrentPath);
						if (ImGui::Selectable(TexPath.c_str(), bSelected))
						{
							ParamValue.Value = Texture;
							SelectedMaterialPtr->SetParam(ParamName, ParamValue);
							FResourceManager::Get().SerializeMaterialInstance(SelectedMaterialPtr->GetFilePath(), Cast<UMaterialInstance>(SelectedMaterialPtr));
						}
						if (bSelected) ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::EndGroup();

			break;
		}
		}
	}

	if (!bIsInstanced)
	{
		ImGui::EndDisabled();
	}
}
