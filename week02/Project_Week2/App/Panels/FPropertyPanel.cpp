#include "Panels/FPropertyPanel.h"

#include "FApplication.h"
#include "Actor/AActor.h"
#include "Component/USceneComponent.h"
#include "GUI.h"

void FPropertyPanel::Render(FApplication* App)
{
    if (!App)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(320.0f, 10.f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(400.0f, 160.0f), ImGuiCond_Once);

    ImGui::Begin("Jungle Property Property");

    AActor* SelectedActor = App->GetSelectedActor();
    USceneComponent* Root = SelectedActor ? SelectedActor->GetRootComponent() : nullptr;

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