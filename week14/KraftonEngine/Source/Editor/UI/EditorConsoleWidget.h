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
	TMap<FString, CommandFn> Commands;

	void RegisterCommand(const FString& Name, CommandFn Fn);
	void ExecCommand(const char* CommandLine);
	static int32 TextEditCallback(ImGuiInputTextCallbackData* Data);
};
