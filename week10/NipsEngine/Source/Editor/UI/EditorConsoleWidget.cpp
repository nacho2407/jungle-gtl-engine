#include "Editor/UI/EditorConsoleWidget.h"

#include <algorithm>

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Object/FName.h"

#include <cctype>
#include <limits>
#include <mutex>

namespace
{
FString ToLowerCopy(FString Value)
{
	std::transform(Value.begin(), Value.end(), Value.begin(),
		[](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
	return Value;
}

int32 EditDistance(const FString& A, const FString& B)
{
	const size_t Rows = A.size() + 1;
	const size_t Cols = B.size() + 1;
	TArray<int32> Prev(Cols, 0);
	TArray<int32> Curr(Cols, 0);

	for (size_t Col = 0; Col < Cols; ++Col)
	{
		Prev[Col] = static_cast<int32>(Col);
	}

	for (size_t Row = 1; Row < Rows; ++Row)
	{
		Curr[0] = static_cast<int32>(Row);
		for (size_t Col = 1; Col < Cols; ++Col)
		{
			const int32 Cost = A[Row - 1] == B[Col - 1] ? 0 : 1;
			Curr[Col] = std::min({
				Prev[Col] + 1,
				Curr[Col - 1] + 1,
				Prev[Col - 1] + Cost
			});
		}
		std::swap(Prev, Curr);
	}

	return Prev[Cols - 1];
}

FString FormatBytes(size_t Bytes)
{
	char Buffer[64];
	const double Value = static_cast<double>(Bytes);
	if (Bytes >= 1024ull * 1024ull)
	{
		snprintf(Buffer, sizeof(Buffer), "%.2f MB", Value / (1024.0 * 1024.0));
	}
	else if (Bytes >= 1024ull)
	{
		snprintf(Buffer, sizeof(Buffer), "%.2f KB", Value / 1024.0);
	}
	else
	{
		snprintf(Buffer, sizeof(Buffer), "%zu B", Bytes);
	}
	return Buffer;
}

std::mutex GConsoleLogMutex;
}

// 콘솔 초기화 시점에 입력될 명령어를 등록한다.
FEditorConsoleWidget::FEditorConsoleWidget() 
{
	FLog::SetSink([](const char* Message)
	{
		FEditorConsoleWidget::AddLog("%s", Message);
	});

	// 임의의 명령어 문자열이 들어왔을 때 뒤의 함수를 실행하도록 분기한다.
	RegisterCommand("clear", "Clear console output.", [this](const TArray<FString>& Args)
	{
		(void)Args;
		Clear();
	});
	RegisterCommand("help", "Show help. Usage: help [command]", [this](const TArray<FString>& Args) { CmdHelp(Args); });
	RegisterCommand("?", "Alias for help.", [this](const TArray<FString>& Args) { CmdHelp(Args); });
	RegisterCommand("commands", "List console commands. Usage: commands [prefix]", [this](const TArray<FString>& Args) { CmdCommands(Args); });
	RegisterCommand("cmds", "Alias for commands.", [this](const TArray<FString>& Args) { CmdCommands(Args); });
	RegisterCommand("list", "Alias for commands.", [this](const TArray<FString>& Args) { CmdCommands(Args); });
	RegisterCommand("suggest", "Show recommended commands. Usage: suggest [prefix]", [this](const TArray<FString>& Args) { CmdSuggest(Args); });
	RegisterCommand("recommend", "Alias for suggest.", [this](const TArray<FString>& Args) { CmdSuggest(Args); });
	RegisterCommand("recommendations", "Alias for suggest.", [this](const TArray<FString>& Args) { CmdSuggest(Args); });
	RegisterCommand("stat", "Viewport and editor stats. Usage: stat <fps|memory|history|nametable|cascadevis|none>", [this](const TArray<FString>& Args) { CmdStat(Args); });

	RegisterCommand("shadow", "Set shadow options. Usage: shadow filter <pcf|vsm>", [this](const TArray<FString>& Args){ CmdShadow(Args); });
}

FEditorConsoleWidget::~FEditorConsoleWidget() 
{
	Clear();
	ClearHistory();
}

void FEditorConsoleWidget::AddLog(const char* fmt, ...) {
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	std::lock_guard<std::mutex> Lock(GConsoleLogMutex);
	Messages.push_back(_strdup(buf));
	if (AutoScroll) ScrollToBottom = true;
}

void FEditorConsoleWidget::Clear()
{
	std::lock_guard<std::mutex> Lock(GConsoleLogMutex);
	for (int32 i = 0; i < Messages.Size; i++) free(Messages[i]);
	Messages.clear();
}

void FEditorConsoleWidget::ClearHistory()
{
	for (int32 i = 0; i < History.Size; i++) free(History[i]);
	History.clear();
}

void FEditorConsoleWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	// 백틱(`) 키 → 콘솔 입력창 포커스
	// Begin() 전에 호출해야 SetNextWindowFocus 가 올바르게 동작합니다.
	if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
	{
		bRequestFocusInput = true;
		ImGui::SetNextWindowFocus();
	}

	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Console"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::SmallButton("Clear")) { Clear(); }

	ImGui::Separator();

	//// Options menu
	if (ImGui::BeginPopup("Options"))
	{
		ImGui::Checkbox("Auto-scroll", &AutoScroll);
		ImGui::EndPopup();
	}

	// Options, Filter
	ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_Tooltip);
	if (ImGui::Button("Options"))
		ImGui::OpenPopup("Options");
	ImGui::SameLine();
	Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
	ImGui::Separator();

