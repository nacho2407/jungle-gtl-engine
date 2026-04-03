#include "Editor/UI/EditorStatWidget.h"

#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <sstream>

void FEditorStatWidget::Render(float DeltaTime)
{
#if STATS
	(void)DeltaTime;

	ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(700.0f, 500.0f), ImGuiCond_Once);
	ImGui::Begin("Stat Profiler");

	// Pause / Resume 버튼
	if (bPaused)
	{
		if (ImGui::Button("Resume"))
		{
			bPaused = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy"))
		{
			std::ostringstream oss;
			auto FormatTable = [&](const char* Title, const TArray<FStatEntry>& Entries)
			{
				oss << "=== " << Title << " ===\n";
				oss << "Name\tCalls\tTotal(ms)\tAvg(ms)\tMax(ms)\tMin(ms)\tLast(ms)\n";
				for (const FStatEntry& E : Entries)
				{
					if (E.CallCount == 0) continue;
					double MinVal = E.MinTime == DBL_MAX ? 0.0 : E.MinTime;
					oss << E.Name << "\t"
						<< E.CallCount << "\t"
						<< E.TotalTime * 1000.0 << "\t"
						<< E.GetAvgTime() * 1000.0 << "\t"
						<< E.MaxTime * 1000.0 << "\t"
						<< MinVal * 1000.0 << "\t"
						<< E.LastTime * 1000.0 << "\n";
				}
				oss << "\n";
			};
			FormatTable("CPU Stats", FrozenCPUEntries);
			FormatTable("GPU Stats", FrozenGPUEntries);
			ImGui::SetClipboardText(oss.str().c_str());
		}
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.0f, 1.0f), "PAUSED");
	}
	else
	{
		if (ImGui::Button("Pause"))
		{
			FrozenDrawCalls = FDrawCallStats::Get();
			FrozenCPUEntries = FStatManager::Get().GetSnapshot();
			FrozenGPUEntries = FGPUProfiler::Get().GetGPUSnapshot();
			bPaused = true;
		}
	}

	// --- Draw Call Count ---
	uint32 DrawCalls = bPaused ? FrozenDrawCalls : FDrawCallStats::Get();
	ImGui::Text("Draw Calls: %u", DrawCalls);
	ImGui::Separator();

	// --- CPU Stats ---
	const TArray<FStatEntry>& CPUSource = bPaused ? FrozenCPUEntries : FStatManager::Get().GetSnapshot();
	if (ImGui::CollapsingHeader("CPU Stats", ImGuiTreeNodeFlags_DefaultOpen))
	{
		RenderStatTable("CPUStatTable", CPUSource, CPUSortColumn, bCPUSortDescending);
	}

	// --- GPU Stats ---
	const TArray<FStatEntry>& GPUSource = bPaused ? FrozenGPUEntries : FGPUProfiler::Get().GetGPUSnapshot();
	if (ImGui::CollapsingHeader("GPU Stats", ImGuiTreeNodeFlags_DefaultOpen))
	{
		RenderStatTable("GPUStatTable", GPUSource, GPUSortColumn, bGPUSortDescending);
	}

	ImGui::End();
#endif
}

void FEditorStatWidget::RenderStatTable(const char* TableID, const TArray<FStatEntry>& Source,
	int& OutSortColumn, bool& OutSortDescending)
{
#if STATS
	if (Source.empty())
	{
		ImGui::Text("No stats recorded.");
		return;
	}

	TArray<FStatEntry> Entries = Source;

	auto SortPredicate = [&](const FStatEntry& A, const FStatEntry& B) -> bool
	{
		double ValA = 0.0, ValB = 0.0;
		switch (OutSortColumn)
		{
		case 0: return OutSortDescending ? (strcmp(A.Name, B.Name) > 0) : (strcmp(A.Name, B.Name) < 0);
		case 1: ValA = (double)A.CallCount; ValB = (double)B.CallCount; break;
		case 2: ValA = A.TotalTime;  ValB = B.TotalTime;  break;
		case 3: ValA = A.GetAvgTime(); ValB = B.GetAvgTime(); break;
		case 4: ValA = A.MaxTime;    ValB = B.MaxTime;    break;
		case 5: ValA = A.MinTime == DBL_MAX ? 0.0 : A.MinTime;
				ValB = B.MinTime == DBL_MAX ? 0.0 : B.MinTime; break;
		case 6: ValA = A.LastTime;   ValB = B.LastTime;   break;
		}
		return OutSortDescending ? (ValA > ValB) : (ValA < ValB);
	};
	std::sort(Entries.begin(), Entries.end(), SortPredicate);

	const char* Headers[] = { "Name", "Calls", "Total(ms)", "Avg(ms)", "Max(ms)", "Min(ms)", "Last(ms)" };

	if (ImGui::BeginTable(TableID, 7,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
		ImVec2(0.0f, 200.0f)))
	{
		for (int i = 0; i < 7; i++)
		{
			ImGui::TableSetupColumn(Headers[i]);
		}
		ImGui::TableHeadersRow();

		for (int i = 0; i < 7; i++)
		{
			if (ImGui::TableGetColumnFlags(i) & ImGuiTableColumnFlags_IsHovered)
			{
				if (ImGui::IsMouseClicked(0))
				{
					if (OutSortColumn == i)
						OutSortDescending = !OutSortDescending;
					else
					{
						OutSortColumn = i;
						OutSortDescending = true;
					}
				}
			}
		}

		for (const FStatEntry& E : Entries)
		{
			if (E.CallCount == 0) continue;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("%s", E.Name);
			ImGui::TableSetColumnIndex(1); ImGui::Text("%u", E.CallCount);
			ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f", E.TotalTime * 1000.0);
			ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f", E.GetAvgTime() * 1000.0);
			ImGui::TableSetColumnIndex(4); ImGui::Text("%.3f", E.MaxTime * 1000.0);
			ImGui::TableSetColumnIndex(5); ImGui::Text("%.3f", E.MinTime == DBL_MAX ? 0.0 : E.MinTime * 1000.0);
			ImGui::TableSetColumnIndex(6); ImGui::Text("%.3f", E.LastTime * 1000.0);
		}

		ImGui::EndTable();
	}
#endif
}
