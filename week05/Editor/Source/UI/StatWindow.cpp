#include "StatWindow.h"
#include "Object/Object.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Memory/MemoryBase.h"
#include "Viewport/ViewportTypes.h"

#include "imgui.h"

#include <cstdio>
#include <string>

namespace
{
	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		if (Value < MinValue)
		{
			return MinValue;
		}
		if (Value > MaxValue)
		{
			return MaxValue;
		}
		return Value;
	}

	std::string FitTextToWidth(const std::string& Text, float MaxWidth)
	{
		if (MaxWidth <= 8.0f)
		{
			return "";
		}

		if (ImGui::CalcTextSize(Text.c_str()).x <= MaxWidth)
		{
			return Text;
		}

		static const char* Ellipsis = "...";
		const float EllipsisWidth = ImGui::CalcTextSize(Ellipsis).x;

		if (EllipsisWidth > MaxWidth)
		{
			return "";
		}

		std::string Result = Text;
		while (!Result.empty())
		{
			Result.pop_back();

			std::string Candidate = Result + Ellipsis;
			if (ImGui::CalcTextSize(Candidate.c_str()).x <= MaxWidth)
			{
				return Candidate;
			}
		}

		return Ellipsis;
	}
}

void FStatWindow::RefreshObjectList()
{
	ObjectEntries.clear();

	for (UObject* Obj : GUObjectArray)
	{
		if (Obj == nullptr)
		{
			continue;
		}

		FObjectEntry Entry;
		Entry.Name = Obj->GetName();
		Entry.ClassName = Obj->GetClass() ? Obj->GetClass()->GetName() : "Unknown";
		Entry.Size = Obj->ObjectSize;
		ObjectEntries.push_back(Entry);
	}

	bShowObjectList = true;
}

void FStatWindow::Render(const FRect& AreaRect)
{
	RefreshObjectList();
	const float ViewportWidth = AreaRect.Width;
	const float ViewportHeight = AreaRect.Height;

	// 뷰포트에 맞춰 창 크기 반응형 계산
	const float MarginX = 20.0f;
	const float MarginY = 20.0f;

	const float MaxWindowWidth = (ViewportWidth > MarginX * 2.0f) ? (ViewportWidth - MarginX * 2.0f) : ViewportWidth;
	const float MaxWindowHeight = (ViewportHeight > MarginY * 2.0f) ? (ViewportHeight - MarginY * 2.0f) : ViewportHeight;

	float WindowWidth = ViewportWidth * 0.70f;
	float WindowHeight = ViewportHeight * 0.55f;

	WindowWidth = ClampFloat(WindowWidth, 320.0f, MaxWindowWidth);
	WindowHeight = ClampFloat(WindowHeight, 220.0f, MaxWindowHeight);

	ImGuiViewport* MainVp = ImGui::GetMainViewport();

	const float CenterX = MainVp->Pos.x + AreaRect.X + AreaRect.Width * 0.5f;
	const float CenterY = MainVp->Pos.y + AreaRect.Y + AreaRect.Height * 0.5f;

	ImGui::SetNextWindowPos(
		ImVec2(CenterX, CenterY),
		ImGuiCond_Always,
		ImVec2(0.5f, 0.5f)
	);

	ImGui::SetNextWindowSize(
		ImVec2(WindowWidth, WindowHeight),
		ImGuiCond_Always
	);

	ImGui::SetNextWindowBgAlpha(0.35f);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.02f, 0.72f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.00f, 1.00f, 1.00f, 0.08f));
	ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.00f, 1.00f, 1.00f, 0.04f));

	ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking;

	const bool bOpen = ImGui::Begin("##StatsOverlay", nullptr, Flags);

	ImGui::PopStyleColor(5);
	ImGui::PopStyleVar(4);

	if (!bOpen)
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Statistics");
	ImGui::Separator();

	ImGui::Text("Objects : %u", ObjectEntries.size());
	ImGui::Text("Current Heap Usage : %.2f KB", GetGMalloc()->MallocStats.CurrentAllocationBytes / 1024.0f);
	ImGui::Text("Current Heap Count : %d", GetGMalloc()->MallocStats.CurrentAllocationCount);

	if (bShowObjectList && !ObjectEntries.empty())
	{
		ImGui::Spacing();
		ImGui::Text("Object List");
		ImGui::Separator();

		// 남은 영역 전체를 리스트가 차지
		ImGui::BeginChild(
			"ObjectListPanel",
			ImVec2(0.0f, 0.0f),
			false,
			ImGuiWindowFlags_AlwaysVerticalScrollbar
		);

		const float StartX = ImGui::GetCursorPosX();
		const float PanelWidth = ImGui::GetContentRegionAvail().x;

		// 패널 폭에 따라 열 너비를 비율로 계산
		float SizeColumnWidth = (PanelWidth < 460.0f) ? 72.0f : 96.0f;
		float Gap1 = 14.0f;
		float Gap2 = 14.0f;

		float ClassColumnWidth = (PanelWidth < 560.0f) ? (PanelWidth * 0.34f) : (PanelWidth * 0.30f);
		if (ClassColumnWidth < 80.0f)
		{
			ClassColumnWidth = 80.0f;
		}

		float NameColumnWidth = PanelWidth - ClassColumnWidth - SizeColumnWidth - Gap1 - Gap2;
		if (NameColumnWidth < 120.0f)
		{
			NameColumnWidth = 120.0f;
			ClassColumnWidth = PanelWidth - NameColumnWidth - SizeColumnWidth - Gap1 - Gap2;
			if (ClassColumnWidth < 70.0f)
			{
				ClassColumnWidth = 70.0f;
			}
		}

		const float NameX = StartX + ClassColumnWidth + Gap1;
		const float SizeX = NameX + NameColumnWidth + Gap2;

		// 헤더
		ImGui::TextDisabled("Class");
		ImGui::SameLine(NameX);
		ImGui::TextDisabled("Name");
		ImGui::SameLine(SizeX);
		ImGui::TextDisabled("Size");
		ImGui::Separator();

		ImGuiListClipper Clipper;
		Clipper.Begin(static_cast<int>(ObjectEntries.size()));

		while (Clipper.Step())
		{
			for (int Index = Clipper.DisplayStart; Index < Clipper.DisplayEnd; ++Index)
			{
				const FObjectEntry& Entry = ObjectEntries[Index];

				const std::string ClassText = FitTextToWidth(Entry.ClassName, ClassColumnWidth);
				const std::string NameText = FitTextToWidth(Entry.Name, NameColumnWidth);

				char SizeBuffer[32];
				std::snprintf(SizeBuffer, sizeof(SizeBuffer), "%u", Entry.Size);

				const float SizeTextWidth = ImGui::CalcTextSize(SizeBuffer).x;
				float SizeTextX = SizeX + (SizeColumnWidth - SizeTextWidth);
				if (SizeTextX < SizeX)
				{
					SizeTextX = SizeX;
				}

				ImGui::TextDisabled("%s", ClassText.c_str());
				ImGui::SameLine(NameX);
				ImGui::Text("%s", NameText.c_str());
				ImGui::SameLine(SizeTextX);
				ImGui::Text("%s", SizeBuffer);
			}
		}

		ImGui::EndChild();
	}

	ImGui::End();
}