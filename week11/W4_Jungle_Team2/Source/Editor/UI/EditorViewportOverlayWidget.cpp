#include "Editor/UI/EditorViewportOverlayWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Slate/SlateApplication.h"
#include "ImGui/imgui.h"
#include "Slate/SSplitter.h"
#include "Slate/SSplitterV.h"
#include "Slate/SSplitterH.h"
#include "Viewport/ViewportLayout.h"

// 뷰포트 타입 → 표시 이름
static const char* GetViewportTypeName(EEditorViewportType Type)
{
	switch (Type)
	{
	case EVT_Perspective: return "Perspective";
	case EVT_OrthoTop:    return "Top";
	case EVT_OrthoBottom: return "Bottom";
	case EVT_OrthoFront:  return "Front";
	case EVT_OrthoBack:   return "Back";
	case EVT_OrthoLeft:   return "Left";
	case EVT_OrthoRight:  return "Right";
	default:              return "Viewport";
	}
}

static const char* GetViewModeName(EViewMode Mode)
{
	switch (Mode)
	{
	case EViewMode::Lit:       return "Lit";
	case EViewMode::Unlit:     return "Unlit";
	case EViewMode::Wireframe: return "Wireframe";
	default:                   return "Lit";
	}
}

// ── 뷰포트별 UE 스타일 View Mode 툴바 ─────────────────────────────
void FEditorViewportOverlayWidget::RenderViewportToolbars()
{
	if (!EditorEngine) return;

	// 버튼 배경색: 약간 어두운 반투명
	ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.15f, 0.15f, 0.75f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.30f, 0.90f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_PopupBg,       ImVec4(0.12f, 0.12f, 0.12f, 0.95f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg,      ImVec4(0.00f, 0.00f, 0.00f, 0.00f)); // 창 배경 투명
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(4.f, 3.f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(4.f, 4.f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,   ImVec2(6.f, 2.f));

	for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
	{
		FEditorViewportState& VS = EditorEngine->GetViewportLayout().GetViewportState(i);
		const FViewportRect& R   = VS.Rect;
		if (R.Width <= 0 || R.Height <= 0) continue;

		const EEditorViewportType VType = EditorEngine->GetViewportLayout().GetViewportClient(i).GetViewportType();

		// 툴바 창: 뷰포트 좌상단에 고정
		ImGui::SetNextWindowPos(ImVec2(static_cast<float>(R.X) + 4.f,
		                               static_cast<float>(R.Y) + 4.f));
		ImGui::SetNextWindowBgAlpha(0.0f);

		char WinName[32];
		snprintf(WinName, sizeof(WinName), "##VPBar_%d", i);

		constexpr ImGuiWindowFlags kFlags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_AlwaysAutoResize;

		ImGui::Begin(WinName, nullptr, kFlags);

		// ── [Perspective / OrthoXXX] 버튼 → 뷰포트 타입 변경 팝업
		// 0번: Perspective 고정 — 버튼 비활성화
		// 1~3번: Orthographic 타입만 선택 가능
		{
			const bool bPerspLocked = (i == 0);

			char TypeBtnName[48];
			snprintf(TypeBtnName, sizeof(TypeBtnName), "%s##vtype_%d", GetViewportTypeName(VType), i);

			if (bPerspLocked) ImGui::BeginDisabled();
			if (ImGui::Button(TypeBtnName))
			{
				char TypePopupId[32];
				snprintf(TypePopupId, sizeof(TypePopupId), "##VTPopup_%d", i);
				ImGui::OpenPopup(TypePopupId);
			}
			if (bPerspLocked) ImGui::EndDisabled();

			char TypePopupId[32];
			snprintf(TypePopupId, sizeof(TypePopupId), "##VTPopup_%d", i);
			if (ImGui::BeginPopup(TypePopupId, ImGuiWindowFlags_NoMove))
			{
				ImGui::TextDisabled("Viewport Type");
				ImGui::Separator();

				// 1~3번 뷰포트: Orthographic 타입만 허용
				static constexpr EEditorViewportType kOrthoTypes[] = {
					EVT_OrthoTop,   EVT_OrthoBottom,
					EVT_OrthoFront, EVT_OrthoBack,
					EVT_OrthoLeft,  EVT_OrthoRight
				};
				for (EEditorViewportType T : kOrthoTypes)
				{
					const bool bSel = (VType == T);
					if (ImGui::Selectable(GetViewportTypeName(T), bSel))
					{
						FEditorViewportClient& VC = EditorEngine->GetViewportLayout().GetViewportClient(i);
						VC.SetViewportType(T);
						VC.ApplyCameraMode(); // 카메라 위치·방향·투영 재설정 + lerp 타겟 초기화
					}
					if (bSel) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndPopup();
			}
		}

		ImGui::SameLine(0.f, 2.f);

		// 뷰포트 1개 <-> 4개 스위칭 버튼
		// 1개는 전체 화면 크기를 차지 + Splitter바를 그리지 않습니다.
		{
			FViewportLayout& Layout = EditorEngine->GetViewportLayout();
			const bool bIsMaximized = Layout.IsSingleViewportMode();

			char MaxBtnName[32];
			snprintf(MaxBtnName, sizeof(MaxBtnName), "%s##max_%d",
				bIsMaximized ? "[4]" : "[1]", i);

			if (ImGui::Button(MaxBtnName))
			{
				if (bIsMaximized)
					Layout.SetSingleViewportMode(false);
				else
					Layout.SetSingleViewportMode(true, i);
			}
		}

		ImGui::SameLine(0.f, 2.f);

		// ── [Lit ▾] 버튼 → 클릭 시 View Mode 팝업
		char BtnName[32];
		snprintf(BtnName, sizeof(BtnName), "%s##vm_%d", GetViewModeName(VS.ViewMode), i);

		if (ImGui::Button(BtnName))
		{
			char PopupId[32];
			snprintf(PopupId, sizeof(PopupId), "##VMPopup_%d", i);
			ImGui::OpenPopup(PopupId);
		}

		// ── View Mode 드롭다운 팝업
		char PopupId[32];
		snprintf(PopupId, sizeof(PopupId), "##VMPopup_%d", i);

		if (ImGui::BeginPopup(PopupId, ImGuiWindowFlags_NoMove))
		{
			ImGui::TextDisabled("View Mode");
			ImGui::Separator();

			const EViewMode Modes[]  = { EViewMode::Lit, EViewMode::Unlit, EViewMode::Wireframe };
			const char*     Labels[] = { "Lit",          "Unlit",          "Wireframe"           };

			for (int32 m = 0; m < 3; ++m)
			{
				bool bSelected = (VS.ViewMode == Modes[m]);
				if (ImGui::Selectable(Labels[m], bSelected))
					VS.ViewMode = Modes[m];
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(5);
}

void FEditorViewportOverlayWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	// 1. 스플리터 바 시각화
	if (EditorEngine)
	{
		FViewportLayout& ViewportLayout = EditorEngine->GetViewportLayout();

		// 1개 모드일 때는 바를 그리지 않음
		if (!ViewportLayout.IsSingleViewportMode())
		{
			ImDrawList* DrawList = ImGui::GetBackgroundDrawList();
			constexpr ImU32 BarColor   = IM_COL32(80,  80,  80,  220);
			constexpr ImU32 HoverColor = IM_COL32(140, 180, 255, 255);

			const SWidget* Hovered  = FSlateApplication::Get().GetHoveredWidget();
			const SWidget* Captured = FSlateApplication::Get().GetCapturedWidget();

			SSplitter* Splitters[] = {
				ViewportLayout.GetRootSplitterV(),
				ViewportLayout.GetTopSplitterH(),
				ViewportLayout.GetBotSplitterH()
			};

			for (SSplitter* S : Splitters)
			{
				if (!S) continue;
				const FRect Bar = S->GetBarRect();

				const SSplitter* Linked = S->GetLinkedSplitter();
				const bool bHighlight = (S == Hovered || S == Captured)
					|| (Linked && (Linked == Hovered || Linked == Captured));

				DrawList->AddRectFilled(
					ImVec2(Bar.X, Bar.Y),
					ImVec2(Bar.X + Bar.Width, Bar.Y + Bar.Height),
					bHighlight ? HoverColor : BarColor);
			}
		}
	}

	// 2. 뷰포트별 View Mode 툴바 (UE 스타일)
	RenderViewportToolbars();

	// 3. Settings 오버레이 패널
	FEditorSettings& Settings = FEditorSettings::Get();

	ImGuiViewport* Viewport = ImGui::GetMainViewport();
	const float Padding = 10.0f;
	ImVec2 WindowPos(Viewport->WorkPos.x + Viewport->WorkSize.x - Padding,
	                 Viewport->WorkPos.y + Padding);

	ImGui::SetNextWindowPos(WindowPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.6f);

	constexpr ImGuiWindowFlags kOverlayFlags =
		ImGuiWindowFlags_NoDecoration    |
		ImGuiWindowFlags_AlwaysAutoResize|
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav           |
		ImGuiWindowFlags_NoMove;

	if (!ImGui::Begin("##ViewportOverlay", nullptr, kOverlayFlags))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button(bExpanded ? "Settings <<" : "Settings >>"))
		bExpanded = !bExpanded;

	if (bExpanded)
	{
		ImGui::Separator();

		// Show Flags
		ImGui::Text("Show");
		ImGui::Checkbox("Primitives",      &Settings.ShowFlags.bPrimitives);
		ImGui::Checkbox("BillboardText",   &Settings.ShowFlags.bBillboardText);
		ImGui::Checkbox("Grid",            &Settings.ShowFlags.bGrid);
		ImGui::Checkbox("Gizmo",           &Settings.ShowFlags.bGizmo);
		ImGui::Checkbox("Bounding Volume", &Settings.ShowFlags.bBoundingVolume);

		ImGui::Separator();

		// Grid Settings
		ImGui::Text("Grid");
		ImGui::SliderFloat("Spacing",       &Settings.GridSpacing,       0.1f, 10.0f, "%.1f");
		ImGui::SliderInt("Half Line Count", &Settings.GridHalfLineCount, 10,   500);

		ImGui::Separator();

		// Camera Sensitivity
		ImGui::Text("Camera");
		ImGui::SliderFloat("Move Sensitivity",   &Settings.CameraMoveSensitivity,   0.1f, 5.0f, "%.1f");
		ImGui::SliderFloat("Rotate Sensitivity", &Settings.CameraRotateSensitivity, 0.1f, 5.0f, "%.1f");
	}

	ImGui::End();
}
