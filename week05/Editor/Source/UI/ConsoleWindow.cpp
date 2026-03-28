#include "ConsoleWindow.h"
#include <cstdio>
#include <cstring>
#include <cctype>

// --- Helpers ---
int32 FConsoleWindow::Stricmp(const char* S1, const char* S2)
{
	int32 d;
	while ((d = toupper(*S2) - toupper(*S1)) == 0 && *S1) { S1++; S2++; }
	return d;
}
int32 FConsoleWindow::Strnicmp(const char* S1, const char* S2, int32 N)
{
	int32 d = 0;
	while (N > 0 && (d = toupper(*S2) - toupper(*S1)) == 0 && *S1) { S1++; S2++; N--; }
	return d;
}
char* FConsoleWindow::Strdup(const char* S)
{
	size_t Len = strlen(S) + 1;
	void* Buf = ImGui::MemAlloc(Len);
	return (char*)memcpy(Buf, S, Len);
}
void FConsoleWindow::Strtrim(char* S)
{
	char* End = S + strlen(S);
	while (End > S && End[-1] == ' ') End--;
	*End = 0;
}

// --- Constructor / Destructor ---
FConsoleWindow::FConsoleWindow()
{
	ClearLog();
	memset(InputBuf, 0, sizeof(InputBuf));
	HistoryPos = -1;

	Commands.push_back("HELP");
	Commands.push_back("HISTORY");
	Commands.push_back("CLEAR");

	AddLog("Welcome to Console.");
}

FConsoleWindow::~FConsoleWindow()
{
	ClearLog();
	for (int32 i = 0; i < History.Size; i++)
		ImGui::MemFree(History[i]);
}

void FConsoleWindow::ClearLog()
{
	for (int32 i = 0; i < Items.Size; i++)
		ImGui::MemFree(Items[i]);
	Items.clear();
}

void FConsoleWindow::AddLog(const char* Fmt, ...)
{
	char Buf[1024];
	va_list Args;
	va_start(Args, Fmt);
	vsnprintf(Buf, IM_COUNTOF(Buf), Fmt, Args);
	Buf[IM_COUNTOF(Buf) - 1] = 0;
	va_end(Args);
	Items.push_back(Strdup(Buf));
}

void FConsoleWindow::RegisterCommand(const char* Command)
{
	Commands.push_back(Command);
}

// --- Render ---
void FConsoleWindow::Render()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	bool bOpen = ImGui::Begin("Console");
	ImGui::PopStyleVar();

	if (!bOpen)
	{
		ImGui::End();
		return;
	}

	// Toolbar
	if (ImGui::SmallButton("Clear")) { ClearLog(); }
	ImGui::SameLine();
	bool bCopyToClipboard = ImGui::SmallButton("Copy");

	ImGui::Separator();

	// Options popup
	if (ImGui::BeginPopup("Options"))
	{
		ImGui::Checkbox("Auto-scroll", &AutoScroll);
		ImGui::EndPopup();
	}
	if (ImGui::Button("Options"))
		ImGui::OpenPopup("Options");
	ImGui::SameLine();
	Filter.Draw("Filter", 180);
	ImGui::Separator();

	// Log area
	ImGuiStyle& Style = ImGui::GetStyle();
	const float FooterHeight = Style.SeparatorSize + Style.ItemSpacing.y
		+ ImGui::GetFrameHeightWithSpacing();

	if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -FooterHeight),
		ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
	{
		if (ImGui::BeginPopupContextWindow())
		{
			if (ImGui::Selectable("Clear")) ClearLog();
			ImGui::EndPopup();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

		if (bCopyToClipboard)
			ImGui::LogToClipboard();

		for (const char* Item : Items)
		{
			if (!Filter.PassFilter(Item))
				continue;

			ImVec4 Color;
			bool   bHasColor = false;

			if (strstr(Item, "[error]")) { Color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); bHasColor = true; }
			else if (strstr(Item, "[warn]")) { Color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); bHasColor = true; }
			else if (strncmp(Item, "# ", 2) == 0) { Color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); bHasColor = true; }

			if (bHasColor) ImGui::PushStyleColor(ImGuiCol_Text, Color);
			ImGui::TextUnformatted(Item);
			if (bHasColor) ImGui::PopStyleColor();
		}

		if (bCopyToClipboard)
			ImGui::LogFinish();

		if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
			ImGui::SetScrollHereY(1.0f);
		ScrollToBottom = false;

		ImGui::PopStyleVar();
	}
	ImGui::EndChild();
	ImGui::Separator();

	// Input field
	bool bReclaimFocus = false;
	ImGuiInputTextFlags InputFlags =
		ImGuiInputTextFlags_EnterReturnsTrue |
		ImGuiInputTextFlags_EscapeClearsAll |
		ImGuiInputTextFlags_CallbackCompletion |
		ImGuiInputTextFlags_CallbackHistory;

	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::InputText("##Input", InputBuf, IM_COUNTOF(InputBuf),
		InputFlags, &TextEditCallbackStub, this))
	{
		Strtrim(InputBuf);
		if (InputBuf[0])
			ExecCommand(InputBuf);
		strcpy_s(InputBuf, "");
		bReclaimFocus = true;
	}

	ImGui::SetItemDefaultFocus();
	if (bReclaimFocus)
		ImGui::SetKeyboardFocusHere(-1);

	ImGui::End();
}

