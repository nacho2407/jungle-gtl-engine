#include "Editor/UI/Asset/AnimGraphEditorWidget.h"

#include "Animation/AnimGraphAsset.h"
#include "Animation/AnimGraphManager.h"
#include "Animation/AnimGraphTypes.h"
#include "Animation/AnimInstance.h"
#include "Asset/AssetRegistry.h"
#include "Core/PropertyTypes.h"
#include "Object/Object.h"
#include "Object/UClass.h"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <cstdio>
#include <filesystem>

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

	FString GetStemFromPath(const FString& Path)
	{
		if (Path.empty()) return FString();
		const std::filesystem::path P(Path);
		return std::filesystem::path(P).stem().string();
	}

	// 노드 타입별 properties. 변경 시 Asset.BumpVersion() — UAnimGraphInstance 가 다음 frame 에
	// 재컴파일하여 in-editor live preview 가 즉시 반영되도록.
	void RenderNodeInspector(UAnimGraphAsset& Asset, FAnimGraphNode& Node)
	{
		ImGui::Text("%s", NodeTypeLabel(Node.Type));
		ImGui::TextDisabled("id=%u", Node.NodeId);
		ImGui::Separator();

		bool bChanged = false;

		switch (Node.Type)
		{
			case EAnimGraphNodeType::SequencePlayer:
			{
				ImGui::TextUnformatted("Sequence");
				const FString PreviewStem = Node.SequencePath.empty() ? FString("None") : GetStemFromPath(Node.SequencePath);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##NodeSequence", PreviewStem.c_str()))
				{
					const bool bSelectedNone = Node.SequencePath.empty();
					if (ImGui::Selectable("None", bSelectedNone))
					{
						if (!Node.SequencePath.empty()) bChanged = true;
						Node.SequencePath.clear();
					}
					if (bSelectedNone) ImGui::SetItemDefaultFocus();

					const TArray<FAssetListItem>& AnimFiles = FAssetRegistry::ListByTypeName("UAnimSequence");
					for (const FAssetListItem& Item : AnimFiles)
					{
						const bool bSelected = (Node.SequencePath == Item.FullPath);
						if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
						{
							if (Node.SequencePath != Item.FullPath) bChanged = true;
							Node.SequencePath = Item.FullPath;
						}
						if (bSelected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::DragFloat("##PlayRate", &Node.PlayRate, 0.05f, -4.0f, 4.0f, "PlayRate %.2f"))
				{
					bChanged = true;
				}
				if (ImGui::Checkbox("Looping", &Node.bLooping))
				{
					bChanged = true;
				}
				break;
			}

			case EAnimGraphNodeType::Slot:
			{
				// SlotName 편집 — 비어있으면 컴파일러가 DefaultMontageSlot 으로 fallback.
				char Buf[64];
				const FString Cur = (Node.SlotName == FName::None) ? FString() : Node.SlotName.ToString();
				std::snprintf(Buf, sizeof(Buf), "%s", Cur.c_str());
				ImGui::TextUnformatted("Slot Name");
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::InputText("##SlotName", Buf, sizeof(Buf)))
				{
					Node.SlotName = (Buf[0] == '\0') ? FName::None : FName(Buf);
					bChanged = true;
				}
				ImGui::TextDisabled("(empty → DefaultSlot)");
				break;
			}

			case EAnimGraphNodeType::LayeredBlendPerBone:
			{
				ImGui::TextUnformatted("Blend Weight");
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::SliderFloat("##BlendWeight", &Node.BlendWeight, 0.0f, 1.0f, "%.2f"))
				{
					bChanged = true;
				}
				ImGui::TextDisabled("(per-bone mask: full blend — 후속 단계)");
				break;
			}

			case EAnimGraphNodeType::VariableGet:
			{
				// Asset.OwnerClassName 의 UPROPERTY 중 Float/Int/Bool/ByteBool 타입 dropdown.
				UClass* OwnerCls = UClass::FindByName(Asset.GetOwnerClassName().c_str());
				const char* Preview = (Node.VariableName == FName::None)
					? "(none)" : Node.VariableName.ToString().c_str();

				ImGui::TextUnformatted("Variable");
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##VariableName", Preview))
				{
					if (!OwnerCls)
					{
						ImGui::TextDisabled("Owner class not found");
					}
					else
					{
						TArray<const FProperty*> Props;
						OwnerCls->GetPropertyRefs(Props);
						for (const FProperty* Prop : Props)
						{
							if (!Prop) continue;
							const EPropertyType T = Prop->GetType();
							const bool bScalar = (T == EPropertyType::Float || T == EPropertyType::Int
								|| T == EPropertyType::Bool || T == EPropertyType::ByteBool);
							if (!bScalar) continue;

							const bool bSelected = (Node.VariableName.ToString() == Prop->Name);
							if (ImGui::Selectable(Prop->Name, bSelected))
							{
								Node.VariableName = FName(Prop->Name);
								bChanged = true;
							}
							if (bSelected) ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				ImGui::TextDisabled("(output: float — bool/int 는 자동 cast)");
				break;
			}

			default:
				ImGui::TextDisabled("(no editable properties yet)");
				break;
		}

		if (bChanged) Asset.BumpVersion();
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

	// Toolbar — transient 자산(SourcePath empty)은 Save 비활성. ContentBrowser 에서 만든 자산만 저장.
	{
		const bool bHasPath = !Asset->GetSourcePath().empty();
		if (!bHasPath) ImGui::BeginDisabled();
		if (ImGui::Button("Save"))
		{
			FAnimGraphManager::Get().Save(Asset);
		}
		if (!bHasPath) ImGui::EndDisabled();
		ImGui::SameLine();
		if (bHasPath)
		{
			ImGui::TextDisabled("%s", Asset->GetSourcePath().c_str());
		}
		else
		{
			ImGui::TextDisabled("(transient — Save 불가. ContentBrowser 에서 생성하세요)");
		}

		// OwnerClass dropdown — VariableGet 노드 inspector 의 변수 dropdown 이 이 클래스의
		// UPROPERTY 만 보여줌. UAnimInstance 자손만 list.
		ImGui::SameLine();
		ImGui::TextUnformatted("Owner:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::BeginCombo("##OwnerClass", Asset->GetOwnerClassName().c_str()))
		{
			UClass* AnimInstanceCls = UClass::FindByName("UAnimInstance");
			for (UClass* C : UClass::GetAllClasses())
			{
				if (!C || !AnimInstanceCls || !C->IsA(AnimInstanceCls)) continue;
				const bool bSelected = (Asset->GetOwnerClassName() == C->GetName());
				if (ImGui::Selectable(C->GetName(), bSelected))
				{
					Asset->SetOwnerClassName(C->GetName());
					Asset->BumpVersion();
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();
	}

	// ── 좌(canvas) / 우(inspector) split ──
	constexpr float InspectorWidth = 280.0f;
	const float Spacing            = ImGui::GetStyle().ItemSpacing.x;
	const float TotalWidth         = ImGui::GetContentRegionAvail().x;
	const float CanvasWidth        = (TotalWidth > InspectorWidth + Spacing + 100.0f)
		? TotalWidth - InspectorWidth - Spacing
		: TotalWidth;

	uint32 SelectedNodeId = 0;

	ImGui::BeginChild("##AnimGraphCanvasChild", ImVec2(CanvasWidth, 0), ImGuiChildFlags_None);

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
	for (FAnimGraphNode& Node : const_cast<TArray<FAnimGraphNode>&>(Asset->GetNodes()))
	{
		const ImVec2 P = ed::GetNodePosition(ToNodeId(Node.NodeId));
		Node.PosX = P.x;
		Node.PosY = P.y;
	}

	// ── 컨텍스트 메뉴 ──
	ed::NodeId   ContextNodeId   = 0;
	ed::PinId    ContextPinId    = 0;
	ed::LinkId   ContextLinkId   = 0;

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
		PendingNewNodePosition = ed::ScreenToCanvas(ImGui::GetMousePos());
		ImGui::OpenPopup("AnimGraphBackgroundMenu");
	}

	if (ImGui::BeginPopup("AnimGraphNodeMenu"))
	{
		if (ImGui::MenuItem("Delete"))
		{
			Asset->RemoveNode(NodeIdToU32(ContextNodeId));
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("AnimGraphLinkMenu"))
	{
		if (ImGui::MenuItem("Delete"))
		{
			Asset->RemoveLink(LinkIdToU32(ContextLinkId));
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("AnimGraphPinMenu"))
	{
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

	// ed::End 직전에 선택된 노드 캡쳐 (inspector pane 이 ed 컨텍스트 외부에서 참조).
	{
		ed::NodeId SelBuf[4];
		const int SelCount = ed::GetSelectedNodes(SelBuf, 4);
		if (SelCount > 0) SelectedNodeId = NodeIdToU32(SelBuf[0]);
	}

	ed::End();
	ed::SetCurrentEditor(nullptr);

	ImGui::EndChild();

	// ── 우측 inspector pane ──
	if (CanvasWidth < TotalWidth)
	{
		ImGui::SameLine();
		ImGui::BeginChild("##AnimGraphInspector", ImVec2(0, 0), ImGuiChildFlags_Borders);

		if (SelectedNodeId != 0)
		{
			if (FAnimGraphNode* SelNode = Asset->FindNode(SelectedNodeId))
			{
				RenderNodeInspector(*Asset, *SelNode);
			}
			else
			{
				ImGui::TextDisabled("(stale selection)");
			}
		}
		else
		{
			ImGui::TextDisabled("Select a node to edit properties.");
		}

		ImGui::EndChild();
	}

	ImGui::End();

	if (!bOpenFlag) Close();
}
