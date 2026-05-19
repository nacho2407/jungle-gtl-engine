#include "AnimationTimelinePanel.h"
#include "AnimationTransportBar.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimDataModel.h"
#include "Animation/AnimNotify.h"
#include "Animation/AnimNotifyState.h"
#include "Animation/AnimationManager.h"
#include "Component/SkeletalMeshComponent.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Object/UClass.h"
#include "Core/PropertyTypes.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>

// 일부 헤더가 Windows.h 를 끌어오면 GetCurrentTime 매크로가 호출을 가로챈다.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace
{
	constexpr float HeaderW    = 190.0f; // 좌측 트랙 헤더 컬럼 폭
	constexpr float RulerH     = 22.0f;  // 프레임 눈금 행 높이
	constexpr float RowH        = 22.0f;  // 트랙 헤더 행 높이
	constexpr float NotifyLaneH = 28.0f;  // Notifies 펼침 시 레인 높이
	constexpr float TransportH  = 36.0f;  // 하단 트랜스포트 행 높이

	constexpr ImU32 ColPanelBg   = IM_COL32(26, 26, 26, 255);
	constexpr ImU32 ColHeaderBg  = IM_COL32(38, 38, 38, 255);
	constexpr ImU32 ColRulerBg   = IM_COL32(33, 33, 33, 255);
	constexpr ImU32 ColSeparator = IM_COL32(18, 18, 18, 255);
	constexpr ImU32 ColTick      = IM_COL32(78, 78, 78, 255);
	constexpr ImU32 ColTickMinor = IM_COL32(52, 52, 52, 255);
	constexpr ImU32 ColLabel     = IM_COL32(150, 150, 150, 255);
	constexpr ImU32 ColRowText   = IM_COL32(205, 205, 205, 255);
	constexpr ImU32 ColPlayhead  = IM_COL32(255, 170, 40, 255);
	constexpr ImU32 ColNotify    = IM_COL32(74, 145, 226, 255);
	constexpr ImU32 ColNotifyDur = IM_COL32(74, 145, 226, 110);

	// 등록된 모든 UClass 중 Base 의 구체 서브클래스만 수집 (Base 자체는 제외).
	// ObjectFactory 가 UCLASS 등록 시 자동으로 클래스명 → 인스턴스 함수를 등록하므로
	// 새 Notify/NotifyState 클래스를 추가하면 별도 작업 없이 자동으로 콤보에 노출된다.
	TArray<UClass*> EnumerateConcreteSubclasses(UClass* Base)
	{
		TArray<UClass*> Out;
		if (!Base) return Out;
		for (UClass* C : UClass::GetAllClasses())
		{
			if (!C || C == Base) continue;
			if (!C->IsA(Base)) continue;
			Out.push_back(C);
		}
		return Out;
	}

	// Notify/NotifyState 인스턴스 생성 + FAnimNotifyEvent 채워 반환.
	// bAsState=true → NotifyState 슬롯, false → Notify 슬롯. ObjectFactory::Create 가 클래스
	// 이름으로 인스턴스 만들고 DataModel 을 Outer 로 매단다 (라이프타임 체인).
	FAnimNotifyEvent MakeNotifyFromClass(UAnimSequence* Seq, UClass* Cls,
	                                     const FString& Name, float Time,
	                                     float Duration, bool bAsState)
	{
		FAnimNotifyEvent Event;
		Event.NotifyName  = FName(Name);
		Event.TriggerTime = Time;
		Event.Duration    = bAsState ? std::max(Duration, 0.01f) : 0.0f;

		if (Cls && Seq)
		{
			UObject* Created = FObjectFactory::Get().Create(Cls->GetName(), Seq->GetDataModel());
			if (bAsState)
			{
				Event.NotifyState = Cast<UAnimNotifyState>(Created);
			}
			else
			{
				Event.Notify = Cast<UAnimNotify>(Created);
			}
		}
		return Event;
	}

	// 가용 폭에 안 맞으면 끝에 "..." 을 붙여 잘라낸다. CalcTextSize 가 픽셀 단위 폭을 알려주므로
	// 끝부터 한 글자씩 줄여가며 ellipsis 와 합한 폭이 들어맞을 때까지 반복. 일반 notify 이름은
	// 짧으므로 linear truncation 비용 무시 가능.
	std::string TruncateWithEllipsis(const std::string& In, float MaxW)
	{
		if (MaxW <= 0.0f || In.empty()) return "";
		const ImVec2 Full = ImGui::CalcTextSize(In.c_str());
		if (Full.x <= MaxW) return In;

		static const std::string Ellipsis = "...";
		const float EllipsisW = ImGui::CalcTextSize(Ellipsis.c_str()).x;
		if (MaxW <= EllipsisW) return Ellipsis;

		std::string Buf = In;
		while (Buf.size() > 1)
		{
			Buf.pop_back();
			if (ImGui::CalcTextSize((Buf + Ellipsis).c_str()).x <= MaxW)
			{
				return Buf + Ellipsis;
			}
		}
		return Ellipsis;
	}

	// 한 UObject 의 UPROPERTY(Edit) 필드를 인플레이스로 그려준다. Notify/NotifyState 의
	// payload 편집용 경량 인스펙터 — 풀 FEditorPropertyWidget 의존성 없이 timeline 패널 안에서
	// 자족. 지원 타입은 Notify payload 에 흔히 쓰일 단순형 (Bool/Int/Float/String/Vec3/Vec4/Color4).
	// 그 외 타입은 disabled placeholder.
	bool RenderObjectPropertiesInline(UObject* Object)
	{
		if (!Object)
		{
			ImGui::TextDisabled("(no object)");
			return false;
		}
		TArray<FPropertyValue> Props;
		Object->GetEditableProperties(Props);
		if (Props.empty())
		{
			ImGui::TextDisabled("(no editable properties)");
			return false;
		}

		bool bAnyChanged = false;

		if (ImGui::BeginTable("##notifyProps", 2,
		                      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

			for (FPropertyValue& Prop : Props)
			{
				const bool bReadOnly = Prop.Property && (Prop.Property->Flags & PF_ReadOnly) != 0;
				const char* Disp = Prop.GetDisplayName();
				if (!Disp || !*Disp) Disp = Prop.GetName();

				ImGui::PushID(Prop.GetName() ? Prop.GetName() : "");
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(Disp ? Disp : "");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);

				if (bReadOnly) ImGui::BeginDisabled();

				bool bChanged = false;
				switch (Prop.GetType())
				{
				case EPropertyType::Bool:
				{
					bool* V = static_cast<bool*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::Checkbox("##v", V);
					break;
				}
				case EPropertyType::ByteBool:
				{
					uint8* V = static_cast<uint8*>(Prop.GetValuePtr());
					if (V)
					{
						bool b = (*V != 0);
						if (ImGui::Checkbox("##v", &b)) { *V = b ? 1 : 0; bChanged = true; }
					}
					break;
				}
				case EPropertyType::Int:
				{
					int32* V = static_cast<int32*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::DragInt("##v", V, Prop.GetSpeed());
					break;
				}
				case EPropertyType::Float:
				{
					float* V = static_cast<float*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::DragFloat("##v", V, Prop.GetSpeed());
					break;
				}
				case EPropertyType::Vec3:
				{
					float* V = static_cast<float*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::DragFloat3("##v", V, Prop.GetSpeed());
					break;
				}
				case EPropertyType::Vec4:
				{
					float* V = static_cast<float*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::DragFloat4("##v", V, Prop.GetSpeed());
					break;
				}
				case EPropertyType::Color4:
				{
					float* V = static_cast<float*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::ColorEdit4("##v", V);
					break;
				}
				case EPropertyType::String:
				{
					FString* S = static_cast<FString*>(Prop.GetValuePtr());
					if (S)
					{
						char Buf[256];
						strncpy_s(Buf, sizeof(Buf), S->c_str(), _TRUNCATE);
						if (ImGui::InputText("##v", Buf, sizeof(Buf)))
						{
							*S = Buf;
							bChanged = true;
						}
					}
					break;
				}
				case EPropertyType::Name:
				{
					FName* N = static_cast<FName*>(Prop.GetValuePtr());
					if (N)
					{
						FString Cur = N->ToString();
						char Buf[256];
						strncpy_s(Buf, sizeof(Buf), Cur.c_str(), _TRUNCATE);
						if (ImGui::InputText("##v", Buf, sizeof(Buf)))
						{
							*N = FName(FString(Buf));
							bChanged = true;
						}
					}
					break;
				}
				default:
					ImGui::TextDisabled("(unsupported type)");
					break;
				}

				if (bReadOnly) ImGui::EndDisabled();

				if (bChanged)
				{
					bAnyChanged = true;
					if (Prop.Property)
					{
						Object->PostEditProperty(Prop.Property->Name);
					}
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		return bAnyChanged;
	}

	int NiceFrameStep(int Raw)
	{
		static const int Steps[] = { 1, 2, 5, 10, 15, 20, 30, 60, 120, 240, 600 };
		for (int S : Steps)
		{
			if (S >= Raw) return S;
		}
		return 1200;
	}

	// 좌측 트랙 헤더 행 한 줄을 그린다. bExpandable 이면 삼각형 토글을 그린다.
	// 클릭 판정은 호출부에서 InvisibleButton 으로 처리.
	void DrawTrackHeaderRow(ImDrawList* DL, const ImVec2& Pos, float Width, float Height,
	                        const char* Label, bool bExpandable, bool bExpanded)
	{
		DL->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + Height), ColHeaderBg);
		DL->AddLine(ImVec2(Pos.x, Pos.y + Height - 1.0f),
		            ImVec2(Pos.x + Width, Pos.y + Height - 1.0f), ColSeparator);

		const float CY = Pos.y + Height * 0.5f;
		float TextX = Pos.x + 10.0f;

		if (bExpandable)
		{
			const float Cx = Pos.x + 12.0f;
			if (bExpanded)
			{
				DL->AddTriangleFilled(ImVec2(Cx - 4.0f, CY - 3.0f),
				                      ImVec2(Cx + 4.0f, CY - 3.0f),
				                      ImVec2(Cx, CY + 4.0f), ColRowText);
			}
			else
			{
				DL->AddTriangleFilled(ImVec2(Cx - 3.0f, CY - 4.0f),
				                      ImVec2(Cx - 3.0f, CY + 4.0f),
				                      ImVec2(Cx + 4.0f, CY), ColRowText);
			}
			TextX = Pos.x + 26.0f;
		}

		const ImVec2 TS = ImGui::CalcTextSize(Label);
		DL->AddText(ImVec2(TextX, CY - TS.y * 0.5f), ColRowText, Label);
	}
}

void FAnimationTimelinePanel::Render(UAnimSingleNodeInstance* NodeInst,
                                     USkeletalMeshComponent* Comp,
                                     UAnimSequence* Seq,
                                     float PanelHeight,
                                     int32& InOutSelectedNotifyIndex)
{
	// 변경 누적 플래그 — drag/resize 등 연속 이벤트는 매 프레임 commit 하지 않고
	// 마우스 release 시점에 일괄 save (디스크 thrash 방지). 인스턴트 이벤트는 즉시 save.
	// 프레임 간 보존 위해 static — 마우스 누르고 있는 동안 dirty 유지, release 에 일괄 flush.
	static bool sPendingSave = false;
	auto SaveSeqNow = [&]() {
		if (Seq) FAnimationManager::Get().SaveAnimationPreservingMetadata(Seq);
	};

	// 선택 인덱스 stale clamp — 시퀀스 전환 등으로 out-of-range 면 -1.
	if (Seq)
	{
		const int32 NotifyCount = static_cast<int32>(Seq->GetNotifies().size());
		if (InOutSelectedNotifyIndex >= NotifyCount) InOutSelectedNotifyIndex = -1;
	}

	ImGui::BeginChild("##AnimTimelinePanel", ImVec2(0.0f, PanelHeight), false,
	                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImDrawList*  DL     = ImGui::GetWindowDrawList();
	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const float  FullW  = ImGui::GetContentRegionAvail().x;

	if (!Seq || Seq->GetPlayLength() <= 0.0f)
	{
		DL->AddRectFilled(Origin, ImVec2(Origin.x + FullW, Origin.y + PanelHeight), ColPanelBg);
		const char* Msg = "No animation selected.";
		const ImVec2 TS = ImGui::CalcTextSize(Msg);
		DL->AddText(ImVec2(Origin.x + (FullW - TS.x) * 0.5f, Origin.y + PanelHeight * 0.5f - TS.y),
		            ColLabel, Msg);
		ImGui::EndChild();
		return;
	}

	static bool bNotifiesExpanded = true;

	const float PlayLength = Seq->GetPlayLength();
	const float FrameRate  = Seq->GetFrameRate() > 0.0f ? Seq->GetFrameRate() : 30.0f;
	const int   NumFrames  = std::max(Seq->GetNumberOfFrames(), 1);
	const int   EndFrame   = std::max(NumFrames - 1, 0);

	const float TrackAreaH = PanelHeight - TransportH;
	const float CanvasX    = Origin.x + HeaderW;
	const float CanvasW    = std::max(FullW - HeaderW, 1.0f);

	auto TimeToX = [&](float T) { return CanvasX + (T / PlayLength) * CanvasW; };

	const float CurrentTime  = NodeInst ? NodeInst->GetCurrentTime() : 0.0f;
	const int   CurrentFrame = static_cast<int>(std::lround((CurrentTime / PlayLength) * EndFrame));

	// ── 배경 ──
	DL->AddRectFilled(Origin, ImVec2(Origin.x + FullW, Origin.y + PanelHeight), ColPanelBg);
	DL->AddRectFilled(ImVec2(CanvasX, Origin.y),
	                  ImVec2(CanvasX + CanvasW, Origin.y + RulerH), ColRulerBg);

	// ── 스크럽 입력 (룰러 + 트랙 영역 전체) ──
	ImGui::SetCursorScreenPos(ImVec2(CanvasX, Origin.y));
	// 노티파이 핸들 등 위에 겹쳐 놓는 아이템이 입력을 먼저 가져갈 수 있도록 허용.
	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton("##scrub", ImVec2(CanvasW, TrackAreaH));
	if ((ImGui::IsItemActive() || ImGui::IsItemHovered()) && ImGui::IsMouseDown(0) && NodeInst)
	{
		const float Frac = std::clamp((ImGui::GetIO().MousePos.x - CanvasX) / CanvasW, 0.0f, 1.0f);
		NodeInst->SetCurrentTime(Frac * PlayLength);
		if (Comp && ImGui::IsItemActive())
		{
			Comp->SetPlaying(false);
		}
	}
	// 빈 영역 클릭 → notify 선택 해제. 노티 배지는 더 늦게 그려져 입력을 먼저 가져가므로
	// scrub.IsItemActivated 가 트리거되었다는 것은 배지 hit 가 아니라는 뜻.
	if (ImGui::IsItemActivated())
	{
		InOutSelectedNotifyIndex = -1;
	}

	// ── 노티파이 레인 우클릭 → "Add Notify" 팝업 ──
	// 클릭 지점 시간에 노티파이(+LogMessage 로직)를 추가 → DataModel 에 기록되어
	// 직렬화되고, RefreshRuntimeNotifies 로 dispatch 캐시에 반영돼 프리뷰에서 실제 발사.
	static float sPendingNotifyTime = 0.0f;
	if (bNotifiesExpanded && ImGui::IsItemHovered() &&
	    ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		const float LaneTop = Origin.y + RulerH + RowH;
		const float LaneBot = LaneTop + NotifyLaneH;
		const float MouseY  = ImGui::GetIO().MousePos.y;
		if (MouseY >= LaneTop && MouseY <= LaneBot)
		{
			const float Frac = std::clamp(
				(ImGui::GetIO().MousePos.x - CanvasX) / CanvasW, 0.0f, 1.0f);
			sPendingNotifyTime = Frac * PlayLength;
			ImGui::OpenPopup("##addNotifyCtx");
		}
	}
	if (ImGui::BeginPopup("##addNotifyCtx"))
	{
		ImGui::TextDisabled("%.3f s", sPendingNotifyTime);
		ImGui::Separator();

		// 등록된 UAnimNotify 자손 (instant) 클래스 enum → 콤보 메뉴.
		if (ImGui::BeginMenu("Add Notify (instant)"))
		{
			const TArray<UClass*> NotifyClasses = EnumerateConcreteSubclasses(UAnimNotify::StaticClass());
			if (NotifyClasses.empty())
			{
				ImGui::TextDisabled("(no UAnimNotify subclass registered)");
			}
			for (UClass* Cls : NotifyClasses)
			{
				if (ImGui::MenuItem(Cls->GetName()))
				{
					static int sNotifyCounter = 0;
					const FString Name = FString(Cls->GetName()) + "_" + std::to_string(++sNotifyCounter);
					Seq->GetMutableModelNotifies().push_back(
						MakeNotifyFromClass(Seq, Cls, Name, sPendingNotifyTime, 0.0f, false));
					Seq->RefreshRuntimeNotifies();
					InOutSelectedNotifyIndex = static_cast<int32>(Seq->GetMutableModelNotifies().size()) - 1;
					SaveSeqNow();
				}
			}
			ImGui::EndMenu();
		}

		// 등록된 UAnimNotifyState 자손 (duration) 클래스 enum → 콤보 메뉴.
		if (ImGui::BeginMenu("Add NotifyState (duration)"))
		{
			const TArray<UClass*> StateClasses = EnumerateConcreteSubclasses(UAnimNotifyState::StaticClass());
			if (StateClasses.empty())
			{
				ImGui::TextDisabled("(no UAnimNotifyState subclass registered)");
			}
			for (UClass* Cls : StateClasses)
			{
				if (ImGui::MenuItem(Cls->GetName()))
				{
					static int sStateCounter = 0;
					const FString Name = FString(Cls->GetName()) + "_" + std::to_string(++sStateCounter);
					const float DefaultDur = std::min(0.3f, std::max(PlayLength - sPendingNotifyTime, 0.05f));
					Seq->GetMutableModelNotifies().push_back(
						MakeNotifyFromClass(Seq, Cls, Name, sPendingNotifyTime, DefaultDur, true));
					Seq->RefreshRuntimeNotifies();
					InOutSelectedNotifyIndex = static_cast<int32>(Seq->GetMutableModelNotifies().size()) - 1;
					SaveSeqNow();
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	// ── 룰러 눈금 / 프레임 번호 ──
	const int RawStep = static_cast<int>(std::lround(NumFrames * 55.0f / CanvasW));
	const int Step    = NiceFrameStep(std::max(RawStep, 1));
	for (int F = 0; F <= EndFrame; ++F)
	{
		const float X = TimeToX((static_cast<float>(F) / EndFrame) * PlayLength);
		const bool  bLabeled = (F % Step == 0) || (F == EndFrame);
		DL->AddLine(ImVec2(X, Origin.y + (bLabeled ? RulerH * 0.45f : RulerH * 0.7f)),
		            ImVec2(X, Origin.y + RulerH), bLabeled ? ColTick : ColTickMinor);
		if (F % Step == 0)
		{
			char Buf[16];
			snprintf(Buf, sizeof(Buf), "%d", F);
			DL->AddText(ImVec2(X + 3.0f, Origin.y + 3.0f), ColLabel, Buf);
		}
	}
	// 룰러 좌측 헤더(필터 라벨)
	DL->AddRectFilled(Origin, ImVec2(Origin.x + HeaderW, Origin.y + RulerH), ColHeaderBg);
	DL->AddText(ImVec2(Origin.x + 8.0f, Origin.y + 4.0f), ColLabel, "Filter");
	DL->AddRect(ImVec2(Origin.x + 44.0f, Origin.y + 3.0f),
	            ImVec2(Origin.x + HeaderW - 6.0f, Origin.y + RulerH - 3.0f), ColTick);

	// 좌측 헤더 우측 끝에 "+" 추가 어포던스를 그린다. 클릭 시 true 반환.
	// (실제 추가 로직은 미연결 — 호출부에서 TODO 처리)
	auto DrawAddButton = [&](const char* Id, float RowTop, float RowHeight) -> bool
	{
		const float BtnSize = 16.0f;
		const ImVec2 BtnPos(Origin.x + HeaderW - BtnSize - 6.0f,
		                    RowTop + (RowHeight - BtnSize) * 0.5f);
		ImGui::SetCursorScreenPos(BtnPos);
		ImGui::InvisibleButton(Id, ImVec2(BtnSize, BtnSize));
		const bool bHov = ImGui::IsItemHovered();
		const ImU32 Col = bHov ? IM_COL32(230, 230, 230, 255) : IM_COL32(150, 150, 150, 255);
		if (bHov)
		{
			DL->AddRectFilled(BtnPos, ImVec2(BtnPos.x + BtnSize, BtnPos.y + BtnSize),
			                  IM_COL32(255, 255, 255, 28), 2.0f);
			ImGui::SetTooltip("Add (not wired yet)");
		}
		const ImVec2 C(BtnPos.x + BtnSize * 0.5f, BtnPos.y + BtnSize * 0.5f);
		DL->AddLine(ImVec2(C.x - 4.0f, C.y), ImVec2(C.x + 4.0f, C.y), Col, 1.5f);
		DL->AddLine(ImVec2(C.x, C.y - 4.0f), ImVec2(C.x, C.y + 4.0f), Col, 1.5f);
		return ImGui::IsItemClicked();
	};

	// ── 트랙 행 ──
	float RowY = Origin.y + RulerH;

	// Notifies (펼침 가능 + 트랙 추가 어포던스)
	const ImVec2 NotifyHeaderPos(Origin.x, RowY);
	ImGui::SetCursorScreenPos(ImVec2(Origin.x, RowY));
	ImGui::InvisibleButton("##notifyToggle", ImVec2(HeaderW, RowH));
	if (ImGui::IsItemClicked())
	{
		bNotifiesExpanded = !bNotifiesExpanded;
	}
	DrawTrackHeaderRow(DL, NotifyHeaderPos, HeaderW, RowH, "Notifies", true, bNotifiesExpanded);
	if (DrawAddButton("##addNotifyTrack", RowY, RowH))
	{
		// TODO: 노티파이 트랙 추가 — 엔진에 노티파이 트랙(인덱스) 데이터 모델이
		// 생기면 여기서 새 트랙을 push 하도록 연결한다. (현재는 표시 전용)
	}
	DL->AddRectFilled(ImVec2(CanvasX, RowY), ImVec2(CanvasX + CanvasW, RowY + RowH),
	                  IM_COL32(30, 30, 30, 255));
	DL->AddLine(ImVec2(CanvasX, RowY + RowH - 1.0f),
	            ImVec2(CanvasX + CanvasW, RowY + RowH - 1.0f), ColSeparator);
	RowY += RowH;

	if (bNotifiesExpanded)
	{
		const float LaneY = RowY;
		DL->AddRectFilled(ImVec2(Origin.x, LaneY),
		                  ImVec2(Origin.x + HeaderW, LaneY + NotifyLaneH), ColHeaderBg);
		DL->AddText(ImVec2(Origin.x + 26.0f, LaneY + NotifyLaneH * 0.5f - 7.0f),
		            ColLabel, "1");
		if (DrawAddButton("##addNotify", LaneY, NotifyLaneH))
		{
			// 같은 컨텍스트 popup 재사용 — playhead 시각으로 진입. 클래스 picker 제공.
			sPendingNotifyTime = CurrentTime;
			ImGui::OpenPopup("##addNotifyCtx");
		}
		DL->AddRectFilled(ImVec2(CanvasX, LaneY),
		                  ImVec2(CanvasX + CanvasW, LaneY + NotifyLaneH), IM_COL32(24, 24, 24, 255));

		// 드래그로 시간 이동 / 우클릭으로 삭제(루프 후 지연 적용).
		// 직렬화 소스(DataModel)를 직접 편집 → 아래에서 dispatch 캐시 동기화.
		TArray<FAnimNotifyEvent>& Notifies = Seq->GetMutableModelNotifies();
		int PendingDelete = -1;
		static char  sRenameBuf[64]   = {};
		static float sGrabOffsetTime  = 0.0f; // 잡은 지점과 앵커의 시간 차(점프 방지)
		const float BadgeTop  = LaneY + 5.0f;
		const float BadgeBot  = LaneY + NotifyLaneH - 5.0f;
		const float BadgeMidY = (BadgeTop + BadgeBot) * 0.5f;
		for (int i = 0; i < static_cast<int>(Notifies.size()); ++i)
		{
			FAnimNotifyEvent& N   = Notifies[i];
			const float       NX  = TimeToX(N.TriggerTime);
			const std::string Nm  = N.NotifyName.ToString();
			const ImVec2      TSz = ImGui::CalcTextSize(Nm.c_str());

			// State (Duration>0) 는 시각적 폭 = Duration 그대로 (이름이 길어도 늘어나지 않음, 잘림).
			// Instant 는 이름 폭 + 패딩 (시간이 0 이라 시각적 폭 의미 없음).
			float BadgeW;
			if (N.Duration > 0.0f)
			{
				BadgeW = std::max(TimeToX(N.TriggerTime + N.Duration) - NX, 6.0f);
			}
			else
			{
				BadgeW = TSz.x + 16.0f;
			}

			ImGui::PushID(i);

			// State notify (Duration > 0) 면 오른쪽 끝에 6px resize 핸들을 분리.
			// 본체 hit-rect 가 핸들 영역을 침범하지 않게 BodyW 를 줄여 click 분리.
			constexpr float HandleW = 6.0f;
			const bool      bHasDur  = (N.Duration > 0.0f);
			const float     FullW    = BadgeW + 12.0f;
			const float     BodyW    = bHasDur ? std::max(FullW - HandleW, 8.0f) : FullW;

			ImGui::SetCursorScreenPos(ImVec2(NX - 6.0f, BadgeTop));
			ImGui::InvisibleButton("##notify", ImVec2(BodyW, BadgeBot - BadgeTop));
			const bool bHovered = ImGui::IsItemHovered();
			const bool bActive  = ImGui::IsItemActive();

			auto MouseTime = [&]() {
				return std::clamp((ImGui::GetIO().MousePos.x - CanvasX) / CanvasW,
				                  0.0f, 1.0f) * PlayLength;
			};

			// 누른 순간 잡은 지점-앵커 시간차를 기록 + selection 설정.
			if (ImGui::IsItemActivated())
			{
				sGrabOffsetTime          = MouseTime() - N.TriggerTime;
				InOutSelectedNotifyIndex = i;
			}
			// 임계값(io.MouseDragThreshold) 이상 움직였을 때만 이동 → 더블클릭은 제외.
			// Duration > 0 이면 End 도 같이 이동하므로 N.Duration 은 그대로, TriggerTime 만 갱신
			// + 시퀀스 우측 경계 클램프 시 (TriggerTime + Duration) 가 PlayLength 넘지 않게.
			if (bActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, -1.0f))
			{
				const float MaxStart = bHasDur ? std::max(PlayLength - N.Duration, 0.0f)
				                               : PlayLength;
				N.TriggerTime    = std::clamp(MouseTime() - sGrabOffsetTime, 0.0f, MaxStart);
				sPendingSave    = true;   // 마우스 release 시 일괄 save.
			}
			if (bHovered || bActive)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			}
			if (bHovered && !bActive)
			{
				if (bHasDur)
				{
					ImGui::SetTooltip("%s\n%.3f s + %.3f s (state)\n(click: select / drag right edge: resize)",
					                  Nm.c_str(), N.TriggerTime, N.Duration);
				}
				else
				{
					ImGui::SetTooltip("%s\n%.3f s\n(click: select)",
					                  Nm.c_str(), N.TriggerTime);
				}
			}

			// 우측 끝 resize 핸들 — state notify 만.
			if (bHasDur)
			{
				ImGui::SetCursorScreenPos(ImVec2(NX - 6.0f + BodyW, BadgeTop));
				ImGui::InvisibleButton("##notifyR", ImVec2(HandleW, BadgeBot - BadgeTop));
				if (ImGui::IsItemHovered() || ImGui::IsItemActive())
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
				}
				if (ImGui::IsItemActivated())
				{
					InOutSelectedNotifyIndex = i;
				}
				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, -1.0f))
				{
					const float NewEnd = std::clamp(MouseTime(),
					                                N.TriggerTime + 0.01f, PlayLength);
					N.Duration     = NewEnd - N.TriggerTime;
					sPendingSave   = true;
				}
			}

			// 우클릭 컨텍스트 메뉴 — Rename / Delete. Properties 편집은 좌상단 AssetDetails 패널.
			bool bOpenRename = false;
			if (ImGui::BeginPopupContextItem("##notifyCtx"))
			{
				InOutSelectedNotifyIndex = i;
				ImGui::TextDisabled("%s", Nm.c_str());
				ImGui::Separator();
				if (ImGui::MenuItem("Rename"))
				{
					bOpenRename = true;
				}
				if (ImGui::MenuItem("Delete"))
				{
					PendingDelete = i;
				}
				ImGui::EndPopup();
			}
			if (bOpenRename)
			{
				snprintf(sRenameBuf, sizeof(sRenameBuf), "%s", Nm.c_str());
				ImGui::OpenPopup("##notifyRename");
			}
			if (ImGui::BeginPopup("##notifyRename"))
			{
				ImGui::TextDisabled("Rename Notify");
				if (ImGui::IsWindowAppearing())
				{
					ImGui::SetKeyboardFocusHere();
				}
				ImGui::SetNextItemWidth(180.0f);
				const bool bCommit = ImGui::InputText("##rn", sRenameBuf, sizeof(sRenameBuf),
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
				ImGui::SameLine();
				if ((bCommit || ImGui::Button("OK")) && sRenameBuf[0] != '\0')
				{
					N.NotifyName = FName(FString(sRenameBuf));
					Seq->RefreshRuntimeNotifies();
					SaveSeqNow();
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();

			const float MarkNX = TimeToX(N.TriggerTime);
			// State (Duration>0) 와 instant 컬러 분리 — state 파랑, instant 초록.
			const ImU32 Fill = bHasDur
				? ((bHovered || bActive) ? IM_COL32(110, 175, 240, 255) : IM_COL32(74, 145, 226, 255))
				: ((bHovered || bActive) ? IM_COL32(120, 205, 125, 255) : IM_COL32(76, 175, 80,  255));
			const ImU32 Border = bHasDur
				? IM_COL32(30, 70, 130, 255)
				: IM_COL32(40, 95, 45,  255);

			const ImVec2 BMin(MarkNX, BadgeTop);
			const ImVec2 BMax(MarkNX + BadgeW, BadgeBot);
			DL->AddRectFilled(BMin, BMax, Fill, 3.0f);
			// 선택된 entry 면 노란 outline 으로 강조 (좌상단 AssetDetails 패널과 동기 확인용).
			if (i == InOutSelectedNotifyIndex)
			{
				DL->AddRect(ImVec2(BMin.x - 1.0f, BMin.y - 1.0f),
				            ImVec2(BMax.x + 1.0f, BMax.y + 1.0f),
				            IM_COL32(255, 200, 60, 255), 3.0f, 0, 2.0f);
			}
			DL->AddRect(BMin, BMax, Border, 3.0f);

			const float  DiaR = 4.5f;
			const ImVec2 DC(MarkNX, BadgeMidY);
			DL->AddQuadFilled(ImVec2(DC.x - DiaR, DC.y), ImVec2(DC.x, DC.y - DiaR),
			                  ImVec2(DC.x + DiaR, DC.y), ImVec2(DC.x, DC.y + DiaR), Fill);
			DL->AddQuad(ImVec2(DC.x - DiaR, DC.y), ImVec2(DC.x, DC.y - DiaR),
			            ImVec2(DC.x + DiaR, DC.y), ImVec2(DC.x, DC.y + DiaR), Border);

			if (!Nm.empty())
			{
				// State notify 는 시각적 폭이 우선 — 이름이 길면 "..." 으로 잘라 표기.
				// (Instant 는 BadgeW 가 이름 폭에 맞춰 자동 확장되므로 truncation 무영향.)
				const float TextStartX = MarkNX + 8.0f;
				const float MaxTextW   = std::max(BMax.x - TextStartX - 4.0f, 0.0f);
				const std::string Disp = TruncateWithEllipsis(Nm, MaxTextW);
				if (!Disp.empty())
				{
					DL->PushClipRect(BMin, BMax, true);
					DL->AddText(ImVec2(TextStartX, BadgeMidY - TSz.y * 0.5f),
					            IM_COL32(20, 35, 22, 255), Disp.c_str());
					DL->PopClipRect();
				}
			}
		}
		if (PendingDelete >= 0 && PendingDelete < static_cast<int>(Notifies.size()))
		{
			Notifies.erase(Notifies.begin() + PendingDelete);

			// 선택 인덱스 stale 처리 — 삭제된 항목이 선택이면 해제, 뒤쪽이었으면 한 칸 당김.
			if (InOutSelectedNotifyIndex == PendingDelete)
			{
				InOutSelectedNotifyIndex = -1;
			}
			else if (InOutSelectedNotifyIndex > PendingDelete)
			{
				--InOutSelectedNotifyIndex;
			}
			SaveSeqNow();
		}
		// 추가/삭제/드래그(시간 변경)를 dispatch 캐시에 반영 → 프리뷰에서 실제 발사.
		Seq->RefreshRuntimeNotifies();
		DL->AddLine(ImVec2(CanvasX, LaneY + NotifyLaneH - 1.0f),
		            ImVec2(CanvasX + CanvasW, LaneY + NotifyLaneH - 1.0f), ColSeparator);
		RowY += NotifyLaneH;
	}

	auto DrawSimpleHeaderRow = [&](const char* Label, bool bExpandable, bool bExpanded)
	{
		DrawTrackHeaderRow(DL, ImVec2(Origin.x, RowY), HeaderW, RowH, Label, bExpandable, bExpanded);
		DL->AddRectFilled(ImVec2(CanvasX, RowY), ImVec2(CanvasX + CanvasW, RowY + RowH),
		                  IM_COL32(30, 30, 30, 255));
		DL->AddLine(ImVec2(CanvasX, RowY + RowH - 1.0f),
		            ImVec2(CanvasX + CanvasW, RowY + RowH - 1.0f), ColSeparator);
	};

	// Curves: 시퀀스에 내장 네임드 플로트 커브가 없으므로 0 (에셋 내장 데이터 읽기 전용)
	DrawSimpleHeaderRow("Curves (0)", false, false);
	RowY += RowH;

	// Additive Layer Tracks: 언리얼에서는 에디터 내 비파괴 보정(저작) 기능이며,
	// 이 엔진에는 해당 저작 데이터 모델이 없으므로 빈 헤더만 표시 (읽기 전용).
	DrawSimpleHeaderRow("Additive Layer Tracks", false, false);
	RowY += RowH;

	// Attributes: 엔진에 커스텀 본 어트리뷰트 데이터가 없어 비어있음 (읽기 전용)
	DrawSimpleHeaderRow("Attributes", false, false);
	RowY += RowH;

	// 남은 캔버스 빈 영역
	if (RowY < Origin.y + TrackAreaH)
	{
		DL->AddRectFilled(ImVec2(CanvasX, RowY),
		                  ImVec2(CanvasX + CanvasW, Origin.y + TrackAreaH), IM_COL32(30, 30, 30, 255));
	}

	// ── 플레이헤드 ──
	const float PX = TimeToX(CurrentTime);
	DL->AddLine(ImVec2(PX, Origin.y + RulerH), ImVec2(PX, Origin.y + TrackAreaH),
	            ColPlayhead, 1.5f);
	DL->AddTriangleFilled(ImVec2(PX - 6.0f, Origin.y),
	                      ImVec2(PX + 6.0f, Origin.y),
	                      ImVec2(PX, Origin.y + 9.0f), ColPlayhead);

	// 헤더/캔버스 구분선 + 외곽
	DL->AddLine(ImVec2(CanvasX, Origin.y), ImVec2(CanvasX, Origin.y + TrackAreaH), ColSeparator);
	DL->AddRect(Origin, ImVec2(Origin.x + FullW, Origin.y + TrackAreaH), ColSeparator);

	// ── 하단 트랜스포트 행 ──
	const float TransportY = Origin.y + TrackAreaH + 4.0f;
	ImGui::SetCursorScreenPos(ImVec2(Origin.x + 6.0f, TransportY + 6.0f));
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%d", 0);                 // 범위 시작 프레임
	ImGui::SameLine(0.0f, 14.0f);

	FAnimationTransportBar::Render(NodeInst, Comp, PlayLength, NumFrames);

	// 우측: 현재 프레임 입력 / 끝 프레임
	ImGui::SameLine(0.0f, 16.0f);
	int FrameInput = CurrentFrame;
	ImGui::SetNextItemWidth(64.0f);
	if (ImGui::InputInt("##curFrame", &FrameInput, 0, 0) && NodeInst)
	{
		FrameInput = std::clamp(FrameInput, 0, EndFrame);
		NodeInst->SetCurrentTime((static_cast<float>(FrameInput) / EndFrame) * PlayLength);
		if (Comp) Comp->SetPlaying(false);
	}
	ImGui::SameLine(0.0f, 6.0f);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("/ %d", EndFrame);

	// 레이아웃 영역 확정
	ImGui::SetCursorScreenPos(Origin);
	ImGui::Dummy(ImVec2(FullW, PanelHeight));

	// Drag/Resize 의 누적 dirty 를 마우스 release 에 일괄 save (frame 별 디스크 thrash 방지).
	if (sPendingSave && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		SaveSeqNow();
		sPendingSave = false;
	}

	ImGui::EndChild();
}

bool FAnimationTimelinePanel::RenderNotifyDetails(UAnimSequence* Seq, int32 SelectedNotifyIndex)
{
	if (!Seq) return false;
	const TArray<FAnimNotifyEvent>& Notifies = Seq->GetNotifies();
	if (SelectedNotifyIndex < 0 || SelectedNotifyIndex >= static_cast<int32>(Notifies.size()))
	{
		return false;
	}

	// const_cast — GetMutableModelNotifies 가 DataModel 측 mutable 컨테이너 보장.
	TArray<FAnimNotifyEvent>& Mutable = Seq->GetMutableModelNotifies();
	if (SelectedNotifyIndex >= static_cast<int32>(Mutable.size())) return false;
	FAnimNotifyEvent& N = Mutable[SelectedNotifyIndex];

	const FString ClsName = N.Notify      ? FString(N.Notify->GetClass()->GetName())
	                      : N.NotifyState ? FString(N.NotifyState->GetClass()->GetName())
	                                      : FString("None");
	const bool bIsState = (N.NotifyState != nullptr) && (N.Duration > 0.0f);

	ImGui::TextUnformatted("Notify Details");
	ImGui::Separator();
	ImGui::TextDisabled("Class:  %s", ClsName.c_str());
	ImGui::TextDisabled("Type:   %s", bIsState ? "State (duration)" : "Instant");
	ImGui::Dummy(ImVec2(0, 4));

	bool bChanged = false;

	// Name 편집 — 인플레이스.
	{
		FString Cur = N.NotifyName.ToString();
		char Buf[256];
		strncpy_s(Buf, sizeof(Buf), Cur.c_str(), _TRUNCATE);
		ImGui::TextUnformatted("Name");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputText("##notifyName", Buf, sizeof(Buf)))
		{
			N.NotifyName = FName(FString(Buf));
			bChanged     = true;
		}
	}

	// TriggerTime 편집 + (state) Duration 편집.
	{
		ImGui::TextUnformatted("Trigger Time (sec)");
		ImGui::SetNextItemWidth(-FLT_MIN);
		const float MaxStart = bIsState
			? std::max(Seq->GetPlayLength() - N.Duration, 0.0f)
			: Seq->GetPlayLength();
		if (ImGui::DragFloat("##trig", &N.TriggerTime, 0.01f, 0.0f, MaxStart, "%.3f"))
		{
			bChanged = true;
		}
	}
	if (bIsState)
	{
		ImGui::TextUnformatted("Duration (sec)");
		ImGui::SetNextItemWidth(-FLT_MIN);
		const float MaxDur = std::max(Seq->GetPlayLength() - N.TriggerTime, 0.01f);
		if (ImGui::DragFloat("##dur", &N.Duration, 0.01f, 0.01f, MaxDur, "%.3f"))
		{
			bChanged = true;
		}
	}

	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Separator();
	ImGui::TextUnformatted("Properties");
	ImGui::Separator();

	if (N.Notify)
	{
		if (RenderObjectPropertiesInline(N.Notify))      bChanged = true;
	}
	if (N.NotifyState)
	{
		if (RenderObjectPropertiesInline(N.NotifyState)) bChanged = true;
	}
	if (!N.Notify && !N.NotifyState)
	{
		ImGui::TextDisabled("(no notify object bound)");
	}

	if (bChanged)
	{
		Seq->RefreshRuntimeNotifies();
		FAnimationManager::Get().SaveAnimationPreservingMetadata(Seq);
	}
	return bChanged;
}
