#include "Editor/UI/EditorScenePanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "GameFramework/World.h"

#include "ImGui/imgui.h"
#include "Profiling/Stats.h"

#include <cstdio>

void FEditorScenePanel::Initialize(UEditorEngine *InEditorEngine)
{
    FEditorPanel::Initialize(InEditorEngine);
}

void FEditorScenePanel::RefreshSceneFileList()
{
}

void FEditorScenePanel::Render(float DeltaTime)
{
    (void)DeltaTime;

    if (!EditorEngine)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(460.0f, 420.0f), ImGuiCond_Once);
    ImGui::Begin("Scene Manager");

    RenderActorOutliner();

    ImGui::End();
}

void FEditorScenePanel::RenderActorOutliner()
{
    SCOPE_STAT_CAT("ScenePanel::ActorOutliner", "5_UI");

    UWorld *World = EditorEngine->GetWorld();
    if (!World)
    {
        ImGui::TextDisabled("No active world.");
        return;
    }

    const TArray<AActor *> &Actors = World->GetActors();
    FSelectionManager &Selection = EditorEngine->GetSelectionManager();

    ValidActorIndices.clear();
    ValidActorIndices.reserve(Actors.size());
    for (int32 i = 0; i < static_cast<int32>(Actors.size()); ++i)
    {
        if (Actors[i] != nullptr)
        {
            ValidActorIndices.push_back(i);
        }
    }

    TArray<AActor *> ActorsToDelete;
    ActorsToDelete.reserve(ValidActorIndices.size());
    for (int32 Row = 0; Row < static_cast<int32>(ValidActorIndices.size()); ++Row)
    {
        AActor *Actor = Actors[ValidActorIndices[Row]];
        if (Actor && Selection.IsSelected(Actor))
        {
            ActorsToDelete.push_back(Actor);
        }
    }

    const int32 SelectedCount = static_cast<int32>(ActorsToDelete.size());

    if (ImGui::Button("Clear Selection"))
    {
        Selection.ClearSelection();
    }

    ImGui::SameLine();

    char DeleteLabel[64];
    std::snprintf(DeleteLabel, sizeof(DeleteLabel), "Delete Selected (%d)", SelectedCount);

    const bool bDeleteDisabled = (SelectedCount == 0);
    if (bDeleteDisabled)
    {
        ImGui::BeginDisabled();
    }

    // Delete 버튼만 붉은 계열 적용
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.22f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.42f, 0.12f, 0.12f, 1.0f));

    if (ImGui::Button(DeleteLabel))
    {
        // 선택 목록이 파괴 대상 액터 포인터를 들고 있으면 프록시 선택 해제/기즈모 동기화 중에
        // dangling 포인터를 참조할 수 있으므로 먼저 Selection을 비웁니다.
        Selection.ClearSelection();

        World->BeginDeferredPickingBVHUpdate();
        for (AActor *Actor : ActorsToDelete)
        {
            if (Actor && Actor->GetWorld() == World)
            {
                World->DestroyActor(Actor);
            }
        }
        World->EndDeferredPickingBVHUpdate();
    }

    ImGui::PopStyleColor(3);

    if (bDeleteDisabled)
    {
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    const ImGuiTableFlags TableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter |
                                       ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                       ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY |
                                       ImGuiTableFlags_NoPadInnerX;

    auto DrawCenteredTextInColumn = [](const char *Text) {
        const float ColumnWidth = ImGui::GetColumnWidth();
        const float TextWidth = ImGui::CalcTextSize(Text).x;
        const float OffsetX = (ColumnWidth - TextWidth) * 0.5f;

        if (OffsetX > 0.0f)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + OffsetX);
        }
        ImGui::TextUnformatted(Text);
    };

    auto DrawCenteredCheckboxInColumn = [](const char *Id, bool *bValue) -> bool {
        // Checkbox는 label이 없더라도 실제 width가 frame height보다 약간 더 크게 잡히는 경우가 있어서
        // CalcItemWidth류 대신 프레임 높이 + 내부 여백을 조금 반영해서 중앙 맞춤
        const float CheckboxSize = ImGui::GetFrameHeight();
        const float ColumnWidth = ImGui::GetColumnWidth();

        const ImGuiStyle &Style = ImGui::GetStyle();
        const float VisualWidth = CheckboxSize + Style.FramePadding.x * 0.5f;
        const float OffsetX = (ColumnWidth - VisualWidth) * 0.5f;

        if (OffsetX > 0.0f)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + OffsetX);
        }

        return ImGui::Checkbox(Id, bValue);
    };

    auto DrawPaddedSelectableInColumn = [](const char *Label, bool bSelected, float LeftPadding) -> bool {
        const float StartX = ImGui::GetCursorPosX();
        ImGui::SetCursorPosX(StartX + LeftPadding);
        return ImGui::Selectable(Label, bSelected, ImGuiSelectableFlags_SpanAllColumns);
    };

    if (ImGui::BeginTable("SceneManagerTable", 3, TableFlags, ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed, 68.0f);
        ImGui::TableSetupColumn("Actor", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 74.0f);

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

        ImGui::TableSetColumnIndex(0);
        DrawCenteredTextInColumn("Select");

        ImGui::TableSetColumnIndex(1);
        DrawCenteredTextInColumn("Actor");

        ImGui::TableSetColumnIndex(2);
        DrawCenteredTextInColumn("Visible");

        for (int32 Row = 0; Row < static_cast<int32>(ValidActorIndices.size()); ++Row)
        {
            AActor *Actor = Actors[ValidActorIndices[Row]];
            if (!Actor)
            {
                continue;
            }

            const bool bSelected = Selection.IsSelected(Actor);
            ImGui::PushID(Actor);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            bool bChecked = bSelected;
            if (DrawCenteredCheckboxInColumn("##selected", &bChecked))
            {
                if (ImGui::GetIO().KeyShift)
                {
                    Selection.ToggleSelect(Actor);
                }
                else
                {
                    if (bChecked)
                    {
                        Selection.Select(Actor);
                    }
                    else
                    {
                        Selection.ClearSelection();
                    }
                }
            }

            ImGui::TableSetColumnIndex(1);
            FString ActorName = Actor->GetFName().ToString();
            if (ActorName.empty())
            {
                ActorName = Actor->GetClass()->GetName();
            }
            if (DrawPaddedSelectableInColumn(ActorName.c_str(), bSelected, 8.0f))
            {
                if (ImGui::GetIO().KeyShift)
                {
                    Selection.ToggleSelect(Actor);
                }
                else
                {
                    Selection.Select(Actor);
                }
            }

            ImGui::TableSetColumnIndex(2);
            bool bVisible = Actor->IsVisible();
            if (DrawCenteredCheckboxInColumn("##visible", &bVisible))
            {
                Actor->SetVisible(bVisible);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}