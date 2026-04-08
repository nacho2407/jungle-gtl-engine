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
	ID3D11ShaderResourceView* Icon = bPlaying ? StopIcon : PlayIcon;
	const char* Id = bPlaying ? "##PIE_Stop" : "##PIE_Play";

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.3f));

	bool bClicked = false;
	if (Icon)
	{
		bClicked = ImGui::ImageButton(
			Id,
			reinterpret_cast<ImTextureID>(Icon),
			ImVec2(IconSize, IconSize));
	}
	else
	{
		bClicked = ImGui::Button(bPlaying ? "Stop" : "Play", ImVec2(IconSize + 8, IconSize + 8));
	}

	ImGui::PopStyleColor(3);

	if (bClicked)
	{
		if (bPlaying)
		{
			Editor->RequestEndPlayMap();
		}
		else
		{
			FRequestPlaySessionParams Params;
			Editor->RequestPlaySession(Params);
		}
	}

	// 다음 콘텐츠는 툴바 아래로 이어지도록 커서 복원
	ImGui::SetCursorScreenPos(ImVec2(CursorStart.x, CursorStart.y + ToolbarHeight));
}
