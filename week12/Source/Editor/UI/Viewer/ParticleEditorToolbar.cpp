#include "ParticleEditorInternal.h"

using namespace ParticleEditorInternal;

void FParticleEditorViewerWidget::RenderToolbar(FParticleEditorViewer* Viewer)
{
	LoadCascadeToolbarIcons();

	constexpr ImGuiWindowFlags ToolbarFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	ImGui::BeginChild("ParticleToolbar", ImVec2(0.0f, 34.0f), false, ToolbarFlags);
	ImGui::SetCursorPos(ImVec2(8.0f, 4.0f));

	const ImVec2 IconSize(26.0f, 26.0f);
	const float OverflowButtonWidth = IconSize.y;
	const float VisibleRight = ImGui::GetWindowContentRegionMax().x - OverflowButtonWidth - 8.0f;
	const int32 LODCount = GetSelectedEmitterLODCount(Viewer);
	const int32 SelectedLODIndex = Viewer ? Viewer->GetSelectedLODIndex() : 0;
	const bool bCanDeleteLOD = LODCount > 1 && SelectedLODIndex > 0;

	bool bHasOverflow = false;
	bool bOpenBackgroundPopup = false;

	auto DrawBackgroundColorPopup = [&]()
	{
		if (ImGui::BeginPopup("ParticleBackgroundColorPopup"))
		{
			FColor Background = Viewer->GetBackgroundColor();
			float Color[4] = { Background.R, Background.G, Background.B, Background.A };
			if (ImGui::ColorPicker4("##ParticleBackgroundColor", Color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
			{
				Viewer->SetBackgroundColor(FColor(Color[0], Color[1], Color[2], Color[3]));
			}
			ImGui::EndPopup();
		}
	};

	auto EstimateButtonWidth = [IconSize](const char* Label)
	{
		return IconSize.x + (Label ? 14.0f + ImGui::CalcTextSize(Label).x : 0.0f);
	};

	auto CanFit = [VisibleRight](float Width)
	{
		return ImGui::GetCursorPosX() + Width <= VisibleRight;
	};

	enum class EToolbarItemType
	{
		Button,
		Separator,
		Background,
		CurrentLOD
	};

	struct FToolbarItem
	{
		EToolbarItemType Type;
		const char* Id;
		ID3D11ShaderResourceView* Icon;
		const char* Tooltip;
		bool bEnabled;
		const char* Label;
		bool bIsHidden;
	};

	FToolbarItem Items[] = {
		{ EToolbarItemType::Button, "Save", ToolbarIcons.SaveIcon.Get(), "Save As", Viewer->GetParticleSystem() != nullptr, nullptr, false },
		{ EToolbarItemType::Button, "Find", ToolbarIcons.FindIcon.Get(), "Find in Content Browser", true, nullptr, false },
		{ EToolbarItemType::Separator, nullptr, nullptr, nullptr, false, nullptr, false },
		{ EToolbarItemType::Button, "RestartSim", ToolbarIcons.RestartSimIcon.Get(), "Restart Simulation", true, "Restart Sim", false },
		{ EToolbarItemType::Button, "RestartLevel", ToolbarIcons.RestartLevelIcon.Get(), "Restart Level", true, "Restart Level", false },
		{ EToolbarItemType::Separator, nullptr, nullptr, nullptr, false, nullptr, false },
		{ EToolbarItemType::Button, "Undo", ToolbarIcons.UndoIcon.Get(), "Undo", Viewer->CanUndo(), "Undo", false },
		{ EToolbarItemType::Button, "Redo", ToolbarIcons.RedoIcon.Get(), "Redo", Viewer->CanRedo(), "Redo", false },
		{ EToolbarItemType::Separator, nullptr, nullptr, nullptr, false, nullptr, false },
		{ EToolbarItemType::Button, "Thumbnail", ToolbarIcons.ThumbnailIcon.Get(), "Thumbnail", false, "Thumbnail", false },
		{ EToolbarItemType::Separator, nullptr, nullptr, nullptr, false, nullptr, false },
		{ EToolbarItemType::Button, "Bounds", ToolbarIcons.BoundsIcon.Get(), Viewer->IsShowBounds() ? "Hide Bounds" : "Show Bounds", true, "Bounds", false },
		{ EToolbarItemType::Button, "Axis", ToolbarIcons.AxisIcon.Get(), Viewer->IsShowGrid() && Viewer->IsShowAxis() ? "Hide Axis/Grid" : "Show Axis/Grid", true, "Axis", false },
		{ EToolbarItemType::Background, "Background", ToolbarIcons.BackgroundIcon.Get(), "Background", true, "Background", false },
		{ EToolbarItemType::Separator, nullptr, nullptr, nullptr, false, nullptr, false },
		{ EToolbarItemType::Button, "RegenLOD", ToolbarIcons.RegenLODIcon.Get(), "Regenerate LOD", false, "Regen LOD", false },
		{ EToolbarItemType::Button, "LowestLOD", ToolbarIcons.LowestLODIcon.Get(), "Lowest LOD", true, "Lowest LOD", false },
		{ EToolbarItemType::Button, "LowerLOD", ToolbarIcons.LowerLODIcon.Get(), "Lower LOD", true, "Lower LOD", false },
		{ EToolbarItemType::Button, "AddLOD", ToolbarIcons.AddLODIcon.Get(), "Add LOD", true, "Add LOD", false },
		{ EToolbarItemType::Button, "DeleteLOD", ToolbarIcons.DeleteLODIcon.Get(), "Delete LOD", bCanDeleteLOD, "Delete LOD", false },
		{ EToolbarItemType::CurrentLOD, nullptr, nullptr, nullptr, false, nullptr, false },
		{ EToolbarItemType::Button, "UpperLOD", ToolbarIcons.UpperLODIcon.Get(), "Upper LOD", true, "Upper LOD", false },
		{ EToolbarItemType::Button, "HighestLOD", ToolbarIcons.HighestLODIcon.Get(), "Highest LOD", true, "Highest LOD", false }
	};

	auto ExecuteAction = [&](const char* Id)
	{
		if (strcmp(Id, "Save") == 0)
		{
			FString SavePath;
			if (OpenParticleSaveFileDialog(ResolveSaveDialogOwnerWindow(EditorEngine), Viewer, SavePath))
				Viewer->SaveAs(SavePath);
		}
		else if (strcmp(Id, "Find") == 0)
		{
			Viewer->FindInContentBrowser();
		}
		else if (strcmp(Id, "RestartSim") == 0)
		{
			Viewer->RestartSimulation();
		}
		else if (strcmp(Id, "RestartLevel") == 0)
		{
			Viewer->RestartLevel();
		}
		else if (strcmp(Id, "Undo") == 0)
		{
			Viewer->Undo();
		}
		else if (strcmp(Id, "Redo") == 0)
		{
			Viewer->Redo();
		}
		else if (strcmp(Id, "Bounds") == 0)
		{
			Viewer->SetShowBounds(!Viewer->IsShowBounds());
		}
		else if (strcmp(Id, "Axis") == 0)
		{
			const bool bNextVisible = !(Viewer->IsShowGrid() && Viewer->IsShowAxis());
			Viewer->SetShowGrid(bNextVisible);
			Viewer->SetShowAxis(bNextVisible);
		}
		else if (strcmp(Id, "LowestLOD") == 0)
		{
			Viewer->SetLowestLOD();
		}
		else if (strcmp(Id, "LowerLOD") == 0)
		{
			Viewer->SelectLowerLOD();
		}
		else if (strcmp(Id, "AddLOD") == 0)
		{
			Viewer->AddLOD();
		}
		else if (strcmp(Id, "DeleteLOD") == 0)
		{
			Viewer->RemoveLOD(Viewer->GetSelectedLODIndex());
		}
		else if (strcmp(Id, "UpperLOD") == 0)
		{
			Viewer->SelectUpperLOD();
		}
		else if (strcmp(Id, "HighestLOD") == 0)
		{
			Viewer->SetHighestLOD();
		}
	};

	for (auto& Item : Items)
	{
		if (Item.Type == EToolbarItemType::Separator)
		{
			if (!CanFit(14.0f))
			{
				bHasOverflow = true;
				Item.bIsHidden = true;
				continue;
			}
			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine();
		}
		else if (Item.Type == EToolbarItemType::CurrentLOD)
		{
			const float Width = IconSize.x + 6.0f + ImGui::CalcTextSize("LOD:").x + 4.0f + 36.0f + 8.0f;
			if (!CanFit(Width))
			{
				bHasOverflow = true;
				Item.bIsHidden = true;
				continue;
			}
			DrawCurrentLODToolbarInput(Viewer, ToolbarIcons.GenericLODIcon.Get(), IconSize, ImVec2(Width, IconSize.y));
			ImGui::SameLine();
		}
		else
		{
			const float Width = EstimateButtonWidth(Item.Label);
			if (!CanFit(Width))
			{
				bHasOverflow = true;
				Item.bIsHidden = true;
				continue;
			}
			if (DrawCascadeToolbarIconButton(Item.Id, Item.Icon, Item.Tooltip, IconSize, Item.bEnabled, Item.Label))
			{
				if (Item.Type == EToolbarItemType::Background)
					bOpenBackgroundPopup = true;
				else
					ExecuteAction(Item.Id);
			}
			ImGui::SameLine();
		}
	}

	if (bHasOverflow)
	{
		ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - OverflowButtonWidth));
		if (ImGui::InvisibleButton("##ParticleToolbarOverflow", ImVec2(OverflowButtonWidth, OverflowButtonWidth)))
		{
			ImGui::OpenPopup("ParticleToolbarOverflowPopup");
		}
		const ImVec2 OverflowMin = ImGui::GetItemRectMin();
		const ImVec2 OverflowMax = ImGui::GetItemRectMax();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 OverflowBg = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImVec4(0.14f, 0.16f, 0.19f, 1.0f) : ImVec4(0.09f, 0.10f, 0.12f, 1.0f));
		const ImU32 OverflowFg = ImGui::GetColorU32(ImVec4(0.94f, 0.95f, 0.98f, 1.0f));
		const ImU32 OverflowBorder = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImVec4(0.48f, 0.52f, 0.60f, 1.0f) : ImVec4(0.30f, 0.33f, 0.39f, 1.0f));
		DrawList->AddRectFilled(OverflowMin, OverflowMax, OverflowBg, 3.0f);
		DrawList->AddRect(OverflowMin, OverflowMax, OverflowBorder, 3.0f, 0, 1.0f);
		for (int32 Line = 0; Line < 3; ++Line)
		{
			const float Y = OverflowMin.y + 7.0f + Line * 5.0f;
			DrawList->AddLine(ImVec2(OverflowMin.x + 7.0f, Y), ImVec2(OverflowMax.x - 7.0f, Y), OverflowFg, 1.4f);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("More particle tools");
		}
	}

	constexpr ImGuiWindowFlags OverflowPopupFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (ImGui::BeginPopup("ParticleToolbarOverflowPopup", OverflowPopupFlags))
	{
		const ImVec2 PopupIconSize(26.0f, 26.0f);
		bool bNeedsSeparator = false;

		for (auto& Item : Items)
		{
			if (!Item.bIsHidden)
				continue;

			if (Item.Type == EToolbarItemType::Separator)
			{
				if (bNeedsSeparator)
				{
					ImGui::Separator();
					bNeedsSeparator = false;
				}
			}
			else if (Item.Type == EToolbarItemType::CurrentLOD)
			{
				DrawCurrentLODToolbarInput(Viewer, ToolbarIcons.GenericLODIcon.Get(), ImVec2(22.0f, 22.0f), ImVec2(94.0f, 24.0f));
				bNeedsSeparator = true;
			}
			else
			{
				const char* LabelToUse = Item.Label ? Item.Label : Item.Id;
				if (DrawCascadeToolbarIconButton(Item.Id, Item.Icon, Item.Tooltip, PopupIconSize, Item.bEnabled, LabelToUse))
				{
					if (Item.Type == EToolbarItemType::Background)
						bOpenBackgroundPopup = true;
					else
						ExecuteAction(Item.Id);
					ImGui::CloseCurrentPopup();
				}
				bNeedsSeparator = true;
			}
		}
		ImGui::EndPopup();
	}

	if (bOpenBackgroundPopup)
	{
		ImGui::OpenPopup("ParticleBackgroundColorPopup");
	}
	DrawBackgroundColorPopup();

	ImGui::EndChild();
}

