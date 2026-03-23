#include "Editor/UI/EditorStatWidget.h"

#include "Core/Stats.h"
#include "ImGui/imgui.h"

#include <algorithm>

void FEditorStatWidget::Render(float DeltaTime)
{
#if STATS
	(void)DeltaTime;

	ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(700.0f, 400.0f), ImGuiCond_Once);
	ImGui::Begin("Stat Profiler");

	// Pause / Resume 버튼
	if (bPaused)
	{
		if (ImGui::Button("Resume"))
		{
			bPaused = false;
		}
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.0f, 1.0f), "PAUSED");
	}
	else
	{
		if (ImGui::Button("Pause"))
		{
			// 현재 라이브 스냅샷을 고정
			FrozenEntries = FStatManager::Get().GetSnapshot();
			bPaused = true;
		}
	}

	// Pause 상태면 고정된 데이터, 아니면 라이브 데이터 표시
	const TArray<FStatEntry>& Source = bPaused ? FrozenEntries : FStatManager::Get().GetSnapshot();
	if (Source.empty())
	{
		ImGui::Text("No stats recorded.");
		ImGui::End();
		return;
	}

	// 정렬 가능한 복사본
	TArray<FStatEntry> Entries = Source;

	auto SortPredicate = [&](const FStatEntry& A, const FStatEntry& B) -> bool
	{
		double ValA = 0.0, ValB = 0.0;
		switch (SortColumn)
		{
		case 0: return bSortDescending ? (strcmp(A.Name, B.Name) > 0) : (strcmp(A.Name, B.Name) < 0);
		case 1: ValA = (double)A.CallCount; ValB = (double)B.CallCount; break;
		case 2: ValA = A.TotalTime;  ValB = B.TotalTime;  break;
		case 3: ValA = A.GetAvgTime(); ValB = B.GetAvgTime(); break;
		case 4: ValA = A.MaxTime;    ValB = B.MaxTime;    break;
		case 5: ValA = A.MinTime == DBL_MAX ? 0.0 : A.MinTime;
				ValB = B.MinTime == DBL_MAX ? 0.0 : B.MinTime; break;
		case 6: ValA = A.LastTime;   ValB = B.LastTime;   break;
		}
		return bSortDescending ? (ValA > ValB) : (ValA < ValB);
	};
	std::sort(Entries.begin(), Entries.end(), SortPredicate);

	const char* Headers[] = { "Name", "Calls", "Total(ms)", "Avg(ms)", "Max(ms)", "Min(ms)", "Last(ms)" };

	if (ImGui::BeginTable("StatTable", 7,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
	{
		for (int i = 0; i < 7; i++)
		{
			ImGui::TableSetupColumn(Headers[i]);
		}
		ImGui::TableHeadersRow();

		// 헤더 클릭 감지
		for (int i = 0; i < 7; i++)
		{
			if (ImGui::TableGetColumnFlags(i) & ImGuiTableColumnFlags_IsHovered)
			{
				if (ImGui::IsMouseClicked(0))
				{
					if (SortColumn == i)
						bSortDescending = !bSortDescending;
					else
					{
						SortColumn = i;
						bSortDescending = true;
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

	ImGui::End();
#endif
}
