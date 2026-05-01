#pragma once
#include "Core/CoreMinimal.h"
#include <cstdarg>
#include <functional>
#include <sstream>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Editor/UI/EditorWidget.h"

class FEditorConsoleWidget : public FEditorWidget
{
public:
	FEditorConsoleWidget();
	~FEditorConsoleWidget();

	static void AddLog(const char* fmt, ...);

	virtual void Render(float DeltaTime) override;
	void RenderDrawerToolbar();
	void RenderLogContents(float Height = 0.0f);
	void RenderInputLine(const char* Id, float Width, bool bRequestFocus);
	void Clear();
	static void ClearHistory();

private:
	char InputBuf[256]{};
	static ImVector<char*> Messages;
	static ImVector<char*> History;
	int32 HistoryPos = -1;
	ImGuiTextFilter Filter;
	static bool AutoScroll;
	static bool ScrollToBottom;

	// 백틱(`) 키로 포커스 요청 시 true — 다음 InputText 렌더링 직전에 SetKeyboardFocusHere 호출
	bool bRequestFocusInput = false;

	//Command Dispatch System
	using CommandFn = std::function<void(const TArray<FString>& args)>;
	TMap<FString, CommandFn> Commands;
	TMap<FString, FString> CommandDescriptions;

	void RegisterCommand(const FString& Name, CommandFn Fn);
	void RegisterCommand(const FString& Name, const FString& Description, CommandFn Fn);
	void ExecCommand(const char* CommandLine);
	static int32 TextEditCallback(ImGuiInputTextCallbackData* Data);

private:
	void CmdHelp(const TArray<FString>& Args);
	void CmdCommands(const TArray<FString>& Args);
	void CmdSuggest(const TArray<FString>& Args);
	void CmdStat(const TArray<FString>& Args);
    void CmdShadow(const TArray<FString>& Args);
	void PrintHistoryStats();
	void PrintCommandList(const FString& Prefix = "");
	FString FindClosestCommand(const FString& Query) const;
	TArray<FString> BuildCommandSuggestions(const FString& Query) const;
	void RenderCommandSuggestions(const char* Id, const ImVec2& InputMin, const ImVec2& InputSize);
};
