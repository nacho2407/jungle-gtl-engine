#include "EditorPlayStreamWidget.h"
#include "Editor/EditorEngine.h"
#include "ImGui/imgui.h"
#include <algorithm> // std::max 사용을 위해 추가

void FEditorPlayStreamWidget::Render(float DeltaTime)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));
    
	// 그려지는 텍스트의 크기를 1.25배로 키웁니다.
    // (원하는 크기에 맞춰 1.3f, 1.5f 등으로 조절 가능합니다)
    ImGui::SetWindowFontScale(1.1f);

    // 1. 현재 엔진 상태(PIE 상태)
    EEditorState CurrentState = EditorEngine->GetEditorState();
    bool bIsEditing = (CurrentState == EEditorState::Edit);
    bool bIsPlaying = (CurrentState == EEditorState::Play);
    bool bIsPaused  = (CurrentState == EEditorState::Pause);

    const char* CurrentPlayPauseLabel = bIsPlaying ? PauseLabel : (bIsPaused ? ResumeLabel : PlayLabel);

    // 💡 [수정됨] 2. 버튼 크기 고정 및 위아래(Height) 키우기
    // Play, Resume, Pause 중 가장 긴 텍스트의 길이를 기준으로 고정 너비를 계산합니다.
    float MaxLabelWidth = std::max({
        ImGui::CalcTextSize(PlayLabel).x,
        ImGui::CalcTextSize(ResumeLabel).x,
        ImGui::CalcTextSize(PauseLabel).x
    });
    
    // 버튼 텍스트 좌우 여백(+30.0f)과 세로 높이(32.0f)를 넉넉하게 지정합니다.
    float ButtonHeight = 26.0f; 
    ImVec2 PlayBtnSize = ImVec2(MaxLabelWidth + 16.0f, ButtonHeight);
    ImVec2 StopBtnSize = ImVec2(ImGui::CalcTextSize(StopLabel).x + 30.0f, ButtonHeight);

    float Spacing = ImGui::GetStyle().ItemSpacing.x;
    float TotalWidth = PlayBtnSize.x + StopBtnSize.x + Spacing;

    float screenWidth = ImGui::GetIO().DisplaySize.x;
    if (screenWidth > TotalWidth)
    {
        ImGui::SetCursorPosX((screenWidth - TotalWidth) * 0.5f);
    }

    // --- 1번 버튼 (Play / Pause / Resume) ---
    if (bIsPlaying || bIsPaused)
    {
        // 💡 [수정됨] 실행 중(Pause)이거나 일시정지(Resume)일 때는 푸른색 계열 적용
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));        // 기본 파란색
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f)); // 마우스 올렸을 때 (조금 더 밝게)
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));  // 클릭할 때 (가장 밝게)
        
        // 버튼 렌더링 시 계산한 고정 크기(PlayBtnSize)를 넘겨줍니다.
        if (ImGui::Button(CurrentPlayPauseLabel, PlayBtnSize))
        {
            if (bIsPlaying) EditorEngine->PausePlaySession();
            else EditorEngine->StartPlaySession(); 
        }
        ImGui::PopStyleColor(3);
    }
    else
    {
        // 완전히 꺼져있음(Edit): 처음 재생(Play) 버튼 (초록 계열 유지)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
        
        if (ImGui::Button(CurrentPlayPauseLabel, PlayBtnSize))
        {
            EditorEngine->StartPlaySession(); 
        }
        ImGui::PopStyleColor(3);
    }
    
    ImGui::SameLine();

    // --- 2번 버튼 (Stop) ---
    if (bIsEditing) ImGui::BeginDisabled();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
    
    // Stop 버튼도 고정 크기(StopBtnSize)를 적용합니다.
    if (ImGui::Button(StopLabel, StopBtnSize))
    {
        EditorEngine->StopPlaySession();
    }
    ImGui::PopStyleColor(3);

    if (bIsEditing) ImGui::EndDisabled();

    ImGui::PopStyleVar();
}