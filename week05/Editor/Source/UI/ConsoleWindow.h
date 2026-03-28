#pragma once
#include "CoreMinimal.h"
#include "imgui.h"
#include <functional>

using FCommandHandler = std::function<void(const char*)>;

class CConsoleWindow
{
public:
	CConsoleWindow();
	~CConsoleWindow();

	void Render();
	void AddLog(const char* Fmt, ...) IM_FMTARGS(2);
	void ClearLog();

	void RegisterCommand(const char* Command);
	void SetCommandHandler(FCommandHandler Handler) { CommandHandler = Handler; }

private:
	void ExecCommand(const char* CommandLine);

	static int32  TextEditCallbackStub(ImGuiInputTextCallbackData* Data);
	int32         TextEditCallback(ImGuiInputTextCallbackData* Data);

	static int32  Stricmp(const char* S1, const char* S2);
	static int32  Strnicmp(const char* S1, const char* S2, int32 N);
	static char* Strdup(const char* S);
	static void  Strtrim(char* S);

	char              InputBuf[256];
	ImVector<char*>   Items;
	ImVector<const char*> Commands;
	ImVector<char*>   History;
	int32               HistoryPos = -1;
	ImGuiTextFilter   Filter;
	bool              AutoScroll = true;
	bool              ScrollToBottom = false;

	FCommandHandler   CommandHandler;
};
