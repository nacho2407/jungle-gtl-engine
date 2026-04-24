// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Core/CoreTypes.h"
#include <cstdarg>
#include <functional>
#include <sstream>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Editor/UI/EditorPanel.h"

// FEditorConsolePanel는 에디터 UI 표시와 입력 처리를 담당합니다.
class FEditorConsolePanel : public FEditorPanel
{
public:
    static void AddLog(const char* fmt, ...);
    virtual void Initialize(UEditorEngine* InEditorEngine) override;
    virtual void Render(float DeltaTime) override;

    void Clear()
    {
        for (int32 i = 0; i < Messages.Size; i++)
            free(Messages[i]);
        Messages.clear();
    }
    static void ClearHistory()
    {
        for (int32 i = 0; i < History.Size; i++)
            free(History[i]);
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

    // Command Dispatch System
    using CommandFn = std::function<void(const TArray<FString>& args)>;
    TMap<FString, CommandFn> Commands;

    void RegisterCommand(const FString& Name, CommandFn Fn);
    void ExecCommand(const char* CommandLine);
    static int32 TextEditCallback(ImGuiInputTextCallbackData* Data);
};

#define UE_LOG(Format, ...) \
    FEditorConsolePanel::AddLog(Format, ##__VA_ARGS__)
