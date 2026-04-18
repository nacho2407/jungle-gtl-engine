#include "Editor/UI/EditorControlPanel.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Core/Common.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Engine/Profiling/Timer.h"
#include "GameFramework/DecalActor.h"
#include "GameFramework/FakeLightActor.h"
#include "GameFramework/FireballActor.h"
#include "GameFramework/HeightFogActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "ImGui/imgui.h"

#include <algorithm>

#define SEPARATOR()                                                                                                    \
    ;                                                                                                                  \
    ImGui::Spacing();                                                                                                  \
    ImGui::Spacing();                                                                                                  \
    ImGui::Separator();                                                                                                \
    ImGui::Spacing();                                                                                                  \
    ImGui::Spacing();

void FEditorControlPanel::Initialize(UEditorEngine *InEditorEngine)
{
    FEditorPanel::Initialize(InEditorEngine);
    SelectedSpawnActorType = static_cast<int32>(ESpawnActorType::Cube);
}

void FEditorControlPanel::Render(float DeltaTime)
{
    (void)DeltaTime;
    if (!EditorEngine)
    {
        return;
    }

    ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 420.0f), ImGuiCond_Once);

    ImGui::Begin("Control Panel");

    ImGui::SeparatorText("Place Actor");

    const float FieldWidth = 220.0f;
    const float LabelOffset = 12.0f;
    const float SpawnButtonWidth = 110.0f;

    // Type
    ImGui::PushItemWidth(FieldWidth);
    ImGui::Combo("##Type", &SelectedSpawnActorType, SpawnActorTypeNames, IM_ARRAYSIZE(SpawnActorTypeNames));
    ImGui::PopItemWidth();
    ImGui::SameLine(0.0f, LabelOffset);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Type");

    // Count
    ImGui::PushItemWidth(FieldWidth);
    ImGui::DragInt("##Count", &NumberOfSpawnedActors, 1.0f, 1, 100);
    ImGui::PopItemWidth();
    NumberOfSpawnedActors = (std::max)(1, NumberOfSpawnedActors);
    ImGui::SameLine(0.0f, LabelOffset);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Count");

    // Spawn
    if (ImGui::Button("Spawn", ImVec2(SpawnButtonWidth, 0.0f)))
    {
        UWorld *World = EditorEngine->GetWorld();
        if (World)
        {
            const ESpawnActorType SpawnActorType = static_cast<ESpawnActorType>(SelectedSpawnActorType);

            for (int32 i = 0; i < NumberOfSpawnedActors; i++)
            {
                switch (SpawnActorType)
                {
                case ESpawnActorType::Cube: {
                    AStaticMeshActor *Actor = World->SpawnActor<AStaticMeshActor>();
                    Actor->SetActorLocation(FVector::ZeroVector);
                    Actor->InitDefaultComponents("Data/BasicShape/Cube.OBJ");
                    World->InsertActorToOctree(Actor);
                    break;
                }
                case ESpawnActorType::Sphere: {
                    AStaticMeshActor *Actor = World->SpawnActor<AStaticMeshActor>();
                    Actor->SetActorLocation(FVector::ZeroVector);
                    Actor->InitDefaultComponents("Data/BasicShape/Sphere.OBJ");
                    World->InsertActorToOctree(Actor);
                    break;
                }
                case ESpawnActorType::StaticMesh: {
                    AStaticMeshActor *Actor = World->SpawnActor<AStaticMeshActor>();
                    Actor->SetActorLocation(FVector::ZeroVector);
                    Actor->InitDefaultComponents("Data/BasicShape/Cube.OBJ");
                    World->InsertActorToOctree(Actor);
                    break;
                }
                case ESpawnActorType::Decal: {
                    ADecalActor *Actor = World->SpawnActor<ADecalActor>();
                    Actor->InitDefaultComponents();
                    Actor->SetActorLocation(FVector::ZeroVector);
                    World->InsertActorToOctree(Actor);
                    break;
                }
                case ESpawnActorType::HeightFog: {
                    AHeightFogActor *Actor = World->SpawnActor<AHeightFogActor>();
                    Actor->InitDefaultComponents();
                    Actor->SetActorLocation(FVector::ZeroVector);
                    break;
                }
                case ESpawnActorType::FakeLight: {
                    AFakeLightActor *Actor = World->SpawnActor<AFakeLightActor>();
                    Actor->InitDefaultComponents();
                    Actor->SetActorLocation(FVector::ZeroVector);
                    break;
                }
                case ESpawnActorType::Fireball: {
                    AFireballActor *Actor = World->SpawnActor<AFireballActor>();
                    Actor->InitDefaultComponents();
                    Actor->SetActorLocation(FVector::ZeroVector);
                    break;
                }
                default:
                    break;
                }
            }
        }
    }

    SEPARATOR();

    ImGui::SeparatorText("Camera");
    UCameraComponent *Camera = EditorEngine->GetCamera();
    if (Camera)
    {
        // 카메라 입력칸도 Place Actor와 같은 길이로 맞춥니다.
        const float ControlWidth = 220.0f;

        if (ImGui::BeginTable("CameraTable", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("CameraControls", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("CameraLabels", ImGuiTableColumnFlags_WidthStretch);

            float CameraFOV_Deg = Camera->GetFOV() * RAD_TO_DEG;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushItemWidth(ControlWidth);
            if (ImGui::DragFloat("##FOV", &CameraFOV_Deg, 0.5f, 1.0f, 90.0f))
            {
                Camera->SetFOV(CameraFOV_Deg * DEG_TO_RAD);
            }
            ImGui::PopItemWidth();
            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("FOV");

            FVector CamPos = Camera->GetWorldLocation();
            float CameraLocation[3] = {CamPos.X, CamPos.Y, CamPos.Z};
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushItemWidth(ControlWidth);
            if (ImGui::DragFloat3("##Location", CameraLocation, 0.1f))
            {
                Camera->SetWorldLocation(FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]));
            }
            ImGui::PopItemWidth();
            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Location");

            FRotator CamRot = Camera->GetRelativeRotation();
            float CameraRotation[3] = {CamRot.Roll, CamRot.Pitch, CamRot.Yaw};
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushItemWidth(ControlWidth);
            if (ImGui::DragFloat3("##Rotation", CameraRotation, 0.1f))
            {
                Camera->SetRelativeRotation(FRotator(CameraRotation[1], CameraRotation[2], CamRot.Roll));
            }
            ImGui::PopItemWidth();
            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Rotation");

            if (FLevelEditorViewportClient* ActiveVC = EditorEngine->GetActiveViewport())
            {
                FViewportRenderOptions& Opts = ActiveVC->GetRenderOptions();

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushItemWidth(ControlWidth);
                ImGui::SliderFloat("##MoveSensitivity", &Opts.CameraMoveSensitivity, 0.1f, 5.0f, "%.1f");
                ImGui::PopItemWidth();
                ImGui::TableSetColumnIndex(1);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Move Sensitivity");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushItemWidth(ControlWidth);
                ImGui::SliderFloat("##RotateSensitivity", &Opts.CameraRotateSensitivity, 0.1f, 5.0f, "%.1f");
                ImGui::PopItemWidth();
                ImGui::TableSetColumnIndex(1);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Rotate Sensitivity");
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}