	const float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -FooterHeight), false, ImGuiWindowFlags_HorizontalScrollbar)) {
		std::lock_guard<std::mutex> Lock(GConsoleLogMutex);
		for (auto& Item : Messages) {
			if (!Filter.PassFilter(Item)) continue;

			ImVec4 Color;
			bool bHasColor = false;
			if (strncmp(Item, "[ERROR]", 7) == 0) {
				Color = ImVec4(1, 0.4f, 0.4f, 1);
				bHasColor = true;
			}
			else if (strncmp(Item, "[WARN]", 6) == 0) {
				Color = ImVec4(1, 0.8f, 0.2f, 1);
				bHasColor = true;
			}
			else if (strncmp(Item, "#", 1) == 0) {
				Color = ImVec4(1, 0.8f, 0.6f, 1);
				bHasColor = true;
			}
			else if (strncmp(Item, "[Build]", 7) == 0) {
				Color = ImVec4(1.0f, 0.58f, 0.18f, 1.0f);
				bHasColor = true;
			}

			if (bHasColor) {
				ImGui::PushStyleColor(ImGuiCol_Text, Color);
			}
			ImGui::TextUnformatted(Item);
			if (bHasColor) {
				ImGui::PopStyleColor();
			}
		}

		if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
			ImGui::SetScrollHereY(1.0f);
		}
		ScrollToBottom = false;
	}
	ImGui::EndChild();
	ImGui::Separator();

	// Input line
	ImGuiInputTextFlags Flags = ImGuiInputTextFlags_EnterReturnsTrue
		| ImGuiInputTextFlags_EscapeClearsAll
		| ImGuiInputTextFlags_CallbackHistory
		| ImGuiInputTextFlags_CallbackCompletion
		| ImGuiInputTextFlags_CallbackCharFilter; // 백틱 문자 필터링용

	// 백틱 포커스 요청 → InputText에 커서 이동
	if (bRequestFocusInput)
	{
		ImGui::SetKeyboardFocusHere(0);
		bRequestFocusInput = false;
	}

	if (ImGui::InputText("Input", InputBuf, sizeof(InputBuf), Flags, &TextEditCallback, this)) {
		ExecCommand(InputBuf);
		strcpy_s(InputBuf, "");
		// 명령 실행 후 포커스를 반환하지 않음 — EnterReturnsTrue 가 자동으로 포커스 해제
	}
	RenderCommandSuggestions("##ConsoleWindowSuggestions", ImGui::GetItemRectMin(), ImGui::GetItemRectSize());

	ImGui::SetItemDefaultFocus();

	ImGui::End();
}

