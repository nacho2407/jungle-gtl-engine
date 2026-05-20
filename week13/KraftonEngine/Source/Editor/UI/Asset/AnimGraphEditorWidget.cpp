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

	// 노드 헤더 텍스트 색상 — 노드 종류 한눈에 구분. UE Blueprint 의 카테고리 컬러 컨벤션 차용.
	ImVec4 NodeHeaderColor(EAnimGraphNodeType Type)
	{
		switch (Type)
		{
			case EAnimGraphNodeType::OutputPose:          return ImVec4(0.95f, 0.45f, 0.45f, 1.0f); // 빨강 — 종착점
			case EAnimGraphNodeType::SequencePlayer:      return ImVec4(0.40f, 0.75f, 1.00f, 1.0f); // 파랑 — leaf pose
			case EAnimGraphNodeType::Slot:                return ImVec4(0.60f, 0.95f, 0.65f, 1.0f); // 녹색 — montage 진입점
			case EAnimGraphNodeType::LayeredBlendPerBone: return ImVec4(0.75f, 0.60f, 0.95f, 1.0f); // 연보라 — blender
			case EAnimGraphNodeType::BlendListByEnum:     return ImVec4(0.70f, 0.70f, 0.95f, 1.0f); // 연보라
			case EAnimGraphNodeType::StateMachine:        return ImVec4(0.90f, 0.55f, 0.95f, 1.0f); // 보라 — FSM
			case EAnimGraphNodeType::VariableGet:         return ImVec4(0.95f, 0.85f, 0.40f, 1.0f); // 노랑 — data
		}
		return ImVec4(1, 1, 1, 1);
	}

	FString GetStemFromPath(const FString& Path)
	{
		if (Path.empty()) return FString();
		const std::filesystem::path P(Path);
		return std::filesystem::path(P).stem().string();
	}

	const char* TransitionOpLabel(ETransitionOp Op)
	{
		switch (Op)
		{
			case ETransitionOp::Greater:      return ">";
			case ETransitionOp::GreaterEqual: return ">=";
			case ETransitionOp::Less:         return "<";
			case ETransitionOp::LessEqual:    return "<=";
			case ETransitionOp::Equal:        return "==";
			case ETransitionOp::NotEqual:     return "!=";
		}
		return "?";
	}

	// State name dropdown — Node.States 의 이름을 list. "(any)" 항목은 FName::None 으로 (AnyState 전이).
	// 반환: 변경 발생 여부.
	bool StateNameCombo(const char* Label, const TArray<FAnimGraphState>& States, FName& InOutName, bool bAllowAny)
	{
		const FString CurStr = (InOutName == FName::None) ? FString(bAllowAny ? "(any)" : "(none)") : InOutName.ToString();
		bool bChanged = false;
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::BeginCombo(Label, CurStr.c_str()))
		{
			if (bAllowAny)
			{
				const bool bSel = (InOutName == FName::None);
				if (ImGui::Selectable("(any)", bSel))
				{
					if (InOutName != FName::None) bChanged = true;
					InOutName = FName::None;
				}
			}
			for (const FAnimGraphState& S : States)
			{
				const FString SName = S.StateName.ToString();
				const bool bSel = (InOutName == S.StateName);
				if (ImGui::Selectable(SName.c_str(), bSel))
				{
					if (InOutName != S.StateName) bChanged = true;
					InOutName = S.StateName;
				}
			}
			ImGui::EndCombo();
		}
		return bChanged;
	}

	// UClass 의 Float/Int/Bool/ByteBool UPROPERTY dropdown. VariableGet inspector 와 같은 패턴.
	bool VariableNameCombo(const char* Label, UClass* OwnerCls, FName& InOutName)
	{
		const FString Preview = (InOutName == FName::None) ? FString("(none)") : InOutName.ToString();
		bool bChanged = false;
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::BeginCombo(Label, Preview.c_str()))
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
					const bool bSel = (InOutName.ToString() == Prop->Name);
					if (ImGui::Selectable(Prop->Name, bSel))
					{
						InOutName = FName(Prop->Name);
						bChanged = true;
					}
				}
			}
			ImGui::EndCombo();
		}
		return bChanged;
	}

	// State 한 개의 form. true 반환 — 변경 발생.
	// OwnerNode (이 state 를 보유한 StateMachine 노드) 의 NodeId 는 sub-graph dropdown 의
	// 자기 자신 제외용. AllNodes 는 sub-graph 후보 (그래프 안의 다른 StateMachine 노드) list.
	bool RenderStateRow(FAnimGraphState& State, int32 Index, uint32 OwnerNodeId, const TArray<FAnimGraphNode>& AllNodes)
	{
		ImGui::PushID(Index);
		bool bChanged = false;

		// Name InputText
		char NameBuf[64];
		const FString Cur = State.StateName.ToString();
		std::snprintf(NameBuf, sizeof(NameBuf), "%s", Cur.c_str());
		ImGui::TextUnformatted("Name");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputText("##Name", NameBuf, sizeof(NameBuf)))
		{
			State.StateName = (NameBuf[0] == '\0') ? FName::None : FName(NameBuf);
			bChanged = true;
		}

		// Sub-Graph dropdown — 그래프 안의 다른 StateMachine 노드 (자기 자신 제외).
		// 선택되면 sequence 무시되고 sub-tree 컴파일됨.
		ImGui::TextUnformatted("Sub-Graph");
		FString SubPreview = "(none)";
		if (State.SubGraphNodeId != 0)
		{
			for (const FAnimGraphNode& N : AllNodes)
			{
				if (N.NodeId == State.SubGraphNodeId)
				{
					char Buf[64];
					std::snprintf(Buf, sizeof(Buf), "%s #%u", N.DisplayName.ToString().c_str(), N.NodeId);
					SubPreview = Buf;
					break;
				}
			}
		}
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::BeginCombo("##SubGraph", SubPreview.c_str()))
		{
			if (ImGui::Selectable("(none)", State.SubGraphNodeId == 0))
			{
				if (State.SubGraphNodeId != 0) bChanged = true;
				State.SubGraphNodeId = 0;
			}
			for (const FAnimGraphNode& N : AllNodes)
			{
				if (N.Type != EAnimGraphNodeType::StateMachine) continue;
				if (N.NodeId == OwnerNodeId) continue; // 자기 자신 제외 (직접 self-ref 금지)
				char Buf[64];
				std::snprintf(Buf, sizeof(Buf), "%s #%u", N.DisplayName.ToString().c_str(), N.NodeId);
				const bool bSel = (State.SubGraphNodeId == N.NodeId);
				if (ImGui::Selectable(Buf, bSel))
				{
					if (State.SubGraphNodeId != N.NodeId) bChanged = true;
					State.SubGraphNodeId = N.NodeId;
				}
			}
			ImGui::EndCombo();
		}

		// Sub-Graph 가 선택된 경우 sequence/playrate/looping 은 무시되지만 form 자체는 그대로 노출 —
		// 사용자가 (none) 으로 되돌렸을 때 직전 sequence 값 보존.
		const bool bSubActive = (State.SubGraphNodeId != 0);
		if (bSubActive) ImGui::BeginDisabled();

		ImGui::TextUnformatted("Sequence");
		const FString PreviewStem = State.SequencePath.empty() ? FString("None") : GetStemFromPath(State.SequencePath);
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::BeginCombo("##Seq", PreviewStem.c_str()))
		{
			const bool bSelNone = State.SequencePath.empty();
			if (ImGui::Selectable("None", bSelNone))
			{
				if (!State.SequencePath.empty()) bChanged = true;
				State.SequencePath.clear();
			}
			const TArray<FAssetListItem>& Anims = FAssetRegistry::ListByTypeName("UAnimSequence");
			for (const FAssetListItem& Item : Anims)
			{
				const bool bSel = (State.SequencePath == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSel))
				{
					if (State.SequencePath != Item.FullPath) bChanged = true;
					State.SequencePath = Item.FullPath;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat("##PlayRate", &State.PlayRate, 0.05f, -4.0f, 4.0f, "PlayRate %.2f"))
		{
			bChanged = true;
		}
		if (ImGui::Checkbox("Looping", &State.bLooping))
		{
			bChanged = true;
		}

		if (bSubActive) ImGui::EndDisabled();

		ImGui::PopID();
		return bChanged;
	}

	// Transition 한 개의 form. true 반환 — 변경 발생.
	bool RenderTransitionRow(FAnimGraphTransition& T, const TArray<FAnimGraphState>& States, UClass* OwnerCls, int32 Index)
	{
		ImGui::PushID(Index);
		bool bChanged = false;

		ImGui::TextUnformatted("From");
		bChanged |= StateNameCombo("##From", States, T.FromStateName, /*bAllowAny*/true);

		ImGui::TextUnformatted("To");
		bChanged |= StateNameCombo("##To", States, T.ToStateName, /*bAllowAny*/false);

		ImGui::TextUnformatted("Variable");
		bChanged |= VariableNameCombo("##Var", OwnerCls, T.VariableName);

		// Op dropdown
		ImGui::TextUnformatted("Op");
		ImGui::SetNextItemWidth(60.0f);
		if (ImGui::BeginCombo("##Op", TransitionOpLabel(T.Op)))
		{
			for (int i = 0; i <= static_cast<int>(ETransitionOp::NotEqual); ++i)
			{
				const ETransitionOp O = static_cast<ETransitionOp>(i);
				const bool bSel = (T.Op == O);
				if (ImGui::Selectable(TransitionOpLabel(O), bSel))
				{
					if (T.Op != O) bChanged = true;
					T.Op = O;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat("##Threshold", &T.Threshold, 0.1f, -1000.0f, 1000.0f, "th %.2f"))
		{
			bChanged = true;
		}

		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat("##BlendTime", &T.BlendTime, 0.01f, 0.0f, 5.0f, "blend %.2fs"))
		{
			bChanged = true;
		}

		ImGui::PopID();
		return bChanged;
	}

	// 노드 타입별 properties. 변경 시 Asset.BumpVersion() — UAnimGraphInstance 가 다음 frame 에
	// 재컴파일하여 in-editor live preview 가 즉시 반영되도록.
	void RenderNodeInspector(UAnimGraphAsset& Asset, FAnimGraphNode& Node)
	{
		ImGui::TextColored(NodeHeaderColor(Node.Type), "%s", NodeTypeLabel(Node.Type));
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
				UClass* OwnerCls = UClass::FindByName(Asset.GetOwnerClassName().c_str());
				ImGui::TextUnformatted("Variable");
				if (VariableNameCombo("##VariableName", OwnerCls, Node.VariableName)) bChanged = true;
				ImGui::TextDisabled("(output: float — bool/int 는 자동 cast)");
				break;
			}

			case EAnimGraphNodeType::StateMachine:
			{
				UClass* OwnerCls = UClass::FindByName(Asset.GetOwnerClassName().c_str());

				ImGui::TextUnformatted("Initial State");
				if (StateNameCombo("##Initial", Node.States, Node.InitialStateName, /*bAllowAny*/false))
				{
					bChanged = true;
				}

				ImGui::Spacing();

				// ── States ──
				char Header[64];
				std::snprintf(Header, sizeof(Header), "States (%zu)###States", Node.States.size());
				if (ImGui::CollapsingHeader(Header, ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (ImGui::Button("Add State"))
					{
						FAnimGraphState S;
						S.StateName = FName("NewState");
						Node.States.push_back(std::move(S));
						bChanged = true;
					}

					int32 PendingDeleteIdx = -1;
					for (int32 i = 0; i < static_cast<int32>(Node.States.size()); ++i)
					{
						ImGui::PushID(i);
						ImGui::Separator();
						if (RenderStateRow(Node.States[i], i, Node.NodeId, Asset.GetNodes())) bChanged = true;
						if (ImGui::Button("Delete##State")) PendingDeleteIdx = i;
						ImGui::PopID();
					}
					if (PendingDeleteIdx >= 0)
					{
						Node.States.erase(Node.States.begin() + PendingDeleteIdx);
						bChanged = true;
					}
				}

				ImGui::Spacing();

				// ── Transitions ──
				std::snprintf(Header, sizeof(Header), "Transitions (%zu)###Transitions", Node.Transitions.size());
				if (ImGui::CollapsingHeader(Header, ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (ImGui::Button("Add Transition"))
					{
						Node.Transitions.push_back(FAnimGraphTransition{});
						bChanged = true;
					}

					int32 PendingDeleteIdx = -1;
					for (int32 i = 0; i < static_cast<int32>(Node.Transitions.size()); ++i)
					{
						ImGui::PushID(i);
						ImGui::Separator();
						if (RenderTransitionRow(Node.Transitions[i], Node.States, OwnerCls, i)) bChanged = true;
						if (ImGui::Button("Delete##Trans")) PendingDeleteIdx = i;
						ImGui::PopID();
					}
					if (PendingDeleteIdx >= 0)
					{
						Node.Transitions.erase(Node.Transitions.begin() + PendingDeleteIdx);
						bChanged = true;
					}
				}
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
	constexpr float InspectorWidth = 380.0f;
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
			ImGui::TextColored(NodeHeaderColor(Node.Type), "%s", NodeTypeLabel(Node.Type));
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

		auto AddItem = [&](EAnimGraphNodeType Type)
		{
			const bool bDisabled = (Type == EAnimGraphNodeType::OutputPose) && Asset->HasOutputPoseNode();
			if (bDisabled) ImGui::BeginDisabled();
			ImGui::PushStyleColor(ImGuiCol_Text, NodeHeaderColor(Type));
			const bool bClicked = ImGui::MenuItem(NodeTypeLabel(Type));
			ImGui::PopStyleColor();
			if (bClicked)
			{
				FAnimGraphNode* NewNode = Asset->AddNodeOfType(Type, PendingNewNodePosition.x, PendingNewNodePosition.y);
				if (NewNode)
				{
					ed::SetNodePosition(ToNodeId(NewNode->NodeId), PendingNewNodePosition);
				}
			}
			if (bDisabled) ImGui::EndDisabled();
		};

		if (ImGui::BeginMenu("Pose"))
		{
			AddItem(EAnimGraphNodeType::SequencePlayer);
			AddItem(EAnimGraphNodeType::Slot);
			AddItem(EAnimGraphNodeType::LayeredBlendPerBone);
			AddItem(EAnimGraphNodeType::BlendListByEnum);
			AddItem(EAnimGraphNodeType::StateMachine);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Data"))
		{
			AddItem(EAnimGraphNodeType::VariableGet);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		AddItem(EAnimGraphNodeType::OutputPose); // 단독 — 이미 있으면 disable.

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
