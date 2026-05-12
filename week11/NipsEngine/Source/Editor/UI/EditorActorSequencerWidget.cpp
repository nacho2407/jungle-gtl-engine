#include "Editor/UI/EditorActorSequencerWidget.h"

#include "Animation/ActorSequence.h"
#include "Asset/CurveFloatAsset.h"
#include "Component/ActorComponent.h"
#include "Component/ActorSequenceComponent.h"
#include "Component/SceneComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorActorSequenceEditModel.h"
#include "Editor/UI/EditorActorSequenceTimeUtils.h"
#include "Editor/UI/EditorMainPanel.h"
#include "GameFramework/AActor.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cmath>
#include <functional>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace
{
    constexpr float SequencerLeftPanelWidth = 300.0f;
    constexpr float SequencerToolbarHeight = 34.0f;
    constexpr float SequencerRulerHeight = 30.0f;
    constexpr float SequencerRowHeight = 28.0f;
    constexpr float SequencerMinVisibleRange = 0.05f;
    constexpr float SequencerMaxVisibleRange = 100000.0f;
    constexpr float SequencerSnapUnit = 0.01f;

    float ClampSequencerRange(float Range)
    {
        return std::clamp(Range, SequencerMinVisibleRange, SequencerMaxVisibleRange);
    }

    ImU32 WithAlpha(ImU32 Color, float Alpha)
    {
        const ImU32 A = static_cast<ImU32>(std::clamp(Alpha, 0.0f, 1.0f) * 255.0f);
        return (Color & 0x00ffffff) | (A << 24);
    }

    UActorComponent* FindComponentByGuid(AActor* Owner, const FGuid& Guid)
    {
        if (!Owner || !Guid.IsValid())
        {
            return nullptr;
        }

        for (UActorComponent* Component : Owner->GetComponents())
        {
            if (Component && Component->GetPersistentGuid() == Guid)
            {
                return Component;
            }
        }
        return nullptr;
    }

    bool ContainsComponent(const TArray<UActorComponent*>& Components, UActorComponent* Candidate)
    {
        return std::find(Components.begin(), Components.end(), Candidate) != Components.end();
    }

    float SnapSequencerTime(float Time)
    {
        return std::round(Time / SequencerSnapUnit) * SequencerSnapUnit;
    }

    bool IsGroupedTrackType(EActorSequenceTrackType TrackType)
    {
        return TrackType == EActorSequenceTrackType::Vec3
            || TrackType == EActorSequenceTrackType::Vec4
            || TrackType == EActorSequenceTrackType::Color
            || TrackType == EActorSequenceTrackType::Transform;
    }
}

void FEditorActorSequencerWidget::Open(UActorSequenceComponent* InSequenceComponent)
{
    SequenceComponent = FEditorActorSequenceEditModel::IsSequenceComponentLive(InSequenceComponent)
        ? InSequenceComponent
        : nullptr;
    bVisible = SequenceComponent != nullptr;
    SelectedTrackIndex = -1;
    SelectedKeyTrackIndex = -1;
    SelectedKeyIndex = -1;
    bDraggingPlaybackStart = false;
    bDraggingPlaybackEnd = false;
    bDraggingSectionStart = false;
    bDraggingSectionEnd = false;
    DraggingSectionTrackIndex = -1;
    bDraggingKey = false;
    DraggingKeyTrackIndex = -1;
    DraggingKeyIndex = -1;

    if (UActorSequence* Sequence = SequenceComponent ? SequenceComponent->GetSequence() : nullptr)
    {
        const float PlaybackStart = Sequence->StartTime;
        const float PlaybackEnd = Sequence->StartTime + std::max(0.0f, Sequence->Duration);
        const float Padding = std::max(0.5f, Sequence->Duration * 0.1f);
        ViewStartTime = PlaybackStart - Padding;
        ViewEndTime = std::max(PlaybackStart + 1.0f, PlaybackEnd) + Padding;
    }
}

void FEditorActorSequencerWidget::ResetTarget()
{
    SequenceComponent = nullptr;
    bVisible = false;
    SelectedTrackIndex = -1;
    SelectedKeyTrackIndex = -1;
    SelectedKeyIndex = -1;
    bDraggingPlaybackStart = false;
    bDraggingPlaybackEnd = false;
    bDraggingSectionStart = false;
    bDraggingSectionEnd = false;
    DraggingSectionTrackIndex = -1;
    bDraggingKey = false;
    DraggingKeyTrackIndex = -1;
    DraggingKeyIndex = -1;
}