void FEditorConsoleWidget::RenderDrawerToolbar()
{
	if (ImGui::SmallButton("Clear"))
	{
		Clear();
	}

	ImGui::SameLine();
	if (ImGui::Button("Options"))
	{
		ImGui::OpenPopup("ConsoleOptions");
	}
	if (ImGui::BeginPopup("ConsoleOptions"))
	{
		ImGui::Checkbox("Auto-scroll", &AutoScroll);
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	Filter.Draw("Filter", 220.0f);
}

void FEditorConsoleWidget::RenderLogContents(float Height)
{
	const ImVec2 Size(0.0f, Height);
	if (ImGui::BeginChild("##ConsoleDrawerScrollingRegion", Size, false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		std::lock_guard<std::mutex> Lock(GConsoleLogMutex);
		for (auto& Item : Messages)
		{
			if (!Filter.PassFilter(Item))
			{
				continue;
			}

			ImVec4 Color;
			bool bHasColor = false;
			if (strncmp(Item, "[ERROR]", 7) == 0)
			{
				Color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
				bHasColor = true;
			}
			else if (strncmp(Item, "[WARN]", 6) == 0)
			{
				Color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
				bHasColor = true;
			}
			else if (strncmp(Item, "#", 1) == 0)
			{
				Color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f);
				bHasColor = true;
			}
			else if (strncmp(Item, "[Build]", 7) == 0)
			{
				Color = ImVec4(1.0f, 0.58f, 0.18f, 1.0f);
				bHasColor = true;
			}

			if (bHasColor)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, Color);
			}
			ImGui::TextUnformatted(Item);
			if (bHasColor)
			{
				ImGui::PopStyleColor();
			}
		}

		if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
		{
			ImGui::SetScrollHereY(1.0f);
		}
		ScrollToBottom = false;
	}
	ImGui::EndChild();
}

void FEditorConsoleWidget::RenderInputLine(const char* Id, float Width, bool bRequestFocus)
{
	ImGuiInputTextFlags Flags = ImGuiInputTextFlags_EnterReturnsTrue
		| ImGuiInputTextFlags_EscapeClearsAll
		| ImGuiInputTextFlags_CallbackHistory
		| ImGuiInputTextFlags_CallbackCompletion
		| ImGuiInputTextFlags_CallbackCharFilter;

	ImGui::SetNextItemWidth(Width);
	if (bRequestFocus || bRequestFocusInput)
	{
		ImGui::SetKeyboardFocusHere();
		bRequestFocusInput = false;
	}

	if (ImGui::InputText(Id, InputBuf, sizeof(InputBuf), Flags, &TextEditCallback, this))
	{
		ExecCommand(InputBuf);
		strcpy_s(InputBuf, "");
	}
	RenderCommandSuggestions(Id, ImGui::GetItemRectMin(), ImGui::GetItemRectSize());
}

void FEditorConsoleWidget::RegisterCommand(const FString& Name, CommandFn Fn) {
	RegisterCommand(Name, "", Fn);
}

void FEditorConsoleWidget::RegisterCommand(const FString& Name, const FString& Description, CommandFn Fn) {
	const FString Key = ToLowerCopy(Name);
	Commands[Key] = Fn;
	CommandDescriptions[Key] = Description;
}

void FEditorConsoleWidget::ExecCommand(const char* CommandLine) {
	AddLog("# %s\n", CommandLine);
	History.push_back(_strdup(CommandLine));
	HistoryPos = -1;

	TArray<FString> Tokens;
	std::istringstream Iss(CommandLine);
	FString Token;
	while (Iss >> Token) Tokens.push_back(Token);
	if (Tokens.empty()) return;

	Tokens[0] = ToLowerCopy(Tokens[0]);
	if (!Tokens[0].empty() && (Tokens[0][0] == '/' || Tokens[0][0] == '\\'))
	{
		Tokens[0].erase(Tokens[0].begin());
	}
	auto It = Commands.find(Tokens[0]);
	if (It != Commands.end()) {
		It->second(Tokens);
	}
	else {
		AddLog("[ERROR] Unknown command: '%s'\n", Tokens[0].c_str());
		const FString Closest = FindClosestCommand(Tokens[0]);
		if (!Closest.empty())
		{
			AddLog("Did you mean '%s'? Type 'help %s' for details.\n", Closest.c_str(), Closest.c_str());
		}
		AddLog("Type 'commands' to list available commands or 'suggest' for recommendations.\n");
	}
}

