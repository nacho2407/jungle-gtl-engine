// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorConsolePanel.h"

#include "Core/Logging/LogMacros.h"
#include "Editor/EditorEngine.h"
#include "Editor/Logging/EditorLogBuffer.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Render/Resources/Shadows/ShadowFilterSettings.h"
#include "Render/Resources/Shadows/ShadowMapSettings.h"

#include <algorithm>
#include <cctype>

namespace
{
ImVec4 GetLogTextColor(ELogLevel Level)
{
    switch (Level)
    {
    case ELogLevel::Verbose:
        return ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
    case ELogLevel::Debug:
        return ImVec4(0.96f, 0.96f, 0.96f, 1.0f);
    case ELogLevel::Info:
        return ImVec4(0.35f, 0.86f, 1.0f, 1.0f);
    case ELogLevel::Warning:
        return ImVec4(0.98f, 0.86f, 0.20f, 1.0f);
    case ELogLevel::Error:
        return ImVec4(0.95f, 0.24f, 0.24f, 1.0f);
    default:
        return ImVec4(1, 1, 1, 1);
    }
}

ImVec4 GetLogChipColor(ELogLevel Level)
{
    switch (Level)
    {
    case ELogLevel::Verbose:
        return ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
    case ELogLevel::Debug:
        return ImVec4(0.96f, 0.96f, 0.96f, 1.0f);
    case ELogLevel::Info:
        return ImVec4(0.35f, 0.86f, 1.0f, 1.0f);
    case ELogLevel::Warning:
        return ImVec4(0.98f, 0.86f, 0.20f, 1.0f);
    case ELogLevel::Error:
        return ImVec4(0.95f, 0.24f, 0.24f, 1.0f);
    default:
        return ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    }
}

ImVec4 Dim(const ImVec4& Color, float Factor)
{
    return ImVec4(Color.x * Factor, Color.y * Factor, Color.z * Factor, Color.w);
}

ImVec4 GetLogButtonTextColor(ELogLevel Level)
{
    switch (Level)
    {
    case ELogLevel::Debug:
    case ELogLevel::Info:
    case ELogLevel::Warning:
        return ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    case ELogLevel::Verbose:
    case ELogLevel::Error:
    default:
        return ImVec4(0.97f, 0.97f, 0.97f, 1.0f);
    }
}

ImVec4 Brighten(const ImVec4& Color, float Amount)
{
    return ImVec4(
        (std::min)(1.0f, Color.x + Amount),
        (std::min)(1.0f, Color.y + Amount),
        (std::min)(1.0f, Color.z + Amount),
        Color.w);
}

const char* GetLogLevelDisplayName(ELogLevel Level)
{
    switch (Level)
    {
    case ELogLevel::Verbose:
        return "Verbose";
    case ELogLevel::Debug:
        return "Debug";
    case ELogLevel::Info:
        return "Info";
    case ELogLevel::Warning:
        return "Warning";
    case ELogLevel::Error:
        return "Error";
    default:
        return "Unknown";
    }
}

FString ToLowerAsciiCopy(const FString& Value)
{
    FString Lower = Value;
    std::transform(Lower.begin(), Lower.end(), Lower.begin(),
                   [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
    return Lower;
}
} // namespace

void FEditorConsolePanel::Initialize(UEditorEngine* InEditorEngine, FEditorLogBuffer* InLogBuffer)
{
    FEditorPanel::Initialize(InEditorEngine);
    LogBuffer = InLogBuffer;
    InputBuf.fill('\0');
    SearchBuf.fill('\0');

    RegisterCommand("clear", [this](const TArray<FString>& Args)
                    {
                        (void)Args;
                        Clear();
                    });

    RegisterCommand("stat", [this](const TArray<FString>& Args)
                    {
                        if (EditorEngine == nullptr)
                        {
                            UE_LOG(Console, Error, "EditorEngine is null.");
                            return;
                        }

                        if (Args.size() < 2)
                        {
                            UE_LOG(Console, Warning, "Usage: stat fps | stat memory | stat shadow | stat lightcull | stat none");
                            return;
                        }

                        FOverlayStatSystem& StatSystem = EditorEngine->GetOverlayStatSystem();
                        const FString& SubCommand = Args[1];

                        if (SubCommand == "fps")
                        {
                            StatSystem.ShowFPS(true);
                            UE_LOG(Console, Info, "Overlay stat enabled: fps");
                        }
                        else if (SubCommand == "memory")
                        {
                            StatSystem.ShowMemory(true);
                            UE_LOG(Console, Info, "Overlay stat enabled: memory");
                        }
                        else if (SubCommand == "shadow")
                        {
                            StatSystem.ShowShadow(true);
                            UE_LOG(Console, Info, "Overlay stat enabled: shadow");
                        }
                        else if (SubCommand == "lightcull")
                        {
                            StatSystem.ShowLightCull(true);
                            UE_LOG(Console, Info, "Overlay stat enabled: lightcull");
                        }
                        else if (SubCommand == "none")
                        {
                            StatSystem.HideAll();
                            UE_LOG(Console, Info, "Overlay stat disabled: all");
                        }
                        else
                        {
                            UE_LOG(Console, Error, "Unknown stat command: '%s'", SubCommand.c_str());
                            UE_LOG(Console, Warning, "Usage: stat fps | stat memory | stat shadow | stat lightcull | stat none");
                        }
                    });

    RegisterCommand("shadow_filter", [this](const TArray<FString>& Args)
                    {
                        if (Args.size() < 2)
                        {
                            UE_LOG(Console, Warning, "Usage: shadow_filter NONE | PCF | VSM | ESM");
                            UE_LOG(Console, Info, "Current shadow filter: %s", GetShadowFilterMethodName(GetShadowFilterMethod()));
                            return;
                        }

                        FString FilterName = Args[1];
                        std::transform(FilterName.begin(), FilterName.end(), FilterName.begin(),
                                       [](unsigned char Ch) { return static_cast<char>(std::toupper(Ch)); });

                        if (FilterName == "NONE")
                        {
                            SetShadowFilterMethod(EShadowFilterMethod::None);
                        }
                        else if (FilterName == "PCF")
                        {
                            SetShadowFilterMethod(EShadowFilterMethod::PCF);
                        }
                        else if (FilterName == "VSM")
                        {
                            SetShadowFilterMethod(EShadowFilterMethod::VSM);
                        }
                        else if (FilterName == "ESM")
                        {
                            SetShadowFilterMethod(EShadowFilterMethod::ESM);
                        }
                        else
                        {
                            UE_LOG(Console, Error, "Unknown shadow filter: '%s'", Args[1].c_str());
                            UE_LOG(Console, Warning, "Usage: shadow_filter NONE | PCF | VSM | ESM");
                            return;
                        }

                        UE_LOG(Console, Info, "Shadow filter changed to: %s", GetShadowFilterMethodName(GetShadowFilterMethod()));
                    });

    RegisterCommand("shadow_mode", [this](const TArray<FString>& Args)
    {
        if (Args.size() < 2)
        {
            UE_LOG(Console, Warning, "Usage: shadow_mode STANDARD | PSM | CASCADE");
            UE_LOG(Console, Info, "Current shadow mode: %s", GetShadowMapMethodName(GetShadowMapMethod()));
            return;
        }

        FString MethodName = Args[1];
        std::transform(MethodName.begin(), MethodName.end(), MethodName.begin(),
                       [](unsigned char Ch) { return static_cast<char>(std::toupper(Ch)); });

        if (MethodName == "STANDARD" || MethodName == "SSM")
        {
            SetShadowMapMethod(EShadowMapMethod::Standard);
        }
        else if (MethodName == "PSM")
        {
            SetShadowMapMethod(EShadowMapMethod::PSM);
        }
        else if (MethodName == "CASCADE" || MethodName == "CSM")
        {
            SetShadowMapMethod(EShadowMapMethod::Cascade);
        }
        else
        {
            UE_LOG(Console, Error, "Unknown shadow map method: '%s'", Args[1].c_str());
            UE_LOG(Console, Warning, "Usage: shadow_mode STANDARD | PSM | CASCADE");
            return;
        }

        UE_LOG(Console, Info, "Shadow map method changed to: %s", GetShadowMapMethodName(GetShadowMapMethod()));
    });

    UE_LOG(Console, Info, "Console panel initialized.");
}

void FEditorConsolePanel::Initialize(UEditorEngine* InEditorEngine)
{
    Initialize(InEditorEngine, nullptr);
}

void FEditorConsolePanel::Clear()
{
    if (LogBuffer)
    {
        LogBuffer->Clear();
    }

    ScrollToBottom = true;
}

bool FEditorConsolePanel::ShouldDisplayEntry(ELogLevel Level, const FString& Text) const
{
    const int32 LevelIndex = static_cast<int32>(Level);
    if (LevelIndex < 0 || LevelIndex >= LogLevelCount)
    {
        return false;
    }

    if (!LevelVisibility[LevelIndex] || static_cast<uint8>(Level) < static_cast<uint8>(MinimumVisibleLevel))
    {
        return false;
    }

    const FString SearchText = ToLowerAsciiCopy(SearchBuf.data());
    if (SearchText.empty())
    {
        return true;
    }

    return ToLowerAsciiCopy(Text).find(SearchText) != FString::npos;
}

void FEditorConsolePanel::DrawToolbar()
{
    ImGui::TextUnformatted("Minimum Level ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::BeginCombo("##MinimumLevel", GetLogLevelDisplayName(MinimumVisibleLevel)))
    {
        for (int32 Index = static_cast<int32>(ELogLevel::Verbose); Index <= static_cast<int32>(ELogLevel::Error); ++Index)
        {
            const ELogLevel Level = static_cast<ELogLevel>(Index);
            const bool bSelected = (MinimumVisibleLevel == Level);
            if (ImGui::Selectable(GetLogLevelDisplayName(Level), bSelected))
            {
                MinimumVisibleLevel = Level;
            }

            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    for (int32 Index = static_cast<int32>(ELogLevel::Verbose); Index <= static_cast<int32>(ELogLevel::Error); ++Index)
    {
        ImGui::SameLine();
        const ELogLevel Level = static_cast<ELogLevel>(Index);
        const bool bVisible = LevelVisibility[Index];
        const ImVec4 BaseColor = bVisible ? GetLogChipColor(Level) : Dim(GetLogChipColor(Level), 0.28f);
        const ImVec4 TextColor = bVisible ? GetLogButtonTextColor(Level) : ImVec4(0.62f, 0.62f, 0.62f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, BaseColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Brighten(BaseColor, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Brighten(BaseColor, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_Text, TextColor);

        if (ImGui::Button(GetLogLevelDisplayName(Level), ImVec2(76.0f, 0.0f)))
        {
            LevelVisibility[Index] = !LevelVisibility[Index];
        }

        ImGui::PopStyleColor(4);
    }

    if (ImGui::Button("Clear"))
    {
        Clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &AutoScroll);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##ConsoleSearch", "Search logs...", SearchBuf.data(), SearchBuf.size());
}

void FEditorConsolePanel::DrawLogOutput()
{
    const float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (!ImGui::BeginChild("ScrollingRegion", ImVec2(0, -FooterHeight), false, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::EndChild();
        return;
    }

    if (LogBuffer)
    {
        const TArray<FEditorLogEntry>& Entries = LogBuffer->GetEntries();
        for (const FEditorLogEntry& Entry : Entries)
        {
            if (!ShouldDisplayEntry(Entry.Level, Entry.FormattedMessage))
            {
                continue;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, GetLogTextColor(Entry.Level));
            ImGui::TextUnformatted(Entry.FormattedMessage.c_str());
            ImGui::PopStyleColor();
        }
    }

    if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ScrollToBottom = false;

    ImGui::EndChild();
}

void FEditorConsolePanel::DrawInputRow()
{
    ImGuiInputTextFlags Flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_EscapeClearsAll |
                                ImGuiInputTextFlags_CallbackHistory |
                                ImGuiInputTextFlags_CallbackCompletion;

    if (ImGui::InputText("Input", InputBuf.data(), InputBuf.size(), Flags, &TextEditCallback, this))
    {
        ExecCommand(InputBuf.data());
        InputBuf[0] = '\0';
        bReclaimFocus = true;
    }

    ImGui::SetItemDefaultFocus();
    if (bReclaimFocus)
    {
        ImGui::SetKeyboardFocusHere(-1);
        bReclaimFocus = false;
    }
}

void FEditorConsolePanel::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Console"))
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

void FEditorConsolePanel::RegisterCommand(const FString& Name, CommandFn Fn)
{
    Commands[Name] = Fn;
}

void FEditorConsolePanel::ExecCommand(const char* CommandLine)
{
    if (CommandLine == nullptr || CommandLine[0] == '\0')
    {
        return;
    }

    UE_LOG(Console, Debug, "> %s", CommandLine);
    History.push_back(CommandLine);
    HistoryPos = -1;
    ScrollToBottom = true;

    TArray<FString> Tokens;
    std::istringstream Iss(CommandLine);
    FString Token;
    while (Iss >> Token)
    {
        Tokens.push_back(Token);
    }

    if (Tokens.empty())
    {
        return;
    }

    auto It = Commands.find(Tokens[0]);
    if (It != Commands.end())
    {
        It->second(Tokens);
    }
    else
    {
        UE_LOG(Console, Error, "Unknown command: '%s'", Tokens[0].c_str());
    }
}

int32 FEditorConsolePanel::TextEditCallback(ImGuiInputTextCallbackData* Data)
{
    FEditorConsolePanel* Console = static_cast<FEditorConsolePanel*>(Data->UserData);

    if (Data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        const int32 PrevPos = Console->HistoryPos;
        if (Data->EventKey == ImGuiKey_UpArrow)
        {
            if (Console->HistoryPos == -1)
            {
                Console->HistoryPos = static_cast<int32>(Console->History.size()) - 1;
            }
            else if (Console->HistoryPos > 0)
            {
                Console->HistoryPos--;
            }
        }
        else if (Data->EventKey == ImGuiKey_DownArrow)
        {
            if (Console->HistoryPos != -1 && ++Console->HistoryPos >= static_cast<int32>(Console->History.size()))
            {
                Console->HistoryPos = -1;
            }
        }

        if (PrevPos != Console->HistoryPos)
        {
            const char* HistoryStr = (Console->HistoryPos >= 0) ? Console->History[Console->HistoryPos].c_str() : "";
            Data->DeleteChars(0, Data->BufTextLen);
            Data->InsertChars(0, HistoryStr);
        }
    }

    if (Data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
    {
        const char* WordEnd = Data->Buf + Data->CursorPos;
        const char* WordStart = WordEnd;
        while (WordStart > Data->Buf && WordStart[-1] != ' ')
        {
            WordStart--;
        }

        ImVector<const char*> Candidates;
        for (auto& Pair : Console->Commands)
        {
            const FString& Name = Pair.first;
            if (strncmp(Name.c_str(), WordStart, WordEnd - WordStart) == 0)
            {
                Candidates.push_back(Name.c_str());
            }
        }

        if (Candidates.Size == 1)
        {
            Data->DeleteChars(static_cast<int32>(WordStart - Data->Buf), static_cast<int32>(WordEnd - WordStart));
            Data->InsertChars(Data->CursorPos, Candidates[0]);
            Data->InsertChars(Data->CursorPos, " ");
        }
        else if (Candidates.Size > 1)
        {
            UE_LOG(Console, Info, "Possible completions:");
            for (auto& Candidate : Candidates)
            {
                UE_LOG(Console, Info, "  %s", Candidate);
            }
        }
    }

    return 0;
}
