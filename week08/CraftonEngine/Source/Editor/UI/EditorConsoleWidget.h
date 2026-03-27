#pragma once
#include "Core/CoreTypes.h"
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
	static void AddLog(const char* fmt, ...);

	virtual void Render(float DeltaTime) override;

	void Clear()
	{
		for (int32 i = 0; i < Messages.Size; i++) free(Messages[i]);
		Messages.clear();
	}
	static void ClearHistory()
	{
		for (int32 i = 0; i < History.Size; i++) free(History[i]);
		History.clear();
	}

private:
	char InputBuf[256]{};
	static ImVector<char*> Messages;
	static ImVector<char*> History;
	int32 HistoryPos = -1;
	ImGuiTextFilter Filter;
	static bool AutoScroll;
	static bool ScrollToBottom;

	//Command Dispatch System
	using CommandFn = std::function<void(const TArray<FString>& args)>;
	TMap<FString, CommandFn> Commands;

	void RegisterCommand(const FString& Name, CommandFn Fn);
	void ExecCommand(const char* CommandLine);
	static int32 TextEditCallback(ImGuiInputTextCallbackData* Data);
};

#define UE_LOG(Format, ...) \
    FEditorConsoleWidget::AddLog(Format, ##__VA_ARGS__)
