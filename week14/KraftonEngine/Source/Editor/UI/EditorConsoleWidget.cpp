#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Object/Object.h"
#include "Render/Types/ShadowSettings.h"
#include "Render/Types/LightFrustumUtils.h"
#include "Render/Types/RenderConstants.h"
#include "Component/CameraComponent.h"

#include <algorithm>

// ============================================================
// FConsoleLogOutputDevice
// ============================================================

void FConsoleLogOutputDevice::Write(const char* Msg)
{
	Messages.push_back(_strdup(Msg));
	if (AutoScroll) ScrollToBottom = true;
}

void FConsoleLogOutputDevice::Clear()
{
	for (int32 i = 0; i < Messages.Size; i++) free(Messages[i]);
	Messages.clear();
}

// ============================================================
// FEditorConsoleWidget
// ============================================================

// 기존 코드 호환용 static 래퍼 — UE_LOG로 위임
void FEditorConsoleWidget::AddLog(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	FLogManager::Get().LogV(fmt, args);
	va_end(args);
}

void FEditorConsoleWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);

	// 에디터 콘솔을 로그 출력 디바이스로 등록
	FLogManager::Get().AddOutputDevice(&ConsoleDevice);

	RegisterCommand("help", [this](const TArray<FString>& Args)
		{
			if (Args.size() >= 2)
			{
				auto It = CommandHelp.find(Args[1]);
				if (It != CommandHelp.end() && !It->second.empty())
				{
					AddLog("%s\n", It->second.c_str());
				}
				else if (Commands.find(Args[1]) != Commands.end())
				{
					AddLog("%s: no detailed help available.\n", Args[1].c_str());
				}
				else
				{
					AddLog("[ERROR] Unknown command: '%s'\n", Args[1].c_str());
				}
				return;
			}

			AddLog("Available commands:\n");
			for (const auto& Pair : Commands)
			{
				const FString& Name = Pair.first;
				const auto HelpIt = CommandHelp.find(Name);
				if (HelpIt != CommandHelp.end() && !HelpIt->second.empty())
				{
					AddLog("  %s - %s\n", Name.c_str(), HelpIt->second.c_str());
				}
				else
				{
					AddLog("  %s\n", Name.c_str());
				}
			}
			AddLog("Use: help <command>\n");
		}, "Lists console commands. Usage: help [command]");

	RegisterCommand("clear", [this](const TArray<FString>& Args)
		{
			(void)Args;
			Clear();
		}, "Clears the console log. Usage: clear");

	auto ContentBrowserCommand = [this](const TArray<FString>& Args)
		{
			if (!EditorEngine)
			{
				AddLog("[ERROR] EditorEngine is null.\n");
				return;
			}

			if (Args.size() < 2)
			{
				AddLog("Usage: content_browser refresh | content_browser icon_size <20-100>\n");
				return;
			}

			if (Args[1] == "refresh")
			{
				EditorEngine->RefreshContentBrowser();
				AddLog("Content browser refreshed.\n");
			}
			else if (Args[1] == "icon_size")
			{
				if (Args.size() < 3)
				{
					AddLog("content_browser icon_size: %.0f\n", EditorEngine->GetContentBrowserIconSize());
					AddLog("Usage: content_browser icon_size <20-100>\n");
					return;
				}

				const float Size = static_cast<float>(std::atof(Args[2].c_str()));
				if (Size < 20.0f || Size > 100.0f)
				{
					AddLog("[ERROR] Icon size must be 20~100.\n");
					return;
				}

				EditorEngine->SetContentBrowserIconSize(Size);
				AddLog("Content browser icon size set to %.0f.\n", Size);
			}
			else
			{
				AddLog("[ERROR] Unknown content_browser subcommand: '%s'\n", Args[1].c_str());
				AddLog("Usage: content_browser refresh | content_browser icon_size <20-100>\n");
			}
		};
	RegisterCommand("content_browser", ContentBrowserCommand, "Controls Content Browser. Usage: content_browser refresh | content_browser icon_size <20-100>");
	RegisterCommand("cb", ContentBrowserCommand, "Alias for content_browser. Usage: cb refresh | cb icon_size <20-100>");

	RegisterCommand("obj", [this](const TArray<FString>& Args)
		{
			if (Args.size() < 2)
			{
				AddLog("Usage: obj list [ClassName]\n");
				return;
			}

			if (Args[1] == "list")
			{
				const FString ClassFilter = (Args.size() >= 3) ? Args[2] : "";

				// 클래스별 카운트 + 인스턴스 크기 집계
				struct FClassEntry
				{
					const char* Name;
					size_t      ClassSize;
					uint32      Count;
				};
				TMap<const char*, FClassEntry> ClassMap;

				for (UObject* Obj : GUObjectArray)
				{
					if (!Obj) continue;
					UClass* Cls = Obj->GetClass();
					if (!Cls) continue;

					const char* ClassName = Cls->GetName();
					if (!ClassFilter.empty())
					{
						// 대소문자 무시 부분 매칭
						FString Name(ClassName);
						FString Filter(ClassFilter);
						std::transform(Name.begin(), Name.end(), Name.begin(), ::tolower);
						std::transform(Filter.begin(), Filter.end(), Filter.begin(), ::tolower);
						if (Name.find(Filter) == FString::npos)
							continue;
					}

					auto It = ClassMap.find(ClassName);
					if (It == ClassMap.end())
						ClassMap[ClassName] = { ClassName, Cls->GetSize(), 1 };
					else
						It->second.Count++;
				}

				// 카운트 내림차순 정렬
				TArray<FClassEntry> Sorted;
				for (auto& Pair : ClassMap)
					Sorted.push_back(Pair.second);
				std::sort(Sorted.begin(), Sorted.end(),
					[](const FClassEntry& A, const FClassEntry& B) { return A.Count > B.Count; });

				uint32 TotalCount = 0;
				size_t TotalBytes = 0;

				AddLog("%-35s %8s %10s\n", "Class", "Count", "Size(KB)");
				AddLog("-------------------------------------------------------------\n");
				for (auto& E : Sorted)
				{
					size_t Bytes = E.ClassSize * E.Count;
					TotalCount += E.Count;
					TotalBytes += Bytes;
					AddLog("%-35s %8u %10.1f\n", E.Name, E.Count, Bytes / 1024.0);
				}
				AddLog("-------------------------------------------------------------\n");
				AddLog("%-35s %8u %10.1f\n", "TOTAL", TotalCount, TotalBytes / 1024.0);
				AddLog("GUObjectArray capacity: %zu\n", GUObjectArray.capacity());
			}
			else
			{
				AddLog("[ERROR] Unknown obj subcommand: '%s'\n", Args[1].c_str());
				AddLog("Usage: obj list [ClassName]\n");
			}
		}, "Object diagnostics. Usage: obj list [ClassName]");

	RegisterCommand("stat", [this](const TArray<FString>& Args)
		{
			if (EditorEngine == nullptr)
			{
				AddLog("[ERROR] EditorEngine is null.\n");
				return;
			}

			if (Args.size() < 2)
			{
				AddLog("Usage: stat fps | stat memory | stat shadow | stat none\n");
				return;
			}

			FOverlayStatSystem& StatSystem = EditorEngine->GetOverlayStatSystem();
			const FString& SubCommand = Args[1];

			if (SubCommand == "fps")
			{
				StatSystem.ShowFPS(true);
				AddLog("Overlay stat enabled: fps\n");
			}
			else if (SubCommand == "memory")
			{
				StatSystem.ShowMemory(true);
				AddLog("Overlay stat enabled: memory\n");
			}
			else if (SubCommand == "shadow")
			{
				StatSystem.ShowShadow(true);
				AddLog("Overlay stat enabled: shadow\n");
			}
			else if (SubCommand == "none")
			{
				StatSystem.HideAll();
				AddLog("Overlay stat disabled: all\n");
			}
			else
			{
				AddLog("[ERROR] Unknown stat command: '%s'\n", SubCommand.c_str());
				AddLog("Usage: stat fps | stat memory | stat shadow | stat none\n");
			}
		}, "Toggles overlay stats. Usage: stat fps | stat memory | stat shadow | stat none");

	RegisterCommand("CSM_resolution", [this](const TArray<FString>& Args)
		{
			FShadowSettings& Settings = FShadowSettings::Get();

			if (Args.size() < 2)
			{
				auto Cur = Settings.GetResolution();
				if (Cur.has_value())
					AddLog("CSM_shadow_resolution: %u\n", Cur.value());
				else
					AddLog("CSM_shadow_resolution: default (%u)\n", FShadowSettings::kDefaultCSMResolution);
				AddLog("Usage: shadow_resolution <size> | shadow_resolution reset\n");
				return;
			}

			if (Args[1] == "reset")
			{
				Settings.ResetResolution();
				AddLog("CSM_Shadow resolution override reset to default.\n");
			}
			else
			{
				uint32 Res = static_cast<uint32>(std::atoi(Args[1].c_str()));
				if (Res < 64 || Res > 8192) { AddLog("[ERROR] Resolution must be 64~8192.\n"); return; }
				Settings.SetResolution(Res);
				AddLog("CSM_Shadow resolution override set to %u.\n", Res);
			}
		}, "Overrides shadow map resolution. Usage: shadow_resolution <size> | shadow_resolution reset");

	RegisterCommand("CSM_split", [this](const TArray<FString>& Args)
		{
			FShadowSettings& Settings = FShadowSettings::Get();

			auto PrintCascadeRanges = [this, &Settings]()
				{
					if (!EditorEngine)
					{
						AddLog("[ERROR] EditorEngine is null.\n");
						return;
					}

					UCameraComponent* Camera = EditorEngine->GetCamera();
					if (!Camera)
					{
						AddLog("[ERROR] Camera is null.\n");
						return;
					}

					const float CameraNearZ = Camera->GetNearPlane();
					const float CameraFarZ = Camera->GetFarPlane();
					const float ShadowDistance = Settings.GetEffectiveShadowDistance();
					const float ShadowFarZ = (CameraFarZ < ShadowDistance) ? CameraFarZ : ShadowDistance;
					const float Lambda = Settings.GetEffectiveCSMCascadeLambda();

					FLightFrustumUtils::FCascadeRange CascadeRanges[MAX_SHADOW_CASCADES];
					FLightFrustumUtils::ComputeCascadeRanges(
						CameraNearZ, ShadowFarZ, MAX_SHADOW_CASCADES, Lambda, CascadeRanges
					);

					AddLog("CSM_split: %.1f (0=linear, 100=log)\n", Lambda * 100.0f);
					for (int32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
					{
						AddLog("  C%d: %.3f ~ %.3f\n", i, CascadeRanges[i].NearZ, CascadeRanges[i].FarZ);
					}
				};

			if (Args.size() < 2)
			{
				auto Cur = Settings.GetCSMCascadeLambda();
				if (Cur.has_value())
					AddLog("CSM_split: %.1f\n", Cur.value() * 100.0f);
				else
					AddLog("CSM_split: default (%.1f)\n", FShadowSettings::kDefaultCSMSplitLambda * 100.0f);

				PrintCascadeRanges();
				AddLog("Usage: CSM_split <0-100> | CSM_split reset\n");
				return;
			}

			if (Args[1] == "reset")
			{
				Settings.ResetCSMCascadeLambda();
				AddLog("CSM_split override reset to default.\n");
				PrintCascadeRanges();
			}
			else
			{
				float LambdaPercent = static_cast<float>(std::atof(Args[1].c_str()));
				if (LambdaPercent < 0.0f || LambdaPercent > 100.0f)
				{
					AddLog("[ERROR] Split percent must be in range 0 ~ 100.\n");
					return;
				}

				const float Lambda = LambdaPercent * 0.01f;
				Settings.SetCSMCascadeLambda(Lambda);
				AddLog("CSM_split override set to %.1f (lambda=%.3f).\n", LambdaPercent, Lambda);
				PrintCascadeRanges();
			}
		}, "Overrides CSM cascade split lambda. Usage: CSM_split <0-100> | CSM_split reset");

	RegisterCommand("CSM_distance", [this](const TArray<FString>& Args)
		{
			FShadowSettings& Settings = FShadowSettings::Get();

			if (Args.size() < 2)
			{
				auto Cur = Settings.GetCSMShadowDistance();
				if (Cur.has_value())
					AddLog("CSM_shadow_distance: %.3f\n", Cur.value());
				else
					AddLog("CSM_shadow_distance: default (%.3f)\n", FShadowSettings::kDefaultDirectionalShadowDistance);
				AddLog("Usage: shadow_distance <distance> | shadow_distance reset\n");
				return;
			}

			if (Args[1] == "reset")
			{
				Settings.ResetCSMShadowDistance();
				AddLog("CSM_Shadow distance override reset to default.\n");
			}
			else
			{
				float Distance = static_cast<float>(std::atof(Args[1].c_str()));
				if (Distance <= 0.0f) { AddLog("[ERROR] Shadow distance must be greater than 0.\n"); return; }
				Settings.SetCSMShadowDistance(Distance);
				AddLog("CSM_Shadow distance override set to %.3f.\n", Distance);
			}
		}, "Overrides directional shadow distance. Usage: shadow_distance <distance> | shadow_distance reset");

	RegisterCommand("CSM_casting_distance", [this](const TArray<FString>& Args)
		{
			FShadowSettings& Settings = FShadowSettings::Get();

			if (Args.size() < 2)
			{
				auto Cur = Settings.GetCSMShadowCasterDistance();
				if (Cur.has_value())
					AddLog("CSM_casting_distance: %.3f\n", Cur.value());
				else
					AddLog("CSM_casting_distance: default (%.3f)\n", FShadowSettings::kDefaultDirectionalShadowCasterDistance);

				AddLog("Usage: CSM_casting_distance <distance> | CSM_casting_distance reset\n");
				return;
			}

			if (Args[1] == "reset")
			{
				Settings.ResetCSMShadowCasterDistance();
				AddLog("CSM_casting_distance override reset to default.\n");
			}
			else
			{
				float Distance = static_cast<float>(std::atof(Args[1].c_str()));
				if (Distance <= 0.0f) { AddLog("[ERROR] Casting distance must be greater than 0.\n"); return; }
				Settings.SetCSMShadowCasterDistance(Distance);
				AddLog("CSM_casting_distance override set to %.3f.\n", Distance);
			}
		}, "Overrides directional shadow caster distance. Usage: CSM_casting_distance <distance> | CSM_casting_distance reset");

	RegisterCommand("CSM_blend", [this](const TArray<FString>& Args)
		{
			FShadowSettings& Settings = FShadowSettings::Get();

			if (Args.size() < 2)
			{
				auto Cur = Settings.GetCSMBlendEnabled();
				if (Cur.has_value())
					AddLog("CSM_blend: %s\n", Cur.value() ? "on" : "off");
				else
					AddLog("CSM_blend: default (%s)\n", FShadowSettings::kDefaultCSMBlendEnabled ? "on" : "off");
				AddLog("Usage: CSM_blend on|off|reset\n");
				return;
			}

			FString Arg = Args[1];
			std::transform(Arg.begin(), Arg.end(), Arg.begin(), ::tolower);

			if (Arg == "reset")
			{
				Settings.ResetCSMBlendEnabled();
				AddLog("CSM_blend override reset to default (%s).\n", FShadowSettings::kDefaultCSMBlendEnabled ? "on" : "off");
			}
			else if (Arg == "on")
			{
				Settings.SetCSMBlendEnabled(true);
				AddLog("CSM_blend set to on.\n");
			}
			else if (Arg == "off")
			{
				Settings.SetCSMBlendEnabled(false);
				AddLog("CSM_blend set to off.\n");
			}
			else
			{
				AddLog("[ERROR] Unknown CSM_blend value: '%s'\n", Args[1].c_str());
				AddLog("Usage: CSM_blend on|off|reset\n");
			}
		}, "Toggles CSM cascade boundary blending. Usage: CSM_blend on|off|reset");

	RegisterCommand("CSM_blend_range", [this](const TArray<FString>& Args)
		{
			FShadowSettings& Settings = FShadowSettings::Get();

			if (Args.size() < 2)
			{
				auto Cur = Settings.GetCSMBlendRange();
				if (Cur.has_value())
					AddLog("CSM_blend_range: %.3f\n", Cur.value());
				else
					AddLog("CSM_blend_range: default (%.3f)\n", FShadowSettings::kDefaultCSMBlendRange);
				AddLog("Usage: CSM_blend_range <range> | CSM_blend_range reset\n");
				return;
			}

			if (Args[1] == "reset")
			{
				Settings.ResetCSMBlendRange();
				AddLog("CSM_blend_range override reset to default.\n");
			}
			else
			{
				float Range = static_cast<float>(std::atof(Args[1].c_str()));
				if (Range < 0.0f)
				{
					AddLog("[ERROR] Blend range must be greater than or equal to 0.\n");
					return;
				}
				Settings.SetCSMBlendRange(Range);
				AddLog("CSM_blend_range override set to %.3f.\n", Range);
			}
		}, "Overrides CSM boundary blend range. Usage: CSM_blend_range <range> | CSM_blend_range reset");

	RegisterCommand("shadow_bias", [this](const TArray<FString>& Args)
		{
			FShadowSettings& Settings = FShadowSettings::Get();

			if (Args.size() < 2)
			{
				auto Bias = Settings.GetBias();
				auto Slope = Settings.GetSlopeBias();
				AddLog("shadow_bias: %s  slope: %s\n",
					Bias.has_value() ? std::to_string(Bias.value()).c_str() : "per-light",
					Slope.has_value() ? std::to_string(Slope.value()).c_str() : "per-light");
				AddLog("Usage: shadow_bias <bias> [slope_bias] | shadow_bias reset\n");
				return;
			}

			if (Args[1] == "reset")
			{
				Settings.ResetBias();
				Settings.ResetSlopeBias();
				AddLog("Shadow bias override reset to per-light values.\n");
			}
			else
			{
				float Bias = static_cast<float>(std::atof(Args[1].c_str()));
				Settings.SetBias(Bias);
				AddLog("Shadow bias override set to %.6f.\n", Bias);

				if (Args.size() >= 3)
				{
					float Slope = static_cast<float>(std::atof(Args[2].c_str()));
					Settings.SetSlopeBias(Slope);
					AddLog("Shadow slope bias override set to %.6f.\n", Slope);
				}
			}
		}, "Overrides shadow bias. Usage: shadow_bias <bias> [slope_bias] | shadow_bias reset");

	RegisterCommand("shadow_filter", [this](const TArray<FString>& Args)
		{
			FShadowSettings& Settings = FShadowSettings::Get();

			if (Args.size() < 2)
			{
				auto Mode = Settings.GetFilterMode();
				const char* Name = "default (Hard)";
				if (Mode.has_value())
				{
					switch (Mode.value())
					{
					case EShadowFilterMode::Hard: Name = "Hard"; break;
					case EShadowFilterMode::PCF:  Name = "PCF";  break;
					case EShadowFilterMode::VSM:  Name = "VSM";  break;
					}
				}
				AddLog("shadow_filter: %s\n", Name);
				AddLog("Usage: shadow_filter Hard|PCF|VSM | shadow_filter reset\n");
				return;
			}

			FString Arg = Args[1];
			// 대소문자 무관 비교
			std::transform(Arg.begin(), Arg.end(), Arg.begin(), ::tolower);

			if (Arg == "reset")
			{
				Settings.ResetFilterMode();
				AddLog("Shadow filter mode reset to default (Hard).\n");
			}
			else if (Arg == "hard")
			{
				Settings.SetFilterMode(EShadowFilterMode::Hard);
				AddLog("Shadow filter mode set to Hard.\n");
			}
			else if (Arg == "pcf")
			{
				Settings.SetFilterMode(EShadowFilterMode::PCF);
				AddLog("Shadow filter mode set to PCF.\n");
			}
			else if (Arg == "vsm")
			{
				Settings.SetFilterMode(EShadowFilterMode::VSM);
				AddLog("Shadow filter mode set to VSM.\n");
			}
			else
			{
				AddLog("[ERROR] Unknown filter mode: '%s'\n", Args[1].c_str());
				AddLog("Usage: shadow_filter Hard|PCF|VSM | shadow_filter reset\n");
			}
		}, "Overrides shadow filter mode. Usage: shadow_filter Hard|PCF|VSM | shadow_filter reset");
}

