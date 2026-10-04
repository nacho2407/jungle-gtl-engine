#include "Editor/UI/EditorMainPanel.h"

#include "Editor/UI/EditorChromeConstants.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace
{
constexpr float WindowButtonWidth = 48.0f;
constexpr float LogoSize = 28.0f;
constexpr float LogoPaddingX = 4.0f;
constexpr float MenuLogoGap = 8.0f;

std::string WideToUtf8(const wchar_t* Text)
{
	if (!Text || Text[0] == L'\0')
	{
		return {};
	}

	const int RequiredSize = WideCharToMultiByte(CP_UTF8, 0, Text, -1, nullptr, 0, nullptr, nullptr);
	if (RequiredSize <= 1)
	{
		return {};
	}

	std::string Result(static_cast<size_t>(RequiredSize), '\0');
	WideCharToMultiByte(CP_UTF8, 0, Text, -1, Result.data(), RequiredSize, nullptr, nullptr);
	Result.pop_back();
	return Result;
}

FWindowHitTestRect MakeHitTestRect(const ImVec2& Min, const ImVec2& Max, const ImVec2& ViewportPos)
{
	FWindowHitTestRect Rect;
	Rect.Left = static_cast<int32>(Min.x - ViewportPos.x);
	Rect.Top = static_cast<int32>(Min.y - ViewportPos.y);
	Rect.Right = static_cast<int32>(Max.x - ViewportPos.x);
	Rect.Bottom = static_cast<int32>(Max.y - ViewportPos.y);
	return Rect;
}

void AddItemHitRect(std::vector<FWindowHitTestRect>& Rects, const ImVec2& ViewportPos)
{
	Rects.push_back(MakeHitTestRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ViewportPos));
}

bool DrawWindowButton(
	const char* Id,
	const char* Tooltip,
	const ImVec2& Size,
	const ImVec4& HoverColor,
	const ImVec4& ActiveColor,
	const std::function<void(ImDrawList*, const ImVec2&, const ImVec2&, ImU32)>& DrawIcon)
{
	ImGui::PushID(Id);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HoverColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ActiveColor);

	const bool bClicked = ImGui::InvisibleButton("##Button", Size);
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const ImU32 BgColor = ImGui::GetColorU32(
		bActive ? ActiveColor : (bHovered ? HoverColor : ImVec4(0.0f, 0.0f, 0.0f, 0.0f)));
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(Min, Max, BgColor, 0.0f);
	DrawIcon(DrawList, Min, Max, ImGui::GetColorU32(ImVec4(0.82f, 0.85f, 0.90f, 1.0f)));

	if (bHovered && Tooltip)
	{
		ImGui::SetTooltip("%s", Tooltip);
	}

	ImGui::PopStyleColor(3);
	ImGui::PopID();
	return bClicked;
}
}

