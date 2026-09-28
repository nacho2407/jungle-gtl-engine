#include "Editor/UI/EditorConsoleWidget.h"

#include <algorithm>

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Object/FName.h"

// 콘솔 초기화 시점에 입력될 명령어를 등록한다.
FEditorConsoleWidget::FEditorConsoleWidget() 
{
	// 임의의 명령어 문자열이 들어왔을 때 뒤의 함수를 실행하도록 분기한다.
	RegisterCommand("stat", [this](const TArray<FString>& Args) { CmdStat(Args); });
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
	Messages.push_back(_strdup(buf));
	if (AutoScroll) ScrollToBottom = true;
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

	ImGui::SetItemDefaultFocus();

	ImGui::End();
}

void FEditorConsoleWidget::RegisterCommand(const FString& Name, CommandFn Fn) {
	Commands[Name] = Fn;
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

	auto It = Commands.find(Tokens[0]);
	if (It != Commands.end()) {
		It->second(Tokens);
	}
	else {
		AddLog("[ERROR] Unknown command: '%s'\n", Tokens[0].c_str());
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
		for (auto& Pair : Console->Commands) {
			const FString& Name = Pair.first;
			if (strncmp(Name.c_str(), WordStart, WordEnd - WordStart) == 0)
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

void FEditorConsoleWidget::CmdStat(const TArray<FString>& Args)
{
	if (Args.size() < 2)
	{
		AddLog("[WARN] Usage: stat <fps|memory|nametable|none>\n");
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
	else if (Target == "none")
	{
		for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
		{
			Layout.GetViewportState(i).bShowStatFPS       = false;
			Layout.GetViewportState(i).bShowStatMemory    = false;
			Layout.GetViewportState(i).bShowStatNameTable = false;
		}
		AddLog("All Stats Disabled\n");
	}
}

ImVector<char*> FEditorConsoleWidget::Messages;
ImVector<char*> FEditorConsoleWidget::History;

bool FEditorConsoleWidget::AutoScroll = true;
bool FEditorConsoleWidget::ScrollToBottom = true;