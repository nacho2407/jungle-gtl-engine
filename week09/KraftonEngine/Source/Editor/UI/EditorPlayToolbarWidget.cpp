#include "Editor/UI/EditorPlayToolbarWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/PIE/PIETypes.h"
#include "Platform/Paths.h"
#include "ImGui/imgui.h"
#include "WICTextureLoader.h"

#include <d3d11.h>

void FEditorPlayToolbarWidget::Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice)
{
	Editor = InEditor;
	if (!InDevice) return;

	const std::wstring IconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/Icons/");

	DirectX::CreateWICTextureFromFile(
		InDevice, (IconDir + L"icon_playInSelectedViewport_16x.png").c_str(),
		nullptr, &PlayIcon);

	DirectX::CreateWICTextureFromFile(
		InDevice, (IconDir + L"generic_stop_16x.png").c_str(),
		nullptr, &StopIcon);
}

void FEditorPlayToolbarWidget::Release()
{
	if (PlayIcon) { PlayIcon->Release(); PlayIcon = nullptr; }
	if (StopIcon) { StopIcon->Release(); StopIcon = nullptr; }
	Editor = nullptr;
}

void FEditorPlayToolbarWidget::Render(float Width)
{
	if (!Editor) return;

	const ImVec2 CursorStart = ImGui::GetCursorScreenPos();

	// 툴바 배경
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(
		CursorStart,
		ImVec2(CursorStart.x + Width, CursorStart.y + ToolbarHeight),
		IM_COL32(40, 40, 40, 255));

	// 내부 버튼 영역을 상자 중앙에 배치
	const float ButtonPadding = (ToolbarHeight - IconSize) * 0.5f;
	ImGui::SetCursorScreenPos(ImVec2(CursorStart.x + ButtonPadding, CursorStart.y + ButtonPadding));

	const bool bPlaying = Editor->IsPlayingInEditor();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.3f));

	auto DrawIconButton = [&](const char* Id, ID3D11ShaderResourceView* Icon, const char* FallbackLabel, bool bDisabled, ImU32 TintColor) -> bool
	{
		if (bDisabled)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
		}

		bool bClicked = false;
		if (Icon)
		{
			ImGui::PushID(Id);
			ImGui::InvisibleButton("##IconButton", ImVec2(IconSize, IconSize));
			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Max = ImGui::GetItemRectMax();
			ImDrawList* LocalDrawList = ImGui::GetWindowDrawList();
			if (ImGui::IsItemHovered())
			{
				LocalDrawList->AddRectFilled(Min, Max, IM_COL32(255, 255, 255, 24), 4.0f);
			}
			LocalDrawList->AddImage((ImTextureID)Icon, Min, Max, ImVec2(0, 0), ImVec2(1, 1), TintColor);
			bClicked = ImGui::IsItemClicked();
			ImGui::PopID();
		}
		else
		{
			bClicked = ImGui::Button(FallbackLabel, ImVec2(IconSize + 8, IconSize + 8));
		}

		if (bDisabled)
		{
			ImGui::PopStyleVar();
			bClicked = false;
		}
		return bClicked;
	};

	if (DrawIconButton("##PIE_Play", PlayIcon, "Play", /*bDisabled=*/bPlaying, IM_COL32(70, 210, 90, 255)))
	{
		FRequestPlaySessionParams Params;
		Params.PlayMode = EPIEPlayMode::PlayInViewport;
		Editor->RequestPlaySession(Params);
	}

	ImGui::SameLine(0.0f, ButtonSpacing);

	if (DrawIconButton("##PIE_Stop", StopIcon, "Stop", /*bDisabled=*/!bPlaying, IM_COL32(220, 70, 70, 255)))
	{
		Editor->StopPlayInEditorImmediate();
	}

	ImGui::PopStyleColor(3);

	// 툴바 아래에 실제 레이아웃 공간을 확보합니다.
	ImGui::SetCursorScreenPos(ImVec2(CursorStart.x, CursorStart.y));
	ImGui::Dummy(ImVec2(Width, ToolbarHeight));
}
