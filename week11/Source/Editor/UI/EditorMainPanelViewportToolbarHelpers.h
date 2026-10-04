#pragma once

#include "Editor/EditorUtils.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Math/Utils.h"
#include "Render/Common/ViewTypes.h"

#include <algorithm>

class FEditorMainPanelViewportToolbarHelpers
{
public:
	static constexpr float MaxCameraSpeedMultiplier = 20.0f;

	static const char* GetViewportTypeName(EEditorViewportType Type)
	{
		switch (Type)
		{
		case EVT_Perspective: return "Perspective";
		case EVT_OrthoTop: return "Top";
		case EVT_OrthoBottom: return "Bottom";
		case EVT_OrthoFront: return "Front";
		case EVT_OrthoBack: return "Back";
		case EVT_OrthoLeft: return "Left";
		case EVT_OrthoRight: return "Right";
		default: return "Viewport";
		}
	}

	static const char* GetViewModeName(EViewMode Mode)
	{
		switch (Mode)
		{
		case EViewMode::Lit_Gouraud: return "Lit (Gouraud)";
		case EViewMode::Lit_Lambert: return "Lit (Lambert)";
		case EViewMode::Lit_BlinnPhong: return "Lit (Blinn-Phong)";
		case EViewMode::Unlit: return "Unlit";
		case EViewMode::Heatmap: return "Heatmap";
		case EViewMode::Wireframe: return "Wireframe";
		case EViewMode::Depth: return "Depth";
		case EViewMode::Normal: return "Normal";
		case EViewMode::IdBuffer: return "ID Buffer";
		default: return "Lit";
		}
	}

	static const char* GetLightCullModeName(ELightCullMode Mode)
	{
		switch (Mode)
		{
		case ELightCullMode::Clustered: return "Clustered";
		case ELightCullMode::Tiled: return "Tiled";
		case ELightCullMode::None: return "None (All Lights)";
		default: return "Clustered";
		}
	}

	static const char* GetViewportSlotName(int32 Index)
	{
		switch (Index)
		{
		case 0: return "Viewport 0";
		case 1: return "Viewport 1";
		case 2: return "Viewport 2";
		case 3: return "Viewport 3";
		default: return "Viewport";
		}
	}

	static const char* GetViewportLayoutLabel(EEditorViewportLayoutMode Mode)
	{
		switch (Mode)
		{
		case EEditorViewportLayoutMode::OnePane: return "One Pane";
		case EEditorViewportLayoutMode::TwoPanesHoriz: return "Two Panes Horiz";
		case EEditorViewportLayoutMode::TwoPanesVert: return "Two Panes Vert";
		case EEditorViewportLayoutMode::ThreePanesLeft: return "Three Panes Left";
		case EEditorViewportLayoutMode::ThreePanesRight: return "Three Panes Right";
		case EEditorViewportLayoutMode::ThreePanesTop: return "Three Panes Top";
		case EEditorViewportLayoutMode::ThreePanesBottom: return "Three Panes Bottom";
		case EEditorViewportLayoutMode::FourPanes2x2: return "Four Panes 2x2";
		case EEditorViewportLayoutMode::FourPanesLeft: return "Four Panes Left";
		case EEditorViewportLayoutMode::FourPanesRight: return "Four Panes Right";
		case EEditorViewportLayoutMode::FourPanesTop: return "Four Panes Top";
		case EEditorViewportLayoutMode::FourPanesBottom: return "Four Panes Bottom";
		default: return "Layout";
		}
	}

	static float GetCameraBaseSpeed()
	{
		return std::max(0.1f, FEditorSettings::Get().CameraSpeed);
	}

	static float GetCameraSpeedMultiplier(FEditorViewportClient* Client)
	{
		if (!Client)
		{
			return 1.0f;
		}

		return MathUtil::Clamp(
			Client->GetMoveSpeed() / GetCameraBaseSpeed(),
			0.01f,
			MaxCameraSpeedMultiplier);
	}

	static void SetCameraSpeedMultiplier(FEditorViewportClient* Client, float Multiplier)
	{
		if (!Client)
		{
			return;
		}

		Client->SetMoveSpeed(MathUtil::Clamp(
			GetCameraBaseSpeed() * Multiplier,
			0.1f,
			GetCameraBaseSpeed() * MaxCameraSpeedMultiplier));
	}
};
