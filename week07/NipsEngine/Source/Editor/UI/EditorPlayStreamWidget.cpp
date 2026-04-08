#include "EditorPlayStreamWidget.h"
#include "Editor/EditorEngine.h"
#include "ImGui/imgui.h"

void FEditorPlayStreamWidget::Render(float DeltaTime)
{
	// 툴바 스타일 설정 (배경색 및 간격)
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));
	bool IsPlaying = (EditorEngine->GetEditorState() == EEditorState::Play);

	float PlayWidth = ImGui::CalcTextSize(PlayLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	float PauseWidth = ImGui::CalcTextSize(PauseLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	float StopWidth = ImGui::CalcTextSize(StopLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	float Spacing = ImGui::GetStyle().ItemSpacing.x;
	float TotalWidth = (IsPlaying ? PauseWidth : PlayWidth) + StopWidth + Spacing;

	float screenWidth = ImGui::GetIO().DisplaySize.x;
	if (screenWidth > TotalWidth)
	{
		ImGui::SetCursorPosX((screenWidth - TotalWidth) * 0.5f);
	}

	// --- 플레이 버튼 (Play) ---
	if (!IsPlaying)
	{
		// 플레이 버튼 색상 (녹색 계열)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
		if (ImGui::Button(PlayLabel))
		{
			EditorEngine->StartPlaySession();
		}
		ImGui::PopStyleColor();

	}

	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 1.0f, 1.0f));
		// --- 일시정지 버튼 (Pause) ---
		if (ImGui::Button(PauseLabel))
		{
			EditorEngine->PausePlaySession();
		}
		ImGui::PopStyleColor();
	}
	
	ImGui::SameLine();

	// --- 정지 버튼 (Stop) ---
	// 정지 버튼 색상 (붉은색 계열)
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
	if (ImGui::Button(StopLabel))
	{
		EditorEngine->StopPlaySession();
	}
	ImGui::PopStyleColor();

	ImGui::PopStyleVar();
}