// History & Tab-Completion Callback____________________________________________________________
int32 FEditorConsoleWidget::TextEditCallback(ImGuiInputTextCallbackData* Data) {
	FEditorConsoleWidget* Console = (FEditorConsoleWidget*)Data->UserData;

	// 백틱(`) 문자는 콘솔 토글 키이므로 입력창에 삽입하지 않음
	if (Data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
	{
		if (Data->EventChar == L'`')
			return 1; // 문자 버림
		return 0;
	}

	if (Data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
		const int32 PrevPos = Console->HistoryPos;
		if (Data->EventKey == ImGuiKey_UpArrow) {
			if (Console->HistoryPos == -1)
				Console->HistoryPos = Console->History.Size - 1;
			else if (Console->HistoryPos > 0)
				Console->HistoryPos--;
		}
		else if (Data->EventKey == ImGuiKey_DownArrow) {
			if (Console->HistoryPos != -1 &&
				++Console->HistoryPos >= Console->History.Size)
				Console->HistoryPos = -1;
		}
		if (PrevPos != Console->HistoryPos) {
			const char* HistoryStr = (Console->HistoryPos >= 0)
				? Console->History[Console->HistoryPos] : "";
			Data->DeleteChars(0, Data->BufTextLen);
			Data->InsertChars(0, HistoryStr);
		}
	}

	if (Data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
		// Find last word typed
		const char* WordEnd = Data->Buf + Data->CursorPos;
		const char* WordStart = WordEnd;
		while (WordStart > Data->Buf && WordStart[-1] != ' ')
			WordStart--;

		// Collect matches from registered commands
		ImVector<const char*> Candidates;
		const size_t PrefixLength = static_cast<size_t>(WordEnd - WordStart);
		for (auto& Pair : Console->Commands) {
			const FString& Name = Pair.first;
			if (strncmp(Name.c_str(), WordStart, PrefixLength) == 0)
				Candidates.push_back(Name.c_str());
		}

		if (Candidates.Size == 1) {
			Data->DeleteChars(static_cast<int32>(WordStart - Data->Buf), static_cast<int32>(WordEnd - WordStart));
			Data->InsertChars(Data->CursorPos, Candidates[0]);
			Data->InsertChars(Data->CursorPos, " ");
		}
		else if (Candidates.Size > 1) {
			Console->AddLog("Possible completions:\n");
			for (auto& C : Candidates) Console->AddLog("  %s\n", C);
		}
	}

	return 0;
}

void FEditorConsoleWidget::CmdHelp(const TArray<FString>& Args)
{
	if (Args.size() < 2)
	{
		AddLog("Console helper\n");
		AddLog("  help [command]     Show command help\n");
		AddLog("  commands [prefix]  List available commands\n");
		AddLog("  suggest [prefix]   Show recommended commands\n");
		AddLog("  Tab                Complete command names\n");
		PrintCommandList();
		return;
	}

	const FString Key = ToLowerCopy(Args[1]);
	auto It = CommandDescriptions.find(Key);
	if (It != CommandDescriptions.end())
	{
		AddLog("%s - %s\n", Key.c_str(), It->second.c_str());
		return;
	}

	AddLog("[WARN] No help for command '%s'.\n", Args[1].c_str());
	const FString Closest = FindClosestCommand(Key);
	if (!Closest.empty())
	{
		AddLog("Did you mean '%s'?\n", Closest.c_str());
	}
}

void FEditorConsoleWidget::CmdCommands(const TArray<FString>& Args)
{
	const FString Prefix = Args.size() >= 2 ? ToLowerCopy(Args[1]) : "";
	PrintCommandList(Prefix);
}

void FEditorConsoleWidget::CmdSuggest(const TArray<FString>& Args)
{
	const FString Prefix = Args.size() >= 2 ? ToLowerCopy(Args[1]) : "";
	AddLog("Recommended commands:\n");
	bool bPrinted = false;
	if (Prefix.empty() || FString("stat").find(Prefix) == 0)
	{
		AddLog("  stat fps              Toggle FPS stat on focused viewport\n");
		AddLog("  stat memory           Toggle memory stat on focused viewport\n");
		AddLog("  stat history          Print Undo/Redo history memory use\n");
		AddLog("  stat none             Disable viewport stats\n");
		bPrinted = true;
	}
	if (Prefix.empty() || FString("shadow").find(Prefix) == 0)
	{
		AddLog("  shadow filter pcf     Use PCF shadow filtering\n");
		AddLog("  shadow filter vsm     Use VSM shadow filtering\n");
		bPrinted = true;
	}
	if (Prefix.empty() || FString("help").find(Prefix) == 0 || FString("commands").find(Prefix) == 0)
	{
		AddLog("  commands              List every command\n");
		AddLog("  help stat             Show stat command usage\n");
		bPrinted = true;
	}
	if (!bPrinted)
	{
		AddLog("  No recommendation matched '%s'. Type 'commands %s' to inspect matching commands.\n", Prefix.c_str(), Prefix.c_str());
	}
}

void FEditorConsoleWidget::PrintCommandList(const FString& Prefix)
{
	AddLog("Available commands%s%s:\n", Prefix.empty() ? "" : " matching ", Prefix.empty() ? "" : Prefix.c_str());
	for (const auto& Pair : Commands)
	{
		const FString& Name = Pair.first;
		if (!Prefix.empty() && Name.find(Prefix) != 0)
		{
			continue;
		}

		const auto DescIt = CommandDescriptions.find(Name);
		const char* Description = DescIt != CommandDescriptions.end() ? DescIt->second.c_str() : "";
		AddLog("  %-10s %s\n", Name.c_str(), Description);
	}
}

FString FEditorConsoleWidget::FindClosestCommand(const FString& Query) const
{
	int32 BestDistance = std::numeric_limits<int32>::max();
	FString BestCommand;
	for (const auto& Pair : Commands)
	{
		const int32 Distance = EditDistance(Query, Pair.first);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestCommand = Pair.first;
		}
	}

	const int32 Threshold = Query.size() <= 4 ? 2 : 3;
	return BestDistance <= Threshold ? BestCommand : "";
}

