#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Object/Object.h"

#include <algorithm>

// ============================================================
// FConsoleLogOutputDevice
// ============================================================

void FConsoleLogOutputDevice::Write(const char* Msg)
{
	Messages.push_back(_strdup(Msg));
	if (AutoScroll) ScrollToBottom = true;
}

void FConsoleLogOutputDevice::Clear()
{
	for (int32 i = 0; i < Messages.Size; i++) free(Messages[i]);
	Messages.clear();
}

// ============================================================
// FEditorConsoleWidget
// ============================================================

// 기존 코드 호환용 static 래퍼 — UE_LOG로 위임
void FEditorConsoleWidget::AddLog(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	FLogManager::Get().LogV(fmt, args);
	va_end(args);
}

void FEditorConsoleWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);

	// 에디터 콘솔을 로그 출력 디바이스로 등록
	FLogManager::Get().AddOutputDevice(&ConsoleDevice);

	RegisterCommand("clear", [this](const TArray<FString>& Args)
		{
			(void)Args;
			Clear();
		});

	RegisterCommand("obj", [this](const TArray<FString>& Args)
		{
			if (Args.size() < 2)
			{
				AddLog("Usage: obj list [ClassName]\n");
				return;
			}

			if (Args[1] == "list")
			{
				const FString ClassFilter = (Args.size() >= 3) ? Args[2] : "";

				// 클래스별 카운트 + 인스턴스 크기 집계
				struct FClassEntry
				{
					const char* Name;
					size_t      ClassSize;
					uint32      Count;
				};
				TMap<const char*, FClassEntry> ClassMap;

				for (UObject* Obj : GUObjectArray)
				{
					if (!Obj) continue;
					UClass* Cls = Obj->GetClass();
					if (!Cls) continue;

					const char* ClassName = Cls->GetName();
					if (!ClassFilter.empty())
					{
						// 대소문자 무시 부분 매칭
						FString Name(ClassName);
						FString Filter(ClassFilter);
						std::transform(Name.begin(), Name.end(), Name.begin(), ::tolower);
						std::transform(Filter.begin(), Filter.end(), Filter.begin(), ::tolower);
						if (Name.find(Filter) == FString::npos)
							continue;
					}

					auto It = ClassMap.find(ClassName);
					if (It == ClassMap.end())
						ClassMap[ClassName] = { ClassName, Cls->GetSize(), 1 };
					else
						It->second.Count++;
				}

				// 카운트 내림차순 정렬
				TArray<FClassEntry> Sorted;
				for (auto& Pair : ClassMap)
					Sorted.push_back(Pair.second);
				std::sort(Sorted.begin(), Sorted.end(),
					[](const FClassEntry& A, const FClassEntry& B) { return A.Count > B.Count; });

				uint32 TotalCount = 0;
				size_t TotalBytes = 0;

				AddLog("%-35s %8s %10s\n", "Class", "Count", "Size(KB)");
				AddLog("-------------------------------------------------------------\n");
				for (auto& E : Sorted)
				{
					size_t Bytes = E.ClassSize * E.Count;
					TotalCount += E.Count;
					TotalBytes += Bytes;
					AddLog("%-35s %8u %10.1f\n", E.Name, E.Count, Bytes / 1024.0);
				}
				AddLog("-------------------------------------------------------------\n");
				AddLog("%-35s %8u %10.1f\n", "TOTAL", TotalCount, TotalBytes / 1024.0);
				AddLog("GUObjectArray capacity: %zu\n", GUObjectArray.capacity());
			}
			else
			{
				AddLog("[ERROR] Unknown obj subcommand: '%s'\n", Args[1].c_str());
				AddLog("Usage: obj list [ClassName]\n");
			}
		});

	RegisterCommand("stat", [this](const TArray<FString>& Args)
		{
			if (EditorEngine == nullptr)
			{
				AddLog("[ERROR] EditorEngine is null.\n");
				return;
			}

			if (Args.size() < 2)
			{
				AddLog("Usage: stat fps | stat memory | stat none\n");
				return;
			}

			FOverlayStatSystem& StatSystem = EditorEngine->GetOverlayStatSystem();
			const FString& SubCommand = Args[1];

			if (SubCommand == "fps")
			{
				StatSystem.ShowFPS(true);
				AddLog("Overlay stat enabled: fps\n");
			}
			else if (SubCommand == "memory")
			{
				StatSystem.ShowMemory(true);
				AddLog("Overlay stat enabled: memory\n");
			}
			else if (SubCommand == "none")
			{
				StatSystem.HideAll();
				AddLog("Overlay stat disabled: all\n");
			}
			else
			{
				AddLog("[ERROR] Unknown stat command: '%s'\n", SubCommand.c_str());
				AddLog("Usage: stat fps | stat memory | stat none\n");
			}
		});
}

