#include "Editor/UI/EditorCurveEditorWidget.h"

#include "Animation/ActorSequence.h"
#include "Asset/CurveFloatAsset.h"
#include "Component/ActorComponent.h"
#include "Component/ActorSequenceComponent.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorMainPanel.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace
{
    constexpr float CurveEditorGridUnit = 0.1f;
    constexpr float CurveEditorSnapUnit = 0.05f;
    constexpr float CurveEditorMinPixelsPerUnit = 24.0f;
    constexpr float CurveEditorMaxPixelsPerUnit = 420.0f;
    constexpr float CurveEditorMaxTangent = 100.0f;

    float SnapCurveEditorValue(float Value, float Unit)
    {
        return std::round(Value / Unit) * Unit;
    }

    float ClampCurveEditorTangent(float Tangent)
    {
        return std::clamp(Tangent, -CurveEditorMaxTangent, CurveEditorMaxTangent);
    }
}

void FEditorCurveEditorWidget::OpenCurveAsset(const FString& CurvePath)
{
    StopReferencePreview();
    CurrentPath = CurvePath;
    CurrentCurve = FResourceManager::Get().LoadCurve(CurrentPath);
    SelectedKeyIndex = CurrentCurve && !CurrentCurve->GetCurve().Keys.empty() ? 0 : -1;
    ActiveKeyDragIndex = -1;
    ActiveTangentKeyIndex = -1;
    ActiveTangentHandle = -1;
    ContextKeyIndex = -1;
    ContextTime = 0.0f;
    ContextValue = 0.0f;
    bDirty = false;
    bVisible = true;
}

void FEditorCurveEditorWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    if (!bVisible)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(620.0f, 520.0f), ImGuiCond_FirstUseEver);

    bool bOpen = bVisible;
    if (!ImGui::Begin("Curve Editor", &bOpen))
    {
        if (!bOpen)
        {
            StopReferencePreview();
        }
        bVisible = bOpen;
        ImGui::End();
        return;
    }
    if (!bOpen)
    {
        StopReferencePreview();
    }
    bVisible = bOpen;

    TickReferencePreview(DeltaTime);

    DrawToolbar();
    ImGui::Separator();

    if (!CurrentCurve)
    {
        ImGui::TextWrapped("Curve asset is not loaded.");
        ImGui::End();
        return;
    }

    DrawCurveCanvas();
    ImGui::Separator();
    DrawKeyList();

    ImGui::End();
}

