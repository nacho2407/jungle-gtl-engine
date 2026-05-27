#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"

#include "LuaBlueprint/LuaBlueprintTypes.h"
#include "imgui.h"

namespace ax { namespace NodeEditor { struct EditorContext; } }

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
	void Render(float DeltaTime) override;

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

	// 변수 패널에서 캔버스 위로 drag → drop 시 Get/Set 팝업.
	void HandleVariableDropOnCanvas();
	void RenameVariableCascade(ULuaBlueprintAsset* Blueprint, const FName& OldName, const FName& NewName);
	void SpawnVariableNode(ULuaBlueprintAsset* Blueprint, ELuaBlueprintNodeType Type, const FName& VariableName, const ImVec2& Position);

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
};
