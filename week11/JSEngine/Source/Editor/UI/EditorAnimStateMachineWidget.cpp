#include "Editor/UI/EditorAnimStateMachineWidget.h"

#include "Asset/AnimStateMachineLoader.h"
#include "Asset/AssetQueryService.h"
#include "Asset/FbxImporter.h"
#include "Animation/AnimStateMachine.h"
#include "Animation/AnimStateMachineInstance.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/AssetPathPolicy.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Editor/EditorEngine.h"
#include "ImGui/imgui.h"
#include "imgui-node-editor/imgui_node_editor.h"
#include "Object/Object.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace ed = ax::NodeEditor;

namespace
{
    constexpr int32 InvalidSelectionId = -1;
    constexpr int32 AnyStateNodeId = 200000000;
    constexpr int32 AnyStateOutputPinId = 200000001;
    constexpr const char* StateAssetPopupName = "##AnimStateMachineStateAssetPopup";
    const FName AnyStateName("Any");

    void CopyToBuffer(const FString& Value, char* Buffer, size_t BufferSize)
    {
        if (!Buffer || BufferSize == 0)
        {
            return;
        }

        std::snprintf(Buffer, BufferSize, "%s", Value.c_str());
    }

    bool InputFString(const char* Label, FString& Value, size_t MaxBytes = 260)
    {
        char Buffer[512] = {};
        const size_t BufferSize = std::min(MaxBytes, sizeof(Buffer));
        CopyToBuffer(Value, Buffer, BufferSize);
        if (ImGui::InputText(Label, Buffer, BufferSize))
        {
            Value = Buffer;
            return true;
        }
        return false;
    }

    bool InputFName(const char* Label, FName& Value, size_t MaxBytes = 128)
    {
        FString Text = Value.ToString();
        if (InputFString(Label, Text, MaxBytes))
        {
            Value = FName(Text);
            return true;
        }
        return false;
    }

    bool NamesEqual(const FName& A, const FName& B)
    {
        return A.IsValid() && B.IsValid() && A == B;
    }

    bool ContainsString(const TArray<FString>& Values, const FString& Target)
    {
        return std::find(Values.begin(), Values.end(), Target) != Values.end();
    }

    bool IsValidStateNameForEditor(const FName& Name)
    {
        return Name.IsValid() && Name != FName::None && !FAnimStateMachineDesc::IsAnyStateName(Name);
    }

    FAnimStateMachineDesc MakeDefaultStateMachineDesc()
    {
        FAnimStateMachineDesc Desc;
        FAnimStateDesc State;
        State.Id = 1;
        State.Name = FName("State_0");
        State.EditorPosition = FVector2(80.0f, 80.0f);
        Desc.EntryState = State.Name;
        Desc.States.push_back(State);
        return Desc;
    }

    const char* GetConditionTypeLabel(EAnimConditionType Type)
    {
        switch (Type)
        {
        case EAnimConditionType::Bool:
            return "Bool";
        case EAnimConditionType::Float:
            return "Float";
        case EAnimConditionType::Trigger:
            return "Trigger";
        case EAnimConditionType::LuaFunction:
            return "Lua Function";
        case EAnimConditionType::None:
        default:
            return "None";
        }
    }

    const char* GetCompareOperatorLabel(EAnimCompareOperator Operator)
    {
        switch (Operator)
        {
        case EAnimCompareOperator::Equal:
            return "==";
        case EAnimCompareOperator::NotEqual:
            return "!=";
        case EAnimCompareOperator::Greater:
            return ">";
        case EAnimCompareOperator::GreaterEqual:
            return ">=";
        case EAnimCompareOperator::Less:
            return "<";
        case EAnimCompareOperator::LessEqual:
            return "<=";
        default:
            return "==";
        }
    }

