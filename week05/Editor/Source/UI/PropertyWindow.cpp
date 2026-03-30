#include "PropertyWindow.h"
#include "EditorEngine.h"
#include "Actor/Actor.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"

void FPropertyWindow::SetTarget(const FVector& Location, const FVector& Rotation,
                                const FVector& Scale, const char* ActorName)
{
	EditLocation = Location;
	EditRotation = Rotation;
	EditScale = Scale;
	bModified = false;

	if (ActorName)
		snprintf(ActorNameBuf, sizeof(ActorNameBuf), "%s", ActorName);
	else
		snprintf(ActorNameBuf, sizeof(ActorNameBuf), "None");
}

void FPropertyWindow::DrawTransformSection()
{
	float Loc[3] = { EditLocation.X, EditLocation.Y, EditLocation.Z };
	float Rot[3] = { EditRotation.X, EditRotation.Y, EditRotation.Z };
	float Scl[3] = { EditScale.X,    EditScale.Y,    EditScale.Z };

	const float ResetBtnWidth = 14.0f;
	const float Spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	const float DragUIWidth = 200.f;

	// Location
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
	if (ImGui::Button("##RL", ImVec2(ResetBtnWidth, 0)))
	{
		EditLocation = { 0.0f, 0.0f, 0.0f };
		bModified = true;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Location");
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, Spacing);
	// ImGui::PushItemWidth(-(ResetBtnWidth));
	ImGui::PushItemWidth(DragUIWidth);
	if (ImGui::DragFloat3("Location", Loc, 0.1f, 0.0f, 0.0f, "%.2f"))
	{
		EditLocation = { Loc[0], Loc[1], Loc[2] };
		bModified = true;
	}
	ImGui::PopItemWidth();

	// Rotation
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.4f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
	if (ImGui::Button("##RR", ImVec2(ResetBtnWidth, 0)))
	{
		EditRotation = { 0.0f, 0.0f, 0.0f };
		bModified = true;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Rotation");
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, Spacing);
	// ImGui::PushItemWidth(-(ResetBtnWidth));
	ImGui::PushItemWidth(DragUIWidth);
	if (ImGui::DragFloat3("Rotation", Rot, 0.5f, -360.0f, 360.0f, "%.1f"))
	{
		EditRotation = { Rot[0], Rot[1], Rot[2] };
		bModified = true;
	}
	ImGui::PopItemWidth();

	// Scale
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.5f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.3f, 0.7f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.4f, 0.9f, 1.0f));
	if (ImGui::Button("##RS", ImVec2(ResetBtnWidth, 0)))
	{
		EditScale = { 1.0f, 1.0f, 1.0f };
		bModified = true;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Scale");
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, Spacing);
	// ImGui::PushItemWidth(-(ResetBtnWidth));
	ImGui::PushItemWidth(DragUIWidth);
	if (ImGui::DragFloat3("Scale", Scl, 0.01f, 0.001f, 100.0f, "%.3f"))
	{
		EditScale = { Scl[0], Scl[1], Scl[2] };
		bModified = true;
	}
	ImGui::PopItemWidth();

	if (bModified && OnChanged)
		OnChanged(EditLocation, EditRotation, EditScale);
}

void FPropertyWindow::Render(FEditorEngine* Engine)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	bool bOpen = ImGui::Begin("Properties");
	ImGui::PopStyleVar();

	if (!bOpen)
	{
		ImGui::End();
		return;
	}

	bModified = false;

	ImGui::TextDisabled("Selected:");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "%s", ActorNameBuf);

	ImGui::Separator();

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(8.0f);
		DrawTransformSection();
		ImGui::Unindent(8.0f);
	}
	if (Engine)
	{
		AActor* SelectedActor = Engine->GetSelectedActor();
		if (SelectedActor)
		{
			if (ImGui::CollapsingHeader("Billboard", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Indent(8.0f);
				for (UActorComponent* Component : SelectedActor->GetComponents())
				{
					if (!Component) continue;

					if (Component->IsA(USubUVComponent::StaticClass()))
					{
						USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Component);
						bool bBillboard = SubUVComp->IsBillboard();
						if (ImGui::Checkbox("SubUV Billboard", &bBillboard))
							SubUVComp->SetBillboard(bBillboard);
					}
					else if (Component->IsA(UTextComponent::StaticClass()) && !Component->IsA(UUUIDBillboardComponent::StaticClass()))
					{
						UTextComponent* TextComp = static_cast<UTextComponent*>(Component);
						bool bBillboard = TextComp->IsBillboard();
						if (ImGui::Checkbox("Text Billboard", &bBillboard))
							TextComp->SetBillboard(bBillboard);
					}
				}
				ImGui::Unindent(8.0f);
			}
			if (UStaticMeshComponent* MeshComp = SelectedActor->GetComponentByClass<UStaticMeshComponent>())
			{
				if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent(8.0f);

					if (UStaticMesh* MeshData = MeshComp->GetStaticMesh())
					{
						// 현재 어떤 모델(Mesh)이 장착되어 있는지 보여줌
						ImGui::TextDisabled("Mesh:");
						ImGui::SameLine();
						ImGui::Text("%s", MeshData->GetAssetPathFileName().c_str());
						ImGui::Spacing();

						// 매니저에서 모든 머티리얼 리스트 가져오기
						TArray<FString> MatNames = FMaterialManager::Get().GetAllMaterialNames();
						uint32 NumSections = MeshData->GetNumSections();

						// 섹션 개수만큼 머티리얼 슬롯(콤보박스) 생성
						for (uint32 i = 0; i < NumSections; ++i)
						{
							std::shared_ptr<FMaterial> CurrentMat = MeshComp->GetMaterial(i);
							std::string CurrentMatName = CurrentMat ? CurrentMat->GetOriginName() : "None";

							ImGui::PushID(i); // ID 충돌 방지
							std::string Label = "Section " + std::to_string(i);

							ImGui::PushItemWidth(180.f); // 콤보박스 너비 조절
							if (ImGui::BeginCombo(Label.c_str(), CurrentMatName.c_str()))
							{
								for (const FString& MatName : MatNames)
								{
									bool bSelected = (CurrentMatName == MatName);
									if (ImGui::Selectable(MatName.c_str(), bSelected))
									{
										// 선택 시 머티리얼 즉시 교체!
										auto SelectedMaterial = FMaterialManager::Get().FindByName(MatName);
										if (SelectedMaterial)
										{
											MeshComp->SetMaterial(i, SelectedMaterial);
										}
									}
									if (bSelected)
									{
										ImGui::SetItemDefaultFocus();
									}
								}
								ImGui::EndCombo();
							}
							ImGui::PopItemWidth();
							ImGui::PopID();
						}
					}
					else
					{
						ImGui::TextDisabled("No Static Mesh Assigned");
					}
					ImGui::Unindent(8.0f);
				}
			}
		}
	}
	ImGui::End();
}