void FEditorActorSequencerWidget::Render(float DeltaTime)
{
    if (!bVisible)
    {
        return;
    }

    bool bOpen = bVisible;
    const ImGuiViewport* Viewport = ImGui::GetMainViewport();
    const ImVec2 WorkPos = Viewport ? Viewport->WorkPos : ImVec2(120.0f, 120.0f);
    const ImVec2 WorkSize = Viewport ? Viewport->WorkSize : ImVec2(1200.0f, 800.0f);
    ImGui::SetNextWindowPos(
        ImVec2(WorkPos.x + 12.0f, WorkPos.y + WorkSize.y - 360.0f),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        ImVec2(std::max(720.0f, WorkSize.x - 24.0f), 340.0f),
        ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Actor Sequencer", &bOpen))
    {
        bVisible = bOpen;
        ImGui::End();
        return;
    }
    bVisible = bOpen;

    if (!FEditorActorSequenceEditModel::IsSequenceComponentLive(SequenceComponent))
    {
        SequenceComponent = nullptr;
        SelectedTrackIndex = -1;
        SelectedKeyTrackIndex = -1;
        SelectedKeyIndex = -1;
        ImGui::TextDisabled("No ActorSequence component");
        ImGui::End();
        return;
    }

    SequenceComponent->ExecutePreviewTick(DeltaTime);

    DrawToolbar(SequenceComponent);
    DrawSequencer(SequenceComponent);

    ImGui::End();
}

FString FEditorActorSequencerWidget::MakeTrackLabel(
    const FActorSequenceTrack& Track,
    const FActorSequenceChannel& Channel) const
{
    if (Track.PropertyPath.empty())
    {
        return "<Unbound>";
    }

    if (Channel.ChannelName.empty() || Channel.ChannelName == "Value")
    {
        return Track.PropertyPath;
    }

    return Track.PropertyPath + "." + Channel.ChannelName;
}

void FEditorActorSequencerWidget::DrawToolbar(UActorSequenceComponent* SequenceComp)
{
    UActorSequence* Sequence = SequenceComp ? SequenceComp->GetSequence() : nullptr;
    UActorSequencePlayer* PreviewPlayer = SequenceComp ? SequenceComp->GetPreviewSequencePlayer() : nullptr;
    if (!Sequence)
    {
        return;
    }

    ImGui::BeginChild("##ActorSequencerToolbar", ImVec2(0.0f, SequencerToolbarHeight), false, ImGuiWindowFlags_NoScrollbar);

    if (ImGui::Button("+ Add"))
    {
        ImGui::OpenPopup("##ActorSequencerAddTrack");
    }
    DrawAddTrackPopup(SequenceComp);

    ImGui::SameLine();
    const bool bCanAddKey = SelectedTrackIndex >= 0;
    if (!bCanAddKey)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Key"))
    {
        AddKeyToSelectedTrack(SequenceComp);
    }
    if (!bCanAddKey)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (ImGui::Button("Play"))
    {
        SequenceComp->PlayPreview();
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause"))
    {
        SequenceComp->PausePreview();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        SequenceComp->StopPreview();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("30 fps");

    ImGui::SameLine();
    ImGui::TextDisabled("| Wheel: Zoom  RMB/MMB Drag: Pan  LMB Ruler Drag: Playhead");

    if (PreviewPlayer)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(96.0f);
        float CurrentTime = PreviewPlayer->GetCurrentTime();
        const float PlaybackStart = Sequence->StartTime;
        const float PlaybackEnd = Sequence->StartTime + std::max(0.0f, Sequence->Duration);
        if (ImGui::DragFloat("##SequencerCurrentTime", &CurrentTime, 0.01f, PlaybackStart, PlaybackEnd, "%.2f"))
        {
            SequenceComp->SetPreviewTime(CurrentTime);
        }
    }

    ImGui::EndChild();
}

bool FEditorActorSequencerWidget::IsComponentPinned(const FGuid& Guid) const
{
    if (!Guid.IsValid())
    {
        return false;
    }

    return std::find(PinnedComponentGuids.begin(), PinnedComponentGuids.end(), Guid) != PinnedComponentGuids.end();
}

void FEditorActorSequencerWidget::PinComponent(UActorComponent* Component)
{
    if (!Component)
    {
        return;
    }

    Component->EnsurePersistentGuid();
    const FGuid& Guid = Component->GetPersistentGuid();
    if (Guid.IsValid() && !IsComponentPinned(Guid))
    {
        PinnedComponentGuids.push_back(Guid);
    }
}

void FEditorActorSequencerWidget::DrawAddTrackPopup(UActorSequenceComponent* SequenceComp)
{
    if (!ImGui::BeginPopup("##ActorSequencerAddTrack"))
    {
        return;
    }

    AActor* Owner = FEditorActorSequenceEditModel::GetLiveOwner(SequenceComp);
    if (!Owner)
    {
        ImGui::TextDisabled("No live actor");
        ImGui::EndPopup();
        return;
    }

    ImGui::TextUnformatted(Owner->GetName().c_str());
    ImGui::Separator();

    bool bAnyComponent = false;
    for (UActorComponent* Component : Owner->GetComponents())
    {
        if (!Component)
        {
            continue;
        }

        Component->EnsurePersistentGuid();
        const FString ComponentLabel = FEditorActorSequenceEditModel::MakeComponentLabel(Owner, Component);
        const bool bPinned = IsComponentPinned(Component->GetPersistentGuid());
        if (ImGui::MenuItem(ComponentLabel.c_str(), nullptr, bPinned))
        {
            PinComponent(Component);
        }
        bAnyComponent = true;
    }

    if (!bAnyComponent)
    {
        ImGui::TextDisabled("No components");
    }

    ImGui::EndPopup();
}

void FEditorActorSequencerWidget::DrawAddPropertyPopup(UActorSequenceComponent* SequenceComp)
{
    if (!ImGui::BeginPopup("##ActorSequencerAddProperty"))
    {
        return;
    }

    AActor* Owner = FEditorActorSequenceEditModel::GetLiveOwner(SequenceComp);
    UActorComponent* Component = FindComponentByGuid(Owner, PendingAddPropertyComponentGuid);
    if (!Component)
    {
        ImGui::TextDisabled("No component");
        ImGui::EndPopup();
        return;
    }

    const FString ComponentLabel = FEditorActorSequenceEditModel::MakeComponentLabel(Owner, Component);
    ImGui::TextUnformatted(ComponentLabel.c_str());
    ImGui::Separator();

    TArray<FPropertyDescriptor> Properties;
    FEditorActorSequenceEditModel::CollectAnimatableScalarProperties(Component, Properties);
    if (Properties.empty())
    {
        ImGui::TextDisabled("No sequencer-compatible properties");
        ImGui::EndPopup();
        return;
    }

    for (const FPropertyDescriptor& Property : Properties)
    {
        if (!Property.Name)
        {
            continue;
        }

        TArray<const char*> ChannelNames;
        FEditorActorSequenceEditModel::GetChannelNames(
            FEditorActorSequenceEditModel::GetTrackType(Property.Type),
            ChannelNames);

        if (ChannelNames.size() <= 1)
        {
            if (ImGui::MenuItem(Property.Name))
            {
                FEditorActorSequenceEditModel::CaptureSequenceUndo(EditorEngine, "Add Actor Sequence Track");
                if (FEditorActorSequenceEditModel::AddTrackForProperty(
                    SequenceComp,
                    Component,
                    Property,
                    FEditorActorSequenceEditModel::GetDefaultChannelName(Property.Type)))
                {
                    FEditorActorSequenceEditModel::NotifySequenceEdited(
                        EditorEngine,
                        SequenceComp,
                        "Add Actor Sequence Track",
                        false);
                }
            }
            continue;
        }

        if (ImGui::BeginMenu(Property.Name))
        {
            if (ImGui::MenuItem("All Channels"))
            {
                FEditorActorSequenceEditModel::CaptureSequenceUndo(EditorEngine, "Add Actor Sequence Channels");
                bool bChanged = false;
                for (const char* ChannelName : ChannelNames)
                {
                    bChanged |= FEditorActorSequenceEditModel::AddTrackForProperty(
                        SequenceComp,
                        Component,
                        Property,
                        ChannelName);
                }
                if (bChanged)
                {
                    FEditorActorSequenceEditModel::NotifySequenceEdited(
                        EditorEngine,
                        SequenceComp,
                        "Add Actor Sequence Channels",
                        false);
                }
            }

            ImGui::Separator();
            for (const char* ChannelName : ChannelNames)
            {
                if (ImGui::MenuItem(ChannelName))
                {
                    FEditorActorSequenceEditModel::CaptureSequenceUndo(EditorEngine, "Add Actor Sequence Track");
                    if (FEditorActorSequenceEditModel::AddTrackForProperty(
                        SequenceComp,
                        Component,
                        Property,
                        ChannelName))
                    {
                        FEditorActorSequenceEditModel::NotifySequenceEdited(
                            EditorEngine,
                            SequenceComp,
                            "Add Actor Sequence Track",
                            false);
                    }
                }
            }
            ImGui::EndMenu();
        }
    }

    ImGui::EndPopup();
}

void FEditorActorSequencerWidget::AddKeyToSelectedTrack(UActorSequenceComponent* SequenceComp)
{
    UActorSequencePlayer* PreviewPlayer = SequenceComp ? SequenceComp->GetPreviewSequencePlayer() : nullptr;
    if (!PreviewPlayer)
    {
        return;
    }

    FActorSequenceChannelHandle Handle;
    if (!FEditorActorSequenceEditModel::ResolveChannelByDisplayIndex(SequenceComp, SelectedTrackIndex, Handle))
    {
        return;
    }

    FEditorActorSequenceEditModel::CaptureSequenceUndo(EditorEngine, "Add Actor Sequence Key");
    if (FEditorActorSequenceEditModel::AddKeyAtCurrentValue(SequenceComp, Handle, PreviewPlayer->GetCurrentTime()))
    {
        FEditorActorSequenceEditModel::NotifySequenceEdited(
            EditorEngine,
            SequenceComp,
            "Add Actor Sequence Key",
            false);
    }
}

void FEditorActorSequencerWidget::DrawSequencer(UActorSequenceComponent* SequenceComp)
{
    UActorSequence* Sequence = SequenceComp ? SequenceComp->GetSequence() : nullptr;
    UActorSequencePlayer* PreviewPlayer = SequenceComp ? SequenceComp->GetPreviewSequencePlayer() : nullptr;
    if (!Sequence)
    {
        return;
    }

    int32 TrackCount = 0;
    for (const FActorSequenceBinding& Binding : Sequence->Bindings)
    {
        for (const FActorSequenceTrack& Track : Binding.Tracks)
        {
            for (const FActorSequenceSection& Section : Track.Sections)
            {
                TrackCount += static_cast<int32>(Section.Channels.size());
            }
        }
    }

    const ImVec2 PanelPos = ImGui::GetCursorScreenPos();
    const ImVec2 PanelSize = ImGui::GetContentRegionAvail();
    const ImVec2 PanelEnd(PanelPos.x + PanelSize.x, PanelPos.y + PanelSize.y);
    const float TimelineX = PanelPos.x + SequencerLeftPanelWidth;
    const float TimelineWidth = std::max(1.0f, PanelEnd.x - TimelineX);
    const float TrackTop = PanelPos.y + SequencerRulerHeight;
    const float TrackHeight = std::max(1.0f, PanelEnd.y - TrackTop);
    const float PlaybackStart = Sequence->StartTime;
    const float PlaybackEnd = Sequence->StartTime + std::max(0.0f, Sequence->Duration);
    constexpr float MinPlaybackDuration = 0.001f;

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImU32 BgColor = ImGui::GetColorU32(ImGuiCol_WindowBg);
    const ImU32 HeaderColor = ImGui::GetColorU32(ImGuiCol_Header);
    const ImU32 BorderColor = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 GridColor = WithAlpha(ImGui::GetColorU32(ImGuiCol_TextDisabled), 0.35f);
    const ImU32 MinorGridColor = WithAlpha(ImGui::GetColorU32(ImGuiCol_TextDisabled), 0.18f);
    const ImU32 SectionColor = ImGui::GetColorU32(ImGuiCol_Button);
    const ImU32 SelectedColor = ImGui::GetColorU32(ImGuiCol_ButtonActive);

    DrawList->AddRectFilled(PanelPos, PanelEnd, BgColor);
    DrawList->AddRectFilled(PanelPos, ImVec2(TimelineX, PanelEnd.y), WithAlpha(HeaderColor, 0.45f));
    DrawList->AddRectFilled(ImVec2(TimelineX, PanelPos.y), PanelEnd, WithAlpha(BgColor, 0.92f));
    DrawList->AddLine(ImVec2(TimelineX, PanelPos.y), ImVec2(TimelineX, PanelEnd.y), BorderColor);
    DrawList->AddLine(ImVec2(PanelPos.x, TrackTop), ImVec2(PanelEnd.x, TrackTop), BorderColor);
    DrawList->AddRect(PanelPos, PanelEnd, BorderColor);

    ImGui::InvisibleButton("##ActorSequencerCanvas", PanelSize);
    const bool bCanvasHovered = ImGui::IsItemHovered();
    const bool bCanvasActive = ImGui::IsItemActive();
    const ImVec2 Mouse = ImGui::GetIO().MousePos;

    const float VisibleRange = ClampSequencerRange(ViewEndTime - ViewStartTime);
    const auto TimeToX = [&](float Time)
    {
        return TimelineX + ((Time - ViewStartTime) / VisibleRange) * TimelineWidth;
    };
    const auto XToTime = [&](float X)
    {
        return ViewStartTime + ((X - TimelineX) / TimelineWidth) * VisibleRange;
    };

    const auto ClampPreviewTime = [&]()
    {
        if (PreviewPlayer)
        {
            const float ClampedTime = std::clamp(
                PreviewPlayer->GetCurrentTime(),
                Sequence->StartTime,
                Sequence->StartTime + std::max(0.0f, Sequence->Duration));
            SequenceComp->SetPreviewTime(ClampedTime);
        }
    };

    const float PlaybackStartX = TimeToX(PlaybackStart);
    const float PlaybackEndX = TimeToX(PlaybackEnd);
    const bool bMouseInRuler = Mouse.x >= TimelineX && Mouse.y >= PanelPos.y && Mouse.y <= TrackTop;
    const bool bHitStartHandle = bMouseInRuler && std::fabs(Mouse.x - PlaybackStartX) <= 8.0f;
    const bool bHitEndHandle = bMouseInRuler && std::fabs(Mouse.x - PlaybackEndX) <= 8.0f;

    if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (bHitStartHandle)
        {
            FEditorActorSequenceEditModel::CaptureSequenceUndo(
                EditorEngine,
                "Edit Actor Sequence Playback Range");
            bDraggingPlaybackStart = true;
            bDraggingPlaybackEnd = false;
        }
        else if (bHitEndHandle)
        {
            FEditorActorSequenceEditModel::CaptureSequenceUndo(
                EditorEngine,
                "Edit Actor Sequence Playback Range");
            bDraggingPlaybackEnd = true;
            bDraggingPlaybackStart = false;
        }
    }

    if ((bDraggingPlaybackStart || bDraggingPlaybackEnd) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float MouseTime = XToTime(std::clamp(Mouse.x, TimelineX, PanelEnd.x));
        if (!ImGui::GetIO().KeyAlt)
        {
            MouseTime = SnapSequencerTime(MouseTime);
        }
        if (bDraggingPlaybackStart)
        {
            FEditorActorSequenceEditModel::ResizePlaybackRange(
                *Sequence,
                MouseTime,
                true,
                MinPlaybackDuration);
        }
        else
        {
            FEditorActorSequenceEditModel::ResizePlaybackRange(
                *Sequence,
                MouseTime,
                false,
                MinPlaybackDuration);
        }

        SequenceComp->MarkSequenceDirty();
        ClampPreviewTime();
    }

    if ((bDraggingPlaybackStart || bDraggingPlaybackEnd) && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        bDraggingPlaybackStart = false;
        bDraggingPlaybackEnd = false;
        FEditorActorSequenceEditModel::NotifySequenceEdited(
            EditorEngine,
            SequenceComp,
            "Edit Actor Sequence Playback Range",
            false);
    }

    if (bCanvasHovered
        && ImGui::IsKeyPressed(ImGuiKey_Delete, false)
        && SelectedKeyTrackIndex >= 0
        && SelectedKeyIndex >= 0)
    {
        FActorSequenceChannelHandle Handle;
        if (FEditorActorSequenceEditModel::ResolveChannelByDisplayIndex(SequenceComp, SelectedKeyTrackIndex, Handle))
        {
            FEditorActorSequenceEditModel::CaptureSequenceUndo(EditorEngine, "Delete Actor Sequence Key");
            if (FEditorActorSequenceEditModel::DeleteKeyByIndex(SequenceComp, Handle, SelectedKeyIndex))
            {
                SelectedKeyIndex = -1;
                SelectedKeyTrackIndex = -1;
                FEditorActorSequenceEditModel::NotifySequenceEdited(
                    EditorEngine,
                    SequenceComp,
                    "Delete Actor Sequence Key",
                    false);
            }
        }
    }

    if (bCanvasHovered && std::fabs(ImGui::GetIO().MouseWheel) > 0.0f)
    {
        const float MouseTime = XToTime(std::clamp(Mouse.x, TimelineX, PanelEnd.x));
        const float ZoomFactor = ImGui::GetIO().MouseWheel > 0.0f ? 0.85f : 1.0f / 0.85f;
        const float NewRange = ClampSequencerRange(VisibleRange * ZoomFactor);
        const float AnchorAlpha = std::clamp((MouseTime - ViewStartTime) / VisibleRange, 0.0f, 1.0f);
        ViewStartTime = MouseTime - NewRange * AnchorAlpha;
        ViewEndTime = ViewStartTime + NewRange;
    }

    if (bCanvasActive
        && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
    {
        const float DeltaTime = -(ImGui::GetIO().MouseDelta.x / TimelineWidth) * VisibleRange;
        ViewStartTime += DeltaTime;
        ViewEndTime += DeltaTime;
    }

    if (bCanvasHovered
        && !bDraggingPlaybackStart
        && !bDraggingPlaybackEnd
        && !bHitStartHandle
        && !bHitEndHandle
        && ImGui::IsMouseDown(ImGuiMouseButton_Left)
        && Mouse.x >= TimelineX
        && Mouse.y <= TrackTop)
    {
        const float NewTime = std::clamp(XToTime(Mouse.x), Sequence->StartTime, Sequence->StartTime + std::max(0.0f, Sequence->Duration));
        SequenceComp->SetPreviewTime(NewTime);
    }

    DrawList->PushClipRect(ImVec2(TimelineX, PanelPos.y), PanelEnd, true);

    const float MajorStepCandidates[] = { 0.01f, 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 30.0f, 60.0f, 300.0f, 600.0f };
    float MajorStep = 1.0f;
    for (float Candidate : MajorStepCandidates)
    {
        if ((Candidate / VisibleRange) * TimelineWidth >= 72.0f)
        {
            MajorStep = Candidate;
            break;
        }
    }
    const float MinorStep = MajorStep * 0.5f;
    const float FirstMinor = std::floor(ViewStartTime / MinorStep) * MinorStep;
    for (float Time = FirstMinor; Time <= ViewEndTime + MinorStep; Time += MinorStep)
    {
        const float X = TimeToX(Time);
        const bool bMajor = std::fabs(std::fmod(std::fabs(Time), MajorStep)) < 0.0001f;
        DrawList->AddLine(ImVec2(X, PanelPos.y), ImVec2(X, PanelEnd.y), bMajor ? GridColor : MinorGridColor);
        if (bMajor)
        {
            char Label[32];
            snprintf(Label, sizeof(Label), "%.2f", Time);
            DrawList->AddText(ImVec2(X + 4.0f, PanelPos.y + 7.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), Label);
        }
    }

    const float CurrentPlaybackStartX = TimeToX(Sequence->StartTime);
    const float CurrentPlaybackEndX = TimeToX(Sequence->StartTime + std::max(0.0f, Sequence->Duration));
    DrawList->AddRectFilled(
        ImVec2(CurrentPlaybackStartX, PanelPos.y),
        ImVec2(CurrentPlaybackEndX, TrackTop),
        IM_COL32(84, 150, 84, 32));
    DrawList->AddLine(ImVec2(CurrentPlaybackStartX, PanelPos.y), ImVec2(CurrentPlaybackStartX, PanelEnd.y), IM_COL32(44, 220, 84, 255), 2.0f);
    DrawList->AddLine(ImVec2(CurrentPlaybackEndX, PanelPos.y), ImVec2(CurrentPlaybackEndX, PanelEnd.y), IM_COL32(220, 24, 24, 255), 2.0f);
    DrawList->AddRectFilled(
        ImVec2(CurrentPlaybackStartX - 5.0f, PanelPos.y + 2.0f),
        ImVec2(CurrentPlaybackStartX + 5.0f, PanelPos.y + 16.0f),
        IM_COL32(44, 220, 84, 255),
        2.0f);
    DrawList->AddRectFilled(
        ImVec2(CurrentPlaybackEndX - 5.0f, PanelPos.y + 2.0f),
        ImVec2(CurrentPlaybackEndX + 5.0f, PanelPos.y + 16.0f),
        IM_COL32(220, 24, 24, 255),
        2.0f);

    const float CurrentTime = PreviewPlayer ? PreviewPlayer->GetCurrentTime() : 0.0f;
    const float PlayheadX = TimeToX(CurrentTime);
    DrawList->AddLine(ImVec2(PlayheadX, PanelPos.y), ImVec2(PlayheadX, PanelEnd.y), IM_COL32(245, 245, 245, 255), 2.0f);
    DrawList->AddRectFilled(ImVec2(PlayheadX - 6.0f, PanelPos.y + 2.0f), ImVec2(PlayheadX + 6.0f, PanelPos.y + 16.0f), IM_COL32(255, 64, 64, 255), 2.0f);
    {
        char StartLabel[32];
        char EndLabel[32];
        snprintf(StartLabel, sizeof(StartLabel), "%.2fs", Sequence->StartTime);
        snprintf(EndLabel, sizeof(EndLabel), "%.2fs", Sequence->StartTime + std::max(0.0f, Sequence->Duration));
        DrawList->AddText(ImVec2(CurrentPlaybackStartX + 4.0f, PanelPos.y + 18.0f), IM_COL32(80, 230, 112, 255), StartLabel);
        DrawList->AddText(ImVec2(CurrentPlaybackEndX + 4.0f, PanelPos.y + 18.0f), IM_COL32(235, 80, 80, 255), EndLabel);
    }

    DrawList->PopClipRect();

    int32 RowIndex = 0;
    int32 DisplayTrackIndex = 0;
    AActor* Owner = FEditorActorSequenceEditModel::GetLiveOwner(SequenceComp);
    TArray<UActorComponent*> TrackedComponents;
    for (const FActorSequenceBinding& Binding : Sequence->Bindings)
    {
        UActorComponent* Component = FEditorActorSequenceEditModel::ResolveBindingComponent(SequenceComp, Binding);
        if (Component && !ContainsComponent(TrackedComponents, Component))
        {
            TrackedComponents.push_back(Component);
        }
    }

    const auto IsComponentVisibleInSequencer = [&](UActorComponent* Component)
    {
        if (!Component)
        {
            return false;
        }
        const bool bPinned = Component->GetPersistentGuid().IsValid() && IsComponentPinned(Component->GetPersistentGuid());
        return bPinned || ContainsComponent(TrackedComponents, Component);
    };

    std::function<bool(USceneComponent*)> HasVisibleDescendant = [&](USceneComponent* SceneComponent)
    {
        if (!SceneComponent)
        {
            return false;
        }
        if (IsComponentVisibleInSequencer(SceneComponent))
        {
            return true;
        }
        for (USceneComponent* Child : SceneComponent->GetChildren())
        {
            if (HasVisibleDescendant(Child))
            {
                return true;
            }
        }
        return false;
    };

    const auto DrawTrackRow = [&](
        FActorSequenceTrack& Track,
        FActorSequenceSection& Section,
        FActorSequenceChannel& Channel,
        int32 Depth)
    {
        const float RowY = TrackTop + RowIndex * SequencerRowHeight;
        if (RowY > PanelEnd.y)
        {
            return;
        }

        const bool bSelected = DisplayTrackIndex == SelectedTrackIndex;
        const ImVec2 RowMin(PanelPos.x, RowY);
        const ImVec2 RowMax(PanelEnd.x, RowY + SequencerRowHeight);
        DrawList->AddRectFilled(
            RowMin,
            RowMax,
            bSelected ? WithAlpha(SelectedColor, 0.55f) : WithAlpha(HeaderColor, RowIndex % 2 == 0 ? 0.18f : 0.08f));
        DrawList->AddLine(ImVec2(PanelPos.x, RowMax.y), ImVec2(PanelEnd.x, RowMax.y), WithAlpha(BorderColor, 0.55f));

        const FString Label = MakeTrackLabel(Track, Channel);
        DrawList->AddText(
            ImVec2(PanelPos.x + 30.0f + Depth * 14.0f, RowY + 7.0f),
            ImGui::GetColorU32(ImGuiCol_Text),
            Label.c_str());

        const float PlaybackRangeMinX = std::max(TimelineX + 2.0f, TimeToX(PlaybackStart));
        const float PlaybackRangeMaxX = std::min(PanelEnd.x - 2.0f, TimeToX(PlaybackEnd));
        if (PlaybackRangeMaxX > PlaybackRangeMinX)
        {
            DrawList->AddRectFilled(
                ImVec2(PlaybackRangeMinX, RowY + 7.0f),
                ImVec2(PlaybackRangeMaxX, RowY + SequencerRowHeight - 7.0f),
                WithAlpha(SectionColor, bSelected ? 0.38f : 0.22f),
                2.0f);
        }

        UCurveFloatAsset* Curve = FEditorActorSequenceEditModel::GetOrCreateChannelCurve(Channel);
        float DisplaySectionStartTime = Section.StartTime;
        float DisplaySectionEndTime = Section.StartTime + std::max(0.0f, Section.Duration);
        FEditorActorSequenceTimeUtils::GetDisplaySectionRange(
            Section,
            Channel,
            Curve,
            DisplaySectionStartTime,
            DisplaySectionEndTime);

        const float SectionMinX = TimeToX(DisplaySectionStartTime);
        const float SectionMaxX = TimeToX(DisplaySectionEndTime);
        const ImVec2 SectionMin(std::max(TimelineX + 2.0f, SectionMinX), RowY + 5.0f);
        const ImVec2 SectionMax(std::min(PanelEnd.x - 2.0f, SectionMaxX), RowY + SequencerRowHeight - 5.0f);
        const bool bSectionVisible = SectionMax.x > TimelineX && SectionMin.x < PanelEnd.x && SectionMax.x > SectionMin.x;
        if (SectionMax.x > TimelineX && SectionMin.x < PanelEnd.x && SectionMax.x > SectionMin.x)
        {
            DrawList->AddRectFilled(SectionMin, SectionMax, bSelected ? SelectedColor : SectionColor, 3.0f);
            DrawList->AddRect(SectionMin, SectionMax, BorderColor, 3.0f);
            DrawList->AddRectFilled(
                ImVec2(SectionMin.x, SectionMin.y),
                ImVec2(SectionMin.x + 4.0f, SectionMax.y),
                WithAlpha(BorderColor, 0.85f),
                2.0f);
            DrawList->AddRectFilled(
                ImVec2(SectionMax.x - 4.0f, SectionMin.y),
                ImVec2(SectionMax.x, SectionMax.y),
                WithAlpha(BorderColor, 0.85f),
                2.0f);
        }

        int32 HoveredKeyIndex = -1;
        const bool bRowTimelineHovered =
            bCanvasHovered
            && Mouse.x >= TimelineX
            && Mouse.x <= PanelEnd.x
            && Mouse.y >= RowMin.y
            && Mouse.y < RowMax.y;
        const bool bSectionHovered =
            bSectionVisible
            && bCanvasHovered
            && Mouse.x >= SectionMin.x
            && Mouse.x <= SectionMax.x
            && Mouse.y >= SectionMin.y
            && Mouse.y <= SectionMax.y;
        const bool bHitSectionStartHandle = bSectionHovered && std::fabs(Mouse.x - SectionMinX) <= 6.0f;
        const bool bHitSectionEndHandle = bSectionHovered && std::fabs(Mouse.x - SectionMaxX) <= 6.0f;

        if (Curve)
        {
            const FFloatCurve& FloatCurve = Curve->GetCurve();
            for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(FloatCurve.Keys.size()); ++KeyIndex)
            {
                const FCurveKey& Key = FloatCurve.Keys[KeyIndex];
                const float KeySequenceTime = FEditorActorSequenceTimeUtils::CurveTimeToSequenceTime(Section, Channel, Key.Time);
                const float KeyX = TimeToX(KeySequenceTime);
                if (KeyX < TimelineX || KeyX > PanelEnd.x)
                {
                    continue;
                }

                const float KeyY = RowY + SequencerRowHeight * 0.5f;
                const bool bSelectedKey = SelectedKeyTrackIndex == DisplayTrackIndex && SelectedKeyIndex == KeyIndex;
                const float KeyRadius = bSelectedKey ? 5.5f : 4.0f;
                DrawList->AddQuadFilled(
                    ImVec2(KeyX, KeyY - KeyRadius),
                    ImVec2(KeyX + KeyRadius, KeyY),
                    ImVec2(KeyX, KeyY + KeyRadius),
                    ImVec2(KeyX - KeyRadius, KeyY),
                    bSelectedKey ? IM_COL32(255, 248, 160, 255) : IM_COL32(245, 220, 86, 255));
                if (bSelectedKey)
                {
                    DrawList->AddQuad(
                        ImVec2(KeyX, KeyY - KeyRadius - 2.0f),
                        ImVec2(KeyX + KeyRadius + 2.0f, KeyY),
                        ImVec2(KeyX, KeyY + KeyRadius + 2.0f),
                        ImVec2(KeyX - KeyRadius - 2.0f, KeyY),
                        IM_COL32(255, 255, 255, 255),
                        1.0f);
                }

                if (bRowTimelineHovered)
                {
                    const float Dx = KeyX - Mouse.x;
                    const float Dy = KeyY - Mouse.y;
                    if ((Dx * Dx + Dy * Dy) <= 100.0f)
                    {
                        HoveredKeyIndex = KeyIndex;
                        break;
                    }
                }
            }
        }

        if (bRowTimelineHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            SelectedTrackIndex = DisplayTrackIndex;
            if (HoveredKeyIndex >= 0 && Curve)
            {
                FEditorActorSequenceEditModel::CaptureSequenceUndo(EditorEngine, "Edit Actor Sequence Key");
                SelectedKeyTrackIndex = DisplayTrackIndex;
                SelectedKeyIndex = HoveredKeyIndex;
                bDraggingKey = true;
                DraggingKeyTrackIndex = DisplayTrackIndex;
                DraggingKeyIndex = HoveredKeyIndex;
                SequenceComp->SetPreviewTime(FEditorActorSequenceTimeUtils::CurveTimeToSequenceTime(
                    Section,
                    Channel,
                    Curve->GetCurve().Keys[HoveredKeyIndex].Time));
            }
            else if (bHitSectionStartHandle || bHitSectionEndHandle)
            {
                FEditorActorSequenceEditModel::CaptureSequenceUndo(EditorEngine, "Edit Actor Sequence Section");
                bDraggingSectionStart = bHitSectionStartHandle;
                bDraggingSectionEnd = bHitSectionEndHandle;
                DraggingSectionTrackIndex = DisplayTrackIndex;
                SelectedKeyTrackIndex = -1;
                SelectedKeyIndex = -1;
            }
            else
            {
                SelectedKeyTrackIndex = -1;
                SelectedKeyIndex = -1;
            }
        }
        if (bRowTimelineHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            SelectedTrackIndex = DisplayTrackIndex;
            ContextTrackIndex = DisplayTrackIndex;
            ContextSequenceTime = SnapSequencerTime(XToTime(Mouse.x));
            SelectedKeyTrackIndex = HoveredKeyIndex >= 0 ? DisplayTrackIndex : -1;
            SelectedKeyIndex = HoveredKeyIndex;
            ImGui::OpenPopup("##ActorSequencerTrackContext");
        }
        if (bDraggingKey
            && DraggingKeyTrackIndex == DisplayTrackIndex
            && ImGui::IsMouseDown(ImGuiMouseButton_Left)
            && Curve)
        {
            float DragTime = XToTime(Mouse.x);
            if (!ImGui::GetIO().KeyAlt)
            {
                DragTime = SnapSequencerTime(DragTime);
            }

            FActorSequenceChannelHandle Handle;
            if (FEditorActorSequenceEditModel::ResolveChannelByDisplayIndex(SequenceComp, DisplayTrackIndex, Handle))
            {
                int32 NewKeyIndex = DraggingKeyIndex;
                if (FEditorActorSequenceEditModel::MoveKeyToSequenceTime(
                    SequenceComp,
                    Handle,
                    DraggingKeyIndex,
                    DragTime,
                    &NewKeyIndex))
                {
                    DraggingKeyIndex = NewKeyIndex;
                    SelectedKeyIndex = NewKeyIndex;
                    SelectedKeyTrackIndex = DisplayTrackIndex;
                    SequenceComp->MarkSequenceDirty();
                }
            }
        }
        if (bDraggingKey
            && DraggingKeyTrackIndex == DisplayTrackIndex
            && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            bDraggingKey = false;
            DraggingKeyTrackIndex = -1;
            DraggingKeyIndex = -1;
            FEditorActorSequenceEditModel::NotifySequenceEdited(
                EditorEngine,
                SequenceComp,
                "Edit Actor Sequence Key",
                false);
        }
        if ((bDraggingSectionStart || bDraggingSectionEnd)
            && DraggingSectionTrackIndex == DisplayTrackIndex
            && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            float DragTime = XToTime(std::clamp(Mouse.x, TimelineX, PanelEnd.x));
            if (!ImGui::GetIO().KeyAlt)
            {
                DragTime = SnapSequencerTime(DragTime);
            }

            if (bDraggingSectionStart)
            {
                FEditorActorSequenceEditModel::ResizeSection(
                    Section,
                    DragTime,
                    true,
                    MinPlaybackDuration);
            }
            else
            {
                FEditorActorSequenceEditModel::ResizeSection(
                    Section,
                    DragTime,
                    false,
                    MinPlaybackDuration);
            }

            SequenceComp->MarkSequenceDirty();
        }
        if ((bDraggingSectionStart || bDraggingSectionEnd)
            && DraggingSectionTrackIndex == DisplayTrackIndex
            && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            bDraggingSectionStart = false;
            bDraggingSectionEnd = false;
            DraggingSectionTrackIndex = -1;
            FEditorActorSequenceEditModel::NotifySequenceEdited(
                EditorEngine,
                SequenceComp,
                "Edit Actor Sequence Section",
                false);
        }
        if (bRowTimelineHovered && Curve)
        {
            const float HoverSequenceTime = XToTime(Mouse.x);
            const float HoverCurveTime = FEditorActorSequenceTimeUtils::SequenceTimeToCurveTime(Section, Channel, HoverSequenceTime);
            const float HoverValue = Curve->Evaluate(HoverCurveTime);
            ImGui::BeginTooltip();
            ImGui::Text("Track: %s", MakeTrackLabel(Track, Channel).c_str());
            ImGui::Text("Time: %.3f", HoverSequenceTime);
            ImGui::Text("Curve Time: %.3f", HoverCurveTime);
            ImGui::Text("Value: %.3f", HoverValue);
            ImGui::EndTooltip();
        }
        if (bRowTimelineHovered
            && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
            && EditorEngine
            && Curve)
        {
            EditorEngine->GetMainPanel().OpenCurveFromActorSequence(
                Curve,
                SequenceComp,
                MakeTrackLabel(Track, Channel),
                Channel.Playback.CurveAssetPath,
                HoveredKeyIndex);
        }

        if (ContextTrackIndex == DisplayTrackIndex && ImGui::BeginPopup("##ActorSequencerTrackContext"))
        {
            ImGui::TextDisabled("%s", MakeTrackLabel(Track, Channel).c_str());
            ImGui::Text("Time: %.3f", ContextSequenceTime);
            if (ImGui::MenuItem("Add Key Here"))
            {
                FActorSequenceChannelHandle Handle;
                if (FEditorActorSequenceEditModel::ResolveChannelByDisplayIndex(SequenceComp, DisplayTrackIndex, Handle))
                {
                    FEditorActorSequenceEditModel::CaptureSequenceUndo(EditorEngine, "Add Actor Sequence Key");
                    SequenceComp->SetPreviewTime(ContextSequenceTime);
                    if (FEditorActorSequenceEditModel::AddKeyAtCurrentValue(SequenceComp, Handle, ContextSequenceTime))
                    {
                        FEditorActorSequenceEditModel::NotifySequenceEdited(
                            EditorEngine,
                            SequenceComp,
                            "Add Actor Sequence Key",
                            false);
                    }
                }
            }
            if (ImGui::MenuItem("Delete Key Near Here"))
            {
                FActorSequenceChannelHandle Handle;
                if (FEditorActorSequenceEditModel::ResolveChannelByDisplayIndex(SequenceComp, DisplayTrackIndex, Handle))
                {
                    FEditorActorSequenceEditModel::CaptureSequenceUndo(EditorEngine, "Delete Actor Sequence Key");
                    const float VisibleSecondsPerPixel = VisibleRange / std::max(1.0f, TimelineWidth);
                    if (FEditorActorSequenceEditModel::RemoveKeyNearTime(
                        SequenceComp,
                        Handle,
                        ContextSequenceTime,
                        VisibleSecondsPerPixel * 10.0f))
                    {
                        FEditorActorSequenceEditModel::NotifySequenceEdited(
                            EditorEngine,
                            SequenceComp,
                            "Delete Actor Sequence Key",
                            false);
                    }
                }
            }
            ImGui::BeginDisabled(SelectedKeyTrackIndex != DisplayTrackIndex || SelectedKeyIndex < 0);
            if (ImGui::MenuItem("Delete Selected Key"))
            {
                FActorSequenceChannelHandle Handle;
                if (FEditorActorSequenceEditModel::ResolveChannelByDisplayIndex(SequenceComp, DisplayTrackIndex, Handle))
                {
                    FEditorActorSequenceEditModel::CaptureSequenceUndo(EditorEngine, "Delete Actor Sequence Key");
                    if (FEditorActorSequenceEditModel::DeleteKeyByIndex(SequenceComp, Handle, SelectedKeyIndex))
                    {
                        SelectedKeyTrackIndex = -1;
                        SelectedKeyIndex = -1;
                        FEditorActorSequenceEditModel::NotifySequenceEdited(
                            EditorEngine,
                            SequenceComp,
                            "Delete Actor Sequence Key",
                            false);
                    }
                }
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }

        if (bCanvasHovered
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && Mouse.x >= PanelPos.x
            && Mouse.x < TimelineX
            && Mouse.y >= RowMin.y
            && Mouse.y < RowMax.y)
        {
            SelectedTrackIndex = DisplayTrackIndex;
        }

        ++RowIndex;
        ++DisplayTrackIndex;
    };

    const auto DrawTrackGroupRow = [&](const FActorSequenceTrack& Track, int32 Depth)
    {
        const float RowY = TrackTop + RowIndex * SequencerRowHeight;
        if (RowY > PanelEnd.y)
        {
            return;
        }

        const ImVec2 RowMin(PanelPos.x, RowY);
        const ImVec2 RowMax(PanelEnd.x, RowY + SequencerRowHeight);
        DrawList->AddRectFilled(RowMin, RowMax, WithAlpha(HeaderColor, 0.20f));
        DrawList->AddLine(ImVec2(PanelPos.x, RowMax.y), ImVec2(PanelEnd.x, RowMax.y), WithAlpha(BorderColor, 0.55f));
        DrawList->AddText(
            ImVec2(PanelPos.x + 24.0f + Depth * 14.0f, RowY + 7.0f),
            ImGui::GetColorU32(ImGuiCol_TextDisabled),
            Track.PropertyPath.empty() ? "<Group>" : Track.PropertyPath.c_str());
        ++RowIndex;
    };

    const auto DrawComponentTracks = [&](UActorComponent* Component, int32 Depth)
    {
        for (FActorSequenceBinding& Binding : Sequence->Bindings)
        {
            UActorComponent* BindingComponent = FEditorActorSequenceEditModel::ResolveBindingComponent(SequenceComp, Binding);
            if (BindingComponent != Component)
            {
                continue;
            }

            for (FActorSequenceTrack& Track : Binding.Tracks)
            {
                const bool bGroupedTrack = IsGroupedTrackType(Track.TrackType);
                int32 ChannelCount = 0;
                for (const FActorSequenceSection& Section : Track.Sections)
                {
                    ChannelCount += static_cast<int32>(Section.Channels.size());
                }
                if (bGroupedTrack && ChannelCount > 1)
                {
                    DrawTrackGroupRow(Track, Depth + 1);
                }

                for (FActorSequenceSection& Section : Track.Sections)
                {
                    for (FActorSequenceChannel& Channel : Section.Channels)
                    {
                        DrawTrackRow(Track, Section, Channel, Depth + (bGroupedTrack && ChannelCount > 1 ? 2 : 1));
                    }
                }
            }
        }
    };

    const auto DrawComponentRow = [&](UActorComponent* Component, int32 Depth)
    {
        if (!Component)
        {
            return;
        }

        const float RowY = TrackTop + RowIndex * SequencerRowHeight;
        if (RowY > PanelEnd.y)
        {
            return;
        }

        const ImVec2 RowMin(PanelPos.x, RowY);
        const ImVec2 RowMax(PanelEnd.x, RowY + SequencerRowHeight);
        DrawList->AddRectFilled(RowMin, RowMax, WithAlpha(HeaderColor, 0.24f));
        DrawList->AddLine(ImVec2(PanelPos.x, RowMax.y), ImVec2(PanelEnd.x, RowMax.y), WithAlpha(BorderColor, 0.65f));

        const FString ComponentLabel = FEditorActorSequenceEditModel::MakeComponentLabel(Owner, Component);
        DrawList->AddText(
            ImVec2(PanelPos.x + 16.0f + Depth * 14.0f, RowY + 7.0f),
            ImGui::GetColorU32(ImGuiCol_Text),
            ComponentLabel.c_str());

        const ImVec2 PlusMin(TimelineX - 28.0f, RowY + 5.0f);
        const ImVec2 PlusMax(TimelineX - 8.0f, RowY + SequencerRowHeight - 5.0f);
        DrawList->AddRectFilled(PlusMin, PlusMax, WithAlpha(SelectedColor, 0.65f), 3.0f);
        DrawList->AddText(ImVec2(PlusMin.x + 6.0f, PlusMin.y + 1.0f), ImGui::GetColorU32(ImGuiCol_Text), "+");

        if (bCanvasHovered
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && Mouse.x >= PlusMin.x
            && Mouse.x <= PlusMax.x
            && Mouse.y >= PlusMin.y
            && Mouse.y <= PlusMax.y)
        {
            Component->EnsurePersistentGuid();
            PendingAddPropertyComponentGuid = Component->GetPersistentGuid();
            PinComponent(Component);
            ImGui::OpenPopup("##ActorSequencerAddProperty");
        }

        ++RowIndex;
        DrawComponentTracks(Component, Depth);
    };

    std::function<void(USceneComponent*, int32)> DrawSceneComponentTree = [&](USceneComponent* SceneComponent, int32 Depth)
    {
        if (!SceneComponent || !HasVisibleDescendant(SceneComponent))
        {
            return;
        }

        if (IsComponentVisibleInSequencer(SceneComponent))
        {
            DrawComponentRow(SceneComponent, Depth);
        }

        for (USceneComponent* Child : SceneComponent->GetChildren())
        {
            DrawSceneComponentTree(Child, Depth + 1);
        }
    };

    if (Owner)
    {
        const ImVec2 ActorRowMin(PanelPos.x, TrackTop);
        const ImVec2 ActorRowMax(PanelEnd.x, TrackTop + SequencerRowHeight);
        DrawList->AddRectFilled(ActorRowMin, ActorRowMax, WithAlpha(HeaderColor, 0.30f));
        DrawList->AddLine(ImVec2(PanelPos.x, ActorRowMax.y), ImVec2(PanelEnd.x, ActorRowMax.y), WithAlpha(BorderColor, 0.75f));
        const FString ActorLabel = "Actor  " + Owner->GetName();
        DrawList->AddText(ImVec2(PanelPos.x + 12.0f, TrackTop + 7.0f), ImGui::GetColorU32(ImGuiCol_Text), ActorLabel.c_str());
        const ImVec2 PlusMin(TimelineX - 28.0f, TrackTop + 5.0f);
        const ImVec2 PlusMax(TimelineX - 8.0f, TrackTop + SequencerRowHeight - 5.0f);
        DrawList->AddRectFilled(PlusMin, PlusMax, WithAlpha(SelectedColor, 0.65f), 3.0f);
        DrawList->AddText(ImVec2(PlusMin.x + 6.0f, PlusMin.y + 1.0f), ImGui::GetColorU32(ImGuiCol_Text), "+");

        if (bCanvasHovered
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && Mouse.x >= PlusMin.x
            && Mouse.x <= PlusMax.x
            && Mouse.y >= PlusMin.y
            && Mouse.y <= PlusMax.y)
        {
            if (UActorComponent* RootComponent = Owner->GetRootComponent())
            {
                RootComponent->EnsurePersistentGuid();
                PendingAddPropertyComponentGuid = RootComponent->GetPersistentGuid();
                PinComponent(RootComponent);
                ImGui::OpenPopup("##ActorSequencerAddProperty");
            }
        }
        ++RowIndex;
    }

    if (Owner)
    {
        if (USceneComponent* RootComponent = Owner->GetRootComponent())
        {
            DrawSceneComponentTree(RootComponent, 1);
        }

        for (UActorComponent* Component : Owner->GetComponents())
        {
            if (!Component || Cast<USceneComponent>(Component))
            {
                continue;
            }

            if (IsComponentVisibleInSequencer(Component))
            {
                DrawComponentRow(Component, 1);
            }
        }
    }

    DrawAddPropertyPopup(SequenceComp);

    if (TrackCount == 0)
    {
        DrawList->AddText(
            ImVec2(PanelPos.x + 16.0f, TrackTop + SequencerRowHeight + 16.0f),
            ImGui::GetColorU32(ImGuiCol_TextDisabled),
            "No tracks");
    }
}