    bool DrawStateNameCombo(const char* Label, const FAnimStateMachineDesc& Desc, FName& Value, bool bAllowAnyState = false)
    {
        bool bChanged = false;
        const FString Preview = Value.ToString();
        if (ImGui::BeginCombo(Label, Preview.empty() ? "<None>" : Preview.c_str()))
        {
            if (bAllowAnyState)
            {
                const bool bAnySelected = FAnimStateMachineDesc::IsAnyStateName(Value);
                if (ImGui::Selectable("Any", bAnySelected))
                {
                    Value = AnyStateName;
                    bChanged = true;
                }
                if (bAnySelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::Separator();
            }

            for (const FAnimStateDesc& State : Desc.States)
            {
                const FString StateName = State.Name.ToString();
                const bool bSelected = NamesEqual(Value, State.Name);
                if (ImGui::Selectable(StateName.c_str(), bSelected))
                {
                    Value = State.Name;
                    bChanged = true;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return bChanged;
    }

    int32 ReapplyAnimStateMachineToLiveInstances(const FString& Path, UAnimStateMachine* StateMachine)
    {
        if (!StateMachine)
        {
            return 0;
        }

        const FString NormalizedPath = FAssetPathPolicy::NormalizeAnimStateMachineAssetPath(Path);
        if (NormalizedPath.empty())
        {
            return 0;
        }

        // 재적용 과정에서 animation sequence가 새로 로드되면 UObject 배열이 바뀔 수 있으므로 스냅샷을 순회합니다.
        const TArray<UObject*> ObjectSnapshot = GUObjectArray;
        int32 ReloadedCount = 0;
        for (UObject* Object : ObjectSnapshot)
        {
            if (!UObjectManager::Get().ContainsObject(Object))
            {
                continue;
            }

            USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Object);
            if (!SkeletalMeshComponent)
            {
                continue;
            }

            UAnimStateMachineInstance* StateMachineInstance = SkeletalMeshComponent->GetStateMachineInstance();
            if (!StateMachineInstance || !StateMachineInstance->UsesStateMachinePath(NormalizedPath))
            {
                continue;
            }

            // hot reload 전에 소비되지 못한 one-shot trigger가 새 graph에서 갑자기 발동하지 않도록 비웁니다.
            StateMachineInstance->ClearAnimTriggers();

            if (StateMachineInstance->SetStateMachine(StateMachine))
            {
                SkeletalMeshComponent->RefreshAnimationPose();
                ++ReloadedCount;
            }
        }

        return ReloadedCount;
    }
}

void FEditorAnimStateMachineWidget::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorWidget::Initialize(InEditorEngine);

    ed::Config Config;
    Config.SettingsFile = nullptr;
    NodeEditorContext = ed::CreateEditor(&Config);
    Desc = MakeDefaultStateMachineDesc();
    SetStatus("Animation State Machine editor ready.");
}

void FEditorAnimStateMachineWidget::Shutdown()
{
    if (NodeEditorContext)
    {
        ed::DestroyEditor(NodeEditorContext);
        NodeEditorContext = nullptr;
    }
}

void FEditorAnimStateMachineWidget::Render(float DeltaTime)
{
    ImGui::SetNextWindowSize(ImVec2(1180.0f, 760.0f), ImGuiCond_Once);
    if (!ImGui::Begin("Animation State Machine"))
    {
        ImGui::End();
        return;
    }

    DrawContent(DeltaTime);
    ImGui::End();
}

void FEditorAnimStateMachineWidget::RenderEmbedded(float DeltaTime)
{
    DrawContent(DeltaTime);
}

bool FEditorAnimStateMachineWidget::OpenAsset(const FString& Path)
{
    FAnimStateMachineLoader Loader;
    FAnimStateMachineDesc LoadedDesc;
    if (!Loader.LoadDescForEditor(Path, LoadedDesc))
    {
        SetStatus("Failed to load .animsm asset.");
        return false;
    }

    Desc = LoadedDesc;
    if (Desc.States.empty())
    {
        Desc = MakeDefaultStateMachineDesc();
        bDirty = true;
    }
    else
    {
        bDirty = false;
    }

    CurrentPath = FAssetPathPolicy::NormalizeAnimStateMachineAssetPath(Path);
    CopyToBuffer(CurrentPath, SaveAsPathBuffer, sizeof(SaveAsPathBuffer));
    CopyToBuffer(CurrentPath, LoadPathBuffer, sizeof(LoadPathBuffer));
    ResetSelection();
    bNeedsInitialNodePlacement = true;
    bShowAnyStateNode = HasAnyTransitions();
    bNeedsAnyStateNodePlacement = bShowAnyStateNode;
    bAssetListsDirty = true;
    SetStatus("Loaded .animsm asset.");
    return true;
}

void FEditorAnimStateMachineWidget::NewDraft()
{
    Desc = MakeDefaultStateMachineDesc();
    CurrentPath.clear();
    StatusMessage.clear();
    CopyToBuffer("Asset/Animation/StateMachine/New Animation State Machine.animsm", SaveAsPathBuffer, sizeof(SaveAsPathBuffer));
    CopyToBuffer("Asset/Animation/StateMachine/New Animation State Machine.animsm", LoadPathBuffer, sizeof(LoadPathBuffer));
    ResetSelection();
    bNeedsInitialNodePlacement = true;
    bShowAnyStateNode = false;
    bNeedsAnyStateNodePlacement = false;
    bAssetListsDirty = true;
    bDirty = true;
    SetStatus("New .animsm draft. Use Save As to create an asset file.");
}

bool FEditorAnimStateMachineWidget::NewAsset(const FString& Path)
{
    Desc = MakeDefaultStateMachineDesc();
    CurrentPath = FAssetPathPolicy::NormalizeAnimStateMachineAssetPath(Path);
    CopyToBuffer(CurrentPath, SaveAsPathBuffer, sizeof(SaveAsPathBuffer));
    CopyToBuffer(CurrentPath, LoadPathBuffer, sizeof(LoadPathBuffer));
    ResetSelection();
    bNeedsInitialNodePlacement = true;
    bShowAnyStateNode = false;
    bNeedsAnyStateNodePlacement = false;
    bAssetListsDirty = true;
    bDirty = true;
    return SaveAsset();
}

bool FEditorAnimStateMachineWidget::SaveAsset()
{
    if (CurrentPath.empty())
    {
        bSaveAsPopupRequested = true;
        return false;
    }

    SyncNodePositionsFromEditor();

    FAnimStateMachineLoader Loader;
    if (!Loader.SaveDescForEditor(CurrentPath, Desc))
    {
        SetStatus("Failed to save .animsm asset.");
        return false;
    }

    UAnimStateMachine* ReloadedStateMachine = FResourceManager::Get().ReloadAnimStateMachine(CurrentPath);
    const int32 ReloadedInstanceCount =
        ReapplyAnimStateMachineToLiveInstances(CurrentPath, ReloadedStateMachine);

    bDirty = false;
    if (ReloadedStateMachine)
    {
        SetStatus("Saved .animsm asset. Hot reloaded " + std::to_string(ReloadedInstanceCount) + " instance(s).");
    }
    else
    {
        SetStatus("Saved .animsm asset. Runtime hot reload skipped because the asset is not runtime-valid.");
    }
    return true;
}

bool FEditorAnimStateMachineWidget::SaveAssetAs(const FString& Path)
{
    const FString NormalizedPath = FAssetPathPolicy::NormalizeAnimStateMachineAssetPath(Path);
    if (NormalizedPath.empty())
    {
        SetStatus("Save As path must end with .animsm.");
        return false;
    }

    CurrentPath = NormalizedPath;
    CopyToBuffer(CurrentPath, SaveAsPathBuffer, sizeof(SaveAsPathBuffer));
    return SaveAsset();
}

void FEditorAnimStateMachineWidget::RequestSaveAs()
{
    const FString DefaultPath = CurrentPath.empty()
        ? FString("Asset/Animation/StateMachine/New Animation State Machine.animsm")
        : CurrentPath;
    CopyToBuffer(DefaultPath, SaveAsPathBuffer, sizeof(SaveAsPathBuffer));
    bSaveAsPopupRequested = true;
}

void FEditorAnimStateMachineWidget::RequestLoad()
{
    const FString DefaultPath = CurrentPath.empty()
        ? FString("Asset/Animation/StateMachine/New Animation State Machine.animsm")
        : CurrentPath;
    CopyToBuffer(DefaultPath, LoadPathBuffer, sizeof(LoadPathBuffer));
    bLoadPopupRequested = true;
}

bool FEditorAnimStateMachineWidget::ValidateAsset()
{
    SyncNodePositionsFromEditor();

    FAnimStateMachineLoader Loader;
    const bool bValid = Loader.ValidateDescForRuntime(Desc, CurrentPath);
    SetStatus(bValid ? "Runtime validation passed." : "Runtime validation failed. Check log for details.");
    return bValid;
}

void FEditorAnimStateMachineWidget::DrawContent(float DeltaTime)
{
    (void)DeltaTime;

    DrawToolbar();
    ImGui::Separator();

    if (ImGui::BeginTable(
        "##AnimStateMachineEditorLayout",
        2,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 360.0f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawGraph();

        ImGui::TableSetColumnIndex(1);
        DrawInspector();

        ImGui::EndTable();
    }

    DrawLoadPopup();
    DrawSaveAsPopup();
}

void FEditorAnimStateMachineWidget::DrawToolbar()
{
    if (ImGui::Button("New"))
    {
        NewDraft();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        RequestLoad();
    }
    ImGui::SameLine(0.0f, 16.0f);

    ImGui::TextDisabled("Asset");
    ImGui::SameLine();
    ImGui::TextUnformatted(CurrentPath.empty() ? "<unsaved>" : CurrentPath.c_str());
    if (bDirty)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.74f, 0.26f, 1.0f), "*");
    }

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::BeginDisabled(!bDirty && !CurrentPath.empty());
    if (ImGui::Button("Save"))
    {
        SaveAsset();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Save As"))
    {
        RequestSaveAs();
    }

    ImGui::SameLine();
    if (ImGui::Button("Validate"))
    {
        ValidateAsset();
    }

    ImGui::SameLine(0.0f, 16.0f);
    if (!StatusMessage.empty())
    {
        ImGui::TextDisabled("%s", StatusMessage.c_str());
    }
}

void FEditorAnimStateMachineWidget::DrawGraph()
{
    if (!NodeEditorContext)
    {
        ImGui::TextDisabled("Node editor is not initialized.");
        return;
    }

    ImGui::BeginChild("##AnimStateMachineGraphHost", ImVec2(0.0f, 0.0f), false);

    if (ImGui::Button("Add State"))
    {
        AddState();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(bShowAnyStateNode);
    if (ImGui::Button("Add Any State"))
    {
        ShowAnyStateNode();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(SelectedStateId == InvalidSelectionId || (!IsAnyStateNodeId(SelectedStateId) && Desc.States.size() <= 1));
    if (ImGui::Button("Delete State"))
    {
        if (IsAnyStateNodeId(SelectedStateId))
        {
            DeleteAnyStateNode();
        }
        else
        {
            DeleteSelectedState();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(SelectedTransitionId == InvalidSelectionId);
    if (ImGui::Button("Delete Transition"))
    {
        DeleteSelectedTransition();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("Drag output pin to another state input pin to create transition.");

    const ImVec2 GraphCanvasStart = ImGui::GetCursorScreenPos();
    StateAssetPopupAnchorX = GraphCanvasStart.x + 8.0f;
    StateAssetPopupAnchorY = GraphCanvasStart.y + 8.0f;

    ed::SetCurrentEditor(NodeEditorContext);
    ed::Begin("AnimStateMachineGraph", ImVec2(0.0f, 0.0f));

    if (bShowAnyStateNode || HasAnyTransitions())
    {
        bShowAnyStateNode = true;

        ed::BeginNode(ed::NodeId(static_cast<uintptr_t>(AnyStateNodeId)));
        ImGui::PushID(AnyStateNodeId);
        ImGui::TextColored(ImVec4(0.78f, 0.62f, 0.98f, 1.0f), "Any State");
        ImGui::TextDisabled("Global transition source");
        ImGui::Spacing();
        ed::BeginPin(ed::PinId(static_cast<uintptr_t>(AnyStateOutputPinId)), ed::PinKind::Output);
        ImGui::TextUnformatted("Out");
        ed::EndPin();
        ImGui::PopID();
        ed::EndNode();
    }

    for (FAnimStateDesc& State : Desc.States)
    {
        const ed::NodeId NodeId(static_cast<uintptr_t>(State.Id));
        const bool bEntry = NamesEqual(Desc.EntryState, State.Name);
        ed::BeginNode(NodeId);
        ImGui::PushID(State.Id);
        if (bEntry)
        {
            ImGui::TextColored(ImVec4(0.90f, 0.75f, 0.24f, 1.0f), "Entry");
        }
        ImGui::TextUnformatted(State.Name.ToString().c_str());
        DrawStateNodeControls(State);

        ed::BeginPin(ed::PinId(static_cast<uintptr_t>(MakeInputPinId(State.Id))), ed::PinKind::Input);
        ImGui::TextUnformatted("In");
        ed::EndPin();
        ImGui::SameLine(120.0f);
        ed::BeginPin(ed::PinId(static_cast<uintptr_t>(MakeOutputPinId(State.Id))), ed::PinKind::Output);
        ImGui::TextUnformatted("Out");
        ed::EndPin();
        ImGui::PopID();
        ed::EndNode();
    }

    for (const FAnimTransitionDesc& Transition : Desc.Transitions)
    {
        const bool bAnyTransition = FAnimStateMachineDesc::IsAnyStateName(Transition.FromState);
        const FAnimStateDesc* FromState = bAnyTransition ? nullptr : Desc.FindStateByName(Transition.FromState);
        const FAnimStateDesc* ToState = Desc.FindStateByName(Transition.ToState);
        if ((!bAnyTransition && !FromState) || !ToState)
        {
            continue;
        }

        const ImVec4 LinkColor = Transition.Condition.Type == EAnimConditionType::None
            ? ImVec4(0.56f, 0.70f, 0.92f, 1.0f)
            : ImVec4(0.95f, 0.67f, 0.28f, 1.0f);
        ed::Link(
            ed::LinkId(static_cast<uintptr_t>(Transition.Id)),
            ed::PinId(static_cast<uintptr_t>(bAnyTransition ? AnyStateOutputPinId : MakeOutputPinId(FromState->Id))),
            ed::PinId(static_cast<uintptr_t>(MakeInputPinId(ToState->Id))),
            LinkColor,
            2.0f);
    }

    ApplyInitialNodePositions();

    if (ed::BeginCreate(ImVec4(0.74f, 0.84f, 1.0f, 1.0f), 2.0f))
    {
        ed::PinId StartPinId;
        ed::PinId EndPinId;
        if (ed::QueryNewLink(&StartPinId, &EndPinId))
        {
            const int32 StartId = static_cast<int32>(StartPinId.Get());
            const int32 EndId = static_cast<int32>(EndPinId.Get());
            const FAnimStateDesc* StartState = FindStateByPinId(StartId);
            const FAnimStateDesc* EndState = FindStateByPinId(EndId);
            const FAnimStateDesc* FromState = nullptr;
            const FAnimStateDesc* ToState = nullptr;
            bool bFromAnyState = false;

            // node-editor는 드래그 방향을 보장하지 않으므로 input->output 드래그도 같은 transition으로 해석합니다.
            if (StartState && EndState && StartId == MakeOutputPinId(StartState->Id) && EndId == MakeInputPinId(EndState->Id))
            {
                FromState = StartState;
                ToState = EndState;
            }
            else if (IsAnyOutputPinId(StartId) && EndState && EndId == MakeInputPinId(EndState->Id))
            {
                bFromAnyState = true;
                ToState = EndState;
            }
            else if (StartState && EndState && StartId == MakeInputPinId(StartState->Id) && EndId == MakeOutputPinId(EndState->Id))
            {
                FromState = EndState;
                ToState = StartState;
            }
            else if (StartState && StartId == MakeInputPinId(StartState->Id) && IsAnyOutputPinId(EndId))
            {
                bFromAnyState = true;
                ToState = StartState;
            }

            const bool bCanCreate =
                (FromState || bFromAnyState) &&
                ToState &&
                (!FromState || FromState != ToState);
            if (bCanCreate)
            {
                if (ed::AcceptNewItem(ImVec4(0.46f, 0.84f, 0.54f, 1.0f), 3.0f))
                {
                    AddTransition(bFromAnyState ? AnyStateName : FromState->Name, ToState->Name);
                }
            }
            else
            {
                ed::RejectNewItem(ImVec4(0.92f, 0.28f, 0.28f, 1.0f), 2.0f);
            }
        }
    }
    ed::EndCreate();

    if (ed::BeginDelete())
    {
        ed::LinkId LinkId;
        while (ed::QueryDeletedLink(&LinkId))
        {
            if (ed::AcceptDeletedItem())
            {
                SelectedTransitionId = static_cast<int32>(LinkId.Get());
                DeleteSelectedTransition();
            }
        }

        ed::NodeId NodeId;
        while (ed::QueryDeletedNode(&NodeId))
        {
            if (ed::AcceptDeletedItem(false))
            {
                SelectedStateId = static_cast<int32>(NodeId.Get());
                if (IsAnyStateNodeId(SelectedStateId))
                {
                    DeleteAnyStateNode();
                }
                else
                {
                    DeleteSelectedState();
                }
            }
        }
    }
    ed::EndDelete();

    SyncSelectionFromNodeEditor();

    DrawGraphContextMenus();
    ed::Suspend();
    DrawStateAssetPopup();
    ed::Resume();
    ed::End();
    SyncNodePositionsFromEditor();
    ed::SetCurrentEditor(nullptr);

    ImGui::EndChild();
}

void FEditorAnimStateMachineWidget::DrawInspector()
{
    ImGui::BeginChild("##AnimStateMachineInspector", ImVec2(0.0f, 0.0f), false);

    const float AvailableY = ImGui::GetContentRegionAvail().y;
    const float PreviewHeight = std::min(300.0f, std::max(180.0f, AvailableY * 0.38f));
    const float InspectorHeight = std::max(140.0f, AvailableY - PreviewHeight - 20.0f);

    {
        ImGui::BeginChild("##AnimStateMachineInspectorControls", ImVec2(0.0f, InspectorHeight), false);
        ImGui::TextUnformatted("Inspector");
        ImGui::Separator();

        ImGui::TextDisabled("Summary");
        ImGui::Text("States: %d", static_cast<int32>(Desc.States.size()));
        ImGui::Text("Transitions: %d", static_cast<int32>(Desc.Transitions.size()));
        ImGui::Text("Entry: %s", Desc.EntryState.ToString().c_str());
        ImGui::Spacing();

        if (FAnimTransitionDesc* Transition = GetSelectedTransition())
        {
            DrawTransitionInspector(*Transition);
        }
        else if (GetSelectedState())
        {
            ImGui::TextDisabled("State settings are edited inside each graph node.");
        }
        else
        {
            ImGui::TextDisabled("Select a transition link to edit transition settings.");
        }

        ImGui::EndChild();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Output Preview");
    const FString PreviewJson = BuildPreviewJson();
    const bool bPreviewVisible = ImGui::BeginChild(
        "##AnimStateMachineJsonPreview",
        ImVec2(0.0f, 0.0f),
        true,
        ImGuiWindowFlags_HorizontalScrollbar);
    if (bPreviewVisible)
    {
        ImGui::TextUnformatted(PreviewJson.c_str());
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

void FEditorAnimStateMachineWidget::DrawStateNodeControls(FAnimStateDesc& State)
{
    ImGui::Spacing();

    FName NewName = State.Name;
    ImGui::SetNextItemWidth(220.0f);
    if (InputFName("Name", NewName))
    {
        RenameState(State, NewName);
    }

    FString FbxPreview = State.Animation.SourceFbxPath.empty() ? "<Select FBX>" : State.Animation.SourceFbxPath;

    ImGui::TextDisabled("FBX");
    if (ImGui::Button(FbxPreview.c_str(), ImVec2(220.0f, 0.0f)))
    {
        RequestOpenStateAssetPopup(State.Id, EStateAssetPopupKind::SourceFbx);
    }

    const bool bHasFbx = !State.Animation.SourceFbxPath.empty();
    const TArray<FString>& Stacks = bHasFbx ? GetAnimStacksForFbx(State.Animation.SourceFbxPath) : GetAnimStacksForFbx(FString());
    const bool bHasStacks = !Stacks.empty();
    const FString StackPreview = State.Animation.AnimStackName.empty() ? (bHasStacks ? Stacks.front() : "<No Animation Stack>") : State.Animation.AnimStackName;

    ImGui::TextDisabled("Animation Stack");
    ImGui::BeginDisabled(!bHasFbx || !bHasStacks);
    if (ImGui::Button(StackPreview.c_str(), ImVec2(220.0f, 0.0f)))
    {
        RequestOpenStateAssetPopup(State.Id, EStateAssetPopupKind::AnimStack);
    }
    ImGui::EndDisabled();

    if (bHasFbx && bHasStacks && !ContainsString(Stacks, State.Animation.AnimStackName))
    {
        State.Animation.AnimStackName = Stacks.front();
        MarkDirty();
    }

    if (ImGui::Checkbox("Loop", &State.bLooping))
    {
        MarkDirty();
    }

    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::DragFloat("Play Rate", &State.PlayRate, 0.01f, 0.0f, 10.0f, "%.2f"))
    {
        MarkDirty();
    }

    const bool bEntry = NamesEqual(Desc.EntryState, State.Name);
    ImGui::BeginDisabled(bEntry);
    if (ImGui::Button("Set Entry"))
    {
        Desc.EntryState = State.Name;
        MarkDirty();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
}

void FEditorAnimStateMachineWidget::DrawStateAssetPopup()
{
    if (bOpenStateAssetPopupRequested)
    {
        ImGui::OpenPopup(StateAssetPopupName);
        bOpenStateAssetPopupRequested = false;
    }

    ImGui::SetNextWindowPos(ImVec2(StateAssetPopupX, StateAssetPopupY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(std::max(320.0f, StateAssetPopupWidth), 0.0f), ImGuiCond_Always);
    if (!ImGui::BeginPopup(StateAssetPopupName))
    {
        return;
    }

    FAnimStateDesc* State = Desc.FindStateById(StateAssetPopupStateId);
    if (!State)
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    if (StateAssetPopupKind == EStateAssetPopupKind::SourceFbx)
    {
        const bool bNoneSelected = State->Animation.SourceFbxPath.empty();
        if (ImGui::Selectable("<None>", bNoneSelected))
        {
            State->Animation.SourceFbxPath.clear();
            State->Animation.AnimStackName.clear();
            MarkDirty();
            ImGui::CloseCurrentPopup();
        }
        if (bNoneSelected)
        {
            ImGui::SetItemDefaultFocus();
        }

        const TArray<FString>& FbxPaths = GetFbxAssetList();
        if (FbxPaths.empty())
        {
            ImGui::TextDisabled("No FBX files under Asset/.");
        }
        for (const FString& FbxPath : FbxPaths)
        {
            const bool bSelected = State->Animation.SourceFbxPath == FbxPath;
            if (ImGui::Selectable(FbxPath.c_str(), bSelected))
            {
                State->Animation.SourceFbxPath = FPaths::Normalize(FbxPath);
                const TArray<FString>& Stacks = GetAnimStacksForFbx(State->Animation.SourceFbxPath);
                State->Animation.AnimStackName = Stacks.empty() ? FString() : Stacks.front();
                MarkDirty();
                ImGui::CloseCurrentPopup();
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
    }
    else if (StateAssetPopupKind == EStateAssetPopupKind::AnimStack)
    {
        const TArray<FString>& Stacks = GetAnimStacksForFbx(State->Animation.SourceFbxPath);
        if (Stacks.empty())
        {
            ImGui::TextDisabled("Selected FBX has no animation stacks.");
        }
        for (const FString& StackName : Stacks)
        {
            const bool bSelected = State->Animation.AnimStackName == StackName;
            if (ImGui::Selectable(StackName.c_str(), bSelected))
            {
                State->Animation.AnimStackName = StackName;
                MarkDirty();
                ImGui::CloseCurrentPopup();
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
    }

    ImGui::EndPopup();
}

void FEditorAnimStateMachineWidget::DrawTransitionInspector(FAnimTransitionDesc& Transition)
{
    ImGui::TextUnformatted("Transition");
    ImGui::Separator();

    if (DrawStateNameCombo("From", Desc, Transition.FromState, true))
    {
        MarkDirty();
    }
    if (DrawStateNameCombo("To", Desc, Transition.ToState))
    {
        MarkDirty();
    }
    if (ImGui::DragFloat("Blend Time", &Transition.BlendTime, 0.01f, 0.0f, 10.0f, "%.2f"))
    {
        Transition.BlendTime = std::max(0.0f, Transition.BlendTime);
        MarkDirty();
    }
    if (ImGui::InputInt("Priority", &Transition.Priority))
    {
        MarkDirty();
    }
    if (ImGui::Checkbox("Can Interrupt", &Transition.bCanInterrupt))
    {
        MarkDirty();
    }
    if (ImGui::Checkbox("Can Be Interrupted", &Transition.bCanBeInterrupted))
    {
        MarkDirty();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Condition");
    EAnimConditionType ConditionTypes[] = {
        EAnimConditionType::None,
        EAnimConditionType::Bool,
        EAnimConditionType::Float,
        EAnimConditionType::Trigger,
        EAnimConditionType::LuaFunction,
    };
    if (ImGui::BeginCombo("Type", GetConditionTypeLabel(Transition.Condition.Type)))
    {
        for (EAnimConditionType Type : ConditionTypes)
        {
            const bool bSelected = Transition.Condition.Type == Type;
            if (ImGui::Selectable(GetConditionTypeLabel(Type), bSelected))
            {
                Transition.Condition.Type = Type;
                MarkDirty();
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (Transition.Condition.Type == EAnimConditionType::Bool)
    {
        if (InputFName("Variable", Transition.Condition.VariableName))
        {
            MarkDirty();
        }
        if (ImGui::Checkbox("Expected Value", &Transition.Condition.BoolValue))
        {
            MarkDirty();
        }
    }
    else if (Transition.Condition.Type == EAnimConditionType::Float)
    {
        if (InputFName("Variable", Transition.Condition.VariableName))
        {
            MarkDirty();
        }

        EAnimCompareOperator Operators[] = {
            EAnimCompareOperator::Equal,
            EAnimCompareOperator::NotEqual,
            EAnimCompareOperator::Greater,
            EAnimCompareOperator::GreaterEqual,
            EAnimCompareOperator::Less,
            EAnimCompareOperator::LessEqual,
        };
        if (ImGui::BeginCombo("Operator", GetCompareOperatorLabel(Transition.Condition.Operator)))
        {
            for (EAnimCompareOperator Operator : Operators)
            {
                const bool bSelected = Transition.Condition.Operator == Operator;
                if (ImGui::Selectable(GetCompareOperatorLabel(Operator), bSelected))
                {
                    Transition.Condition.Operator = Operator;
                    MarkDirty();
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::DragFloat("Value", &Transition.Condition.FloatValue, 0.01f))
        {
            MarkDirty();
        }
    }
    else if (Transition.Condition.Type == EAnimConditionType::Trigger)
    {
        if (InputFName("Variable", Transition.Condition.VariableName))
        {
            MarkDirty();
        }
    }
    else if (Transition.Condition.Type == EAnimConditionType::LuaFunction)
    {
        if (InputFName("Function", Transition.Condition.LuaFunctionName))
        {
            MarkDirty();
        }
    }
}

void FEditorAnimStateMachineWidget::DrawLoadPopup()
{
    if (bLoadPopupRequested)
    {
        ImGui::OpenPopup("Load Animation State Machine");
        bLoadPopupRequested = false;
    }

    if (ImGui::BeginPopupModal("Load Animation State Machine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Project-relative .animsm path");
        ImGui::SetNextItemWidth(420.0f);
        const bool bEnter = ImGui::InputText(
            "##AnimStateMachineLoadPath",
            LoadPathBuffer,
            IM_ARRAYSIZE(LoadPathBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::Button("Load") || bEnter)
        {
            if (OpenAsset(LoadPathBuffer))
            {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FEditorAnimStateMachineWidget::DrawSaveAsPopup()
{
    if (bSaveAsPopupRequested)
    {
        ImGui::OpenPopup("Save Animation State Machine As");
        bSaveAsPopupRequested = false;
    }

    if (ImGui::BeginPopupModal("Save Animation State Machine As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Project-relative .animsm path");
        ImGui::SetNextItemWidth(420.0f);
        const bool bEnter = ImGui::InputText(
            "##AnimStateMachineSaveAsPath",
            SaveAsPathBuffer,
            IM_ARRAYSIZE(SaveAsPathBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::Button("Save") || bEnter)
        {
            if (SaveAssetAs(SaveAsPathBuffer))
            {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FEditorAnimStateMachineWidget::DrawGraphContextMenus()
{
    ed::NodeId ContextNodeId;
    ed::LinkId ContextLinkId;
    if (ed::ShowNodeContextMenu(&ContextNodeId))
    {
        SelectedStateId = static_cast<int32>(ContextNodeId.Get());
        SelectedTransitionId = InvalidSelectionId;
        ed::Suspend();
        ImGui::OpenPopup("AnimStateMachineNodeContext");
        ed::Resume();
    }
    if (ed::ShowLinkContextMenu(&ContextLinkId))
    {
        SelectedTransitionId = static_cast<int32>(ContextLinkId.Get());
        SelectedStateId = InvalidSelectionId;
        ed::Suspend();
        ImGui::OpenPopup("AnimStateMachineLinkContext");
        ed::Resume();
    }
    if (ed::ShowBackgroundContextMenu())
    {
        ed::Suspend();
        ImGui::OpenPopup("AnimStateMachineGraphContext");
        ed::Resume();
    }

    ed::Suspend();
    if (ImGui::BeginPopup("AnimStateMachineNodeContext"))
    {
        const bool bAnyNodeSelected = IsAnyStateNodeId(SelectedStateId);
        ImGui::BeginDisabled(bAnyNodeSelected);
        if (ImGui::MenuItem("Set Entry State"))
        {
            if (FAnimStateDesc* State = GetSelectedState())
            {
                Desc.EntryState = State->Name;
                MarkDirty();
            }
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!bAnyNodeSelected && Desc.States.size() <= 1);
        if (ImGui::MenuItem("Delete State"))
        {
            if (bAnyNodeSelected)
            {
                DeleteAnyStateNode();
            }
            else
            {
                DeleteSelectedState();
            }
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("AnimStateMachineLinkContext"))
    {
        if (ImGui::MenuItem("Delete Transition"))
        {
            DeleteSelectedTransition();
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("AnimStateMachineGraphContext"))
    {
        if (ImGui::MenuItem("Add State"))
        {
            AddState();
        }
        ImGui::EndPopup();
    }
    ed::Resume();
}

void FEditorAnimStateMachineWidget::MarkDirty()
{
    bDirty = true;
}

void FEditorAnimStateMachineWidget::SetStatus(const FString& Message)
{
    StatusMessage = Message;
}

void FEditorAnimStateMachineWidget::ResetSelection()
{
    SelectedStateId = InvalidSelectionId;
    SelectedTransitionId = InvalidSelectionId;
}

void FEditorAnimStateMachineWidget::SyncSelectionFromNodeEditor()
{
    ed::NodeId SelectedNodes[1];
    ed::LinkId SelectedLinks[1];
    const int LinkCount = ed::GetSelectedLinks(SelectedLinks, 1);
    const int NodeCount = ed::GetSelectedNodes(SelectedNodes, 1);

    // link와 node가 동시에 잡히는 프레임이 있어 inspector 기준 선택은 transition을 우선합니다.
    if (LinkCount > 0)
    {
        SelectedTransitionId = static_cast<int32>(SelectedLinks[0].Get());
        SelectedStateId = InvalidSelectionId;
    }
    else if (NodeCount > 0)
    {
        SelectedStateId = static_cast<int32>(SelectedNodes[0].Get());
        SelectedTransitionId = InvalidSelectionId;
    }
    else
    {
        ResetSelection();
    }
}

void FEditorAnimStateMachineWidget::SyncNodePositionsFromEditor()
{
    if (!NodeEditorContext || bNeedsInitialNodePlacement)
    {
        return;
    }

    ed::EditorContext* PreviousContext = ed::GetCurrentEditor();
    ed::SetCurrentEditor(NodeEditorContext);

    for (FAnimStateDesc& State : Desc.States)
    {
        const ImVec2 Position = ed::GetNodePosition(ed::NodeId(static_cast<uintptr_t>(State.Id)));
        if (std::fabs(State.EditorPosition.X - Position.x) > 0.5f ||
            std::fabs(State.EditorPosition.Y - Position.y) > 0.5f)
        {
            State.EditorPosition = FVector2(Position.x, Position.y);
            MarkDirty();
        }
    }

    ed::SetCurrentEditor(PreviousContext);
}

void FEditorAnimStateMachineWidget::ApplyInitialNodePositions()
{
    if (!bNeedsInitialNodePlacement && !bNeedsAnyStateNodePlacement)
    {
        return;
    }

    if (bNeedsInitialNodePlacement)
    {
        for (const FAnimStateDesc& State : Desc.States)
        {
            ed::SetNodePosition(
                ed::NodeId(static_cast<uintptr_t>(State.Id)),
                ImVec2(State.EditorPosition.X, State.EditorPosition.Y));
        }
        bNeedsInitialNodePlacement = false;
    }

    if (bNeedsAnyStateNodePlacement && bShowAnyStateNode)
    {
        ed::SetNodePosition(
            ed::NodeId(static_cast<uintptr_t>(AnyStateNodeId)),
            ImVec2(40.0f, 40.0f));
        bNeedsAnyStateNodePlacement = false;
    }
}

FAnimStateDesc* FEditorAnimStateMachineWidget::GetSelectedState()
{
    return SelectedStateId == InvalidSelectionId ? nullptr : Desc.FindStateById(SelectedStateId);
}

FAnimTransitionDesc* FEditorAnimStateMachineWidget::GetSelectedTransition()
{
    return SelectedTransitionId == InvalidSelectionId ? nullptr : Desc.FindTransitionById(SelectedTransitionId);
}

FString FEditorAnimStateMachineWidget::BuildPreviewJson() const
{
    FAnimStateMachineLoader Loader;
    return Loader.BuildDescJsonForEditor(Desc);
}

void FEditorAnimStateMachineWidget::RefreshFbxAssetList()
{
    CachedFbxPaths = FAssetQueryService::GetFbxSourcePaths();
    std::sort(CachedFbxPaths.begin(), CachedFbxPaths.end());
    CachedAnimStacksByFbx.clear();
    bAssetListsDirty = false;
}

const TArray<FString>& FEditorAnimStateMachineWidget::GetFbxAssetList()
{
    if (bAssetListsDirty)
    {
        RefreshFbxAssetList();
    }
    return CachedFbxPaths;
}

const TArray<FString>& FEditorAnimStateMachineWidget::GetAnimStacksForFbx(const FString& FbxPath)
{
    static const TArray<FString> EmptyStacks;
    if (FbxPath.empty())
    {
        return EmptyStacks;
    }

    const FString NormalizedPath = FPaths::Normalize(FbxPath);
    auto Found = CachedAnimStacksByFbx.find(NormalizedPath);
    if (Found == CachedAnimStacksByFbx.end())
    {
        FFbxImporter Importer;
        TArray<FString> Stacks = Importer.ListAnimStacks(NormalizedPath);
        std::sort(Stacks.begin(), Stacks.end());
        CachedAnimStacksByFbx[NormalizedPath] = Stacks;
        Found = CachedAnimStacksByFbx.find(NormalizedPath);
    }

    return Found != CachedAnimStacksByFbx.end() ? Found->second : EmptyStacks;
}

void FEditorAnimStateMachineWidget::RequestOpenStateAssetPopup(
    int32 StateId,
    EStateAssetPopupKind Kind)
{
    StateAssetPopupStateId = StateId;
    StateAssetPopupKind = Kind;
    // node-editor 내부 버튼 좌표는 popup 위치 계산이 흔들릴 수 있어 graph 좌상단에 고정합니다.
    StateAssetPopupX = StateAssetPopupAnchorX;
    StateAssetPopupY = StateAssetPopupAnchorY;
    StateAssetPopupWidth = 360.0f;
    bOpenStateAssetPopupRequested = true;
}

bool FEditorAnimStateMachineWidget::HasAnyTransitions() const
{
    for (const FAnimTransitionDesc& Transition : Desc.Transitions)
    {
        if (FAnimStateMachineDesc::IsAnyStateName(Transition.FromState))
        {
            return true;
        }
    }
    return false;
}

bool FEditorAnimStateMachineWidget::IsAnyStateNodeId(int32 NodeId) const
{
    return NodeId == AnyStateNodeId;
}

bool FEditorAnimStateMachineWidget::IsAnyOutputPinId(int32 PinId) const
{
    return PinId == AnyStateOutputPinId;
}

bool FEditorAnimStateMachineWidget::ShowAnyStateNode()
{
    if (bShowAnyStateNode)
    {
        return false;
    }

    bShowAnyStateNode = true;
    bNeedsAnyStateNodePlacement = true;
    SelectedStateId = AnyStateNodeId;
    SelectedTransitionId = InvalidSelectionId;
    return true;
}

bool FEditorAnimStateMachineWidget::DeleteAnyStateNode()
{
    if (!bShowAnyStateNode && !HasAnyTransitions())
    {
        return false;
    }

    const size_t OldTransitionCount = Desc.Transitions.size();
    Desc.Transitions.erase(
        std::remove_if(
            Desc.Transitions.begin(),
            Desc.Transitions.end(),
            [](const FAnimTransitionDesc& Transition)
            {
                return FAnimStateMachineDesc::IsAnyStateName(Transition.FromState);
            }),
        Desc.Transitions.end());

    bShowAnyStateNode = false;
    bNeedsAnyStateNodePlacement = false;
    ResetSelection();
    if (Desc.Transitions.size() != OldTransitionCount)
    {
        MarkDirty();
    }
    return true;
}

const FAnimStateDesc* FEditorAnimStateMachineWidget::FindStateByPinId(int32 PinId) const
{
    const int32 StateId = PinId / 10;
    return Desc.FindStateById(StateId);
}

int32 FEditorAnimStateMachineWidget::MakeInputPinId(int32 StateId) const
{
    return StateId * 10 + 1;
}

int32 FEditorAnimStateMachineWidget::MakeOutputPinId(int32 StateId) const
{
    return StateId * 10 + 2;
}

int32 FEditorAnimStateMachineWidget::AllocateStateId() const
{
    int32 MaxId = 0;
    for (const FAnimStateDesc& State : Desc.States)
    {
        MaxId = std::max(MaxId, State.Id);
    }
    return MaxId + 1;
}

int32 FEditorAnimStateMachineWidget::AllocateTransitionId() const
{
    int32 MaxId = 0;
    for (const FAnimTransitionDesc& Transition : Desc.Transitions)
    {
        MaxId = std::max(MaxId, Transition.Id);
    }
    return MaxId + 1;
}

FName FEditorAnimStateMachineWidget::MakeUniqueStateName() const
{
    for (int32 Index = 0; Index < 10000; ++Index)
    {
        const FName Candidate("State_" + std::to_string(Index));
        if (!Desc.FindStateByName(Candidate))
        {
            return Candidate;
        }
    }
    return FName("State_New");
}

bool FEditorAnimStateMachineWidget::AddState()
{
    FAnimStateDesc State;
    State.Id = AllocateStateId();
    State.Name = MakeUniqueStateName();
    State.EditorPosition = FVector2(80.0f + State.Id * 30.0f, 100.0f + State.Id * 20.0f);

    Desc.States.push_back(State);
    if (!Desc.EntryState.IsValid() || Desc.EntryState == FName::None)
    {
        Desc.EntryState = State.Name;
    }

    SelectedStateId = State.Id;
    SelectedTransitionId = InvalidSelectionId;
    MarkDirty();
    bNeedsInitialNodePlacement = true;
    return true;
}

bool FEditorAnimStateMachineWidget::DeleteSelectedState()
{
    if (SelectedStateId == InvalidSelectionId || Desc.States.size() <= 1)
    {
        return false;
    }

    const FAnimStateDesc* State = Desc.FindStateById(SelectedStateId);
    if (!State)
    {
        return false;
    }

    const FName RemovedName = State->Name;
    Desc.States.erase(
        std::remove_if(
            Desc.States.begin(),
            Desc.States.end(),
            [this](const FAnimStateDesc& Candidate)
            {
                return Candidate.Id == SelectedStateId;
            }),
        Desc.States.end());
    RemoveTransitionsTouchingState(RemovedName);

    if (NamesEqual(Desc.EntryState, RemovedName) && !Desc.States.empty())
    {
        Desc.EntryState = Desc.States.front().Name;
    }

    ResetSelection();
    MarkDirty();
    return true;
}

bool FEditorAnimStateMachineWidget::AddTransition(const FName& FromState, const FName& ToState)
{
    const bool bAnyFromState = FAnimStateMachineDesc::IsAnyStateName(FromState);
    if ((!bAnyFromState && !Desc.FindStateByName(FromState)) ||
        !Desc.FindStateByName(ToState) ||
        NamesEqual(FromState, ToState))
    {
        return false;
    }

    FAnimTransitionDesc Transition;
    Transition.Id = AllocateTransitionId();
    Transition.FromState = bAnyFromState ? AnyStateName : FromState;
    Transition.ToState = ToState;
    Desc.Transitions.push_back(Transition);

    SelectedStateId = InvalidSelectionId;
    SelectedTransitionId = Transition.Id;
    MarkDirty();
    return true;
}

bool FEditorAnimStateMachineWidget::DeleteSelectedTransition()
{
    if (SelectedTransitionId == InvalidSelectionId)
    {
        return false;
    }

    const size_t OldSize = Desc.Transitions.size();
    Desc.Transitions.erase(
        std::remove_if(
            Desc.Transitions.begin(),
            Desc.Transitions.end(),
            [this](const FAnimTransitionDesc& Transition)
            {
                return Transition.Id == SelectedTransitionId;
            }),
        Desc.Transitions.end());

    const bool bRemoved = Desc.Transitions.size() != OldSize;
    if (bRemoved)
    {
        ResetSelection();
        MarkDirty();
    }
    return bRemoved;
}

bool FEditorAnimStateMachineWidget::RenameState(FAnimStateDesc& State, const FName& NewName)
{
    const FName OldName = State.Name;
    if (NamesEqual(OldName, NewName))
    {
        return false;
    }
    if (!IsValidStateNameForEditor(NewName))
    {
        SetStatus("State name cannot be empty, None, Any, or *.");
        return false;
    }

    for (const FAnimStateDesc& Other : Desc.States)
    {
        if (&Other != &State && NamesEqual(Other.Name, NewName))
        {
            SetStatus("State name must be unique.");
            return false;
        }
    }

    State.Name = NewName;

    // State 이름은 transition과 entry에서 참조하므로, 이름 변경 시 참조도 같이 옮깁니다.
    for (FAnimTransitionDesc& Transition : Desc.Transitions)
    {
        if (NamesEqual(Transition.FromState, OldName))
        {
            Transition.FromState = State.Name;
        }
        if (NamesEqual(Transition.ToState, OldName))
        {
            Transition.ToState = State.Name;
        }
    }
    if (NamesEqual(Desc.EntryState, OldName))
    {
        Desc.EntryState = State.Name;
    }

    MarkDirty();
    return true;
}

void FEditorAnimStateMachineWidget::RemoveTransitionsTouchingState(const FName& StateName)
{
    Desc.Transitions.erase(
        std::remove_if(
            Desc.Transitions.begin(),
            Desc.Transitions.end(),
            [&StateName](const FAnimTransitionDesc& Transition)
            {
                return NamesEqual(Transition.FromState, StateName) || NamesEqual(Transition.ToState, StateName);
            }),
        Desc.Transitions.end());
}
