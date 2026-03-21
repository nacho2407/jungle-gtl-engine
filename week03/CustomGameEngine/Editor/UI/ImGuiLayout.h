#pragma once
#include "EngineCore/EngineTypes.h"
#include "ImGui/imgui.h"

struct ImGuiWindowLayout
{
	ImVec2 PosXY;
	ImVec2 SizeWH;
};

struct ImGuiLayout {
	ImGuiWindowLayout ControlPanel =	{ {10, 10},  { 535, 460 } };
	ImGuiWindowLayout Console =			{ {10, 735}, { 710, 300} };
	ImGuiWindowLayout PropertyPanel =	{ {1600,10}, { 300, 300} };
	ImGuiWindowLayout StatPanel =		{ {10, 573}, { 135, 80} };
	ImGuiWindowLayout Toolbar =			{ {10, 475}, { 180, 90 } };
	ImGuiWindowLayout SceneMgrPanel =	{ {1600, 320}, { 300, 700 } };
};