void FEditorMainPanel::RenderApplicationChrome(float DeltaTime)
{
	(void)DeltaTime;

	Widgets.ToolbarWidget.ProcessShortcuts();

	ImGuiViewport* Viewport = ImGui::GetMainViewport();
	if (!Viewport)
	{
		return;
	}

	std::vector<FWindowHitTestRect> InteractiveRects;
	InteractiveRects.reserve(6);

	constexpr float TitleBarHeight = FEditorChromeMetrics::ApplicationTitleBarHeight;
	const ImVec2 WindowPos = Viewport->Pos;
	const ImVec2 WindowSize(Viewport->Size.x, TitleBarHeight);

	ImGui::SetNextWindowPos(WindowPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(WindowSize, ImGuiCond_Always);
	ImGui::SetNextWindowViewport(Viewport->ID);

	constexpr ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(13.0f, 8.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(9.0f, 4.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.060f, 0.072f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.055f, 0.060f, 0.072f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.20f, 0.25f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.15f, 0.17f, 0.22f, 1.0f));

	if (ImGui::Begin("##JSEditorApplicationChrome", nullptr, Flags))
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const float ButtonStartX = std::max(0.0f, WindowSize.x - WindowButtonWidth * 3.0f);

		if (IconResources.HomeIcon)
		{
			DrawList->AddImage(
				reinterpret_cast<ImTextureID>(IconResources.HomeIcon),
				ImVec2(WindowPos.x + LogoPaddingX, WindowPos.y + (TitleBarHeight - LogoSize) * 0.5f),
				ImVec2(WindowPos.x + LogoPaddingX + LogoSize, WindowPos.y + (TitleBarHeight + LogoSize) * 0.5f));
		}
		else
		{
			const ImVec2 Center(WindowPos.x + LogoPaddingX + LogoSize * 0.5f, WindowPos.y + TitleBarHeight * 0.5f);
			DrawList->AddCircleFilled(Center, LogoSize * 0.48f, ImGui::GetColorU32(ImVec4(0.30f, 0.55f, 1.0f, 1.0f)), 24);
		}

		float MenuEndX = LogoPaddingX + LogoSize + MenuLogoGap;
		const ImVec2 ButtonSize(WindowButtonWidth, TitleBarHeight);
		if (ImGui::BeginMenuBar())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 8.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));

			ImGui::SetCursorPosX(MenuEndX);
			Widgets.ToolbarWidget.RenderMenuContents();
			MenuEndX = std::min(ButtonStartX, ImGui::GetCursorScreenPos().x - WindowPos.x + 8.0f);

			ImGui::SetCursorPos(ImVec2(ButtonStartX, 0.0f));
			if (DrawWindowButton(
				"Minimize",
				"Minimize",
				ButtonSize,
				ImVec4(0.14f, 0.16f, 0.20f, 1.0f),
				ImVec4(0.18f, 0.20f, 0.25f, 1.0f),
				[](ImDrawList* InDrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
				{
					const float Y = (Min.y + Max.y) * 0.5f + 4.0f;
					InDrawList->AddLine(ImVec2(Min.x + 17.0f, Y), ImVec2(Max.x - 17.0f, Y), Color, 1.6f);
				}))
			{
				if (Window)
				{
					Window->Minimize();
				}
			}
			AddItemHitRect(InteractiveRects, WindowPos);

			ImGui::SameLine(0.0f, 0.0f);
			if (DrawWindowButton(
				"Maximize",
				Window && Window->IsWindowMaximized() ? "Restore" : "Maximize",
				ButtonSize,
				ImVec4(0.14f, 0.16f, 0.20f, 1.0f),
				ImVec4(0.18f, 0.20f, 0.25f, 1.0f),
				[this](ImDrawList* InDrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
				{
					const bool bMaximized = Window && Window->IsWindowMaximized();
					const ImVec2 A(Min.x + 17.0f, Min.y + 12.0f);
					const ImVec2 B(Max.x - 17.0f, Max.y - 12.0f);
					if (bMaximized)
					{
						InDrawList->AddRect(ImVec2(A.x + 3.0f, A.y), ImVec2(B.x + 3.0f, B.y - 3.0f), Color, 0.0f, 0, 1.4f);
						InDrawList->AddRect(ImVec2(A.x, A.y + 3.0f), ImVec2(B.x, B.y), Color, 0.0f, 0, 1.4f);
					}
					else
					{
						InDrawList->AddRect(A, B, Color, 0.0f, 0, 1.4f);
					}
				}))
			{
				if (Window)
				{
					Window->ToggleMaximize();
				}
			}
			AddItemHitRect(InteractiveRects, WindowPos);

			ImGui::SameLine(0.0f, 0.0f);
			if (DrawWindowButton(
				"Close",
				"Close",
				ButtonSize,
				ImVec4(0.62f, 0.18f, 0.20f, 1.0f),
				ImVec4(0.46f, 0.10f, 0.13f, 1.0f),
				[](ImDrawList* InDrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
				{
					InDrawList->AddLine(ImVec2(Min.x + 17.0f, Min.y + 12.0f), ImVec2(Max.x - 17.0f, Max.y - 12.0f), Color, 1.6f);
					InDrawList->AddLine(ImVec2(Max.x - 17.0f, Min.y + 12.0f), ImVec2(Min.x + 17.0f, Max.y - 12.0f), Color, 1.6f);
				}))
			{
				if (Window)
				{
					Window->Close();
				}
			}
			AddItemHitRect(InteractiveRects, WindowPos);

			ImGui::PopStyleVar(3);
			ImGui::EndMenuBar();
		}
		InteractiveRects.push_back(FWindowHitTestRect{
			0,
			0,
			static_cast<int32>(std::max(MenuEndX, LogoPaddingX + LogoSize + 4.0f)),
			static_cast<int32>(TitleBarHeight)
		});

		const std::string Title = Window ? WideToUtf8(Window->GetTitle().c_str()) : std::string("JS Engine");
		const ImVec2 TitleSize = ImGui::CalcTextSize(Title.c_str());
		const float TitleCenterX = WindowSize.x * 0.5f;
		const float TitleX = std::clamp(
			TitleCenterX - TitleSize.x * 0.5f,
			MenuEndX + 12.0f,
			std::max(MenuEndX + 12.0f, ButtonStartX - TitleSize.x - 12.0f));
		DrawList->AddText(
			ImVec2(WindowPos.x + TitleX, WindowPos.y + (TitleBarHeight - TitleSize.y) * 0.5f),
			ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.84f, 1.0f)),
			Title.empty() ? "JS Engine" : Title.c_str());
	}
	ImGui::End();

	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(5);

	if (Window)
	{
		Window->SetCustomTitleBarMetrics(static_cast<int32>(TitleBarHeight), InteractiveRects);
	}
}
