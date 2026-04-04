#include "Editor/UI/EditorStatWidget.h"

#include "Editor/Settings/EditorSettings.h"
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
				// 카테고리 기준 정렬
				TArray<FStatEntry> Sorted = Entries;
				std::sort(Sorted.begin(), Sorted.end(),
					[](const FStatEntry& A, const FStatEntry& B)
					{
						int cmp = strcmp(A.Category, B.Category);
						if (cmp != 0) return cmp < 0;
						return A.TotalTime > B.TotalTime;
					});

				oss << "=== " << Title << " ===\n";
				oss << "Category\tName\tCalls\tTotal(ms)\tAvg(ms)\tMax(ms)\tMin(ms)\tLast(ms)\n";
				for (const FStatEntry& E : Sorted)
				{
					if (E.CallCount == 0) continue;
					oss << E.Category << "\t"
						<< E.Name << "\t"
						<< E.CallCount << "\t"
						<< E.TotalTime * 1000.0 << "\t"
						<< E.AvgTime * 1000.0 << "\t"
						<< E.MaxTime * 1000.0 << "\t"
						<< E.MinTime * 1000.0 << "\t"
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

	// --- Runtime Optimization ---
	ImGui::Checkbox("Runtime Optimization", &FEditorSettings::Get().bRunTimeOptimization);
	ImGui::Separator();

	// --- Draw Call Count ---
	uint32 DrawCalls = bPaused ? FrozenDrawCalls : FDrawCallStats::Get();
	ImGui::Text("Draw Calls: %u", DrawCalls);
	ImGui::Text("LOD0: %u  LOD1: %u  LOD2: %u",
		FLODStats::GetLOD0(), FLODStats::GetLOD1(), FLODStats::GetLOD2());
	ImGui::Separator();

	// 남은 공간을 CPU/GPU 테이블이 반씩 사용
	float AvailableHeight = ImGui::GetContentRegionAvail().y;
	float HalfHeight = (AvailableHeight - ImGui::GetFrameHeightWithSpacing() * 2.0f) * 0.5f;
	if (HalfHeight < 100.0f) HalfHeight = 100.0f;

	// --- CPU Stats ---
	const TArray<FStatEntry>& CPUSource = bPaused ? FrozenCPUEntries : FStatManager::Get().GetSnapshot();
	if (ImGui::CollapsingHeader("CPU Stats", ImGuiTreeNodeFlags_DefaultOpen))
	{
		RenderStatTable("CPUStatTable", CPUSource, CPUSortColumn, bCPUSortDescending, HalfHeight);
	}

	// --- GPU Stats ---
	const TArray<FStatEntry>& GPUSource = bPaused ? FrozenGPUEntries : FGPUProfiler::Get().GetGPUSnapshot();
	if (ImGui::CollapsingHeader("GPU Stats", ImGuiTreeNodeFlags_DefaultOpen))
	{
		RenderStatTable("GPUStatTable", GPUSource, GPUSortColumn, bGPUSortDescending, HalfHeight);
	}

	ImGui::End();
#endif
}

void FEditorStatWidget::RenderStatTable(const char* TableID, const TArray<FStatEntry>& Source,
	int& OutSortColumn, bool& OutSortDescending, float TableHeight)
{
#if STATS
	if (Source.empty())
	{
		ImGui::Text("No stats recorded.");
		return;
	}

	TArray<FStatEntry> Entries = Source;

	// Category 우선 정렬 후, 선택된 컬럼으로 2차 정렬
	auto SortPredicate = [&](const FStatEntry& A, const FStatEntry& B) -> bool
	{
		int catCmp = strcmp(A.Category, B.Category);
		if (catCmp != 0) return catCmp < 0;

		double ValA = 0.0, ValB = 0.0;
		switch (OutSortColumn)
		{
		case 0: return OutSortDescending ? (strcmp(A.Category, B.Category) > 0) : (strcmp(A.Category, B.Category) < 0);
		case 1: return OutSortDescending ? (strcmp(A.Name, B.Name) > 0) : (strcmp(A.Name, B.Name) < 0);
		case 2: ValA = (double)A.CallCount; ValB = (double)B.CallCount; break;
		case 3: ValA = A.TotalTime;  ValB = B.TotalTime;  break;
		case 4: ValA = A.AvgTime;    ValB = B.AvgTime;    break;
		case 5: ValA = A.MaxTime;    ValB = B.MaxTime;    break;
		case 6: ValA = A.MinTime;    ValB = B.MinTime;    break;
		case 7: ValA = A.LastTime;   ValB = B.LastTime;   break;
		}
		return OutSortDescending ? (ValA > ValB) : (ValA < ValB);
	};
	std::sort(Entries.begin(), Entries.end(), SortPredicate);

	const char* Headers[] = { "Category", "Name", "Calls", "Total(ms)", "Avg(ms)", "Max(ms)", "Min(ms)", "Last(ms)" };

	if (ImGui::BeginTable(TableID, 8,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
		ImVec2(0.0f, TableHeight)))
	{
		for (int i = 0; i < 8; i++)
		{
			ImGui::TableSetupColumn(Headers[i]);
		}
		ImGui::TableHeadersRow();

		for (int i = 0; i < 8; i++)
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

		const char* LastCategory = nullptr;
		for (const FStatEntry& E : Entries)
		{
			if (E.CallCount == 0) continue;

			// 카테고리 구분선
			if (!LastCategory || strcmp(LastCategory, E.Category) != 0)
			{
				LastCategory = E.Category;
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", E.Category);
				for (int col = 1; col < 8; col++)
				{
					ImGui::TableSetColumnIndex(col);
					ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "---");
				}
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("%s", E.Category);
			ImGui::TableSetColumnIndex(1); ImGui::Text("%s", E.Name);
			ImGui::TableSetColumnIndex(2); ImGui::Text("%u", E.CallCount);
			ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f", E.TotalTime * 1000.0);
			ImGui::TableSetColumnIndex(4); ImGui::Text("%.3f", E.AvgTime * 1000.0);
			ImGui::TableSetColumnIndex(5); ImGui::Text("%.3f", E.MaxTime * 1000.0);
			ImGui::TableSetColumnIndex(6); ImGui::Text("%.3f", E.MinTime * 1000.0);
			ImGui::TableSetColumnIndex(7); ImGui::Text("%.3f", E.LastTime * 1000.0);
		}

		ImGui::EndTable();
	}
#endif
}
