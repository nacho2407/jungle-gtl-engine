#pragma once

#include "Panel.h"

#include "Core/Containers/String.h"

#include <array>

class FEditorLogBuffer;

class FConsolePanel : public IPanel
{
  public:
    explicit FConsolePanel(FEditorLogBuffer* InLogBuffer = nullptr);

    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    bool ShouldOpenByDefault() const override { return true; }
    int32 GetWindowMenuOrder() const override { return 30; }

    void Draw() override;

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
};

