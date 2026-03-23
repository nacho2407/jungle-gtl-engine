#include "PropertiesPanel.h"

#include "Editor/EditorContext.h"
#include "CoreUObject/Object.h"
#include "Core/Misc/Name.h"
#include "Engine/Component/SceneComponent.h"
#include "Engine/Game/Actor.h"
#include "imgui.h"

namespace
{
    FString GetObjectDisplayName(const UObject* Object)
    {
        if (Object == nullptr)
        {
            return {};
        }

        if (Object->Name.IsValid())
        {
            return Object->Name.ToFString();
        }

        return "Unnamed";
    }

    void DrawVectorRow(const char* Label, FVector& Value, float Speed = 0.1f)
    {
        ImGui::PushID(Label);
        ImGui::TextUnformatted(Label);
        ImGui::SameLine(120.0f);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat3("##Value", Value.XYZ, Speed);
        ImGui::PopID();
    }

    void DrawRotatorRow(const char* Label, FRotator& Value, float Speed = 0.5f)
    {
        ImGui::PushID(Label);
        ImGui::TextUnformatted(Label);
        ImGui::SameLine(120.0f);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat3("##Value", &Value.Pitch, Speed);
        ImGui::PopID();
    }
} // namespace

const wchar_t* FPropertiesPanel::GetPanelID() const
{
    return L"PropertiesPanel";
}

const wchar_t* FPropertiesPanel::GetDisplayName() const
{
    return L"Properties";
}

void FPropertiesPanel::Draw()
{
    if (!ImGui::Begin("Properties", nullptr))
    {
        ImGui::End();
        return;
    }

    AActor* SelectedActor = nullptr;
    Engine::Component::USceneComponent* TargetComponent = ResolveTargetComponent(SelectedActor);

    if (GetContext() == nullptr || GetContext()->SelectedObject == nullptr)
    {
        DrawNoSelectionState();
        ImGui::End();
        return;
    }

    if (TargetComponent == nullptr)
    {
        DrawUnsupportedSelectionState();
        ImGui::End();
        return;
    }

    DrawSelectionSummary(SelectedActor, TargetComponent);
    ImGui::Separator();
    DrawTransformEditor(TargetComponent);

    ImGui::End();
}

Engine::Component::USceneComponent* FPropertiesPanel::ResolveTargetComponent(
    AActor*& OutSelectedActor) const
{
    OutSelectedActor = nullptr;

    if (GetContext() == nullptr)
    {
        return nullptr;
    }

    UObject* SelectedObject = GetContext()->SelectedObject;
    if (SelectedObject == nullptr)
    {
        return nullptr;
    }

    if (AActor* SelectedActor = Cast<AActor>(SelectedObject))
    {
        OutSelectedActor = SelectedActor;
        return SelectedActor->GetRootComponent();
    }

    if (Engine::Component::USceneComponent* SelectedComponent =
            Cast<Engine::Component::USceneComponent>(SelectedObject))
    {
        return SelectedComponent;
    }

    return nullptr;
}

void FPropertiesPanel::DrawNoSelectionState() const
{
    ImGui::TextUnformatted("No actor selected.");
    ImGui::Spacing();
    ImGui::TextWrapped("Select an actor or scene component to edit its transform.");
}

void FPropertiesPanel::DrawUnsupportedSelectionState() const
{
    ImGui::TextUnformatted("Selected object has no editable transform.");
    ImGui::Spacing();
    ImGui::TextWrapped("Only Actor root components and SceneComponent selections are supported.");
}

void FPropertiesPanel::DrawSelectionSummary(
    AActor* SelectedActor, Engine::Component::USceneComponent* TargetComponent) const
{
    UObject* SelectedObject = GetContext()->SelectedObject;

    ImGui::Text("Selected: %s", GetObjectDisplayName(SelectedObject).c_str());
    if (SelectedActor != nullptr)
    {
        ImGui::Text("Actor: %s", GetObjectDisplayName(SelectedActor).c_str());
    }

    if (TargetComponent != nullptr)
    {
        ImGui::Text("Component: %s", GetObjectDisplayName(TargetComponent).c_str());
    }
}

void FPropertiesPanel::DrawTransformEditor(
    Engine::Component::USceneComponent* TargetComponent) const
{
    FVector Location = TargetComponent->GetRelativeLocation();
    FRotator Rotation = TargetComponent->GetRelativeRotation();
    FVector Scale = TargetComponent->GetRelativeScale3D();

    DrawVectorRow("Location", Location, 0.1f);
    DrawRotatorRow("Rotation", Rotation, 0.5f);
    DrawVectorRow("Scale", Scale, 0.01f);

    if (ImGui::Button("Reset Transform"))
    {
        Location = FVector::ZeroVector;
        Rotation = FRotator::ZeroRotator;
        Scale = FVector::OneVector;
    }

    if (TargetComponent->GetRelativeLocation() != Location)
    {
        TargetComponent->SetRelativeLocation(Location);
    }

    if (!TargetComponent->GetRelativeRotation().Equals(Rotation))
    {
        TargetComponent->SetRelativeRotation(Rotation);
    }

    if (TargetComponent->GetRelativeScale3D() != Scale)
    {
        TargetComponent->SetRelativeScale3D(Scale);
    }
}