TArray<FString> FEditorConsoleWidget::BuildCommandSuggestions(const FString& Query) const
{
	FString Normalized = ToLowerCopy(Query);
	while (!Normalized.empty() && std::isspace(static_cast<unsigned char>(Normalized.front())))
	{
		Normalized.erase(Normalized.begin());
	}
	if (!Normalized.empty() && (Normalized[0] == '/' || Normalized[0] == '\\'))
	{
		Normalized.erase(Normalized.begin());
	}
	if (Normalized.empty())
	{
		return {};
	}

	TArray<FString> Candidates = {
		"help",
		"commands",
		"clear",
		"suggest",
		"stat fps",
		"stat history",
		"stat memory",
		"stat cascadevis",
		"stat nametable list",
		"stat none",
		"shadow filter pcf",
		"shadow filter vsm",
	};

	if (Normalized == "help" || Normalized == "?")
	{
		TArray<FString> AllCommands;
		for (const auto& Pair : Commands)
		{
			AllCommands.push_back(Pair.first);
		}
		std::sort(AllCommands.begin(), AllCommands.end());
		return AllCommands;
	}

	TArray<FString> Matches;
	for (const FString& Candidate : Candidates)
	{
		if (Candidate.find(Normalized) == 0)
		{
			Matches.push_back(Candidate);
		}
	}

	if (Matches.empty())
	{
		for (const auto& Pair : Commands)
		{
			if (Pair.first.find(Normalized) == 0)
			{
				Matches.push_back(Pair.first);
			}
		}
	}

	constexpr int32 MaxSuggestions = 8;
	if (static_cast<int32>(Matches.size()) > MaxSuggestions)
	{
		Matches.resize(MaxSuggestions);
	}
	return Matches;
}

