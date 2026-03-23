#include "OutlinerPanel.h"

#include "Editor/Editor.h"
#include "Editor/EditorContext.h"
#include "Core/Misc/Name.h"
#include "Engine/Game/Actor.h"
#include "Engine/Game/CubeActor.h"
#include "Engine/Game/UnknownActor.h"
#include "Engine/Scene.h"
#include "Input/ContextModeTypes.h"
#include "imgui.h"

namespace
{
    constexpr ImVec4 UnknownItemColor = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);

    FString GetBaseActorDisplayName(const AActor* Actor)
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

    bool IsUnknownActor(const AActor* Actor)
    {
        return Actor != nullptr && Actor->IsA(AUnknownActor::GetClass());
    }

    FString GetActorDisplayName(const AActor* Actor)
    {
        FString DisplayName = GetBaseActorDisplayName(Actor);
        if (IsUnknownActor(Actor))
        {
            DisplayName += " (UnknownActor)";
        }

        return DisplayName;
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

    DrawToolbar();
    ImGui::Separator();

    const TArray<AActor*>* Actors = GetContext()->Scene->GetActors();
    if (Actors == nullptr || Actors->empty())
    {
        DrawEmptyState();
        ImGui::End();
        return;
    }

    for (AActor* Actor : *Actors)
    {
        DrawActorRow(Actor);
    }

    ImGui::End();
}

void FOutlinerPanel::DrawToolbar() const
{
    if (ImGui::Button("Add CubeActor"))
    {
        SpawnCubeActor();
    }
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

    bool bIsSelected = false;
    if (GetContext() != nullptr)
    {
        if (GetContext()->Editor != nullptr)
        {
            bIsSelected =
                GetContext()->Editor->GetViewportClient().GetSelectionController().IsSelected(Actor);
        }
        else
        {
            bIsSelected = GetContext()->SelectedObject == Actor;
        }
    }

    const FString Label = GetActorDisplayName(Actor);

    if (IsUnknownActor(Actor))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, UnknownItemColor);
    }

    if (ImGui::Selectable(Label.c_str(), bIsSelected))
    {
        if (GetContext() != nullptr && GetContext()->Editor != nullptr)
        {
            const ESelectionMode SelectionMode =
                ImGui::GetIO().KeyCtrl ? ESelectionMode::Toggle : ESelectionMode::Replace;
            GetContext()->Editor->GetViewportClient().GetSelectionController().SelectActor(
                Actor, SelectionMode);
        }
    }

    if (IsUnknownActor(Actor))
    {
        ImGui::PopStyleColor();
    }
}

void FOutlinerPanel::SpawnCubeActor() const
{
    if (GetContext() == nullptr || GetContext()->Scene == nullptr)
    {
        return;
    }

    ACubeActor* CubeActor = new ACubeActor();
    const TArray<AActor*>* Actors = GetContext()->Scene->GetActors();
    const size_t ActorIndex = (Actors != nullptr) ? (Actors->size() + 1) : 1;
    CubeActor->Name = "CubeActor " + std::to_string(ActorIndex);

    if (GetContext()->Editor != nullptr)
    {
        GetContext()->Editor->AddActorToScene(CubeActor, true);
        return;
    }

    GetContext()->Scene->AddActor(CubeActor);
}
