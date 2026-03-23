#include "OutlinerPanel.h"

#include "Editor/EditorContext.h"
#include "Core/Misc/Name.h"
#include "Engine/Game/Actor.h"
#include "Engine/Scene.h"
#include "imgui.h"

namespace
{
    FString GetActorDisplayName(const AActor* Actor)
    {
        if (Actor == nullptr)
        {
            return {};
        }

        if (Actor->Name.IsValid())
        {
            return Actor->Name.ToFString();
        }

        return "Actor " + std::to_string(Actor->UUID);
    }
} // namespace

const wchar_t* FOutlinerPanel::GetPanelID() const
{
    return L"OutlinerPanel";
}

const wchar_t* FOutlinerPanel::GetDisplayName() const
{
    return L"Outliner";
}

void FOutlinerPanel::Draw()
{
    if (!ImGui::Begin("Outliner", nullptr))
    {
        ImGui::End();
        return;
    }

    if (GetContext() == nullptr || GetContext()->Scene == nullptr)
    {
        DrawEmptyState();
        ImGui::End();
        return;
    }

    const TArray<AActor*>& Actors = GetContext()->Scene->GetActors();
    if (Actors.empty())
    {
        DrawEmptyState();
        ImGui::End();
        return;
    }

    for (AActor* Actor : Actors)
    {
        DrawActorRow(Actor);
    }

    ImGui::End();
}

void FOutlinerPanel::DrawEmptyState() const
{
    ImGui::TextUnformatted("Current scene has no actors.");
    ImGui::Spacing();
    ImGui::TextWrapped("Actors added to the current scene will appear here.");
}

void FOutlinerPanel::DrawActorRow(AActor* Actor) const
{
    if (Actor == nullptr)
    {
        return;
    }

    const bool bIsSelected =
        GetContext() != nullptr && GetContext()->SelectedObject == Actor;
    const FString Label = GetActorDisplayName(Actor);

    if (ImGui::Selectable(Label.c_str(), bIsSelected))
    {
        // Outliner에서 액터를 클릭하면 Properties 패널이 같은 선택을 바로 볼 수 있게 연결합니다.
        GetContext()->SelectedObject = Actor;
    }
}
