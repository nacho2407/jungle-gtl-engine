#include "Editor/UI/Asset/AnimGraphEditorWidget.h"

#include "Animation/AnimGraphAsset.h"
#include "Animation/AnimGraphTypes.h"
#include "Object/Object.h"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <cstdio>

namespace ed = ax::NodeEditor;

namespace
{
	// 데이터 모델의 동일 namespace id 공간을 그대로 imgui-node-editor 의 NodeId/PinId/LinkId 로 캐스팅.
	// 0 == invalid 를 양쪽이 공유하므로 안전.
	inline ed::NodeId ToNodeId(uint32 Id) { return static_cast<ed::NodeId>(Id); }
	inline ed::PinId  ToPinId (uint32 Id) { return static_cast<ed::PinId >(Id); }
	inline ed::LinkId ToLinkId(uint32 Id) { return static_cast<ed::LinkId>(Id); }

	const char* NodeTypeLabel(EAnimGraphNodeType Type)
	{
		switch (Type)
		{
			case EAnimGraphNodeType::OutputPose:          return "Output Pose";
			case EAnimGraphNodeType::SequencePlayer:      return "Sequence Player";
			case EAnimGraphNodeType::StateMachine:        return "State Machine";
			case EAnimGraphNodeType::Slot:                return "Slot";
			case EAnimGraphNodeType::LayeredBlendPerBone: return "Layered Blend";
			case EAnimGraphNodeType::BlendListByEnum:     return "Blend List By Enum";
			case EAnimGraphNodeType::VariableGet:         return "Variable Get";
		}
		return "Node";
	}
}

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
	bPositionsPushed = false; // 새 자산 → 첫 프레임에 다시 push.
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

	// 첫 프레임: 데이터 모델의 좌표를 node-editor 컨텍스트에 한 번 push.
	// 이후엔 컨텍스트가 사용자 드래그를 관리 — 모델로의 역동기화는 편집 단계 도입 시 추가.
	if (!bPositionsPushed)
	{
		for (const FAnimGraphNode& Node : Asset->GetNodes())
		{
			ed::SetNodePosition(ToNodeId(Node.NodeId), ImVec2(Node.PosX, Node.PosY));
		}
		bPositionsPushed = true;
	}

	for (const FAnimGraphNode& Node : Asset->GetNodes())
	{
		ed::BeginNode(ToNodeId(Node.NodeId));
			ImGui::Text("%s", NodeTypeLabel(Node.Type));
			ImGui::Separator();

			for (const FAnimGraphPin& Pin : Node.Pins)
			{
				ed::BeginPin(ToPinId(Pin.PinId), Pin.Kind == EAnimGraphPinKind::Input
					? ed::PinKind::Input : ed::PinKind::Output);

				if (Pin.Kind == EAnimGraphPinKind::Input)
				{
					ImGui::Text("-> %s", Pin.DisplayName.ToString().c_str());
				}
				else
				{
					ImGui::Text("%s ->", Pin.DisplayName.ToString().c_str());
				}

				ed::EndPin();
			}
		ed::EndNode();
	}

	for (const FAnimGraphLink& Link : Asset->GetLinks())
	{
		ed::Link(ToLinkId(Link.LinkId), ToPinId(Link.FromPinId), ToPinId(Link.ToPinId));
	}

	ed::End();
	ed::SetCurrentEditor(nullptr);

	ImGui::End();

	if (!bOpenFlag) Close();
}
