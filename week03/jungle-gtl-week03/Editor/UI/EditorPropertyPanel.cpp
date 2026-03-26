#include "EditorPropertyPanel.h"

#include <algorithm>
#include <cstring>

#include "ImGui/imgui.h"

EditorPropertyPanel::EditorPropertyPanel(Editor* parent) : editor(parent)
{
}

EditorPropertyPanel::~EditorPropertyPanel()
{
}

void EditorPropertyPanel::Draw(USceneComponent* selectedComponent)
{
	//마지막으로 픽킹한 애만 적용됨.
	ImGui::Begin("Jungle Property Panel");

	if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete) && !editor->GetSelectedComponents().empty())
	{
		editor->RemoveSelectedComponents();
		ImGui::End();
		return;
	}

	if (selectedComponent)
	{
		SyncNameBuffer(selectedComponent);
		DrawNameProperty(selectedComponent);
		const bool bTransformChanged = DrawTransformProperties(selectedComponent);

		if (bTransformChanged)
		{
			// primitive transform 갱신 필요함
			if (UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(selectedComponent))
			{
				primitive->MarkRenderStateDirty();
			}
		}

		if (ImGui::Button("Delete", { 100, 30 }))
		{
			editor->RemoveSelectedComponents();
			ImGui::End();
			return;
		}

		if (UBillboardSubUVComp* subUVComponent = Cast<UBillboardSubUVComp>(selectedComponent))
		{
			DrawSubUVProperties(subUVComponent);
		}
		else if (UTextMeshComponent* textMeshComponent = Cast<UTextMeshComponent>(selectedComponent))
		{
			SyncTextBuffer(textMeshComponent);
			DrawTextMeshProperties(textMeshComponent);
		}
	}
	else
	{
		LastSelectedUUID = 0;
		NameBuffer.fill('\0');
		LastTextUUID = 0;
		TextBuffer.fill('\0');
	}

	ImGui::End();
}

void EditorPropertyPanel::RemoveSelected()
{
	editor->RemoveSelectedComponents();
}

/**
 * @brief 선택된 오브젝트의 이름 편집 버퍼를 동기화합니다.
 *
 * @param selectedComponent 현재 선택된 scene component
 */
void EditorPropertyPanel::SyncNameBuffer(USceneComponent* selectedComponent)
{
	if (selectedComponent == nullptr)
	{
		LastSelectedUUID = 0;
		NameBuffer.fill('\0');
		return;
	}

	const FString PlainName = selectedComponent->Name.GetPlainDisplayName();
	if (LastSelectedUUID == selectedComponent->UUID && PlainName == FString(NameBuffer.data()))
	{
		return;
	}

	// 선택 변경 또는 외부 이름 변경 시 버퍼 재동기화함
	LastSelectedUUID = selectedComponent->UUID;
	NameBuffer.fill('\0');
	const size_t CopyLength = (std::min)(PlainName.size(), NameBuffer.size() - 1);
	std::memcpy(NameBuffer.data(), PlainName.c_str(), CopyLength);
	NameBuffer[CopyLength] = '\0';
}

/**
 * @brief 선택된 오브젝트의 이름 UI를 그립니다.
 *
 * @param selectedComponent 현재 선택된 scene component
 */
void EditorPropertyPanel::DrawNameProperty(USceneComponent* selectedComponent)
{
	if (selectedComponent == nullptr)
	{
		return;
	}

	ImGui::Text("Object UUID : %u", selectedComponent->UUID);
	if (ImGui::InputText("Name", NameBuffer.data(), NameBuffer.size()))
	{
		// 입력 중간 조합도 바로 이름에 반영함
		selectedComponent->SetName(FString(NameBuffer.data()));
	}

	ImGui::Text("Resolved Name : %s", selectedComponent->Name.GetDisplayName().c_str());
}

/**
 * @brief 선택된 오브젝트의 transform UI를 그립니다.
 *
 * @param selectedComponent 현재 선택된 scene component
 * @return transform 값이 변경되었는지 여부
 */
bool EditorPropertyPanel::DrawTransformProperties(USceneComponent* selectedComponent)
{
	if (selectedComponent == nullptr)
	{
		return false;
	}

	bool bTransformChanged = false;

	FVector Location = selectedComponent->GetRelativeLocation();
	if (ImGui::InputFloat3("Location", &Location.x))
	{
		// 위치 즉시 반영함
		selectedComponent->SetRelativeLocation(Location);
		bTransformChanged = true;
	}

	FRotator Rotation = selectedComponent->GetRelativeRotation();
	if (ImGui::InputFloat3("Rotation", &Rotation.Roll))
	{
		// 회전 즉시 반영함
		selectedComponent->SetRelativeRotation(Rotation);
		bTransformChanged = true;
	}

	FVector Scale = selectedComponent->GetRelativeScale3D();
	if (ImGui::InputFloat3("Scale", &Scale.x))
	{
		// 스케일 즉시 반영함
		selectedComponent->SetRelativeScale3D(Scale);
		bTransformChanged = true;
	}

	return bTransformChanged;
}

void EditorPropertyPanel::DrawSubUVProperties(UBillboardSubUVComp* subUVComponent)
{
	if (subUVComponent == nullptr)
	{
		return;
	}
	ImGui::Separator();
	if (!ImGui::CollapsingHeader("SubUV", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	ImGui::TextUnformatted("Size follows Transform Scale.");
	float framesPerSecond = subUVComponent->GetFramesPerSecond();
	bool bLoop = subUVComponent->IsLooping();
	bool bPlaybackChanged = false;
	bPlaybackChanged |= ImGui::DragFloat("FPS", &framesPerSecond, 1.0f, 0.0f, 100.0f);
	bPlaybackChanged |= ImGui::Checkbox("Loop", &bLoop);
	if (bPlaybackChanged)
	{
		subUVComponent->SetPlayback(framesPerSecond, bLoop);
	}
}

void EditorPropertyPanel::SyncTextBuffer(UTextMeshComponent* textMeshComponent)
{
	if (textMeshComponent == nullptr)
	{
		LastTextUUID = 0;
		TextBuffer.fill('\0');
		return;
	}

	const FString& CurrentText = textMeshComponent->GetText();
	if (LastTextUUID == textMeshComponent->UUID && CurrentText == FString(TextBuffer.data()))
	{
		return;
	}

	LastTextUUID = textMeshComponent->UUID;
	TextBuffer.fill('\0');
	const size_t CopyLength = (std::min)(CurrentText.size(), TextBuffer.size() - 1);
	std::memcpy(TextBuffer.data(), CurrentText.c_str(), CopyLength);
	TextBuffer[CopyLength] = '\0';
}

void EditorPropertyPanel::DrawTextMeshProperties(UTextMeshComponent* textMeshComponent)
{
	if (textMeshComponent == nullptr)
	{
		return;
	}

	ImGui::Separator();
	if (!ImGui::CollapsingHeader("TextMesh", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	if (ImGui::InputText("Text", TextBuffer.data(), TextBuffer.size()))
	{
		textMeshComponent->SetText(FString(TextBuffer.data()));
	}
}