void FEditorConsoleWidget::RenderCommandSuggestions(const char* Id, const ImVec2& InputMin, const ImVec2& InputSize)
{
	(void)Id;
	const TArray<FString> Suggestions = BuildCommandSuggestions(InputBuf);
	if (Suggestions.empty())
	{
		return;
	}

	const float RowHeight = ImGui::GetFrameHeight();
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float ContentWidth = std::max(InputSize.x, 240.0f);
	const float Height = std::min(8.0f, static_cast<float>(Suggestions.size())) * RowHeight + Style.WindowPadding.y * 2.0f;
	const ImVec2 PanelMin(InputMin.x, InputMin.y - Height - 4.0f);
	const ImVec2 PanelMax(PanelMin.x + ContentWidth, PanelMin.y + Height);
	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	const float TextOffsetY = (RowHeight - ImGui::GetTextLineHeight()) * 0.5f;
	const ImU32 BgColor = ImGui::GetColorU32(ImGuiCol_PopupBg);
	const ImU32 BorderColor = ImGui::GetColorU32(ImGuiCol_Border);
	const ImU32 HoverColor = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
	ImDrawList* DrawList = ImGui::GetForegroundDrawList();

	DrawList->AddRectFilled(PanelMin, PanelMax, BgColor, Style.WindowRounding);
	DrawList->AddRect(PanelMin, PanelMax, BorderColor, Style.WindowRounding);

	bool bExecutedSuggestion = false;
	for (int32 Index = 0; Index < static_cast<int32>(Suggestions.size()); ++Index)
	{
		const FString& Suggestion = Suggestions[Index];
		const ImVec2 RowMin(PanelMin.x + Style.WindowPadding.x, PanelMin.y + Style.WindowPadding.y + RowHeight * static_cast<float>(Index));
		const ImVec2 RowMax(PanelMax.x - Style.WindowPadding.x, RowMin.y + RowHeight);
		const bool bHovered =
			MousePos.x >= RowMin.x && MousePos.x <= RowMax.x &&
			MousePos.y >= RowMin.y && MousePos.y <= RowMax.y;

		if (bHovered)
		{
			DrawList->AddRectFilled(RowMin, RowMax, HoverColor, 3.0f);
		}

		DrawList->AddText(ImVec2(RowMin.x + 4.0f, RowMin.y + TextOffsetY),
			ImGui::GetColorU32(ImGuiCol_Text), Suggestion.c_str());

		if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			ExecCommand(Suggestion.c_str());
			strcpy_s(InputBuf, "");
			bExecutedSuggestion = true;
			break;
		}
	}

	if (bExecutedSuggestion)
	{
		bRequestFocusInput = true;
	}
}

