#pragma once

#include "Core/CoreMinimal.h"

#include "Panel.h"


#include <array>

class FEditorLogBuffer;

class FConsolePanel : public IPanel
{
  public:
    explicit FConsolePanel(FEditorLogBuffer* InLogBuffer = nullptr);

    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    bool ShouldOpenByDefault() const override { return false; }
    int32 GetWindowMenuOrder() const override { return 30; }

    void Draw() override;

    void RequestInputFocus() { bReclaimInputFocus = true; }
    bool IsInputFocused() const { return bInputFocused; }

  private:
    void DrawToolbar();
    void DrawLogOutput();
    void DrawInputRow();
    void SubmitInput();
    void ExecuteCommand(const FString& CommandLine);

  private:
    FEditorLogBuffer* LogBuffer = nullptr;
    std::array<char, 256> InputBuffer{};
    int32 LastVisibleLogCount = 0;
    bool bAutoScroll = true;
    bool bScrollToBottom = false;
    bool bReclaimInputFocus = false;
    bool bInputFocused = false;
};

