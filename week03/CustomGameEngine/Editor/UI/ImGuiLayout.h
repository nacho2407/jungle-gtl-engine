#pragma once
#include "EngineCore/EngineTypes.h"
#include "ImGui/imgui.h"

struct ImGuiWindowLayout
{
	ImVec2 PosXY;
	ImVec2 SizeWH;
};
/**
 * docking 레이아웃 이전 ImGUI의 패널 위치 및 크기를 조절하기 위한 상태 구조체
 * 현재는 제거 가능한 상태입니다.
 */
struct ImGuiLayout {
	ImGuiWindowLayout ControlPanel =	{ {10, 10},  { 535, 600 } };
	ImGuiWindowLayout Console =			{ {10, 735}, { 710, 300} };
	ImGuiWindowLayout PropertyPanel =	{ {1600,10}, { 300, 300} };
	ImGuiWindowLayout StatPanel =		{ {10, 640}, { 135, 80} };
	ImGuiWindowLayout Toolbar =			{ {170, 640}, { 180, 90 } };
	ImGuiWindowLayout SceneMgrPanel =	{ {1600, 315}, { 300, 700 } };
};
