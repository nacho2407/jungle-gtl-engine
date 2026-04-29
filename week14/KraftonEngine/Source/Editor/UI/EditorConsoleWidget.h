#pragma once
#include "Core/CoreTypes.h"
#include "Core/Log.h"
#include <cstdarg>
#include <functional>
#include <sstream>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Editor/UI/EditorWidget.h"

// ============================================================
// FConsoleLogOutputDevice — ImGui 콘솔에 로그를 출력하는 디바이스
// FEditorConsoleWidget이 소유하며, Initialize/Shutdown 시 등록/해제한다.
// ============================================================
class FConsoleLogOutputDevice : public ILogOutputDevice
{
public:
	void Write(const char* Msg) override;

	void Clear();
	int32 GetMessageCount() const { return Messages.Size; }
	char* GetMessageAt(int32 Index) const { return Messages[Index]; }
	bool PassFilter(const ImGuiTextFilter& Filter, int32 Index) const { return Filter.PassFilter(Messages[Index]); }

private:
	ImVector<char*> Messages;
	bool AutoScroll = true;
	bool ScrollToBottom = true;

	friend class FEditorConsoleWidget;
};

class FEditorConsoleWidget : public FEditorWidget
{
public:
	static void AddLog(const char* fmt, ...);
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;
	virtual void Shutdown();

	void Clear();
	void RenderDrawerToolbar();
	void RenderLogContents(float Height = 0.0f);
	void RenderInputLine(const char* Label = "Input", float Width = 0.0f, bool bFocusInput = false);
	const char* GetLatestLogMessage() const;
	static void ClearHistory()
	{
		for (int32 i = 0; i < History.Size; i++) free(History[i]);
		History.clear();
	}

private:
	char InputBuf[256]{};
	static ImVector<char*> History;
	int32 HistoryPos = -1;
	ImGuiTextFilter Filter;

	FConsoleLogOutputDevice ConsoleDevice;

	//Command Dispatch System
	using CommandFn = std::function<void(const TArray<FString>& args)>;
	struct FConsoleCommand
	{
		CommandFn Fn;
		FString Category;
		FString Usage;
		FString Description;
	};

	struct FCompletionCandidate
	{
		FString CommandName;
		FString DisplayText;
	};

	TMap<FString, FConsoleCommand> Commands;
	TArray<FCompletionCandidate> CompletionCandidates;

	void RegisterCommand(const FString& Name, CommandFn Fn, const FString& Category, const FString& Usage, const FString& Description);
	void RegisterDefaultCommands();
	void RegisterSystemCommands();
	void RegisterEditorCommands();
	void RegisterDiagnosticsCommands();
	void RegisterRenderCommands();

	void RenderCompletionCandidates();
	void UpdateCompletionCandidates();
	TArray<FCompletionCandidate> GetCompletionCandidates(const FString& Input) const;
	bool PrintCompactHelp(const FString& CategoryFilter = "");
	bool TryFindCommand(const TArray<FString>& Tokens, FString& OutCommandName, const FConsoleCommand*& OutCommand, int32& OutConsumedTokens) const;
	void ExecCommand(const char* CommandLine);

	void HandleHelp(const TArray<FString>& Args);
	void HandleHideWindows(const TArray<FString>& Args);
	void HandleShowWindows(const TArray<FString>& Args);
	void HandleShowEditorOnly(const TArray<FString>& Args);
	void HandleHideEditorOnly(const TArray<FString>& Args);
	void HandleContentBrowserRefresh(const TArray<FString>& Args);
	void HandleContentBrowserIconSize(const TArray<FString>& Args);
	void HandleObjList(const TArray<FString>& Args);
	void HandleStatFPS(const TArray<FString>& Args);
	void HandleStatMemory(const TArray<FString>& Args);
	void HandleStatShadow(const TArray<FString>& Args);
	void HandleStatNone(const TArray<FString>& Args);
	void HandleCSMResolution(const TArray<FString>& Args);
	void HandleCSMSplit(const TArray<FString>& Args);
	void HandleCSMDistance(const TArray<FString>& Args);
	void HandleCSMCastingDistance(const TArray<FString>& Args);
	void HandleCSMBlend(const TArray<FString>& Args);
	void HandleCSMBlendRange(const TArray<FString>& Args);
	void HandleShadowBias(const TArray<FString>& Args);
	void HandleShadowFilter(const TArray<FString>& Args);
	void PrintCSMCascadeRanges();

	static int32 TextEditCallback(ImGuiInputTextCallbackData* Data);
};
