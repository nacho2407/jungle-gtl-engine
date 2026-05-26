#include "ParticleEditorInternal.h"

using namespace ParticleEditorInternal;

void FParticleEditorViewerWidget::RenderCurveEditor(FParticleEditorViewer* Viewer)
{
	LoadCascadeToolbarIcons();

	UParticleModule* Module = ResolveParticleModule(
		Viewer,
		CurveState.Type,
		CurveState.EmitterIndex,
		CurveState.LODIndex,
		CurveState.ModuleIndex);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
	ImGui::BeginChild(
		"ParticleCurveToolbar",
		ImVec2(0.0f, 66.0f),
		false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	constexpr float CurveToolbarItemGap = 2.0f;
	DrawParticleCurveToolbarButton("HorizontalFit", ToolbarIcons.CurveHorizontalIcon.Get(), "Horizontal", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("VerticalFit", ToolbarIcons.CurveVerticalIcon.Get(), "Vertical", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("FitCurve", ToolbarIcons.CurveFitIcon.Get(), "Fit", false, Module != nullptr))
	{
		CurveState.CanvasPanTime = 0.0f;
		CurveState.CanvasPanValue = 0.0f;
		CurveState.CanvasZoomX = 1.0f;
		CurveState.CanvasZoomY = 1.0f;
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("PanCurve", ToolbarIcons.CurvePanIcon.Get(), "Pan", CurveState.ActiveTool == EParticleCurveEditorTool::Pan, true))
	{
		CurveState.ActiveTool = EParticleCurveEditorTool::Pan;
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("ZoomCurve", ToolbarIcons.CurveZoomIcon.Get(), "Zoom", CurveState.ActiveTool == EParticleCurveEditorTool::Zoom, true))
	{
		CurveState.ActiveTool = EParticleCurveEditorTool::Zoom;
	}
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarSeparator("CurveEditTangents");
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("AutoCurve", ToolbarIcons.CurveAutoIcon.Get(), "Auto", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("AutoClampedCurve", ToolbarIcons.CurveAutoClampedIcon.Get(), "Auto/Clamp", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("UserCurve", ToolbarIcons.CurveUserIcon.Get(), "User", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("BreakCurve", ToolbarIcons.CurveBreakIcon.Get(), "Break", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("LinearCurve", ToolbarIcons.CurveLinearIcon.Get(), "Linear", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("ConstantCurve", ToolbarIcons.CurveConstantIcon.Get(), "Constant", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarSeparator("CurveEditInterpolation");
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("FlattenCurve", ToolbarIcons.CurveFlattenIcon.Get(), "Flatten", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("StraightenCurve", ToolbarIcons.CurveStraightenIcon.Get(), "Straighten", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("ShowAllCurve", ToolbarIcons.CurveShowAllIcon.Get(), "Show All", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarSeparator("CurveEditDisplay");
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	DrawParticleCurveToolbarButton("CreateCurve", ToolbarIcons.CurveCreateIcon.Get(), "Create", false, Module != nullptr);
	ImGui::SameLine(0.0f, CurveToolbarItemGap);
	if (DrawParticleCurveToolbarButton("DeleteCurve", ToolbarIcons.CurveDeleteIcon.Get(), "Delete", false, Module != nullptr))
	{
		CurveState.Clear();
		Module = nullptr;
	}

	ImGui::SameLine(0.0f, 16.0f);
	ImGui::BeginGroup();
	ImGui::TextDisabled("Current Tab:");
	ImGui::SetNextItemWidth(168.0f);
	if (ImGui::BeginCombo("##ParticleCurveCurrentTab", Module ? "Default" : "No Curve"))
	{
		ImGui::Selectable("Default", Module != nullptr, ImGuiSelectableFlags_Disabled);
		ImGui::EndCombo();
	}
	ImGui::EndGroup();

	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	const ImVec2 Available = ImGui::GetContentRegionAvail();
	if (Available.x <= 1.0f || Available.y <= 1.0f)
	{
		return;
	}

	const float ChannelWidth = std::min(196.0f, std::max(132.0f, Available.x * 0.24f));
	const ImVec2 CanvasStart = ImGui::GetCursorScreenPos();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 CanvasEnd(CanvasStart.x + Available.x, CanvasStart.y + Available.y);
	const ImVec2 ChannelEnd(CanvasStart.x + ChannelWidth, CanvasEnd.y);
	const ImVec2 GraphStart(ChannelEnd.x, CanvasStart.y);
	const ImVec2 GraphSize(std::max(1.0f, CanvasEnd.x - GraphStart.x), std::max(1.0f, CanvasEnd.y - GraphStart.y));

	CurveState.CanvasZoomX = std::clamp(CurveState.CanvasZoomX, 0.25f, 8.0f);
	CurveState.CanvasZoomY = std::clamp(CurveState.CanvasZoomY, 0.35f, 8.0f);

	constexpr float BasePixelsPerTime = 43.0f;
	constexpr float BasePixelsPerValue = 252.0f;
	constexpr float BaseFirstTime = -13.5f;
	constexpr float BaseValueMax = 0.5f;
	constexpr float ValueMin = -0.6f;

	float PixelsPerTime = BasePixelsPerTime * CurveState.CanvasZoomX;
	float PixelsPerValue = BasePixelsPerValue * CurveState.CanvasZoomY;
	float FirstTime = BaseFirstTime + CurveState.CanvasPanTime;
	float ValueMax = BaseValueMax + CurveState.CanvasPanValue;

	ImGui::SetCursorScreenPos(GraphStart);
	ImGui::InvisibleButton(
		"##ParticleCurveEditorGraphCanvas",
		GraphSize,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
	const bool bGraphHovered = ImGui::IsItemHovered();
	const bool bGraphDragging =
		ImGui::IsItemActive() &&
		(ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
		 ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
		 ImGui::IsMouseDragging(ImGuiMouseButton_Right));
	const ImGuiIO& IO = ImGui::GetIO();
	if (bGraphDragging)
	{
		if (CurveState.ActiveTool == EParticleCurveEditorTool::Zoom)
		{
			const ImVec2 Mouse = IO.MousePos;
			const float AnchorTime = FirstTime + (Mouse.x - GraphStart.x) / std::max(1.0f, PixelsPerTime);
			const float AnchorValue = ValueMax - (Mouse.y - GraphStart.y) / std::max(1.0f, PixelsPerValue);
			const float ZoomFactorX = std::pow(1.01f, IO.MouseDelta.x);
			const float ZoomFactorY = std::pow(1.01f, -IO.MouseDelta.y);
			CurveState.CanvasZoomX = std::clamp(CurveState.CanvasZoomX * ZoomFactorX, 0.25f, 8.0f);
			CurveState.CanvasZoomY = std::clamp(CurveState.CanvasZoomY * ZoomFactorY, 0.35f, 8.0f);
			PixelsPerTime = BasePixelsPerTime * CurveState.CanvasZoomX;
			PixelsPerValue = BasePixelsPerValue * CurveState.CanvasZoomY;
			FirstTime = AnchorTime - (Mouse.x - GraphStart.x) / std::max(1.0f, PixelsPerTime);
			ValueMax = AnchorValue + (Mouse.y - GraphStart.y) / std::max(1.0f, PixelsPerValue);
			CurveState.CanvasPanTime = FirstTime - BaseFirstTime;
			CurveState.CanvasPanValue = ValueMax - BaseValueMax;
		}
		else
		{
			CurveState.CanvasPanTime -= IO.MouseDelta.x / std::max(1.0f, PixelsPerTime);
			CurveState.CanvasPanValue += IO.MouseDelta.y / std::max(1.0f, PixelsPerValue);
			FirstTime = BaseFirstTime + CurveState.CanvasPanTime;
			ValueMax = BaseValueMax + CurveState.CanvasPanValue;
		}
	}
	if (bGraphHovered && std::fabs(IO.MouseWheel) > 0.0f)
	{
		const ImVec2 Mouse = IO.MousePos;
		const float AnchorTime = FirstTime + (Mouse.x - GraphStart.x) / std::max(1.0f, PixelsPerTime);
		const float AnchorValue = ValueMax - (Mouse.y - GraphStart.y) / std::max(1.0f, PixelsPerValue);
		const float ZoomFactor = IO.MouseWheel > 0.0f ? 1.12f : 1.0f / 1.12f;

		if (IO.KeyShift)
		{
			CurveState.CanvasZoomX = std::clamp(CurveState.CanvasZoomX * ZoomFactor, 0.25f, 8.0f);
		}
		else if (IO.KeyCtrl)
		{
			CurveState.CanvasZoomY = std::clamp(CurveState.CanvasZoomY * ZoomFactor, 0.35f, 8.0f);
		}
		else
		{
			CurveState.CanvasZoomX = std::clamp(CurveState.CanvasZoomX * ZoomFactor, 0.25f, 8.0f);
			CurveState.CanvasZoomY = std::clamp(CurveState.CanvasZoomY * ZoomFactor, 0.35f, 8.0f);
		}

		PixelsPerTime = BasePixelsPerTime * CurveState.CanvasZoomX;
		PixelsPerValue = BasePixelsPerValue * CurveState.CanvasZoomY;
		FirstTime = AnchorTime - (Mouse.x - GraphStart.x) / std::max(1.0f, PixelsPerTime);
		ValueMax = AnchorValue + (Mouse.y - GraphStart.y) / std::max(1.0f, PixelsPerValue);
		CurveState.CanvasPanTime = FirstTime - BaseFirstTime;
		CurveState.CanvasPanValue = ValueMax - BaseValueMax;
	}

	DrawList->AddRectFilled(CanvasStart, ChannelEnd, IM_COL32(33, 34, 38, 255), 0.0f);
	DrawList->AddRectFilled(CanvasStart, ImVec2(ChannelEnd.x, CanvasStart.y + 32.0f), IM_COL32(42, 44, 51, 255), 0.0f);
	DrawList->AddRect(CanvasStart, ChannelEnd, IM_COL32(75, 75, 82, 255), 0.0f);
	DrawList->AddRectFilled(GraphStart, CanvasEnd, IM_COL32(48, 48, 48, 255), 0.0f);
	DrawList->AddLine(ImVec2(ChannelEnd.x, CanvasStart.y), ImVec2(ChannelEnd.x, CanvasEnd.y), IM_COL32(78, 80, 88, 255), 1.0f);

	const float HeaderHeight = 32.0f;
	const char* ChannelName = Module ? GetObjectLabel(Module) : "Select Curve Source";
	DrawList->AddText(
		ImVec2(CanvasStart.x + 6.0f, CanvasStart.y + 7.0f),
		Module ? IM_COL32(236, 236, 238, 255) : IM_COL32(166, 170, 180, 255),
		ChannelName);

	if (Module)
	{
		const float SwatchY = CanvasStart.y + HeaderHeight - 8.0f;
		DrawList->AddRectFilled(ImVec2(CanvasStart.x + 6.0f, SwatchY), ImVec2(CanvasStart.x + 13.0f, SwatchY + 7.0f), IM_COL32(255, 0, 44, 255), 0.0f);
		DrawList->AddRect(ImVec2(CanvasStart.x + 6.0f, SwatchY), ImVec2(CanvasStart.x + 13.0f, SwatchY + 7.0f), IM_COL32(0, 0, 0, 255), 0.0f);
		DrawList->AddRectFilled(ImVec2(ChannelEnd.x - 12.0f, SwatchY), ImVec2(ChannelEnd.x - 5.0f, SwatchY + 7.0f), IM_COL32(236, 203, 34, 255), 0.0f);
		DrawList->AddRect(ImVec2(ChannelEnd.x - 12.0f, SwatchY), ImVec2(ChannelEnd.x - 5.0f, SwatchY + 7.0f), IM_COL32(0, 0, 0, 255), 0.0f);
	}

	const float GraphHeight = GraphSize.y;

	const float TimeGridStep = ChooseParticleCurveGridStep(PixelsPerTime, 56.0f);
	const float ValueGridStep = ChooseParticleCurveGridStep(PixelsPerValue, 30.0f);
	const float FirstGridTime = std::ceil(FirstTime / TimeGridStep) * TimeGridStep;
	for (float Time = FirstGridTime; ; Time += TimeGridStep)
	{
		const float X = GraphStart.x + (Time - FirstTime) * PixelsPerTime;
		if (X > CanvasEnd.x + 0.5f)
		{
			break;
		}
		DrawList->AddLine(ImVec2(X, GraphStart.y), ImVec2(X, CanvasEnd.y), IM_COL32(158, 158, 158, 255), 1.0f);
		if (GraphHeight > 34.0f)
		{
			char Label[32];
			snprintf(Label, sizeof(Label), "%.2f", Time);
			DrawList->AddText(ImVec2(X + 3.0f, CanvasEnd.y - 18.0f), IM_COL32(224, 224, 224, 255), Label);
		}
	}

	const float VisibleValueMin = ValueMax - GraphSize.y / std::max(1.0f, PixelsPerValue);
	const float FirstGridValue = std::floor(ValueMax / ValueGridStep) * ValueGridStep;
	for (float Value = FirstGridValue; Value >= std::min(ValueMin, VisibleValueMin) - 0.0001f; Value -= ValueGridStep)
	{
		const float Y = GraphStart.y + (ValueMax - Value) * PixelsPerValue;
		if (Y > CanvasEnd.y + 0.5f)
		{
			break;
		}
		DrawList->AddLine(ImVec2(GraphStart.x, Y), ImVec2(CanvasEnd.x, Y), IM_COL32(158, 158, 158, 255), 1.0f);
		char Label[32];
		snprintf(Label, sizeof(Label), "%.2f", Value);
		DrawList->AddText(ImVec2(GraphStart.x + 3.0f, Y - 8.0f), IM_COL32(232, 232, 232, 255), Label);
	}

	const float ZeroY = GraphStart.y + (ValueMax - 0.0f) * PixelsPerValue;
	if (ZeroY >= GraphStart.y && ZeroY <= CanvasEnd.y)
	{
		DrawList->AddLine(ImVec2(GraphStart.x, ZeroY), ImVec2(CanvasEnd.x, ZeroY), IM_COL32(255, 0, 178, 255), 1.0f);
	}

	DrawList->AddRect(CanvasStart, CanvasEnd, IM_COL32(82, 82, 82, 255), 0.0f);
	if (bGraphHovered)
	{
		ImGui::SetMouseCursor(
			CurveState.ActiveTool == EParticleCurveEditorTool::Zoom
				? ImGuiMouseCursor_ResizeAll
				: (bGraphDragging ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Hand));
	}
}

