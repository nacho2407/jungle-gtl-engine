#pragma once
#include "Core/CoreTypes.h"
#include <string>
#include <cstdarg> 
#include <functional>
#include <unordered_map>
#include <sstream>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"


class FEditorConsole
{
public:
    static void AddLog(const char* fmt, ...);

    void Draw(const char* Title, bool* p_open);
    void Clear() { Messages.clear(); }

private:
    char InputBuf[256]{};
    static ImVector<char*> Messages;
    static ImVector<char*> History;
    int HistoryPos;
    ImGuiTextFilter Filter;
    static bool AutoScroll;
    static bool ScrollToBottom;

    //Command Dispatch System
    using CommandFn = std::function<void(const std::vector<std::string>& args)>;
    std::unordered_map<std::string, CommandFn> commands;

    void RegisterCommand(const std::string& name, CommandFn fn);
    void ExecCommand(const char* command_line);
    static int TextEditCallback(ImGuiInputTextCallbackData* data);
};

#define UE_LOG(Format, ...) \
    FEditorConsole::AddLog(Format, ##__VA_ARGS__)