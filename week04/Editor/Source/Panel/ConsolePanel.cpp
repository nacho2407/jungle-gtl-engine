#include "ConsolePanel.h"

#include "Editor/Logging/EditorLogBuffer.h"
#include "Core/Logging/LogMacros.h"
#include "imgui.h"

namespace
{
    FString TrimCopy(const FString& Value)
    {
        const size_t Start = Value.find_first_not_of(" \t\r\n");
        if (Start == FString::npos)
        {
            return {};
        }

        const size_t End = Value.find_last_not_of(" \t\r\n");
        return Value.substr(Start, End - Start + 1);
    }

    bool StartsWith(const FString& Value, const char* Prefix)
    {
        const size_t PrefixLength = std::char_traits<char>::length(Prefix);
        return Value.size() >= PrefixLength && Value.compare(0, PrefixLength, Prefix) == 0;
    }

    ImVec4 GetLogColor(ELogVerbosity Verbosity)
    {
        switch (Verbosity)
        {
        case ELogVerbosity::Warning:
            return ImVec4(0.95f, 0.78f, 0.31f, 1.0f);
        case ELogVerbosity::Error:
            return ImVec4(0.95f, 0.38f, 0.35f, 1.0f);
        case ELogVerbosity::Log:
        default:
            return ImVec4(0.83f, 0.85f, 0.88f, 1.0f);
        }
    }
} // namespace

FConsolePanel::FConsolePanel(FEditorLogBuffer* InLogBuffer)
    : LogBuffer(InLogBuffer)
{
    InputBuffer.fill('\0');
}

const wchar_t* FConsolePanel::GetPanelID() const
{
    return L"ConsolePanel";
}

const wchar_t* FConsolePanel::GetDisplayName() const
{
    return L"Console Panel";
}

void FConsolePanel::Draw()
{
    if (!ImGui::Begin("Console Panel", nullptr))
    {
        ImGui::End();
        return;
    }

    DrawToolbar();
    ImGui::Separator();
    DrawLogOutput();
    ImGui::Separator();
    DrawInputRow();

    ImGui::End();
}

void FConsolePanel::DrawToolbar()
{
    if (ImGui::Button("Clear"))
    {
        ExecuteCommand("clear");
    }

    ImGui::SameLine();
    if (ImGui::Button("Help"))
    {
        ExecuteCommand("help");
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &bAutoScroll);
}

void FConsolePanel::DrawLogOutput()
{
    const float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (!ImGui::BeginChild("##ConsoleOutput", ImVec2(0.0f, -FooterHeight), ImGuiChildFlags_Borders))
    {
        ImGui::EndChild();
        return;
    }

    if (LogBuffer == nullptr)
    {
        ImGui::TextUnformatted("Console log buffer is unavailable.");
        ImGui::EndChild();
        return;
    }

    const TArray<FEditorLogEntry>& Entries = LogBuffer->GetLogBuffer();
    for (const FEditorLogEntry& Entry : Entries)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(Entry.Verbosity));
        ImGui::TextWrapped("%s", Entry.Message.c_str());
        ImGui::PopStyleColor();
    }

    if (Entries.size() != LastVisibleLogCount)
    {
        if (bAutoScroll || bScrollToBottom)
        {
            ImGui::SetScrollHereY(1.0f);
        }

        LastVisibleLogCount = static_cast<int32>(Entries.size());
        bScrollToBottom = false;
    }

    ImGui::EndChild();
}

void FConsolePanel::DrawInputRow()
{
    if (bReclaimInputFocus)
    {
        ImGui::SetKeyboardFocusHere();
        bReclaimInputFocus = false;
    }

    ImGui::PushItemWidth(-1.0f);
    const bool bSubmitted =
        ImGui::InputText("##ConsoleInput", InputBuffer.data(), InputBuffer.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();

    if (bSubmitted)
    {
        SubmitInput();
    }
}

void FConsolePanel::SubmitInput()
{
    const FString CommandLine = TrimCopy(InputBuffer.data());
    InputBuffer.fill('\0');
    bReclaimInputFocus = true;

    if (CommandLine.empty())
    {
        return;
    }

    ExecuteCommand(CommandLine);
}

void FConsolePanel::ExecuteCommand(const FString& CommandLine)
{
    const FString TrimmedCommand = TrimCopy(CommandLine);
    if (TrimmedCommand.empty())
    {
        return;
    }

    if (TrimmedCommand == "clear")
    {
        if (LogBuffer != nullptr)
        {
            LogBuffer->Clear();
        }

        UE_LOG(Console, ELogVerbosity::Log, "Console cleared.");
        bScrollToBottom = true;
        return;
    }

    if (TrimmedCommand == "help")
    {
        UE_LOG(Console, ELogVerbosity::Log, "Commands: help, clear, log <text>, warn <text>, error <text>");
        bScrollToBottom = true;
        return;
    }

    if (StartsWith(TrimmedCommand, "warn "))
    {
        UE_LOG(Console, ELogVerbosity::Warning, "%s", TrimmedCommand.substr(5).c_str());
        bScrollToBottom = true;
        return;
    }

    if (StartsWith(TrimmedCommand, "error "))
    {
        UE_LOG(Console, ELogVerbosity::Error, "%s", TrimmedCommand.substr(6).c_str());
        bScrollToBottom = true;
        return;
    }

    if (StartsWith(TrimmedCommand, "log "))
    {
        UE_LOG(Console, ELogVerbosity::Log, "%s", TrimmedCommand.substr(4).c_str());
        bScrollToBottom = true;
        return;
    }

    UE_LOG(Console, ELogVerbosity::Log, "%s", TrimmedCommand.c_str());
    bScrollToBottom = true;
}
