#include "ShortcutsPanel.h"

#include "imgui.h"

namespace
{
    struct FShortcutEntry
    {
        const char* Keys = "";
        const char* Description = "";
    };

    void DrawShortcutSection(const char* SectionTitle, const FShortcutEntry* Entries,
                             size_t EntryCount)
    {
        if (!ImGui::CollapsingHeader(SectionTitle, ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        if (ImGui::BeginTable(SectionTitle, 2,
                              ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (size_t Index = 0; Index < EntryCount; ++Index)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(Entries[Index].Keys);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", Entries[Index].Description);
            }

            ImGui::EndTable();
        }
    }
} // namespace

const wchar_t* FShortcutsPanel::GetPanelID() const
{
    return L"ShortcutsPanel";
}

const wchar_t* FShortcutsPanel::GetDisplayName() const
{
    return L"Shortcuts";
}

void FShortcutsPanel::Draw()
{
    static constexpr FShortcutEntry ViewportEntries[] = {
        {"Mouse Right Drag", "뷰포트 카메라 회전"},
        {"Mouse Middle Drag", "뷰포트 카메라 팬 이동"},
        {"Alt + Mouse Left Drag", "선택 대상을 기준으로 오빗 회전"},
        {"Alt + Mouse Right Drag", "카메라 돌리 인/아웃"},
        {"Mouse Wheel", "원근 카메라 FOV 또는 직교 카메라 높이 조절"},
        {"Mouse Wheel while rotating", "카메라 이동 속도 조절"},
        {"W / A / S / D / Q / E", "카메라 이동 (회전 중일 때만 적용)"},
        {"F", "현재 선택된 Actor 쪽으로 카메라 포커스"}};

    static constexpr FShortcutEntry SelectionEntries[] = {
        {"Mouse Left Click", "Actor 단일 선택"},
        {"Shift + Mouse Left Click", "선택 추가"},
        {"Ctrl + Mouse Left Click", "선택 토글"},
        {"Ctrl + Alt + Drag", "박스 선택"},
        {"Ctrl + Alt + Shift + Drag", "기존 선택에 박스 선택 추가"}};

    static constexpr FShortcutEntry GizmoEntries[] = {
        {"W / E / R", "기즈모 모드 전환 (이동 / 회전 / 스케일)"},
        {"Mouse Left Drag", "기즈모 축 드래그 조작"},
        {"Space", "기즈모 타입 순환"},
        {"X", "월드/로컬 기즈모 모드 전환"}};

    static constexpr FShortcutEntry EditorEntries[] = {
        {"1 / 2 / 3", "뷰 모드 전환 (Lit / Unlit / Wireframe)"},
        {"F2", "선택된 대상의 이름 변경"},
        {"Delete", "선택된 Actor 또는 Component 삭제"}};

    bool bOpen = IsOpen();
    if (ImGui::Begin("Shortcuts", &bOpen))
    {
        ImGui::TextWrapped("현재 코드상 실제로 동작하는 에디터 단축키만 정리했습니다.");
        ImGui::Spacing();

        DrawShortcutSection("Viewport Navigation", ViewportEntries,
                            IM_ARRAYSIZE(ViewportEntries));
        DrawShortcutSection("Selection", SelectionEntries, IM_ARRAYSIZE(SelectionEntries));
        DrawShortcutSection("Gizmo", GizmoEntries, IM_ARRAYSIZE(GizmoEntries));
        DrawShortcutSection("Editor", EditorEntries, IM_ARRAYSIZE(EditorEntries));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextWrapped(
            "참고: ImGui 입력창이 키보드를 잡고 있을 때는 일부 단축키가 동작하지 않습니다.");
    }
    ImGui::End();

    if (!bOpen)
    {
        SetOpen(false);
    }
}
