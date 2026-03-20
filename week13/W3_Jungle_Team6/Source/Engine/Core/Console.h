#pragma once
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include <functional>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>

struct Console {
	bool AutoScroll;
	bool ScrollToBottom;

	int HistoryPos;

	ImVector<char*> Items;
	ImVector<char*> History;

	char InputBuf[256];
	
	ImGuiTextFilter Filter;

	Console() {
		// Initialize input buffer with zeroes
		memset(InputBuf, 0, sizeof(InputBuf));
		HistoryPos = -1; // -1 = new line
		AutoScroll = true;
		ScrollToBottom = false;
	}

	void AddLog(const char* fmt, ...) {
		char buf[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);
		Items.push_back(_strdup(buf));
		if (AutoScroll) ScrollToBottom = true;
	}

	void Draw(const char* title, bool* p_open) {
		ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(title, p_open)) {
			ImGui::End();
			return;
		}

		Filter.Draw("Filter", -100.0f);
		ImGui::Separator();

		const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar)) {
			for (auto& item : Items) {
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

	// Command Dispatch System__________________________________________________________
	using CommandFn = std::function<void(const std::vector<std::string>& args)>;
	std::unordered_map<std::string, CommandFn> commands;

	void RegisterCommand(const std::string& name, CommandFn fn) {
		commands[name] = fn;
	}

	void ExecCommand(const char* command_line) {
		AddLog("# %s\n", command_line);
		History.push_back(_strdup(command_line));
		HistoryPos = -1;

		// Parse tokens
		std::vector<std::string> tokens;
		std::istringstream iss(command_line);
		std::string token;
		while (iss >> token) tokens.push_back(token);
		if (tokens.empty()) return;

		auto it = commands.find(tokens[0]);
		if (it != commands.end()) {
			it->second(tokens);
		}
		else {
			AddLog("[ERROR] Unknown command: '%s'\n", tokens[0].c_str());
		}
	}

	// History & Tab-Completion Callback____________________________________________________________
	static int TextEditCallback(ImGuiInputTextCallbackData* data) {
		Console* console = (Console*)data->UserData;

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

	// Commands_____________________________________________________________________
	// Rendering
	//registercommand("r.wireframe", [&](auto& args) {
	//	renderer.wireframe = !renderer.wireframe;
	//	console.addlog("wireframe: %s\n", renderer.wireframe ? "on" : "off");
	//	});

	//registercommand("r.vsync", [&](auto& args) {
	//	bool enable = args.size() > 1 && args[1] == "1";
	//	renderer.setvsync(enable);
	//	});

	//// scene
	//registercommand("scene.load", [&](auto& args) {
	//	if (args.size() < 2) { console.addlog("[warn] usage: scene.load <path>\n"); return; }
	//	scenemanager.load(args[1]);
	//	});

	//// diagnostics
	//registercommand("stats", [&](auto& args) {
	//	console.addlog("fps: %.1f | draw calls: %d | triangles: %dk\n",
	//		stats.fps, stats.drawcalls, stats.triangles / 1000);
	//	});

	//registercommand("help", [&](auto& args) {
	//	for (auto& [name, _] : console.commands)
	//		console.addlog("  %s\n", name.c_str());
	//	});
};