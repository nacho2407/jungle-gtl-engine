#include "Panels/FPropertyPanel.h"

#include "FApplication.h"
#include "Actor/AActor.h"
#include "Component/USceneComponent.h"
#include "GUI.h"
#include "Component/EGizmoMode.h"

void FPropertyPanel::Render(FApplication* App)
{
    if (!App)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(320.0f, 10.f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(400.0f, 160.0f), ImGuiCond_Once);

    ImGui::Begin("Jungle Property Window");

    AActor* SelectedActor = App->GetSelectedActor();
    USceneComponent* Root = SelectedActor ? SelectedActor->GetRootComponent() : nullptr;

    ImGui::SeparatorText("Gizmo Mode");

    const EGizmoMode CurrentMode = App->GetCurrentGizmoMode();

    if (CurrentMode == EGizmoMode::Translate)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
    }
    if (ImGui::Button("Translate"))
    {
        App->SetGizmoMode(EGizmoMode::Translate);
    }
    if (CurrentMode == EGizmoMode::Translate)
    {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    if (CurrentMode == EGizmoMode::Rotate)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
    }
    if (ImGui::Button("Rotate"))
    {
        App->SetGizmoMode(EGizmoMode::Rotate);
    }
    if (CurrentMode == EGizmoMode::Rotate)
    {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    if (CurrentMode == EGizmoMode::Scale)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
    }
    if (ImGui::Button("Scale"))
    {
        App->SetGizmoMode(EGizmoMode::Scale);
    }
    if (CurrentMode == EGizmoMode::Scale)
    {
        ImGui::PopStyleColor();
    }


    if (Root)
    {
        FVector Translation = Root->GetRelativeLocation();
        FVector Rotation = Root->GetRelativeRotation();
        FVector Scale = Root->GetRelativeScale();

        ImGui::SeparatorText("Transform");

        bool bChanged = false;
        bChanged |= DrawFloat3Control("Translation", Translation, 0.1f);
        bChanged |= DrawFloat3Control("Rotation", Rotation, 0.05f);
        bChanged |= DrawFloat3Control("Scale", Scale, 0.05f);

        if (bChanged)
        {
            Root->SetRelativeLocation(Translation);
            Root->SetRelativeRotation(Rotation);
            Root->SetRelativeScale(Scale);

            App->NotifySelectedActorTransformChanged();
        }
    }

    ImGui::End();
}

bool FPropertyPanel::DrawFloat3Control(const char* Label, FVector& Value, float Speed)
{
    bool bChanged = false;

    ImGui::PushID(Label);

    ImGui::SetNextItemWidth(90.0f);
    bChanged |= ImGui::DragFloat("##X", &Value.X, Speed);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    bChanged |= ImGui::DragFloat("##Y", &Value.Y, Speed);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    bChanged |= ImGui::DragFloat("##Z", &Value.Z, Speed);

    ImGui::SameLine();
    ImGui::Text("%s", Label);

    ImGui::PopID();
    return bChanged;
}