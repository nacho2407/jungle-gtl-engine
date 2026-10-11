#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"

#include "imgui.h" // ImVec2 — popup 위치 캐시용.

namespace ax { namespace NodeEditor { struct EditorContext; } }

// UAnimGraphAsset 의 시각 노드 그래프 에디터.
// imgui-node-editor 캔버스에 자산 데이터 모델을 그리고 편집 (노드 추가/삭제, 링크 생성/삭제,
// 노드 위치 드래그 동기화) 까지 담당. 컴파일 → 런타임 트리는 후속 단계.
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

	// 배경 우클릭 시 캡쳐한 캔버스 좌표 — 같은 프레임의 BeginPopup 안에서 신규 노드 spawn 위치로 사용.
	ImVec2 PendingNewNodePosition = ImVec2(0, 0);
};