void FEditorConsoleWidget::Shutdown()
{
	FLogManager::Get().RemoveOutputDevice(&ConsoleDevice);
}

void FEditorConsoleWidget::Clear()
{
	ConsoleDevice.Clear();
}

void FEditorConsoleWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Console"))
	{
		ImGui::End();
		return;
	}

	RenderDrawerToolbar();
	const float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	RenderLogContents(-FooterHeight);
	ImGui::Separator();
	RenderInputLine("Input");

	ImGui::End();
}

void FEditorConsoleWidget::RenderDrawerToolbar()
{
	if (ImGui::BeginPopup("ConsoleOptions"))
	{
		ImGui::Checkbox("Auto-scroll", &ConsoleDevice.AutoScroll);
		ImGui::EndPopup();
	}

	if (ImGui::SmallButton("Clear")) { Clear(); }
	ImGui::SameLine();
	if (ImGui::SmallButton("Options"))
	{
		ImGui::OpenPopup("ConsoleOptions");
	}
	ImGui::SameLine();
	Filter.Draw("Filter (\"incl,-excl\")", 180.0f);
}

void FEditorConsoleWidget::RenderLogContents(float Height)
{
	if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, Height), false, ImGuiWindowFlags_HorizontalScrollbar)) {
		for (int32 i = 0; i < ConsoleDevice.GetMessageCount(); ++i) {
			char* Item = ConsoleDevice.GetMessageAt(i);
			if (!Filter.PassFilter(Item)) continue;

			ImVec4 Color;
			bool bHasColor = false;
			if (strncmp(Item, "[ERROR]", 7) == 0) {
				Color = ImVec4(1, 0.4f, 0.4f, 1);
				bHasColor = true;
			}
			else if (strncmp(Item, "[WARN]", 6) == 0) {
				Color = ImVec4(1, 0.8f, 0.2f, 1);
				bHasColor = true;
			}
			else if (strncmp(Item, "#", 1) == 0) {
				Color = ImVec4(1, 0.8f, 0.6f, 1);
				bHasColor = true;
			}

			if (bHasColor) {
				ImGui::PushStyleColor(ImGuiCol_Text, Color);
			}
			ImGui::TextUnformatted(Item);
			if (bHasColor) {
				ImGui::PopStyleColor();
			}
		}

		if (ConsoleDevice.ScrollToBottom || (ConsoleDevice.AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
			ImGui::SetScrollHereY(1.0f);
		}
		ConsoleDevice.ScrollToBottom = false;
	}
	ImGui::EndChild();
}

