#include "OutlinerPanel.h"

#include "Editor/Editor.h"
#include "Editor/EditorContext.h"
#include "Core/Misc/Name.h"
#include "Engine/Game/Actor.h"
#include "Engine/Game/CubeActor.h"
#include "Engine/Game/SphereActor.h"
#include "Engine/Game/UnknownActor.h"
#include "Engine/Scene.h"
#include "Input/ContextModeTypes.h"
#include "imgui.h"
#include "Engine/Game/ConeActor.h"
#include "Engine/Game/CylinderActor.h"
#include "Engine/Game/RingActor.h"
#include "Engine/Game/TriangleActor.h"
#include "Engine/Game/SpriteActor.h"
#include "Engine/Game/EffectActor.h"
#include "Engine/Game/TextActor.h"
#include "Engine/Game/SubUVTextureActor.h"


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

    const char* GetSpawnActorTypeLabel(FOutlinerPanel::ESpawnActorType InType)
    {
        switch (InType)
        {
        case FOutlinerPanel::ESpawnActorType::Cube:
            return "CubeActor";
        case FOutlinerPanel::ESpawnActorType::Sphere:
            return "SphereActor";
        case FOutlinerPanel::ESpawnActorType::Cone:
            return "ConeActor";
        case FOutlinerPanel::ESpawnActorType::Cylinder:
            return "CylinderActor";
        case FOutlinerPanel::ESpawnActorType::Ring:
            return "RingActor";
        case FOutlinerPanel::ESpawnActorType::Triangle:
            return "TriangleActor";
        case FOutlinerPanel::ESpawnActorType::Sprite:
            return "SpriteActor";
        case FOutlinerPanel::ESpawnActorType::Effect:
            return "EffectActor";
        case FOutlinerPanel::ESpawnActorType::Text:
            return "TextActor";
        case FOutlinerPanel::ESpawnActorType::SubUVTexture:
            return "SubUVActor";
        default:
            return "Unknown";
        }
    }

    const char* const SpawnActorTypeLabels[] = {
        "CubeActor",     "SphereActor", "ConeActor",   "CylinderActor", "RingActor",
        "TriangleActor", "SpriteActor", "EffectActor", "TextActor",     "SubUVActor"
    };

    AActor* CreateActorByType(FOutlinerPanel::ESpawnActorType InType)
    {
        switch (InType)
        {
        case FOutlinerPanel::ESpawnActorType::Cube:
            return new ACubeActor();
        case FOutlinerPanel::ESpawnActorType::Sphere:
            return new ASphereActor();
        case FOutlinerPanel::ESpawnActorType::Cone:
            return new AConeActor();
        case FOutlinerPanel::ESpawnActorType::Cylinder:
            return new ACylinderActor();
        case FOutlinerPanel::ESpawnActorType::Ring:
            return new ARingActor();
        case FOutlinerPanel::ESpawnActorType::Triangle:
            return new ATriangleActor();
        case FOutlinerPanel::ESpawnActorType::Sprite:
            return new ASpriteActor();
        case FOutlinerPanel::ESpawnActorType::Effect:
            return new AEffectActor();
        case FOutlinerPanel::ESpawnActorType::Text:
            return new ATextActor();
        case FOutlinerPanel::ESpawnActorType::SubUVTexture:
            return new ASubUVTextureActor();
        default:
            return nullptr;
        }
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

const wchar_t* FOutlinerPanel::GetPanelID() const { return L"OutlinerPanel"; }

const wchar_t* FOutlinerPanel::GetDisplayName() const { return L"Outliner"; }

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

void FOutlinerPanel::DrawToolbar()
{
    ImGui::SetNextItemWidth(140.0f);
    int32 SpawnActorTypeIndex = static_cast<int32>(SpawnActorType);
    if (ImGui::Combo("##SpawnActorType", &SpawnActorTypeIndex, SpawnActorTypeLabels,
                     IM_ARRAYSIZE(SpawnActorTypeLabels)))
    {
        SpawnActorType = static_cast<ESpawnActorType>(SpawnActorTypeIndex);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(72.0f);
    ImGui::InputInt("##SpawnActorCount", &SpawnCount, 1, 10);
    if (SpawnCount < 1)
    {
        SpawnCount = 1;
    }

    ImGui::SameLine();
    if (ImGui::Button("Spawn"))
    {
        SpawnActors();
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
                GetContext()->Editor->GetViewportClient().GetSelectionController().IsSelected(
                    Actor);
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

void FOutlinerPanel::SpawnActors() const
{
    if (GetContext() == nullptr || GetContext()->Scene == nullptr)
    {
        return;
    }

    const TArray<AActor*>* Actors = GetContext()->Scene->GetActors();
    const size_t           ExistingActorCount = (Actors != nullptr) ? Actors->size() : 0;
    AActor*                LastSpawnedActor = nullptr;

    for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
    {
        AActor* NewActor = CreateActorByType(SpawnActorType);
        if (NewActor == nullptr)
        {
            continue;
        }

        const size_t ActorIndex = ExistingActorCount + static_cast<size_t>(SpawnIndex) + 1;
        NewActor->Name =
            FString(GetSpawnActorTypeLabel(SpawnActorType)) + " " + std::to_string(ActorIndex);

        if (GetContext()->Editor != nullptr)
        {
            GetContext()->Editor->AddActorToScene(NewActor, false);
        }
        else
        {
            GetContext()->Scene->AddActor(NewActor);
        }

        LastSpawnedActor = NewActor;
    }

    if (LastSpawnedActor != nullptr && GetContext()->Editor != nullptr)
    {
        GetContext()->Editor->GetViewportClient().GetSelectionController().SelectActor(
            LastSpawnedActor, ESelectionMode::Replace);
    }
}