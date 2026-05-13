#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Undo/EditorUndoSystem.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "GameFramework/World.h"
#include "Math/Utils.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>

namespace
{
constexpr float MaxDebugCameraSpeedMultiplier = 20.0f;

FString FormatHistoryBytes(size_t Bytes)
{
    char Buffer[64];
    const double Value = static_cast<double>(Bytes);
    if (Bytes >= 1024ull * 1024ull)
    {
        snprintf(Buffer, sizeof(Buffer), "%.2f MB", Value / (1024.0 * 1024.0));
    }
    else if (Bytes >= 1024ull)
    {
        snprintf(Buffer, sizeof(Buffer), "%.2f KB", Value / 1024.0);
    }
    else
    {
        snprintf(Buffer, sizeof(Buffer), "%zu B", Bytes);
    }
    return Buffer;
}

float GetDebugCameraBaseSpeed()
{
    return std::max(0.1f, FEditorSettings::Get().CameraSpeed);
}

float GetDebugCameraSpeedMultiplier(FEditorViewportClient* Client)
{
    if (!Client)
    {
        return 1.0f;
    }
    return MathUtil::Clamp(Client->GetMoveSpeed() / GetDebugCameraBaseSpeed(), 0.01f, MaxDebugCameraSpeedMultiplier);
}

void SetDebugCameraSpeedMultiplier(FEditorViewportClient* Client, float Multiplier)
{
    if (!Client)
    {
        return;
    }

    Client->SetMoveSpeed(MathUtil::Clamp(
        GetDebugCameraBaseSpeed() * Multiplier,
        0.1f,
        GetDebugCameraBaseSpeed() * MaxDebugCameraSpeedMultiplier));
}

} // namespace

