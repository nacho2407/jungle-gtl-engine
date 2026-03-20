#include "Editor/Core/EditorConsole.h"

void FEditorConsole::AddLog(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Messages.push_back(_strdup(buf));
    if (AutoScroll) ScrollToBottom = true;
}

void FEditorConsole::Draw(const char* Title, bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Title, p_open))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Close Console"))
            *p_open = false;
        ImGui::EndPopup();
    }

    //// TODO: display items starting from the bottom

    if (ImGui::SmallButton("Clear")) { Clear(); }
    //static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t = ImGui::GetTime(); AddLog("Spam %f", t); }

    ImGui::Separator();

    //// Options menu
    if (ImGui::BeginPopup("Options"))
    {
        ImGui::Checkbox("Auto-scroll", &AutoScroll);
        ImGui::EndPopup();
    }

    // Options, Filter
    ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_Tooltip);
    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
    ImGui::Separator();

    const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        for (auto& item : Messages) {
            if (!Filter.PassFilter(item)) continue;

            ImVec4 color;
            bool has_color = false;
            if (strncmp(item, "[ERROR]", 7) == 0) {
                color = ImVec4(1, 0.4f, 0.4f, 1);
                has_color = true;
            }
            else if (strncmp(item, "[WARN]", 6) == 0) {
                color = ImVec4(1, 0.8f, 0.2f, 1);
                has_color = true;
            }
            else if (strncmp(item, "#", 2) == 0) {
                color = ImVec4(1, 0.8f, 0.6f, 1);
                has_color = true;
            }

            if (has_color) {
                ImGui::PushStyleColor(ImGuiCol_Text, color);
            }
            ImGui::TextUnformatted(item);
            if (has_color) {
                ImGui::PopStyleColor();
            }
        }

        if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
            ImGui::SetScrollHereY(1.0f);
        }
        ScrollToBottom = false;
    }
    ImGui::EndChild();
    ImGui::Separator();

    // Input line
    bool reclaim_focus = false;
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue
        | ImGuiInputTextFlags_EscapeClearsAll
        | ImGuiInputTextFlags_CallbackHistory
        | ImGuiInputTextFlags_CallbackCompletion;
    if (ImGui::InputText("Input", InputBuf, sizeof(InputBuf), flags, &TextEditCallback, this)) {
        ExecCommand(InputBuf);
        strcpy_s(InputBuf, "");
        reclaim_focus = true;
    }

    ImGui::SetItemDefaultFocus();
    if (reclaim_focus) {
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();
}

void FEditorConsole::RegisterCommand(const std::string& name, CommandFn fn) {
    commands[name] = fn;
}

void FEditorConsole::ExecCommand(const char* command_line) {
    AddLog("# %s\n", command_line);
    History.push_back(_strdup(command_line));
    HistoryPos = -1;

    // Parse tokens
    std::vector<std::string> tokens;
    std::istringstream iss(command_line);
    std::string token;
    while (iss >> token) tokens.push_back(token);
    if (tokens.empty()) return;

    /*auto it = commands.find(tokens[0]);
    if (it != commands.end()) {
        it->second(tokens);
    }
    else {
        AddLog("[ERROR] Unknown command: '%s'\n", tokens[0].c_str());
    */
}

// History & Tab-Completion Callback____________________________________________________________
int FEditorConsole::TextEditCallback(ImGuiInputTextCallbackData* data) {
    FEditorConsole* console = (FEditorConsole*)data->UserData;

    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        const int prev_pos = console->HistoryPos;
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (console->HistoryPos == -1)
                console->HistoryPos = console->History.Size - 1;
            else if (console->HistoryPos > 0)
                console->HistoryPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow) {
            if (console->HistoryPos != -1 &&
                ++console->HistoryPos >= console->History.Size)
                console->HistoryPos = -1;
        }
        if (prev_pos != console->HistoryPos) {
            const char* history_str = (console->HistoryPos >= 0)
                ? console->History[console->HistoryPos] : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, history_str);
        }
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        // Find last word typed
        const char* word_end = data->Buf + data->CursorPos;
        const char* word_start = word_end;
        while (word_start > data->Buf && word_start[-1] != ' ')
            word_start--;

        // Collect matches from registered commands
        ImVector<const char*> candidates;
        for (auto& pair : console->commands) {
            const std::string& name = pair.first;
            if (strncmp(name.c_str(), word_start, word_end - word_start) == 0)
                candidates.push_back(name.c_str());
        }

        if (candidates.Size == 1) {
            data->DeleteChars(static_cast<int>(word_start - data->Buf), static_cast<int>(word_end - word_start));
            data->InsertChars(data->CursorPos, candidates[0]);
            data->InsertChars(data->CursorPos, " ");
        }
        else if (candidates.Size > 1) {
            console->AddLog("Possible completions:\n");
            for (auto& c : candidates) console->AddLog("  %s\n", c);
        }
    }

    return 0;
}

ImVector<char*> FEditorConsole::Messages;
ImVector<char*> FEditorConsole::History;

bool FEditorConsole::AutoScroll = true;
bool FEditorConsole::ScrollToBottom = true;

