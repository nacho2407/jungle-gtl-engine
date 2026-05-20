#include "Editor/UI/Asset/AnimGraphEditorWidget.h"

#include "Animation/AnimGraphAsset.h"
#include "Object/Object.h"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <cstdio>

namespace ed = ax::NodeEditor;

FAnimGraphEditorWidget::~FAnimGraphEditorWidget()
{
	DestroyContext();
}

bool FAnimGraphEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UAnimGraphAsset>();
}

void FAnimGraphEditorWidget::Open(UObject* Object)
{
	if (!CanEdit(Object))
	{
		return;
	}

	FAssetEditorWidget::Open(Object);
	EnsureContext();
}

void FAnimGraphEditorWidget::Close()
{
	DestroyContext();
	FAssetEditorWidget::Close();
}

void FAnimGraphEditorWidget::EnsureContext()
{
	if (NodeEditorContext) return;

	ed::Config Cfg;
	Cfg.SettingsFile = nullptr; // 자산 자체에 직렬화 — node-editor 의 디스크 캐시 비활성.
	NodeEditorContext = ed::CreateEditor(&Cfg);
}

void FAnimGraphEditorWidget::DestroyContext()
{
	if (NodeEditorContext)
	{
		ed::DestroyEditor(NodeEditorContext);
		NodeEditorContext = nullptr;
	}
}

void FAnimGraphEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!IsOpen() || !EditedObject || !NodeEditorContext)
	{
		return;
	}

	UAnimGraphAsset* Asset = static_cast<UAnimGraphAsset*>(EditedObject);

	// 자산별 윈도우 고유 ID — 동시 다중 인스턴스 대비 (현재는 단일이지만 충돌 회피).
	char WindowTitle[128];
	std::snprintf(WindowTitle, sizeof(WindowTitle),
		"AnimGraph Editor##%p", static_cast<const void*>(Asset));

	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	bool bOpenFlag = true;
	if (!ImGui::Begin(WindowTitle, &bOpenFlag))
	{
		ImGui::End();
		if (!bOpenFlag) Close();
		return;
	}

	ed::SetCurrentEditor(NodeEditorContext);
	ed::Begin("AnimGraphCanvas");

	// Placeholder 노드 — 데이터 모델 정의 전까지 캔버스가 비어있지 않도록.
	ed::BeginNode(1);
		ImGui::Text("Input Pose");
		ed::BeginPin(101, ed::PinKind::Output);
			ImGui::Text("Pose ->");
		ed::EndPin();
	ed::EndNode();

	ed::BeginNode(2);
		ImGui::Text("Output Pose");
		ed::BeginPin(102, ed::PinKind::Input);
			ImGui::Text("-> Pose");
		ed::EndPin();
	ed::EndNode();

	ed::Link(1001, 101, 102);

	ed::End();
	ed::SetCurrentEditor(nullptr);

	ImGui::End();

	if (!bOpenFlag) Close();
}