void FEditorCurveEditorWidget::DrawToolbar()
{
    ImGui::Text("Asset: %s", CurrentPath.empty() ? "<None>" : CurrentPath.c_str());
    if (bDirty)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("*");
    }

    ImGui::BeginDisabled(!CurrentCurve || CurrentPath.empty() || !bDirty);
    if (ImGui::Button("Save"))
    {
        SaveCurve();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(CurrentPath.empty());
    if (ImGui::Button("Reload"))
    {
        ReloadCurve();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!CurrentCurve || CurrentPath.empty());
    if (!bReferencePreviewActive)
    {
        if (ImGui::Button("Preview References"))
        {
            StartReferencePreview();
        }
    }
    else
    {
        if (ImGui::Button("Stop Reference Preview"))
        {
            StopReferencePreview();
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+Wheel: Zoom | Alt: Free drag");
}

void FEditorCurveEditorWidget::DrawCurveCanvas()
{
    const FFloatCurve& Curve = CurrentCurve->GetCurve();
    const float OuterAvailableHeight = ImGui::GetContentRegionAvail().y;
    const float ChildHeight = std::clamp(
        std::min(CanvasHeight + 18.0f, OuterAvailableHeight - 190.0f),
        220.0f,
        380.0f);

    ImGui::BeginChild(
        "##CurveCanvasScroll",
        ImVec2(0.0f, ChildHeight),
        true,
        ImGuiWindowFlags_HorizontalScrollbar);

    CanvasHeight = std::clamp(CanvasHeight, 260.0f, 1200.0f);
    CanvasPixelsPerUnit = std::clamp(
        CanvasPixelsPerUnit,
        CurveEditorMinPixelsPerUnit,
        CurveEditorMaxPixelsPerUnit);

    if (Curve.Keys.empty())
    {
        const ImVec2 CanvasSize(720.0f, CanvasHeight);
        const ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 CanvasEnd(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y);
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        DrawList->AddRectFilled(CanvasPos, CanvasEnd, ImGui::GetColorU32(ImGuiCol_FrameBg));
        DrawList->AddRect(CanvasPos, CanvasEnd, ImGui::GetColorU32(ImGuiCol_Border));
        ImGui::InvisibleButton("##CurveCanvas", CanvasSize);
        if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl && std::fabs(ImGui::GetIO().MouseWheel) > 0.0f)
        {
            const float ZoomFactor = ImGui::GetIO().MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f;
            CanvasPixelsPerUnit = std::clamp(
                CanvasPixelsPerUnit * ZoomFactor,
                CurveEditorMinPixelsPerUnit,
                CurveEditorMaxPixelsPerUnit);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            const ImVec2 Mouse = ImGui::GetIO().MousePos;
            ContextKeyIndex = -1;
            ContextTime = SnapCurveEditorValue((Mouse.x - CanvasPos.x) / CanvasPixelsPerUnit, CurveEditorSnapUnit);
            ContextValue = SnapCurveEditorValue(1.0f - ((Mouse.y - CanvasPos.y) / CanvasSize.y), CurveEditorSnapUnit);
            ImGui::OpenPopup("CurveCanvasContextMenu");
        }
        if (ImGui::BeginPopup("CurveCanvasContextMenu"))
        {
            if (ImGui::MenuItem("Add Key"))
            {
                AddKeyAt(std::max(0.0f, ContextTime), ContextValue);
            }
            ImGui::EndPopup();
        }
        DrawList->AddText(ImVec2(CanvasPos.x + 12.0f, CanvasPos.y + 12.0f),
            ImGui::GetColorU32(ImGuiCol_TextDisabled),
            "No keys");
        ImGui::EndChild();
        return;
    }

    float MinTime = Curve.Keys.front().Time;
    float MaxTime = Curve.Keys.front().Time;
    float MinValue = Curve.Keys.front().Value;
    float MaxValue = Curve.Keys.front().Value;

    for (const FCurveKey& Key : Curve.Keys)
    {
        MinTime = std::min(MinTime, Key.Time);
        MaxTime = std::max(MaxTime, Key.Time);
        MinValue = std::min(MinValue, Key.Value);
        MaxValue = std::max(MaxValue, Key.Value);
    }

    const float KeyMinValue = MinValue;
    const float KeyMaxValue = MaxValue;

    if (std::fabs(MaxTime - MinTime) < 0.0001f)
    {
        MaxTime = MinTime + 1.0f;
    }
    if (std::fabs(MaxValue - MinValue) < 0.0001f)
    {
        MinValue -= 0.5f;
        MaxValue += 0.5f;
    }

    const float TimePadding = (MaxTime - MinTime) * 0.08f;
    MinTime -= TimePadding;
    MaxTime += TimePadding;

    const int32 RangeSampleCount = 128;
    const float KeyValueRange = std::max(0.0001f, KeyMaxValue - KeyMinValue);
    const float AutoValueExpandLimit = std::max(KeyValueRange * 2.0f, 1.0f);
    const float SampleMinValue = KeyMinValue - AutoValueExpandLimit;
    const float SampleMaxValue = KeyMaxValue + AutoValueExpandLimit;
    for (int32 Index = 0; Index <= RangeSampleCount; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / static_cast<float>(RangeSampleCount);
        const float Time = MinTime + (MaxTime - MinTime) * Alpha;
        const float Value = std::clamp(Curve.Evaluate(Time), SampleMinValue, SampleMaxValue);
        MinValue = std::min(MinValue, Value);
        MaxValue = std::max(MaxValue, Value);
    }

    if (std::fabs(MaxValue - MinValue) < 0.0001f)
    {
        MinValue -= 0.5f;
        MaxValue += 0.5f;
    }

    const float ValuePadding = std::max((MaxValue - MinValue) * 0.18f, 0.1f);
    MinValue -= ValuePadding;
    MaxValue += ValuePadding;
    MinTime = std::floor(MinTime / CurveEditorGridUnit) * CurveEditorGridUnit;
    MaxTime = std::ceil(MaxTime / CurveEditorGridUnit) * CurveEditorGridUnit;
    MinValue = std::floor(MinValue / CurveEditorGridUnit) * CurveEditorGridUnit;
    MaxValue = std::ceil(MaxValue / CurveEditorGridUnit) * CurveEditorGridUnit;

    const float MinCanvasWidth = 720.0f;
    const float TimeRange = std::max(0.0001f, MaxTime - MinTime);
    const float ValueRange = std::max(0.0001f, MaxValue - MinValue);
    const ImVec2 CanvasSize(
        std::max(MinCanvasWidth, TimeRange * CanvasPixelsPerUnit),
        std::max(CanvasHeight, ValueRange * CanvasPixelsPerUnit));
    const ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
    const ImVec2 CanvasEnd(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    DrawList->AddRectFilled(CanvasPos, CanvasEnd, ImGui::GetColorU32(ImGuiCol_FrameBg));
    DrawList->AddRect(CanvasPos, CanvasEnd, ImGui::GetColorU32(ImGuiCol_Border));

    ImGui::InvisibleButton("##CurveCanvas", CanvasSize);
    const ImVec2 AfterCanvasCursorPos = ImGui::GetCursorScreenPos();
    if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl && std::fabs(ImGui::GetIO().MouseWheel) > 0.0f)
    {
        const float ZoomFactor = ImGui::GetIO().MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f;
        CanvasPixelsPerUnit = std::clamp(
            CanvasPixelsPerUnit * ZoomFactor,
            CurveEditorMinPixelsPerUnit,
            CurveEditorMaxPixelsPerUnit);
    }

    const auto ClampToCanvas = [&](const ImVec2& Pos) -> ImVec2
    {
        return ImVec2(
            std::clamp(Pos.x, CanvasPos.x, CanvasEnd.x),
            std::clamp(Pos.y, CanvasPos.y, CanvasEnd.y));
    };

    const auto ToCanvas = [&](float Time, float Value) -> ImVec2
    {
        const float X01 = (Time - MinTime) / (MaxTime - MinTime);
        const float Y01 = (Value - MinValue) / (MaxValue - MinValue);
        return ImVec2(
            CanvasPos.x + X01 * CanvasSize.x,
            CanvasPos.y + (1.0f - Y01) * CanvasSize.y);
    };

    const auto ToCurve = [&](const ImVec2& Pos, float& OutTime, float& OutValue)
    {
        const float X01 = std::clamp((Pos.x - CanvasPos.x) / CanvasSize.x, 0.0f, 1.0f);
        const float Y01 = 1.0f - std::clamp((Pos.y - CanvasPos.y) / CanvasSize.y, 0.0f, 1.0f);
        OutTime = MinTime + (MaxTime - MinTime) * X01;
        OutValue = MinValue + (MaxValue - MinValue) * Y01;
    };

    const ImU32 GridColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const ImU32 MinorGridColor = ImGui::GetColorU32(ImGuiCol_Border);
    DrawList->PushClipRect(CanvasPos, CanvasEnd, true);
    const float GridTimeStart = std::ceil(MinTime / CurveEditorGridUnit) * CurveEditorGridUnit;
    for (float Time = GridTimeStart; Time <= MaxTime + 0.0001f; Time += CurveEditorGridUnit)
    {
        const float X = ToCanvas(Time, MinValue).x;
        const bool bMajor = (std::abs(std::round(Time) - Time) < 0.0001f);
        DrawList->AddLine(ImVec2(X, CanvasPos.y), ImVec2(X, CanvasEnd.y), bMajor ? GridColor : MinorGridColor);
    }
    const float GridValueStart = std::ceil(MinValue / CurveEditorGridUnit) * CurveEditorGridUnit;
    for (float Value = GridValueStart; Value <= MaxValue + 0.0001f; Value += CurveEditorGridUnit)
    {
        const float Y = ToCanvas(MinTime, Value).y;
        const bool bMajor = (std::abs(std::round(Value) - Value) < 0.0001f);
        DrawList->AddLine(ImVec2(CanvasPos.x, Y), ImVec2(CanvasEnd.x, Y), bMajor ? GridColor : MinorGridColor);
    }

    const int32 SampleCount = std::max(128, static_cast<int32>(CanvasSize.x / 4.0f));
    ImVec2 Previous = ToCanvas(MinTime, Curve.Evaluate(MinTime));
    for (int32 Index = 1; Index <= SampleCount; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / static_cast<float>(SampleCount);
        const float Time = MinTime + (MaxTime - MinTime) * Alpha;
        const ImVec2 Current = ToCanvas(Time, Curve.Evaluate(Time));
        DrawList->AddLine(Previous, Current, ImGui::GetColorU32(ImGuiCol_PlotLines), 2.0f);
        Previous = Current;
    }

    const ImVec2 Mouse = ImGui::GetIO().MousePos;
    int32 HoveredKeyIndex = -1;
    float BestDistanceSq = 144.0f;

    for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve.Keys.size()); ++KeyIndex)
    {
        const FCurveKey& Key = Curve.Keys[KeyIndex];
        const ImVec2 KeyPos = ToCanvas(Key.Time, Key.Value);
        const float Dx = KeyPos.x - Mouse.x;
        const float Dy = KeyPos.y - Mouse.y;
        const float DistanceSq = Dx * Dx + Dy * Dy;
        if (DistanceSq < BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            HoveredKeyIndex = KeyIndex;
        }

        const bool bSelected = KeyIndex == SelectedKeyIndex;
        DrawList->AddCircleFilled(
            KeyPos,
            bSelected ? 5.5f : 4.0f,
            ImGui::GetColorU32(bSelected ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered));
    }

    bool bKeyHovered = HoveredKeyIndex >= 0;
    bool bTangentHandleHovered = false;
    bool bTangentHandleClicked = false;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        ActiveKeyDragIndex = -1;
        ActiveTangentKeyIndex = -1;
        ActiveTangentHandle = -1;
    }

    if (ImGui::IsItemHovered()
        && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && HoveredKeyIndex >= 0)
    {
        SelectedKeyIndex = HoveredKeyIndex;
        ActiveKeyDragIndex = HoveredKeyIndex;
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        float PopupTime = 0.0f;
        float PopupValue = 0.0f;
        ToCurve(ClampToCanvas(ImGui::GetIO().MousePos), PopupTime, PopupValue);
        ContextKeyIndex = HoveredKeyIndex;
        ContextTime = SnapCurveEditorValue(PopupTime, CurveEditorSnapUnit);
        ContextValue = SnapCurveEditorValue(PopupValue, CurveEditorSnapUnit);
        ImGui::OpenPopup("CurveCanvasContextMenu");
    }

    if (ImGui::BeginPopup("CurveCanvasContextMenu"))
    {
        if (ImGui::MenuItem("Add Key"))
        {
            AddKeyAt(ContextTime, ContextValue);
        }
        if (ContextKeyIndex >= 0)
        {
            ImGui::Separator();
        }
        ImGui::BeginDisabled(ContextKeyIndex < 0);
        if (ImGui::MenuItem("Remove Key"))
        {
            RemoveKeyAtIndex(ContextKeyIndex);
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    if (ActiveKeyDragIndex >= 0
        && ActiveTangentHandle < 0
        && ActiveKeyDragIndex < static_cast<int32>(CurrentCurve->GetMutableCurve().Keys.size())
        && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        FFloatCurve& MutableCurve = CurrentCurve->GetMutableCurve();
        float DragTime = 0.0f;
        float DragValue = 0.0f;
        ToCurve(ClampToCanvas(ImGui::GetIO().MousePos), DragTime, DragValue);
        if (!ImGui::GetIO().KeyAlt)
        {
            DragTime = SnapCurveEditorValue(DragTime, CurveEditorSnapUnit);
            DragValue = SnapCurveEditorValue(DragValue, CurveEditorSnapUnit);
        }

        MutableCurve.Keys[ActiveKeyDragIndex].Time = DragTime;
        MutableCurve.Keys[ActiveKeyDragIndex].Value = DragValue;
        MutableCurve.SortKeys();

        int32 BestIndex = 0;
        float BestScore = FLT_MAX;
        for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(MutableCurve.Keys.size()); ++KeyIndex)
        {
            const float TimeDelta = MutableCurve.Keys[KeyIndex].Time - DragTime;
            const float ValueDelta = MutableCurve.Keys[KeyIndex].Value - DragValue;
            const float Score = TimeDelta * TimeDelta + ValueDelta * ValueDelta;
            if (Score < BestScore)
            {
                BestScore = Score;
                BestIndex = KeyIndex;
            }
        }

        ActiveKeyDragIndex = BestIndex;
        SelectedKeyIndex = BestIndex;
        MarkDirty();
    }

    if (SelectedKeyIndex >= 0 && SelectedKeyIndex < static_cast<int32>(Curve.Keys.size()))
    {
        FFloatCurve& MutableCurve = CurrentCurve->GetMutableCurve();
        FCurveKey& Key = MutableCurve.Keys[SelectedKeyIndex];
        const ImVec2 KeyPos = ToCanvas(Key.Time, Key.Value);
        const bool bCubicKey = Key.InterpMode == ECurveInterpMode::Cubic;

        if (bCubicKey)
        {
            const float HandleScale = 0.5f;
            const float DefaultHandleTime = (MaxTime - MinTime) * 0.08f * HandleScale;
            const float ArriveTimeDelta = SelectedKeyIndex > 0
                ? std::max(0.0001f, Key.Time - MutableCurve.Keys[SelectedKeyIndex - 1].Time) * 0.333f * HandleScale
                : DefaultHandleTime;
            const float LeaveTimeDelta = SelectedKeyIndex + 1 < static_cast<int32>(MutableCurve.Keys.size())
                ? std::max(0.0001f, MutableCurve.Keys[SelectedKeyIndex + 1].Time - Key.Time) * 0.333f * HandleScale
                : DefaultHandleTime;

            const ImVec2 ArrivePos = ClampToCanvas(ToCanvas(
                Key.Time - ArriveTimeDelta,
                Key.Value - Key.ArriveTangent * ArriveTimeDelta));
            const ImVec2 LeavePos = ClampToCanvas(ToCanvas(
                Key.Time + LeaveTimeDelta,
                Key.Value + Key.LeaveTangent * LeaveTimeDelta));

            const ImU32 HandleLineColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            const ImU32 ArriveColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
            const ImU32 LeaveColor = ImGui::GetColorU32(ImGuiCol_ButtonActive);
            DrawList->AddLine(KeyPos, ArrivePos, HandleLineColor, 1.0f);
            DrawList->AddLine(KeyPos, LeavePos, HandleLineColor, 1.0f);
            DrawList->AddCircleFilled(ArrivePos, 5.0f, ArriveColor);
            DrawList->AddCircleFilled(LeavePos, 5.0f, LeaveColor);

            auto UpdateHandleControl = [&](const ImVec2& HandlePos, int32 HandleIndex, float TimeDelta)
            {
                const float Dx = HandlePos.x - Mouse.x;
                const float Dy = HandlePos.y - Mouse.y;
                const bool bHovered = (Dx * Dx + Dy * Dy) <= 100.0f;
                bTangentHandleHovered |= bHovered;

                if (bHovered)
                {
                    ImGui::SetTooltip(HandleIndex == 1 ? "Leave tangent" : "Arrive tangent");
                }

                if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    ActiveTangentKeyIndex = SelectedKeyIndex;
                    ActiveTangentHandle = HandleIndex;
                    bTangentHandleClicked = true;
                }

                if (ActiveTangentKeyIndex == SelectedKeyIndex
                    && ActiveTangentHandle == HandleIndex
                    && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    float DragTime = 0.0f;
                    float DragValue = 0.0f;
                    ToCurve(ClampToCanvas(ImGui::GetIO().MousePos), DragTime, DragValue);

                    const bool bLeaveHandle = HandleIndex == 1;
                    const float Tangent = ClampCurveEditorTangent(bLeaveHandle
                        ? (DragValue - Key.Value) / TimeDelta
                        : (Key.Value - DragValue) / TimeDelta);

                    if (bLeaveHandle)
                    {
                        Key.LeaveTangent = Tangent;
                    }
                    else
                    {
                        Key.ArriveTangent = Tangent;
                    }

                    Key.TangentMode = ECurveTangentMode::User;
                    MarkDirty();
                }
            };

            UpdateHandleControl(ArrivePos, 0, ArriveTimeDelta);
            UpdateHandleControl(LeavePos, 1, LeaveTimeDelta);
        }
    }
    DrawList->PopClipRect();

    if (bTangentHandleHovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    else if (bKeyHovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    if (bTangentHandleClicked)
    {
        SelectedKeyIndex = ActiveTangentKeyIndex;
    }

    ImGui::SetCursorScreenPos(AfterCanvasCursorPos);
    ImGui::InvisibleButton("##CurveCanvasResize", ImVec2(CanvasSize.x, 6.0f));
    if (ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        ImGui::SetTooltip("Resize curve canvas");
    }
    if (ImGui::IsItemActive())
    {
        CanvasHeight = std::clamp(CanvasHeight + ImGui::GetIO().MouseDelta.y, 260.0f, 1200.0f);
    }
    ImGui::EndChild();
}

void FEditorCurveEditorWidget::DrawKeyList()
{
    FFloatCurve& Curve = CurrentCurve->GetMutableCurve();

    if (ImGui::Button("Add Key"))
    {
        AddKey();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(SelectedKeyIndex < 0 || SelectedKeyIndex >= static_cast<int32>(Curve.Keys.size()));
    if (ImGui::Button("Remove Key"))
    {
        RemoveSelectedKey();
    }
    ImGui::EndDisabled();

    if (ImGui::BeginTable("##CurveKeys", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Sel", ImGuiTableColumnFlags_WidthFixed, 32.0f);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Interp", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Arrive", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Leave", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve.Keys.size()); ++KeyIndex)
        {
            FCurveKey& Key = Curve.Keys[KeyIndex];
            ImGui::PushID(KeyIndex);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            const bool bSelected = KeyIndex == SelectedKeyIndex;
            if (ImGui::RadioButton("##SelectKey", bSelected))
            {
                SelectedKeyIndex = KeyIndex;
            }

            bool bChanged = false;
            ImGui::TableSetColumnIndex(1);
            bChanged |= ImGui::DragFloat("##Time", &Key.Time, 0.01f);

            ImGui::TableSetColumnIndex(2);
            bChanged |= ImGui::DragFloat("##Value", &Key.Value, 0.01f);

            ImGui::TableSetColumnIndex(3);
            static const char* InterpNames[] = { "Constant", "Linear", "Cubic" };
            int32 InterpIndex = static_cast<int32>(Key.InterpMode);
            if (ImGui::Combo("##Interp", &InterpIndex, InterpNames, IM_ARRAYSIZE(InterpNames)))
            {
                Key.InterpMode = static_cast<ECurveInterpMode>(InterpIndex);
                bChanged = true;
            }

            ImGui::TableSetColumnIndex(4);
            bChanged |= ImGui::DragFloat("##Arrive", &Key.ArriveTangent, 0.01f);

            ImGui::TableSetColumnIndex(5);
            bChanged |= ImGui::DragFloat("##Leave", &Key.LeaveTangent, 0.01f);

            if (bChanged)
            {
                Key.ArriveTangent = ClampCurveEditorTangent(Key.ArriveTangent);
                Key.LeaveTangent = ClampCurveEditorTangent(Key.LeaveTangent);
                Curve.SortKeys();
                SelectedKeyIndex = std::clamp(SelectedKeyIndex, 0, static_cast<int32>(Curve.Keys.size()) - 1);
                MarkDirty();
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void FEditorCurveEditorWidget::AddKey()
{
    if (!CurrentCurve)
    {
        return;
    }

    FFloatCurve& Curve = CurrentCurve->GetMutableCurve();
    FCurveKey Key;
    if (!Curve.Keys.empty())
    {
        const float EndTime = Curve.GetEndTime();
        Key.Time = SnapCurveEditorValue(EndTime + CurveEditorGridUnit, CurveEditorSnapUnit);
        Key.Value = Curve.Evaluate(EndTime);
        Key.InterpMode = Curve.Keys.back().InterpMode;
    }

    AddKeyAt(Key.Time, Key.Value);
}

void FEditorCurveEditorWidget::AddKeyAt(float Time, float Value)
{
    if (!CurrentCurve)
    {
        return;
    }

    FFloatCurve& Curve = CurrentCurve->GetMutableCurve();
    FCurveKey Key;
    Key.Time = SnapCurveEditorValue(Time, CurveEditorSnapUnit);
    Key.Value = SnapCurveEditorValue(Value, CurveEditorSnapUnit);
    if (!Curve.Keys.empty())
    {
        Key.InterpMode = Curve.Keys.back().InterpMode;
        Key.TangentMode = Curve.Keys.back().TangentMode;
    }

    Curve.Keys.push_back(Key);
    Curve.SortKeys();

    SelectedKeyIndex = 0;
    float BestScore = FLT_MAX;
    for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve.Keys.size()); ++KeyIndex)
    {
        const float TimeDelta = Curve.Keys[KeyIndex].Time - Key.Time;
        const float ValueDelta = Curve.Keys[KeyIndex].Value - Key.Value;
        const float Score = TimeDelta * TimeDelta + ValueDelta * ValueDelta;
        if (Score < BestScore)
        {
            BestScore = Score;
            SelectedKeyIndex = KeyIndex;
        }
    }

    ActiveKeyDragIndex = -1;
    ActiveTangentKeyIndex = -1;
    ActiveTangentHandle = -1;
    MarkDirty();
}

void FEditorCurveEditorWidget::RemoveSelectedKey()
{
    RemoveKeyAtIndex(SelectedKeyIndex);
}

void FEditorCurveEditorWidget::RemoveKeyAtIndex(int32 KeyIndex)
{
    if (!CurrentCurve)
    {
        return;
    }

    FFloatCurve& Curve = CurrentCurve->GetMutableCurve();
    if (KeyIndex < 0 || KeyIndex >= static_cast<int32>(Curve.Keys.size()))
    {
        return;
    }

    Curve.Keys.erase(Curve.Keys.begin() + KeyIndex);
    if (Curve.Keys.empty())
    {
        SelectedKeyIndex = -1;
    }
    else
    {
        SelectedKeyIndex = std::min(KeyIndex, static_cast<int32>(Curve.Keys.size()) - 1);
    }

    ActiveKeyDragIndex = -1;
    ActiveTangentKeyIndex = -1;
    ActiveTangentHandle = -1;
    MarkDirty();
}

void FEditorCurveEditorWidget::StartReferencePreview()
{
    StopReferencePreview();

    UWorld* World = EditorEngine ? EditorEngine->GetFocusedWorld() : nullptr;
    if (!World || CurrentPath.empty())
    {
        return;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor)
        {
            continue;
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            UActorSequenceComponent* SequenceComp = Cast<UActorSequenceComponent>(Component);
            if (!SequenceComp || !DoesSequenceReferenceCurrentCurve(SequenceComp))
            {
                continue;
            }

            ReferencePreviewTargets.push_back(SequenceComp);
            SequenceComp->SetPreviewTime(0.0f);
            SequenceComp->PlayPreview();
        }
    }

    bReferencePreviewActive = !ReferencePreviewTargets.empty();
    if (EditorEngine)
    {
        EditorEngine->GetMainPanel().PushFooterLog(
            bReferencePreviewActive
                ? "Curve reference preview started"
                : "No ActorSequence reference found for curve");
    }
}

void FEditorCurveEditorWidget::StopReferencePreview()
{
    for (UActorSequenceComponent* SequenceComp : ReferencePreviewTargets)
    {
        if (SequenceComp)
        {
            SequenceComp->StopPreview();
        }
    }

    ReferencePreviewTargets.clear();
    bReferencePreviewActive = false;
}

void FEditorCurveEditorWidget::TickReferencePreview(float DeltaTime)
{
    if (!bReferencePreviewActive)
    {
        return;
    }

    for (int32 Index = static_cast<int32>(ReferencePreviewTargets.size()) - 1; Index >= 0; --Index)
    {
        UActorSequenceComponent* SequenceComp = ReferencePreviewTargets[Index];
        if (!SequenceComp)
        {
            ReferencePreviewTargets.erase(ReferencePreviewTargets.begin() + Index);
            continue;
        }

        SequenceComp->ExecutePreviewTick(DeltaTime);
    }

    bReferencePreviewActive = !ReferencePreviewTargets.empty();
}

bool FEditorCurveEditorWidget::DoesSequenceReferenceCurrentCurve(UActorSequenceComponent* SequenceComp) const
{
    UActorSequence* Sequence = SequenceComp ? SequenceComp->GetSequence() : nullptr;
    if (!Sequence)
    {
        return false;
    }

    const FString NormalizedCurrentPath = FPaths::Normalize(CurrentPath);
    for (const FActorSequenceBinding& Binding : Sequence->Bindings)
    {
        for (const FActorSequenceTrack& Track : Binding.Tracks)
        {
            for (const FActorSequenceSection& Section : Track.Sections)
            {
                for (const FActorSequenceChannel& Channel : Section.Channels)
                {
                    if (FPaths::Normalize(Channel.Playback.CurveAssetPath) == NormalizedCurrentPath)
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void FEditorCurveEditorWidget::MarkDirty()
{
    bDirty = true;
}

bool FEditorCurveEditorWidget::SaveCurve()
{
    if (!CurrentCurve || CurrentPath.empty())
    {
        return false;
    }

    if (!FResourceManager::Get().SaveCurve(CurrentPath, CurrentCurve))
    {
        if (EditorEngine)
        {
            EditorEngine->GetMainPanel().PushFooterLog("Curve save failed");
        }
        return false;
    }

    bDirty = false;
    if (EditorEngine)
    {
        EditorEngine->GetMainPanel().PushFooterLog("Curve saved");
    }
    return true;
}

bool FEditorCurveEditorWidget::ReloadCurve()
{
    if (CurrentPath.empty())
    {
        return false;
    }

    CurrentCurve = FResourceManager::Get().LoadCurve(CurrentPath);
    SelectedKeyIndex = CurrentCurve && !CurrentCurve->GetCurve().Keys.empty() ? 0 : -1;
    bDirty = false;
    return CurrentCurve != nullptr;
}