void FEditorConsoleWidget::RenderInputLine(const char* Label, float Width, bool bFocusInput)
{
	if (bFocusInput)
	{
		ImGui::SetKeyboardFocusHere();
	}

	if (Width > 0.0f)
	{
		ImGui::PushItemWidth(Width);
	}

	// 콘솔 전환 키는 명령어 문자로 입력되지 않도록 필터링한다.
	bool bReclaimFocus = false;
	ImGuiInputTextFlags Flags = ImGuiInputTextFlags_EnterReturnsTrue
		| ImGuiInputTextFlags_EscapeClearsAll
		| ImGuiInputTextFlags_CallbackHistory
		| ImGuiInputTextFlags_CallbackCompletion
		| ImGuiInputTextFlags_CallbackCharFilter;
	if (ImGui::InputText(Label, InputBuf, sizeof(InputBuf), Flags, &TextEditCallback, this)) {
		ExecCommand(InputBuf);
		strcpy_s(InputBuf, "");
		bReclaimFocus = true;
	}

	if (Width > 0.0f)
	{
		ImGui::PopItemWidth();
	}

	ImGui::SetItemDefaultFocus();
	if (bReclaimFocus) {
		ImGui::SetKeyboardFocusHere(-1);
	}
}

const char* FEditorConsoleWidget::GetLatestLogMessage() const
{
	const int32 MessageCount = ConsoleDevice.GetMessageCount();
	return MessageCount > 0 ? ConsoleDevice.GetMessageAt(MessageCount - 1) : "";
}