void FEditorConsoleWidget::CmdStat(const TArray<FString>& Args)
{
	if (Args.size() < 2)
	{
		AddLog("[WARN] Usage: stat <fps|memory|nametable|none>\n");
		AddLog("[WARN]        stat history         -- print Undo/Redo history memory use\n");
		AddLog("[WARN]        stat nametable list  -- dump all entries\n");
		return;
	}

	if (!EditorEngine) return;

	FString Target = Args[1];
	std::transform(Target.begin(), Target.end(), Target.begin(), ::tolower);

	FEditorViewportLayout& Layout = EditorEngine->GetViewportLayout();
	const int32 FocusedIdx  = Layout.GetLastFocusedViewportIndex();

	if (Target == "fps")
	{
		bool& bFlag = Layout.GetViewportState(FocusedIdx).bShowStatFPS;
		bFlag = !bFlag;
		AddLog("Stat FPS %s (viewport %d)\n", bFlag ? "Enabled" : "Disabled", FocusedIdx);
	}
	else if (Target == "memory")
	{
		bool& bFlag = Layout.GetViewportState(FocusedIdx).bShowStatMemory;
		bFlag = !bFlag;
		AddLog("Stat Memory %s (viewport %d)\n", bFlag ? "Enabled" : "Disabled", FocusedIdx);
	}
	else if (Target == "history")
	{
		PrintHistoryStats();
	}
	else if (Target == "nametable")
	{
		// "stat nametable list" → 콘솔에 전체 덤프
		if (Args.size() >= 3 && Args[2] == "list")
		{
			FNamePool& Pool = FNamePool::Get();
			const uint32 Count = Pool.GetEntryCount();
			AddLog("--- FNamePool NameTable (%u entries) ---\n", Count);
			const TArray<FString>& Entries = Pool.GetEntries();
			for (uint32 j = 0; j < Count; ++j)
			{
				AddLog("  [%u] %s\n", j, Entries[j].c_str());
			}
			AddLog("----------------------------------------\n");
		}
		else
		{
			// 뷰포트 오버레이 토글
			AddLog("  'stat nametable list' to dump all entries to console.\n");
		}
	}
	else if (Target == "cascadevis")
	{
		bool& bFlag = Layout.GetViewportState(FocusedIdx).bShowCascadeVis;
		bFlag = !bFlag;
		AddLog("Stat CascadeVis %s (viewport %d)\n", bFlag ? "Enabled" : "Disabled", FocusedIdx);
	}
	else if (Target == "none")
	{
		for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
		{
			Layout.GetViewportState(i).bShowStatFPS       = false;
			Layout.GetViewportState(i).bShowStatMemory    = false;
			Layout.GetViewportState(i).bShowStatNameTable = false;
			Layout.GetViewportState(i).bShowCascadeVis    = false;
		}
		AddLog("All Stats Disabled\n");
	}
}

void FEditorConsoleWidget::PrintHistoryStats()
{
	if (!EditorEngine)
	{
		AddLog("[WARN] EditorEngine is not available.\n");
		return;
	}

	const FUndoHistoryStats Stats = EditorEngine->GetUndoHistoryStats();
	AddLog("--- Undo History Stats ---\n");
	AddLog("  Entries      : %d / %d (Undo %d, Redo %d)\n",
		Stats.UndoCount + Stats.RedoCount,
		Stats.MaxEntries,
		Stats.UndoCount,
		Stats.RedoCount);
	AddLog("  Snapshot Data: %s\n", FormatBytes(Stats.LogicalBytes).c_str());
	AddLog("  Reserved Mem : %s\n", FormatBytes(Stats.ReservedBytes).c_str());
	AddLog("  Entry Storage: %s\n", FormatBytes(Stats.EntryOverheadBytes).c_str());
	AddLog("  Approx Total : %s\n", FormatBytes(Stats.ApproxTotalBytes).c_str());
	AddLog("--------------------------\n");
}

void FEditorConsoleWidget::CmdShadow(const TArray<FString>& Args)
{
    if (Args.size() < 3)
    {
        AddLog("[WARN] Usage: shadow filter <pcf|vsm>\n");
        return;
    }

    FString CommandTarget = Args[1];
    FString CommandValue = Args[2];

    // 대소문자 구분 없이 처리하기 위해 소문자로 변환
    std::transform(CommandTarget.begin(), CommandTarget.end(), CommandTarget.begin(), ::tolower);
    std::transform(CommandValue.begin(), CommandValue.end(), CommandValue.begin(), ::tolower);

    if (CommandTarget == "filter")
    {
        if (CommandValue == "vsm")
        {
            FEditorSettings::Get().ShadowFilterMode = EShadowFilter::VSM;
            AddLog("Shadow filter mode changed to: VSM\n");
        }
        else if (CommandValue == "pcf")
        {
            FEditorSettings::Get().ShadowFilterMode = EShadowFilter::PCF;
            AddLog("Shadow filter mode changed to: PCF\n");
        }
        else
        {
            AddLog("[ERROR] Invalid shadow filter mode: %s\n", CommandValue.c_str());
        }
    }
    else
    {
        AddLog("[ERROR] Unknown shadow command target: %s\n", CommandTarget.c_str());
    }
}

ImVector<char*> FEditorConsoleWidget::Messages;
ImVector<char*> FEditorConsoleWidget::History;

bool FEditorConsoleWidget::AutoScroll = true;
bool FEditorConsoleWidget::ScrollToBottom = true;
