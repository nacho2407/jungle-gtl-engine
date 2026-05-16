#include "AnimationTimelinePanel.h"
#include "AnimationTransportBar.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BoneAnimationTrack.h"
#include "Animation/RawAnimSequenceTrack.h"
#include "Component/SkeletalMeshComponent.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

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
                                     float PanelHeight)
{
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
	static bool bAdditiveExpanded = false;

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

	// ── 노티파이 레인 우클릭 → "Add Notify" 팝업 ──
	// 클릭 지점 시간에 인메모리로 노티파이를 추가해 타임라인에 즉시 표시한다.
	// (트리거 로직/직렬화는 아직 미연결 — 표시·배치까지만)
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
		if (ImGui::MenuItem("Add Notify"))
		{
			static int sNotifyCounter = 0;
			FAnimNotifyEvent NewNotify;
			NewNotify.NotifyName  = FName(FString("Notify_") + std::to_string(++sNotifyCounter));
			NewNotify.TriggerTime = sPendingNotifyTime;
			NewNotify.Duration    = 0.0f;
			// TODO: 트리거 로직/직렬화 연결 (현재는 표시·배치 전용)
			Seq->GetMutableNotifies().push_back(NewNotify);
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
			// TODO: 이 트랙에 노티파이 추가 — 엔진에 노티파이 생성/이동/삭제 API가
			// 생기면 클릭 위치 시간으로 Seq->GetMutableNotifies() 에 추가하도록 연결.
		}
		DL->AddRectFilled(ImVec2(CanvasX, LaneY),
		                  ImVec2(CanvasX + CanvasW, LaneY + NotifyLaneH), IM_COL32(24, 24, 24, 255));

		// 마커 드래그(시간 이동) / 우클릭 삭제. 삭제는 루프 후 지연 적용.
		TArray<FAnimNotifyEvent>& Notifies = Seq->GetMutableNotifies();
		int PendingDelete = -1;
		for (int i = 0; i < static_cast<int>(Notifies.size()); ++i)
		{
			FAnimNotifyEvent& N  = Notifies[i];
			const float       NX = TimeToX(N.TriggerTime);

			ImGui::PushID(i);
			const ImVec2 HitMin(NX - 7.0f, LaneY);
			ImGui::SetCursorScreenPos(HitMin);
			ImGui::InvisibleButton("##notify", ImVec2(14.0f, NotifyLaneH));
			const bool bHovered = ImGui::IsItemHovered();
			const bool bActive  = ImGui::IsItemActive();

			if (bActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
			{
				const float Frac = std::clamp(
					(ImGui::GetIO().MousePos.x - CanvasX) / CanvasW, 0.0f, 1.0f);
				N.TriggerTime = Frac * PlayLength;
			}
			if (bHovered || bActive)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			}
			if (bHovered && !bActive)
			{
				ImGui::SetTooltip("%s\n%.3f s",
				                  N.NotifyName.ToString().c_str(), N.TriggerTime);
			}
			if (ImGui::BeginPopupContextItem("##notifyCtx"))
			{
				ImGui::TextDisabled("%s", N.NotifyName.ToString().c_str());
				ImGui::Separator();
				if (ImGui::MenuItem("Delete"))
				{
					PendingDelete = i;
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();

			const ImU32 MarkCol = (bHovered || bActive)
				? IM_COL32(120, 185, 255, 255) : ColNotify;
			const float MarkNX = TimeToX(N.TriggerTime);
			if (N.Duration > 0.0f)
			{
				const float NX2 = TimeToX(N.TriggerTime + N.Duration);
				DL->AddRectFilled(ImVec2(MarkNX, LaneY + 4.0f),
				                  ImVec2(NX2, LaneY + NotifyLaneH - 4.0f), ColNotifyDur, 2.0f);
				DL->AddRect(ImVec2(MarkNX, LaneY + 4.0f),
				            ImVec2(NX2, LaneY + NotifyLaneH - 4.0f), MarkCol, 2.0f);
			}
			DL->AddTriangleFilled(ImVec2(MarkNX - 5.0f, LaneY + 3.0f),
			                      ImVec2(MarkNX + 5.0f, LaneY + 3.0f),
			                      ImVec2(MarkNX, LaneY + 12.0f), MarkCol);
			DL->AddLine(ImVec2(MarkNX, LaneY + 3.0f),
			            ImVec2(MarkNX, LaneY + NotifyLaneH - 3.0f), MarkCol, 1.5f);
			if (!N.NotifyName.ToString().empty())
			{
				DL->AddText(ImVec2(MarkNX + 7.0f, LaneY + 4.0f), ColRowText,
				            N.NotifyName.ToString().c_str());
			}
		}
		if (PendingDelete >= 0 && PendingDelete < static_cast<int>(Notifies.size()))
		{
			Notifies.erase(Notifies.begin() + PendingDelete);
		}
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

	// Additive Layer Tracks: FBX 임포트 시 파싱된 SourceCurveLayers(본 트랙별 레이어)를
	// 에셋 내장 데이터로 읽어서 표시 (읽기 전용)
	struct FLayerInfo { std::string Name; float Weight; bool bSolo; };
	std::vector<FLayerInfo> Layers;
	{
		std::set<std::string> Seen;
		for (const FBoneAnimationTrack& BT : Seq->GetBoneTracks())
		{
			for (const FSourceTransformCurveLayer& L : BT.InternalTrackData.SourceCurveLayers)
			{
				const std::string Key = L.LayerName.empty() ? std::string("Layer") : L.LayerName;
				if (Seen.insert(Key).second)
				{
					Layers.push_back({ Key, L.LayerWeight, L.bSolo });
				}
			}
		}
	}
	const bool bHasLayers = !Layers.empty();
	char AddLabel[48];
	snprintf(AddLabel, sizeof(AddLabel), "Additive Layer Tracks (%d)", static_cast<int>(Layers.size()));

	ImGui::SetCursorScreenPos(ImVec2(Origin.x, RowY));
	ImGui::InvisibleButton("##additiveToggle", ImVec2(HeaderW, RowH));
	if (ImGui::IsItemClicked() && bHasLayers)
	{
		bAdditiveExpanded = !bAdditiveExpanded;
	}
	DrawSimpleHeaderRow(AddLabel, bHasLayers, bAdditiveExpanded && bHasLayers);
	RowY += RowH;

	if (bAdditiveExpanded && bHasLayers)
	{
		for (const FLayerInfo& LI : Layers)
		{
			if (RowY + RowH > Origin.y + TrackAreaH) break;
			DL->AddRectFilled(ImVec2(Origin.x, RowY),
			                  ImVec2(Origin.x + HeaderW, RowY + RowH), ColHeaderBg);
			char Buf[96];
			snprintf(Buf, sizeof(Buf), "%s   w=%.2f%s",
			         LI.Name.c_str(), LI.Weight, LI.bSolo ? "  [solo]" : "");
			DL->AddText(ImVec2(Origin.x + 28.0f, RowY + RowH * 0.5f - 7.0f), ColRowText, Buf);
			DL->AddRectFilled(ImVec2(CanvasX, RowY),
			                  ImVec2(CanvasX + CanvasW, RowY + RowH), IM_COL32(24, 24, 24, 255));
			DL->AddLine(ImVec2(CanvasX, RowY + RowH - 1.0f),
			            ImVec2(CanvasX + CanvasW, RowY + RowH - 1.0f), ColSeparator);
			RowY += RowH;
		}
	}

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

	ImGui::EndChild();
}
