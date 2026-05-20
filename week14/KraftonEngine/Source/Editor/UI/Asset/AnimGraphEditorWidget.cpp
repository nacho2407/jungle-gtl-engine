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

	inline uint32 NodeIdToU32(ed::NodeId Id) { return static_cast<uint32>(Id.Get()); }
	inline uint32 PinIdToU32 (ed::PinId  Id) { return static_cast<uint32>(Id.Get()); }
	inline uint32 LinkIdToU32(ed::LinkId Id) { return static_cast<uint32>(Id.Get()); }

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

	// 노드 팔레트 — 배경 우클릭 메뉴에 노출되는 항목. OutputPose 는 메뉴 항목에서 제외 (1개만 허용).
	constexpr EAnimGraphNodeType PaletteTypes[] = {
		EAnimGraphNodeType::SequencePlayer,
		EAnimGraphNodeType::StateMachine,
		EAnimGraphNodeType::Slot,
		EAnimGraphNodeType::LayeredBlendPerBone,
		EAnimGraphNodeType::BlendListByEnum,
		EAnimGraphNodeType::VariableGet,
	};
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
	bPositionsPushed = false;
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

	// 자산별 윈도우 고유 ID — 동시 다중 인스턴스 대비.
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

	// 첫 프레임에 데이터 모델 좌표를 ed 컨텍스트로 push (1회). 이후 매 프레임 GetNodePosition
	// 으로 pull 해 모델에 반영 — 단방향 (model → ed) 1회 + (ed → model) 매 프레임.
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

	// ── 핀 드래그로 링크 생성 ──
	if (ed::BeginCreate())
	{
		ed::PinId StartId, EndId;
		if (ed::QueryNewLink(&StartId, &EndId))
		{
			if (StartId && EndId)
			{
				uint32 FromU = 0, ToU = 0;
				const bool bOk = Asset->CanLinkPins(PinIdToU32(StartId), PinIdToU32(EndId), &FromU, &ToU);
				if (bOk)
				{
					if (ed::AcceptNewItem())
					{
						Asset->AddLink(FromU, ToU);
					}
				}
				else
				{
					ed::RejectNewItem(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 2.0f);
				}
			}
		}
	}
	ed::EndCreate();

	// ── Delete 키 / 메뉴 → BeginDelete 큐 ──
	if (ed::BeginDelete())
	{
		ed::LinkId DeletedLink;
		while (ed::QueryDeletedLink(&DeletedLink))
		{
			if (ed::AcceptDeletedItem())
			{
				Asset->RemoveLink(LinkIdToU32(DeletedLink));
			}
		}

		ed::NodeId DeletedNode;
		while (ed::QueryDeletedNode(&DeletedNode))
		{
			if (ed::AcceptDeletedItem())
			{
				Asset->RemoveNode(NodeIdToU32(DeletedNode));
			}
		}
	}
	ed::EndDelete();

	// ── 위치 동기화 (ed → model) ──
	// 사용자 드래그가 매 프레임 모델에 반영되어, Close 후 다음 Open 시에도 위치 유지.
	for (FAnimGraphNode& Node : const_cast<TArray<FAnimGraphNode>&>(Asset->GetNodes()))
	{
		const ImVec2 P = ed::GetNodePosition(ToNodeId(Node.NodeId));
		Node.PosX = P.x;
		Node.PosY = P.y;
	}

	// ── 컨텍스트 메뉴 ──
	// ed::ShowXxxContextMenu 는 ed::End 호출 전, ed::Suspend() 상태에서만 popup 호출 가능.
	ed::NodeId   ContextNodeId   = 0;
	ed::PinId    ContextPinId    = 0;
	ed::LinkId   ContextLinkId   = 0;
	ImVec2       NewNodePosition = ImVec2(0, 0);

	ed::Suspend();
	if (ed::ShowNodeContextMenu(&ContextNodeId))
	{
		ImGui::OpenPopup("AnimGraphNodeMenu");
	}
	else if (ed::ShowPinContextMenu(&ContextPinId))
	{
		ImGui::OpenPopup("AnimGraphPinMenu");
	}
	else if (ed::ShowLinkContextMenu(&ContextLinkId))
	{
		ImGui::OpenPopup("AnimGraphLinkMenu");
	}
	else if (ed::ShowBackgroundContextMenu())
	{
		// 마우스 위치를 캔버스 좌표로 캡쳐 — popup 안에서 노드 추가 시 거기에 spawn.
		NewNodePosition = ed::ScreenToCanvas(ImGui::GetMousePos());
		PendingNewNodePosition = NewNodePosition;
		ImGui::OpenPopup("AnimGraphBackgroundMenu");
	}

	if (ImGui::BeginPopup("AnimGraphNodeMenu"))
	{
		const uint32 NodeU = NodeIdToU32(ContextNodeId);
		if (ImGui::MenuItem("Delete"))
		{
			ed::DeleteNode(ContextNodeId);
		}
		(void)NodeU;
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("AnimGraphLinkMenu"))
	{
		if (ImGui::MenuItem("Delete"))
		{
			ed::DeleteLink(ContextLinkId);
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("AnimGraphPinMenu"))
	{
		// 단계 C 에선 핀 단위 액션 없음 — 빈 메뉴.
		ImGui::TextDisabled("(no actions)");
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("AnimGraphBackgroundMenu"))
	{
		ImGui::TextDisabled("Add Node");
		ImGui::Separator();
		for (EAnimGraphNodeType Type : PaletteTypes)
		{
			const bool bDisabled = (Type == EAnimGraphNodeType::OutputPose) && Asset->HasOutputPoseNode();
			if (bDisabled) ImGui::BeginDisabled();
			if (ImGui::MenuItem(NodeTypeLabel(Type)))
			{
				FAnimGraphNode* NewNode = Asset->AddNodeOfType(Type, PendingNewNodePosition.x, PendingNewNodePosition.y);
				if (NewNode)
				{
					ed::SetNodePosition(ToNodeId(NewNode->NodeId), PendingNewNodePosition);
				}
			}
			if (bDisabled) ImGui::EndDisabled();
		}
		ImGui::EndPopup();
	}
	ed::Resume();

	ed::End();
	ed::SetCurrentEditor(nullptr);

	ImGui::End();

	if (!bOpenFlag) Close();
}
