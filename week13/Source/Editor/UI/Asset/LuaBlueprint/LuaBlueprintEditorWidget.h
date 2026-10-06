#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"

#include "imgui.h"
#include "LuaBlueprint/LuaBlueprintTypes.h"

namespace ax::NodeEditor { struct EditorContext; }

class ULuaBlueprintAsset;
struct FLuaBlueprintNode;
struct FLuaBlueprintPin;
struct FLuaBlueprintVariable;


class FLuaBlueprintEditorWidget : public FAssetEditorWidget
{
public:
	FLuaBlueprintEditorWidget() = default;
	~FLuaBlueprintEditorWidget() override;

	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Close() override;
	void Render(const FEditorPanelContext& Context) override;

private:
	void EnsureContext();
	void DestroyContext();

	ULuaBlueprintAsset* GetBlueprint() const;
	void RenderToolbar(ULuaBlueprintAsset* Blueprint);
	void RenderCompileErrorPanel(ULuaBlueprintAsset* Blueprint);
	void RenderVariables(ULuaBlueprintAsset* Blueprint);
	void RenderGraph(ULuaBlueprintAsset* Blueprint);
	void RenderNodeBody(ULuaBlueprintAsset* Blueprint, FLuaBlueprintNode& Node);
	void RenderNodeInspector(ULuaBlueprintAsset* Blueprint, FLuaBlueprintNode& Node);
	void RenderVariableEditor(ULuaBlueprintAsset* Blueprint, FLuaBlueprintVariable& Variable, int32 Index);
	void RenderDiagnostics(ULuaBlueprintAsset* Blueprint);
	void RenderGeneratedLua(ULuaBlueprintAsset* Blueprint);

	bool RenderInlinePinLiteral(ULuaBlueprintAsset* Blueprint, FLuaBlueprintNode& Node, FLuaBlueprintPin& Pin);

	bool AddNodeMenuItem(ULuaBlueprintAsset* Blueprint, ELuaBlueprintNodeType Type);
	bool AddVariableMenuItem(ULuaBlueprintAsset* Blueprint, ELuaBlueprintPinType Type, const char* Label);
	void RenderAddNodeMenu(ULuaBlueprintAsset* Blueprint);
	void RenderPinSpawnMenu(ULuaBlueprintAsset* Blueprint);
	bool AddContextNodeMenuItem(ULuaBlueprintAsset* Blueprint, ELuaBlueprintNodeType Type);
	bool NodeTypeCanConnectToPendingPin(ELuaBlueprintNodeType Type) const;
	FLuaBlueprintPin* FindFirstCompatiblePinOnNode(ULuaBlueprintAsset* Blueprint, FLuaBlueprintNode& Node, const FLuaBlueprintPin& DragPin) const;

	// 변수 패널에서 캔버스 위로 drag → drop 시 Get/Set 팝업.
	void HandleVariableDropOnCanvas();
	void RenameVariableCascade(ULuaBlueprintAsset* Blueprint, const FName& OldName, const FName& NewName);
	void SpawnVariableNode(ULuaBlueprintAsset* Blueprint, ELuaBlueprintNodeType Type, const FName& VariableName, const ImVec2& Position);

	// 에디터 대형 기능: snapshot 기반 Undo/Redo 와 선택 노드 copy/paste.
	void CaptureInitialUndoSnapshot(ULuaBlueprintAsset* Blueprint);
	void CommitBlueprintEdit(ULuaBlueprintAsset* Blueprint);
	void UndoBlueprintEdit(ULuaBlueprintAsset* Blueprint);
	void RedoBlueprintEdit(ULuaBlueprintAsset* Blueprint);
	bool RestoreBlueprintSnapshot(ULuaBlueprintAsset* Blueprint, const TArray<uint8>& Snapshot);
	bool GatherSelectedNodes(ULuaBlueprintAsset* Blueprint, TArray<FLuaBlueprintNode>& OutNodes, TArray<FLuaBlueprintLink>& OutLinks) const;
	bool CloneNodeFragment(ULuaBlueprintAsset* Blueprint, const TArray<FLuaBlueprintNode>& SourceNodes, const TArray<FLuaBlueprintLink>& SourceLinks, const ImVec2& TargetAnchor, TArray<uint32>* OutNewNodeIds = nullptr, const ImVec2* SourceAnchorOverride = nullptr);
	void SelectOnlyNodes(const TArray<uint32>& NodeIds);
	void CopySelectedNodes(ULuaBlueprintAsset* Blueprint);
	void PasteCopiedNodes(ULuaBlueprintAsset* Blueprint, const ImVec2* OverrideAnchor = nullptr);
	void DeleteSelectedNodes(ULuaBlueprintAsset* Blueprint);
	bool DeleteNodesIncludingContainedGroups(ULuaBlueprintAsset* Blueprint, const TArray<uint32>& RootNodeIds);
	void DuplicateSelectedNodes(ULuaBlueprintAsset* Blueprint);
	// 선택된 노드들의 bounding box 를 감싸는 Comment(Group) 노드 생성.
	void GroupSelectedNodesAsComment(ULuaBlueprintAsset* Blueprint);
	void ProcessQueuedNodeEditorCommands(ULuaBlueprintAsset* Blueprint);
	void RemoveVariableCascade(ULuaBlueprintAsset* Blueprint, const FName& VariableName);
	void RenderInputPinConnectionStatus(ULuaBlueprintAsset* Blueprint, const FLuaBlueprintPin& Pin);

private:
	ax::NodeEditor::EditorContext* NodeEditorContext = nullptr;
	bool   bPositionsPushed = false;
	ImVec2 PendingNewNodePosition = ImVec2(0, 0);

	char AddNodeSearchBuf[64] = {};

	// Variable → canvas drop 처리:
	//   frame N : drop 수신, 페이로드 + 스크린 좌표 캡쳐.
	//   frame N+1: ed::Begin 안에서 screen→canvas 변환 + Get/Set 팝업 오픈.
	// 한 프레임 지연 비용으로 ed::ScreenToCanvas 호출 컨텍스트 안정성 확보.
	bool    bPendingVariableDrop = false;
	FName   PendingVariableDropName;
	ImVec2  PendingVariableScreenPos = ImVec2(0, 0);
	ImVec2  PendingVariableDropPos   = ImVec2(0, 0);
	bool    bShowVariableDropMenu    = false;

	// 핀을 빈 공간으로 끌어 놓았을 때 UE Blueprint 처럼 context-sensitive node list 를 띄운다.
	bool   bShowPinSpawnMenu    = false;
	uint32 PendingPinSpawnPinId  = 0;
	ImVec2 PendingPinSpawnPos    = ImVec2(0, 0);
	char   PinSpawnSearchBuf[64] = {};

	TArray<TArray<uint8>> UndoStack;
	TArray<TArray<uint8>> RedoStack;
	TArray<FLuaBlueprintNode> ClipboardNodes;
	TArray<FLuaBlueprintLink> ClipboardLinks;
	bool bRestoringSnapshot = false;

	// NodeEditor API selection calls require NodeEditorContext to be current.
	// Hotkeys/menu entries can fire before ed::Begin(), so selection commands are queued
	// and executed inside RenderGraph() after SetCurrentEditor/Begin.
	bool bQueuedCopySelected = false;
	bool bQueuedPasteNodes = false;
	bool bQueuedDuplicateSelected = false;
	bool bQueuedDeleteSelected = false;
	bool bQueuedGroupSelected = false;
	bool bPendingInitialContentFit = false;
	bool bPendingNodeGeometryEdit = false;
};
