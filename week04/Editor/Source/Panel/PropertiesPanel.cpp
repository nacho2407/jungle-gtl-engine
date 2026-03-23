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

    void DrawRotatorRow(const char* Label, FVector& Value, float Speed = 0.5f)
    {
        FVector EulerDegrees = Value;

        ImGui::PushID(Label);
        ImGui::TextUnformatted(Label);
        ImGui::SameLine(120.0f);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat3("##Value", EulerDegrees.XYZ, Speed))
        {
            // UI는 LH Z-Up 기준 축 회전(X,Y,Z)을 보여 주고,
            // 엔진 내부 Rotator(Pitch,Yaw,Roll) 순서로 다시 변환해 저장합니다.
            Value = EulerDegrees;
        }
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
        CachedTargetComponent = nullptr;
        DrawNoSelectionState();
        ImGui::End();
        return;
    }

    if (TargetComponent == nullptr)
    {
        CachedTargetComponent = nullptr;
        DrawUnsupportedSelectionState();
        ImGui::End();
        return;
    }

    SyncEditTransformFromTarget(TargetComponent);
    DrawSelectionSummary(SelectedActor, TargetComponent);
    ImGui::Separator();
    DrawTransformEditor(TargetComponent);

    ImGui::End();
}

void FPropertiesPanel::SetTarget(const FVector& Location, const FVector& Rotation, const FVector& Scale)
{
    EditLocation = Location;
    EditRotation = Rotation;
    EditScale = Scale;
}

void FPropertiesPanel::SyncEditTransformFromTarget(
    Engine::Component::USceneComponent* TargetComponent)
{
    if (TargetComponent == nullptr)
    {
        CachedTargetComponent = nullptr;
        return;
    }

    const FVector CurrentLocation = TargetComponent->GetRelativeLocation();
    const FQuat CurrentRotation = TargetComponent->GetRelativeQuaternion();
    const FVector CurrentScale = TargetComponent->GetRelativeScale3D();

    const bool bTargetChanged = (CachedTargetComponent != TargetComponent);
    const bool bLocationChangedExternally = (EditLocation != CurrentLocation);
    const bool bScaleChangedExternally = (EditScale != CurrentScale);
    const bool bRotationChangedExternally =
        !CurrentRotation.Equals(FRotator::MakeFromEuler(EditRotation).Quaternion());

    // 회전 편집 중에는 패널이 표시 중인 Euler 값을 유지해야 합니다.
    // 매 프레임 quat -> euler 로 다시 변환하면 Pitch(Y축)가 +/-90 부근에서
    // 등가 회전의 다른 표현으로 튀어 특이점처럼 보이는 현상이 발생합니다.
    if (bTargetChanged || bLocationChangedExternally || bScaleChangedExternally
        || bRotationChangedExternally)
    {
        SetTarget(CurrentLocation, CurrentRotation.Euler(), CurrentScale);
        CachedTargetComponent = TargetComponent;
    }
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
    Engine::Component::USceneComponent* TargetComponent)
{
    DrawVectorRow("Location", EditLocation, 0.1f);
    DrawRotatorRow("Rotation", EditRotation, 0.5f);
    DrawVectorRow("Scale", EditScale, 0.01f);

    if (ImGui::Button("Reset Transform"))
    {
        EditLocation = FVector::ZeroVector;
        EditRotation = FVector::ZeroVector;
        EditScale = FVector::OneVector;
    }

    if (TargetComponent->GetRelativeLocation() != EditLocation)
    {
        TargetComponent->SetRelativeLocation(EditLocation);
    }

    if (!TargetComponent->GetRelativeQuaternion().Equals(
            FRotator::MakeFromEuler(EditRotation).Quaternion()))
    {
        TargetComponent->SetRelativeRotation(FRotator::MakeFromEuler(EditRotation));
    }

    if (TargetComponent->GetRelativeScale3D() != EditScale)
    {
        TargetComponent->SetRelativeScale3D(EditScale);
    }
}
