#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"

namespace ax { namespace NodeEditor { struct EditorContext; } }

// UAnimGraphAsset 의 시각 노드 그래프 에디터.
// 본 단계는 깡통 — imgui-node-editor 캔버스 + placeholder 노드 2개만.
// 데이터 모델 (Nodes/Pins/Links) 이 추가되면 이 위젯이 그래프 그리기/편집/컴파일을 담당.
class FAnimGraphEditorWidget : public FAssetEditorWidget
{
public:
	FAnimGraphEditorWidget() = default;
	~FAnimGraphEditorWidget() override;

	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object)    override;
	void Close()                  override;
	void Render(float DeltaTime)  override;

private:
	void EnsureContext();
	void DestroyContext();

	ax::NodeEditor::EditorContext* NodeEditorContext = nullptr;

	// 첫 프레임에 데이터 모델의 좌표를 ed::SetNodePosition 으로 push 했는지.
	// Open 마다 false 로 리셋 — 같은 위젯 인스턴스가 다른 자산을 받았을 때 재초기화.
	bool bPositionsPushed = false;
};