// --- Command execution ---
void FConsoleWindow::ExecCommand(const char* CommandLine)
{
	AddLog("# %s\n", CommandLine);

	HistoryPos = -1;
	for (int32 i = History.Size - 1; i >= 0; i--)
		if (Stricmp(History[i], CommandLine) == 0)
		{
			ImGui::MemFree(History[i]);
			History.erase(History.begin() + i);
			break;
		}
	History.push_back(Strdup(CommandLine));

	if (Stricmp(CommandLine, "CLEAR") == 0)
	{
		ClearLog();
	}
	else if (Stricmp(CommandLine, "HELP") == 0)
	{
		AddLog("Commands:");
		for (int32 i = 0; i < Commands.Size; i++)
			AddLog("- %s", Commands[i]);
	}
	else if (Stricmp(CommandLine, "HISTORY") == 0)
	{
		int32 First = History.Size - 10;
		for (int32 i = First > 0 ? First : 0; i < History.Size; i++)
			AddLog("%3d: %s\n", i, History[i]);
	}
	else if (CommandHandler)
	{
		CommandHandler(CommandLine);
	}
	else
	{
		AddLog("[error] Unknown command: '%s'\n", CommandLine);
	}

	ScrollToBottom = true;
}

// --- Text callback ---
int32 FConsoleWindow::TextEditCallbackStub(ImGuiInputTextCallbackData* Data)
{
	return ((FConsoleWindow*)Data->UserData)->TextEditCallback(Data);
}

int32 FConsoleWindow::TextEditCallback(ImGuiInputTextCallbackData* Data)
{
	switch (Data->EventFlag)
	{
	case ImGuiInputTextFlags_CallbackCompletion:
	{
		const char* WordEnd = Data->Buf + Data->CursorPos;
		const char* WordStart = WordEnd;
		while (WordStart > Data->Buf)
		{
			const char C = WordStart[-1];
			if (C == ' ' || C == '\t' || C == ',' || C == ';') break;
			WordStart--;
		}

		ImVector<const char*> Candidates;
		for (int32 i = 0; i < Commands.Size; i++)
			if (Strnicmp(Commands[i], WordStart, (int32)(WordEnd - WordStart)) == 0)
				Candidates.push_back(Commands[i]);

		if (Candidates.Size == 0)
		{
			AddLog("No match for \"%.*s\"!\n", (int32)(WordEnd - WordStart), WordStart);
		}
		else if (Candidates.Size == 1)
		{
			Data->DeleteChars((int32)(WordStart - Data->Buf), (int32)(WordEnd - WordStart));
			Data->InsertChars(Data->CursorPos, Candidates[0]);
			Data->InsertChars(Data->CursorPos, " ");
		}
		else
		{
			int32 MatchLen = (int32)(WordEnd - WordStart);
			for (;;)
			{
				int32  C = 0;
				bool bAllMatch = true;
				for (int32 i = 0; i < Candidates.Size && bAllMatch; i++)
					if (i == 0) C = toupper(Candidates[i][MatchLen]);
					else if (C == 0 || C != toupper(Candidates[i][MatchLen])) bAllMatch = false;
				if (!bAllMatch) break;
				MatchLen++;
			}
			if (MatchLen > 0)
			{
				Data->DeleteChars((int32)(WordStart - Data->Buf), (int32)(WordEnd - WordStart));
				Data->InsertChars(Data->CursorPos, Candidates[0], Candidates[0] + MatchLen);
			}
			AddLog("Possible matches:\n");
			for (int32 i = 0; i < Candidates.Size; i++)
				AddLog("- %s\n", Candidates[i]);
		}
		break;
	}
	case ImGuiInputTextFlags_CallbackHistory:
	{
		const int32 PrevPos = HistoryPos;
		if (Data->EventKey == ImGuiKey_UpArrow)
		{
			if (HistoryPos == -1) HistoryPos = History.Size - 1;
			else if (HistoryPos > 0) HistoryPos--;
		}
		else if (Data->EventKey == ImGuiKey_DownArrow)
		{
			if (HistoryPos != -1 && ++HistoryPos >= History.Size)
				HistoryPos = -1;
		}
		if (PrevPos != HistoryPos)
		{
			const char* HistStr = HistoryPos >= 0 ? History[HistoryPos] : "";
			Data->DeleteChars(0, Data->BufTextLen);
			Data->InsertChars(0, HistStr);
		}
		break;
	}
	}
	return 0;
}