void FParticleEditorViewerWidget::RenderViewportOptions(FParticleEditorViewer* Viewer)
{
	if (DrawRoundedToolbarButton("ParticleViewportView", "View", "View", ImVec2(50.0f, 26.0f)))
	{
		ImGui::OpenPopup("##ParticleViewportViewPopup");
	}
	if (ImGui::BeginPopup("##ParticleViewportViewPopup"))
	{
		if (ImGui::BeginMenu("View Overlays"))
		{
			bool bGrid = Viewer->IsShowGrid();
			if (ImGui::MenuItem("Grid", nullptr, bGrid))
			{
				Viewer->SetShowGrid(!bGrid);
			}
			bool bBounds = Viewer->IsShowBounds();
			if (ImGui::MenuItem("Particle System Bounds", nullptr, bBounds))
			{
				Viewer->SetShowBounds(!bBounds);
			}
			ImGui::MenuItem("Particle Count", nullptr, false, false);
			ImGui::MenuItem("Distance", nullptr, false, false);
			ImGui::MenuItem("Elapsed Time", nullptr, false, false);
			ImGui::MenuItem("Memory", nullptr, false, false);
			ImGui::MenuItem("Event Count", nullptr, false, false);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View Modes"))
		{
			DrawViewModeMenuItems(Viewer);
			ImGui::MenuItem("Shader Complexity", nullptr, false, false);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Detail Modes"))
		{
			ImGui::MenuItem("Low", nullptr, false, false);
			ImGui::MenuItem("Medium", nullptr, true, false);
			ImGui::MenuItem("High", nullptr, false, false);
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
	ImGui::SameLine(0.0f, 6.0f);
	if (DrawRoundedToolbarButton("ParticleViewportTime", "Time", "Time", ImVec2(52.0f, 26.0f)))
	{
		ImGui::OpenPopup("##ParticleViewportTimePopup");
	}
	if (ImGui::BeginPopup("##ParticleViewportTimePopup"))
	{
		RenderTimeControls(Viewer);
		ImGui::EndPopup();
	}
}

void FParticleEditorViewerWidget::RenderTimeControls(FParticleEditorViewer* Viewer)
{
	bool bPlaying = Viewer->IsPlaying();
	if (ImGui::MenuItem(bPlaying ? "Pause" : "Play", nullptr, bPlaying))
	{
		Viewer->SetPlaying(!bPlaying);
	}
	bool bRealtime = Viewer->IsRealtime();
	if (ImGui::MenuItem("Realtime", nullptr, bRealtime))
	{
		Viewer->SetRealtime(!bRealtime);
	}
	bool bLooping = Viewer->IsLooping();
	if (ImGui::MenuItem("Loop", nullptr, bLooping))
	{
		Viewer->SetLooping(!bLooping);
	}
}

void FParticleEditorViewerWidget::LoadCascadeToolbarIcons()
{
	if (ToolbarIcons.bLoadAttempted)
	{
		return;
	}

	ToolbarIcons.bLoadAttempted = true;
	if (!EditorEngine)
	{
		return;
	}

	ID3D11Device* Device = EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!Device)
	{
		return;
	}

	const std::wstring IconDir = FEditorResourcePaths::IconsAbsoluteDir();
	const std::wstring ToolIconDir = FEditorResourcePaths::ToolIconsAbsoluteDir();
	auto LoadIcon = [Device](const std::wstring& BaseDir, const wchar_t* FileName, TComPtr<ID3D11ShaderResourceView>& OutIcon)
	{
		const std::wstring IconPath = BaseDir + FileName;
		DirectX::CreateWICTextureFromFile(Device, IconPath.c_str(), nullptr, OutIcon.GetAddressOf());
	};

	LoadIcon(ToolIconDir, L"Save.png", ToolbarIcons.SaveIcon);
	LoadIcon(ToolIconDir, L"Browser.png", ToolbarIcons.FindIcon);
	LoadIcon(IconDir, L"Cascade_RestartSim_40x.png", ToolbarIcons.RestartSimIcon);
	LoadIcon(IconDir, L"Cascade_Restart40x.png", ToolbarIcons.RestartLevelIcon);
	LoadIcon(ToolIconDir, L"PlayControlsToPrevious.png", ToolbarIcons.UndoIcon);
	LoadIcon(ToolIconDir, L"PlayControlsToNext.png", ToolbarIcons.RedoIcon);
	LoadIcon(IconDir, L"Cascade_Bounds_40x.png", ToolbarIcons.BoundsIcon);
	LoadIcon(IconDir, L"Cascade_Axis_40x.png", ToolbarIcons.AxisIcon);
	LoadIcon(IconDir, L"Cascade_Color_40x.png", ToolbarIcons.BackgroundIcon);
	LoadIcon(IconDir, L"Cascade_Thumbnail_40x.png", ToolbarIcons.ThumbnailIcon);
	LoadIcon(IconDir, L"Cascade_RegenLOD1_512x.png", ToolbarIcons.RegenLODIcon);
	LoadIcon(IconDir, L"Cascade_LowestLOD_512x.png", ToolbarIcons.LowestLODIcon);
	LoadIcon(IconDir, L"Cascade_HighestLOD_512x.png", ToolbarIcons.HighestLODIcon);
	LoadIcon(IconDir, L"Cascade_LowerLOD_512x.png", ToolbarIcons.LowerLODIcon);
	LoadIcon(IconDir, L"Cascade_HigherLOD_512x.png", ToolbarIcons.UpperLODIcon);
	LoadIcon(IconDir, L"Cascade_AddLOD1_512x.png", ToolbarIcons.AddLODIcon);
	LoadIcon(IconDir, L"Cascade_DeleteLOD_512x.png", ToolbarIcons.DeleteLODIcon);
	LoadIcon(IconDir, L"Cascade_GenericLOD_40x.png", ToolbarIcons.GenericLODIcon);
	LoadIcon(IconDir, L"CurveEditor_Horizontal_40x.png", ToolbarIcons.CurveHorizontalIcon);
	LoadIcon(IconDir, L"CurveEditor_Vertical_40x.png", ToolbarIcons.CurveVerticalIcon);
	LoadIcon(IconDir, L"CurveEditor_ZoomToFit_40x.png", ToolbarIcons.CurveFitIcon);
	LoadIcon(IconDir, L"CurveEditor_Pan_40x.png", ToolbarIcons.CurvePanIcon);
	LoadIcon(IconDir, L"CurveEditor_Zoom_40x.png", ToolbarIcons.CurveZoomIcon);
	LoadIcon(IconDir, L"CurveEditor_Auto_40x.png", ToolbarIcons.CurveAutoIcon);
	LoadIcon(IconDir, L"CurveEditor_AutoClamped_40x.png", ToolbarIcons.CurveAutoClampedIcon);
	LoadIcon(IconDir, L"CurveEditor_User_40x.png", ToolbarIcons.CurveUserIcon);
	LoadIcon(IconDir, L"CurveEditor_Break_40x.png", ToolbarIcons.CurveBreakIcon);
	LoadIcon(IconDir, L"CurveEditor_Linear_40x.png", ToolbarIcons.CurveLinearIcon);
	LoadIcon(IconDir, L"CurveEditor_Constant_40x.png", ToolbarIcons.CurveConstantIcon);
	LoadIcon(IconDir, L"CurveEditor_Flatten_40x.png", ToolbarIcons.CurveFlattenIcon);
	LoadIcon(IconDir, L"CurveEditor_Straighten_40x.png", ToolbarIcons.CurveStraightenIcon);
	LoadIcon(IconDir, L"CurveEditor_ShowAll_40x.png", ToolbarIcons.CurveShowAllIcon);
	LoadIcon(IconDir, L"CurveEditor_Create_40x.png", ToolbarIcons.CurveCreateIcon);
	LoadIcon(IconDir, L"CurveEditor_DeleteTab_40x.png", ToolbarIcons.CurveDeleteIcon);
}

bool FParticleEditorViewerWidget::DrawCascadeToolbarIconButton(
	const char* Id,
	ID3D11ShaderResourceView* Icon,
	const char* Tooltip,
	const ImVec2& Size,
	bool bEnabled,
	const char* Label)
{
	ImGui::PushID(Id);
	if (!bEnabled)
	{
		ImGui::BeginDisabled();
	}

	const ImVec2 LabelSize = Label ? ImGui::CalcTextSize(Label) : ImVec2(0.0f, 0.0f);
	const float LabelGap = Label ? 6.0f : 0.0f;
	const ImVec2 ButtonSize(
		Size.x + LabelGap + LabelSize.x + (Label ? 8.0f : 0.0f),
		Size.y);
	const bool bClicked = ImGui::InvisibleButton("##CascadeToolbarIcon", ButtonSize);
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 BgColor = ImGui::GetColorU32(
		bActive ? ImVec4(0.18f, 0.20f, 0.23f, 1.0f) : bHovered ? ImVec4(0.14f, 0.16f, 0.19f, 1.0f)
															   : ImVec4(0.09f, 0.10f, 0.12f, 1.0f));
	DrawList->AddRectFilled(Min, Max, BgColor, 3.0f);

	if (Icon)
	{
		const float Padding = std::max(4.0f, Size.x * 0.16f);
		DrawList->AddImage(
			reinterpret_cast<ImTextureID>(Icon),
			ImVec2(Min.x + Padding, Min.y + Padding),
			ImVec2(Min.x + Size.x - Padding, Min.y + Size.y - Padding));
	}
	else if (Id && Id[0] != '\0')
	{
		const char Fallback[2] = { Id[0], '\0' };
		const ImVec2 TextSize = ImGui::CalcTextSize(Fallback);
		DrawList->AddText(
			ImVec2(Min.x + (Size.x - TextSize.x) * 0.5f, Min.y + (Size.y - TextSize.y) * 0.5f),
			ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.84f, 1.0f)),
			Fallback);
	}
	if (Label)
	{
		DrawList->AddText(
			ImVec2(Min.x + Size.x + LabelGap, Min.y + (ButtonSize.y - LabelSize.y) * 0.5f),
			ImGui::GetColorU32(ImVec4(0.94f, 0.95f, 0.98f, bEnabled ? 1.0f : 0.45f)),
			Label);
	}

	if (bHovered && Tooltip)
	{
		ImGui::SetTooltip("%s", Tooltip);
	}

	if (!bEnabled)
	{
		ImGui::EndDisabled();
	}
	ImGui::PopID();
	return bEnabled && bClicked;
}