void FEditorConsoleWidget::Shutdown()
{
	FLogManager::Get().RemoveOutputDevice(&ConsoleDevice);
}

void FEditorConsoleWidget::Clear()
{
	ConsoleDevice.Clear();
}

void FEditorConsoleWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Console"))
	{
		ImGui::End();
		return;
	}

	RenderDrawerToolbar();
	const float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	RenderLogContents(-FooterHeight);
	ImGui::Separator();
	RenderInputLine("Input");

	ImGui::End();
}

void FEditorConsoleWidget::RenderDrawerToolbar()
{
	if (ImGui::BeginPopup("ConsoleOptions"))
	{
		ImGui::Checkbox("Auto-scroll", &ConsoleDevice.AutoScroll);
		ImGui::EndPopup();
	}

	if (ImGui::SmallButton("Clear")) { Clear(); }
	ImGui::SameLine();
	if (ImGui::SmallButton("Options"))
	{
		ImGui::OpenPopup("ConsoleOptions");
	}
	ImGui::SameLine();
	Filter.Draw("Filter (\"incl,-excl\")", 180.0f);
}

void FEditorConsoleWidget::RenderLogContents(float Height)
{
	if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, Height), false, ImGuiWindowFlags_HorizontalScrollbar)) {
		for (int32 i = 0; i < ConsoleDevice.GetMessageCount(); ++i) {
			char* Item = ConsoleDevice.GetMessageAt(i);
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

		if (ConsoleDevice.ScrollToBottom || (ConsoleDevice.AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
			ImGui::SetScrollHereY(1.0f);
		}
		ConsoleDevice.ScrollToBottom = false;
	}
	ImGui::EndChild();
}

void FEditorConsoleWidget::RenderInputLine(const char* Label, float Width, bool bFocusInput)
{
	if (bFocusInput)
	{
		ImGui::SetKeyboardFocusHere();
	}

	if (Width > 0.0f)
	{
		ImGui::PushItemWidth(Width);
	}

	// 콘솔 전환 키는 명령어 문자로 입력되지 않도록 필터링한다.
	bool bReclaimFocus = false;
	ImGuiInputTextFlags Flags = ImGuiInputTextFlags_EnterReturnsTrue
		| ImGuiInputTextFlags_EscapeClearsAll
		| ImGuiInputTextFlags_CallbackHistory
		| ImGuiInputTextFlags_CallbackCompletion
		| ImGuiInputTextFlags_CallbackCharFilter;
	if (ImGui::InputText(Label, InputBuf, sizeof(InputBuf), Flags, &TextEditCallback, this)) {
		ExecCommand(InputBuf);
		strcpy_s(InputBuf, "");
		bReclaimFocus = true;
	}

	if (Width > 0.0f)
	{
		ImGui::PopItemWidth();
	}

	ImGui::SetItemDefaultFocus();
	if (bReclaimFocus) {
		ImGui::SetKeyboardFocusHere(-1);
	}
}

const char* FEditorConsoleWidget::GetLatestLogMessage() const
{
	const int32 MessageCount = ConsoleDevice.GetMessageCount();
	return MessageCount > 0 ? ConsoleDevice.GetMessageAt(MessageCount - 1) : "";
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

	if (Data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
		if (Data->EventChar == '`' || Data->EventChar == '~') {
			return 1;
		}
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

ImVector<char*> FEditorConsoleWidget::History;
