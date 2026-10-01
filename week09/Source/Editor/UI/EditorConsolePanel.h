// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

#include <array>
#include <functional>
#include <sstream>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Core/Logging/LogOutputDevice.h"
#include "Editor/UI/EditorPanel.h"

class FEditorLogBuffer;

// FEditorConsolePanel는 에디터 UI 표시와 입력 처리를 담당합니다.
class FEditorConsolePanel : public FEditorPanel
{
public:
    void Initialize(UEditorEngine* InEditorEngine, FEditorLogBuffer* InLogBuffer);
    virtual void Initialize(UEditorEngine* InEditorEngine) override;
    virtual void Render(float DeltaTime) override;

    void Clear();

private:
    static constexpr int32 LogLevelCount = static_cast<int32>(ELogLevel::Error) + 1;

    std::array<char, 256> InputBuf{};
    std::array<char, 256> SearchBuf{};
    TArray<FString> History;
    int32 HistoryPos = -1;
    bool AutoScroll = true;
    bool ScrollToBottom = true;
    bool bReclaimFocus = false;
    ELogLevel MinimumVisibleLevel = ELogLevel::Verbose;
    std::array<bool, LogLevelCount> LevelVisibility{ true, true, true, true, true };
    FEditorLogBuffer* LogBuffer = nullptr;

    using CommandFn = std::function<void(const TArray<FString>& args)>;
    TMap<FString, CommandFn> Commands;

    bool ShouldDisplayEntry(ELogLevel Level, const FString& Text) const;
    void DrawToolbar();
    void DrawLogOutput();
    void DrawInputRow();
    void RegisterCommand(const FString& Name, CommandFn Fn);
    void ExecCommand(const char* CommandLine);
    static int32 TextEditCallback(ImGuiInputTextCallbackData* Data);
};