void FEditorConsoleWidget::RegisterCommand(const FString& Name, CommandFn Fn, const FString& Help) {
	Commands[Name] = Fn;
	CommandHelp[Name] = Help;
}

void FEditorConsoleWidget::ExecCommand(const char* CommandLine) {
	AddLog("# %s\n", CommandLine);
	History.push_back(_strdup(CommandLine));
	HistoryPos = -1;

	TArray<FString> Tokens;
	std::istringstream Iss(CommandLine);
	FString Token;
	while (Iss >> Token) Tokens.push_back(Token);
	if (Tokens.empty()) return;

	auto It = Commands.find(Tokens[0]);
	if (It != Commands.end()) {
		It->second(Tokens);
	}
	else {
		AddLog("[ERROR] Unknown command: '%s'\n", Tokens[0].c_str());
	}
}

// History & Tab-Completion Callback____________________________________________________________
int32 FEditorConsoleWidget::TextEditCallback(ImGuiInputTextCallbackData* Data) {
	FEditorConsoleWidget* Console = (FEditorConsoleWidget*)Data->UserData;

	if (Data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
		if (Data->EventChar == '`' || Data->EventChar == '~') {
			return 1;
		}
		return 0;
	}

	if (Data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
		const int32 PrevPos = Console->HistoryPos;
		if (Data->EventKey == ImGuiKey_UpArrow) {
			if (Console->HistoryPos == -1)
				Console->HistoryPos = Console->History.Size - 1;
			else if (Console->HistoryPos > 0)
				Console->HistoryPos--;
		}
		else if (Data->EventKey == ImGuiKey_DownArrow) {
			if (Console->HistoryPos != -1 &&
				++Console->HistoryPos >= Console->History.Size)
				Console->HistoryPos = -1;
		}
		if (PrevPos != Console->HistoryPos) {
			const char* HistoryStr = (Console->HistoryPos >= 0)
				? Console->History[Console->HistoryPos] : "";
			Data->DeleteChars(0, Data->BufTextLen);
			Data->InsertChars(0, HistoryStr);
		}
	}

	if (Data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
		// Find last word typed
		const char* WordEnd = Data->Buf + Data->CursorPos;
		const char* WordStart = WordEnd;
		while (WordStart > Data->Buf && WordStart[-1] != ' ')
			WordStart--;

		// Collect matches from registered commands
		ImVector<const char*> Candidates;
		for (auto& Pair : Console->Commands) {
			const FString& Name = Pair.first;
			if (strncmp(Name.c_str(), WordStart, WordEnd - WordStart) == 0)
				Candidates.push_back(Name.c_str());
		}

		if (Candidates.Size == 1) {
			Data->DeleteChars(static_cast<int32>(WordStart - Data->Buf), static_cast<int32>(WordEnd - WordStart));
			Data->InsertChars(Data->CursorPos, Candidates[0]);
			Data->InsertChars(Data->CursorPos, " ");
		}
		else if (Candidates.Size > 1) {
			Console->AddLog("Possible completions:\n");
			for (auto& C : Candidates) Console->AddLog("  %s\n", C);
		}
	}

	return 0;
}

ImVector<char*> FEditorConsoleWidget::History;