void FEditorMainPanel::RenderUndoHistoryPanel(float DeltaTime)
{
    (void)DeltaTime;
    if (!PanelVisibility.bShowUndoHistory || !EditorEngine)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Undo History", &PanelVisibility.bShowUndoHistory))
    {
        ImGui::End();
        return;
    }

    const FEditorUndoSystem& UndoSystem = EditorEngine->GetUndoSystem();
    const TArray<FUndoSnapshotEntry>& UndoEntries = UndoSystem.GetUndoHistory();
    const TArray<FUndoSnapshotEntry>& RedoEntries = UndoSystem.GetRedoHistory();
    const FUndoHistoryStats HistoryStats = UndoSystem.GetStats();

    const bool bCanUndo = !UndoEntries.empty();
    const bool bCanRedo = !RedoEntries.empty();
    ImGui::BeginDisabled(!bCanUndo);
    if (ImGui::Button("Undo", ImVec2(86.0f, 0.0f)))
    {
        EditorEngine->GetCommandSystem().Execute(EEditorCommand::Undo);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!bCanRedo);
    if (ImGui::Button("Redo", ImVec2(86.0f, 0.0f)))
    {
        EditorEngine->GetCommandSystem().Execute(EEditorCommand::Redo);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!bCanUndo && !bCanRedo);
    if (ImGui::Button("Clear", ImVec2(86.0f, 0.0f)))
    {
        EditorEngine->GetCommandSystem().Execute(EEditorCommand::ClearUndoHistory);
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Stat History", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Entries: %d / %d", HistoryStats.UndoCount + HistoryStats.RedoCount, HistoryStats.MaxEntries);
        ImGui::TextDisabled("Undo %d, Redo %d", HistoryStats.UndoCount, HistoryStats.RedoCount);
        ImGui::Text("Snapshot Data: %s", FormatHistoryBytes(HistoryStats.LogicalBytes).c_str());
        ImGui::Text("Reserved Memory: %s", FormatHistoryBytes(HistoryStats.ReservedBytes).c_str());
        ImGui::TextDisabled("Approx Total: %s", FormatHistoryBytes(HistoryStats.ApproxTotalBytes).c_str());
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Approx Total = string reserved capacity + entry storage. Scene restore also creates a temporary world only while undo/redo is executing.");
        }
    }
    ImGui::Separator();
    ImGui::TextDisabled("Undo");
    ImGui::BeginChild("##UndoHistoryList", ImVec2(0.0f, ImGui::GetContentRegionAvail().y * 0.62f), true);
    if (UndoEntries.empty())
    {
        ImGui::TextDisabled("No undo history.");
    }
    else
    {
        for (int32 Index = static_cast<int32>(UndoEntries.size()) - 1; Index >= 0; --Index)
        {
            ImGui::PushID(Index);
            const FString Label = UndoEntries[Index].Label.empty() ? FString("Scene Edit") : UndoEntries[Index].Label;
            if (ImGui::Selectable(Label.c_str()))
            {
                FEditorCommandArgs Args;
                Args.HistoryIndex = Index;
                EditorEngine->GetCommandSystem().Execute(EEditorCommand::RestoreUndoHistoryIndex, Args);
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::TextDisabled("Redo");
    ImGui::BeginChild("##RedoHistoryList", ImVec2(0.0f, 0.0f), true);
    if (RedoEntries.empty())
    {
        ImGui::TextDisabled("No redo history.");
    }
    else
    {
        for (int32 Index = static_cast<int32>(RedoEntries.size()) - 1; Index >= 0; --Index)
        {
            const FString Label = RedoEntries[Index].Label.empty() ? FString("Scene Edit") : RedoEntries[Index].Label;
            ImGui::TextUnformatted(Label.c_str());
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void FEditorMainPanel::RenderEditorDebugPanel(float DeltaTime)
{
    (void)DeltaTime;
    if (!PanelVisibility.bShowEditorDebug || !EditorEngine)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(500.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Editor Debug", &PanelVisibility.bShowEditorDebug))
    {
        ImGui::End();
        return;
    }

    FEditorSettings& Settings = FEditorSettings::Get();
    if (ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Camera Base Speed", &Settings.CameraSpeed, 0.1f, 0.1f, 100.0f, "%.1f");
        ImGui::DragFloat("Camera Rotate Speed", &Settings.CameraRotationSpeed, 1.0f, 1.0f, 720.0f, "%.0f");
        ImGui::DragFloat("Camera Zoom Speed", &Settings.CameraZoomSpeed, 1.0f, 10.0f, 5000.0f, "%.0f");
        ImGui::DragFloat("Dolly Speed Scale", &Settings.CameraDollySpeedScale, 0.01f, 0.05f, 5.0f, "%.2fx");
        ImGui::DragFloat("Pan Speed Scale", &Settings.CameraPanSpeedScale, 0.05f, 0.05f, 10.0f, "%.2fx");
        const char* PickingModeItems[] = { "ID Buffer", "Ray-Triangle" };
        int32 PickingModeIndex = static_cast<int32>(Settings.PickingMode);
        if (ImGui::Combo("Picking Mode", &PickingModeIndex, PickingModeItems, IM_ARRAYSIZE(PickingModeItems)))
        {
            if (PickingModeIndex >= 0 && PickingModeIndex < static_cast<int32>(EEditorPickingMode::Count))
            {
                Settings.PickingMode = static_cast<EEditorPickingMode>(PickingModeIndex);
            }
        }
        ImGui::Checkbox("Camera Smoothing", &Settings.bEnableCameraSmoothing);
        ImGui::BeginDisabled(!Settings.bEnableCameraSmoothing);
        ImGui::DragFloat("Move Smooth Speed", &Settings.CameraMoveSmoothSpeed, 0.05f, 0.1f, 40.0f, "%.2f");
        ImGui::DragFloat("Rotate Smooth Speed", &Settings.CameraRotateSmoothSpeed, 0.05f, 0.1f, 40.0f, "%.2f");
        ImGui::EndDisabled();

        FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
        if (FEditorViewportClient* FocusedClient = Layout.GetViewportClient(Layout.GetLastFocusedViewportIndex()))
        {
            float SpeedMultiplier = GetDebugCameraSpeedMultiplier(FocusedClient);
            if (ImGui::DragFloat(
                "Focused Speed Multiplier",
                &SpeedMultiplier,
                0.05f,
                0.01f,
                MaxDebugCameraSpeedMultiplier,
                "%.2fx"))
            {
                SetDebugCameraSpeedMultiplier(FocusedClient, SpeedMultiplier);
            }
        }
    }

    if (ImGui::CollapsingHeader("Show Flags", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Primitives", &Settings.ShowFlags.bPrimitives);
        ImGui::Checkbox("Skeletal Mesh", &Settings.ShowFlags.bSkeletalMesh);
        ImGui::Checkbox("BillboardText", &Settings.ShowFlags.bBillboardText);
        ImGui::Checkbox("Axis", &Settings.ShowFlags.bAxis);
        ImGui::Checkbox("Grid", &Settings.ShowFlags.bGrid);
        ImGui::Checkbox("Gizmo", &Settings.ShowFlags.bGizmo);
        ImGui::Checkbox("Bounding Volume", &Settings.ShowFlags.bBoundingVolume);
        if (Settings.ShowFlags.bBoundingVolume)
        {
            ImGui::Indent();
            ImGui::Checkbox("BVH Bounding Volume", &Settings.ShowFlags.bBVHBoundingVolume);
            ImGui::Unindent();
        }
        ImGui::Checkbox("Enable LOD", &Settings.ShowFlags.bEnableLOD);
        ImGui::Checkbox("Decals", &Settings.ShowFlags.bDecals);
        ImGui::Checkbox("Fog", &Settings.ShowFlags.bFog);
        ImGui::Checkbox("Shadow", &Settings.ShowFlags.bShadow);
        ImGui::Checkbox("Gamma Correction", &Settings.ShowFlags.bGammaCorrection);
        if (Settings.ShowFlags.bGammaCorrection)
        {
            ImGui::Indent();
            ImGui::SliderFloat("Gamma", &Settings.ShowFlags.GammaValue, 1.0f, 3.0f, "%.2f");
            ImGui::Unindent();
        }
        ImGui::Checkbox("FXAA", &Settings.bEnableFXAA);
    }

    if (ImGui::CollapsingHeader("Place Actors (Grid)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const int32 PrimitiveCount = Widgets.ControlWidget.GetPrimitiveTypeCount();
        DebugGridState.PrimitiveType = MathUtil::Clamp(DebugGridState.PrimitiveType, 0, PrimitiveCount - 1);

        if (ImGui::BeginCombo("Actor Type", Widgets.ControlWidget.GetPrimitiveTypeLabel(DebugGridState.PrimitiveType)))
        {
            for (int32 i = 0; i < PrimitiveCount; ++i)
            {
                const bool bSelected = (DebugGridState.PrimitiveType == i);
                if (ImGui::Selectable(Widgets.ControlWidget.GetPrimitiveTypeLabel(i), bSelected))
                {
                    DebugGridState.PrimitiveType = i;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::DragInt("Rows", &DebugGridState.Rows, 1.0f, 1, 128, "%d");
        ImGui::DragInt("Cols", &DebugGridState.Cols, 1.0f, 1, 128, "%d");
        ImGui::DragInt("Layers", &DebugGridState.Layers, 1.0f, 1, 32, "%d");
        ImGui::DragFloat("Grid Spacing", &DebugGridState.Spacing, 0.1f, 0.1f, 1000.0f, "%.2f");
        ImGui::Checkbox("Center Grid Around Origin", &DebugGridState.bCenter);
        ImGui::DragFloat3("Origin", &DebugGridState.Origin.X, 0.1f, -100000.0f, 100000.0f, "%.2f");

        DebugGridState.Rows = MathUtil::Clamp(DebugGridState.Rows, 1, 128);
        DebugGridState.Cols = MathUtil::Clamp(DebugGridState.Cols, 1, 128);
        DebugGridState.Layers = MathUtil::Clamp(DebugGridState.Layers, 1, 32);
        DebugGridState.Spacing = std::max(0.1f, DebugGridState.Spacing);

        const int32 TotalActors = DebugGridState.Rows * DebugGridState.Cols * DebugGridState.Layers;
        ImGui::Text("Total Actors: %d", TotalActors);
        if (TotalActors > 2048)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "Large grid; spawn is capped at 2048 per click.");
        }

        if (ImGui::Button("Spawn Grid Actors"))
        {
            const float RowOffset = DebugGridState.bCenter ? static_cast<float>(DebugGridState.Rows - 1) * 0.5f : 0.0f;
            const float ColOffset = DebugGridState.bCenter ? static_cast<float>(DebugGridState.Cols - 1) * 0.5f : 0.0f;
            const float LayerOffset = DebugGridState.bCenter ? static_cast<float>(DebugGridState.Layers - 1) * 0.5f : 0.0f;
            const int32 SpawnLimit = std::min(TotalActors, 2048);
            int32 SpawnedCount = 0;

            for (int32 Layer = 0; Layer < DebugGridState.Layers && SpawnedCount < SpawnLimit; ++Layer)
            {
                for (int32 Row = 0; Row < DebugGridState.Rows && SpawnedCount < SpawnLimit; ++Row)
                {
                    for (int32 Col = 0; Col < DebugGridState.Cols && SpawnedCount < SpawnLimit; ++Col)
                    {
                        const FVector Location(
                            DebugGridState.Origin.X + (static_cast<float>(Col) - ColOffset) * DebugGridState.Spacing,
                            DebugGridState.Origin.Y + (static_cast<float>(Row) - RowOffset) * DebugGridState.Spacing,
                            DebugGridState.Origin.Z + (static_cast<float>(Layer) - LayerOffset) * DebugGridState.Spacing);
                        if (Widgets.ControlWidget.SpawnPrimitive(DebugGridState.PrimitiveType, Location, 1))
                        {
                            ++SpawnedCount;
                        }
                    }
                }
            }

            if (UWorld* World = EditorEngine->GetFocusedWorld())
            {
                World->RebuildSpatialIndex();
            }
            FEditorConsoleWidget::AddLog(
                "Editor Debug grid spawned %d %s actors\n",
                SpawnedCount,
                Widgets.ControlWidget.GetPrimitiveTypeLabel(DebugGridState.PrimitiveType));
        }
    }

    ImGui::End();
}
