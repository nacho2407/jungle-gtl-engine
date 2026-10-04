#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Animation/AnimStateMachineTypes.h"

namespace ax
{
namespace NodeEditor
{
    struct EditorContext;
}
}

class FEditorAnimStateMachineWidget : public FEditorWidget
{
public:
    void Initialize(UEditorEngine* InEditorEngine) override;
    void Render(float DeltaTime) override;
    void RenderEmbedded(float DeltaTime);
    void Shutdown();

    bool OpenAsset(const FString& Path);
    void NewDraft();
    bool NewAsset(const FString& Path);
    bool SaveAsset();
    bool SaveAssetAs(const FString& Path);
    void RequestLoad();
    void RequestSaveAs();
    bool ValidateAsset();

    const FString& GetAssetPath() const { return CurrentPath; }
    const FString& GetStatusMessage() const { return StatusMessage; }
    bool IsDirty() const { return bDirty; }

private:
    enum class EStateAssetPopupKind
    {
        None,
        SourceFbx,
        AnimStack,
    };

    void DrawContent(float DeltaTime);
    void DrawToolbar();
    void DrawGraph();
    void DrawInspector();
    void DrawStateNodeControls(FAnimStateDesc& State);
    void DrawStateAssetPopup();
    void DrawTransitionInspector(FAnimTransitionDesc& Transition);
    void DrawLoadPopup();
    void DrawSaveAsPopup();
    void DrawGraphContextMenus();

    void MarkDirty();
    void SetStatus(const FString& Message);
    void ResetSelection();
    void SyncSelectionFromNodeEditor();
    void SyncNodePositionsFromEditor();
    void ApplyInitialNodePositions();

    FAnimStateDesc* GetSelectedState();
    FAnimTransitionDesc* GetSelectedTransition();
    FString BuildPreviewJson() const;
    void RefreshFbxAssetList();
    const TArray<FString>& GetFbxAssetList();
    const TArray<FString>& GetAnimStacksForFbx(const FString& FbxPath);
    void RequestOpenStateAssetPopup(int32 StateId, EStateAssetPopupKind Kind);
    bool HasAnyTransitions() const;
    bool IsAnyStateNodeId(int32 NodeId) const;
    bool IsAnyOutputPinId(int32 PinId) const;
    bool ShowAnyStateNode();
    bool DeleteAnyStateNode();
    const FAnimStateDesc* FindStateByPinId(int32 PinId) const;
    int32 MakeInputPinId(int32 StateId) const;
    int32 MakeOutputPinId(int32 StateId) const;
    int32 AllocateStateId() const;
    int32 AllocateTransitionId() const;
    FName MakeUniqueStateName() const;
    bool AddState();
    bool DeleteSelectedState();
    bool AddTransition(const FName& FromState, const FName& ToState);
    bool DeleteSelectedTransition();
    bool RenameState(FAnimStateDesc& State, const FName& NewName);
    void RemoveTransitionsTouchingState(const FName& StateName);

private:
    ax::NodeEditor::EditorContext* NodeEditorContext = nullptr;
    FAnimStateMachineDesc Desc;
    FString CurrentPath;
    FString StatusMessage;
    int32 SelectedStateId = -1;
    int32 SelectedTransitionId = -1;
    char LoadPathBuffer[260] = {};
    char SaveAsPathBuffer[260] = {};
    TArray<FString> CachedFbxPaths;
    TMap<FString, TArray<FString>> CachedAnimStacksByFbx;
    int32 StateAssetPopupStateId = -1;
    EStateAssetPopupKind StateAssetPopupKind = EStateAssetPopupKind::None;
    float StateAssetPopupX = 0.0f;
    float StateAssetPopupY = 0.0f;
    float StateAssetPopupWidth = 260.0f;
    float StateAssetPopupAnchorX = 0.0f;
    float StateAssetPopupAnchorY = 0.0f;
    bool bOpenStateAssetPopupRequested = false;
    bool bDirty = false;
    bool bNeedsInitialNodePlacement = false;
    bool bShowAnyStateNode = false;
    bool bNeedsAnyStateNodePlacement = false;
    bool bSaveAsPopupRequested = false;
    bool bLoadPopupRequested = false;
    bool bAssetListsDirty = true;
};
