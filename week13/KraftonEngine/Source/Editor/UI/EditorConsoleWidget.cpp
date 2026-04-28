#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Object/Object.h"
#include "Render/Types/ShadowSettings.h"
#include "Render/Types/LightFrustumUtils.h"
#include "Render/Types/RenderConstants.h"
#include "Component/CameraComponent.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <set>

namespace
{
	FString ToLower(FString Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(),
			[](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
		return Value;
	}

	TArray<FString> Tokenize(const FString& Text)
	{
		TArray<FString> Tokens;
		std::istringstream Iss(Text);
		FString Token;
		while (Iss >> Token)
		{
			Tokens.push_back(ToLower(Token));
		}
		return Tokens;
	}

	bool StartsWith(const FString& Text, const FString& Prefix)
	{
		return Text.size() >= Prefix.size() && Text.compare(0, Prefix.size(), Prefix) == 0;
	}

	bool CommandStartsWithInput(const FString& CommandName, const FString& Input)
	{
		if (Input.empty())
		{
			return false;
		}
		if (!StartsWith(CommandName, Input))
		{
			return false;
		}
		return CommandName.size() == Input.size() || Input.back() == ' ' || CommandName[Input.size()] == ' ';
	}

	bool TokensMatchCommandPrefix(const TArray<FString>& InputTokens, const TArray<FString>& CommandTokens)
	{
		if (InputTokens.size() > CommandTokens.size())
		{
			return false;
		}
		for (size_t i = 0; i < InputTokens.size(); ++i)
		{
			if (!StartsWith(CommandTokens[i], InputTokens[i]))
			{
				return false;
			}
		}
		return true;
	}

	FString JoinTokens(const TArray<FString>& Tokens, size_t BeginIndex)
	{
		FString Result;
		for (size_t i = BeginIndex; i < Tokens.size(); ++i)
		{
			if (!Result.empty())
			{
				Result += " ";
			}
			Result += Tokens[i];
		}
		return Result;
	}
}

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

// 기존 코드 호환용 static 래퍼: UE_LOG로 위임
void FEditorConsoleWidget::AddLog(const char* fmt, ...)
{
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
	RegisterDefaultCommands();
}

void FEditorConsoleWidget::RegisterDefaultCommands()
{
	RegisterSystemCommands();
	RegisterEditorCommands();
	RegisterDiagnosticsCommands();
	RegisterRenderCommands();
}

void FEditorConsoleWidget::RegisterSystemCommands()
{
	RegisterCommand("help", [this](const TArray<FString>& Args) { HandleHelp(Args); },
		"System", "help [category|command]", "Lists commands or shows detailed help.");
	RegisterCommand("clear", [this](const TArray<FString>& Args) { (void)Args; Clear(); },
		"System", "clear", "Clears the console log.");
}

void FEditorConsoleWidget::RegisterEditorCommands()
{
	RegisterCommand("cb refresh", [this](const TArray<FString>& Args) { HandleContentBrowserRefresh(Args); },
		"Editor", "cb refresh", "Refreshes the content browser.");
	RegisterCommand("cb icon size", [this](const TArray<FString>& Args) { HandleContentBrowserIconSize(Args); },
		"Editor", "cb icon size <20-100>", "Sets the content browser icon size.");
}

void FEditorConsoleWidget::RegisterDiagnosticsCommands()
{
	RegisterCommand("obj list", [this](const TArray<FString>& Args) { HandleObjList(Args); },
		"Diagnostics", "obj list [ClassName]", "Lists live UObject counts and memory by class.");
	RegisterCommand("stat fps", [this](const TArray<FString>& Args) { HandleStatFPS(Args); },
		"Diagnostics", "stat fps", "Shows the FPS overlay stat.");
	RegisterCommand("stat memory", [this](const TArray<FString>& Args) { HandleStatMemory(Args); },
		"Diagnostics", "stat memory", "Shows the memory overlay stat.");
	RegisterCommand("stat shadow", [this](const TArray<FString>& Args) { HandleStatShadow(Args); },
		"Diagnostics", "stat shadow", "Shows the shadow overlay stat.");
	RegisterCommand("stat none", [this](const TArray<FString>& Args) { HandleStatNone(Args); },
		"Diagnostics", "stat none", "Hides all overlay stats.");
}

void FEditorConsoleWidget::RegisterRenderCommands()
{
	RegisterCommand("csm resolution", [this](const TArray<FString>& Args) { HandleCSMResolution(Args); },
		"Render", "csm resolution <size>|reset", "Overrides directional light CSM shadow map resolution.");
	RegisterCommand("csm split", [this](const TArray<FString>& Args) { HandleCSMSplit(Args); },
		"Render", "csm split <0-100>|reset", "Overrides directional light CSM cascade split lambda.");
	RegisterCommand("csm distance", [this](const TArray<FString>& Args) { HandleCSMDistance(Args); },
		"Render", "csm distance <distance>|reset", "Overrides directional light CSM shadow distance.");
	RegisterCommand("csm casting distance", [this](const TArray<FString>& Args) { HandleCSMCastingDistance(Args); },
		"Render", "csm casting distance <distance>|reset", "Overrides directional light CSM caster distance.");
	RegisterCommand("csm blend", [this](const TArray<FString>& Args) { HandleCSMBlend(Args); },
		"Render", "csm blend on|off|reset", "Toggles directional light CSM cascade boundary blending.");
	RegisterCommand("csm blend range", [this](const TArray<FString>& Args) { HandleCSMBlendRange(Args); },
		"Render", "csm blend range <range>|reset", "Overrides directional light CSM boundary blend range.");
	RegisterCommand("shadow bias", [this](const TArray<FString>& Args) { HandleShadowBias(Args); },
		"Render", "shadow bias <bias> [slope_bias]|reset", "Overrides global shadow bias values.");
	RegisterCommand("shadow filter", [this](const TArray<FString>& Args) { HandleShadowFilter(Args); },
		"Render", "shadow filter hard|pcf|vsm|reset", "Overrides shadow filter mode.");
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

	UpdateCompletionCandidates();
	RenderDrawerToolbar();
	const float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing()
		+ (CompletionCandidates.empty() ? 0.0f : (CompletionCandidates.size() * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y));
	RenderLogContents(-FooterHeight);
	ImGui::Separator();
	RenderCompletionCandidates();
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

void FEditorConsoleWidget::RenderCompletionCandidates()
{
	if (CompletionCandidates.empty())
	{
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.85f, 1.0f, 1.0f));
	for (const FString& Candidate : CompletionCandidates)
	{
		ImGui::TextUnformatted(Candidate.c_str());
	}
	ImGui::PopStyleColor();
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
		CompletionCandidates.clear();
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

void FEditorConsoleWidget::RegisterCommand(const FString& Name, CommandFn Fn, const FString& Category, const FString& Usage, const FString& Description)
{
	Commands[ToLower(Name)] = { Fn, Category, Usage, Description };
}

void FEditorConsoleWidget::UpdateCompletionCandidates()
{
	CompletionCandidates = GetCompletionCandidates(InputBuf);
}

TArray<FString> FEditorConsoleWidget::GetCompletionCandidates(const FString& Input) const
{
	FString LowerInput = ToLower(Input);
	while (!LowerInput.empty() && std::isspace(static_cast<unsigned char>(LowerInput.front())))
	{
		LowerInput.erase(LowerInput.begin());
	}
	if (LowerInput.empty())
	{
		return {};
	}

	TArray<FString> Candidates;
	for (const auto& Pair : Commands)
	{
		const FString& Name = Pair.first;
		if (CommandStartsWithInput(Name, LowerInput))
		{
			Candidates.push_back(Name);
		}
	}

	if (Candidates.empty())
	{
		const TArray<FString> InputTokens = Tokenize(LowerInput);
		for (const auto& Pair : Commands)
		{
			const TArray<FString> CommandTokens = Tokenize(Pair.first);
			if (TokensMatchCommandPrefix(InputTokens, CommandTokens))
			{
				Candidates.push_back(Pair.first);
			}
		}
	}

	std::sort(Candidates.begin(), Candidates.end());
	if (Candidates.size() > 8)
	{
		Candidates.resize(8);
	}
	return Candidates;
}

bool FEditorConsoleWidget::TryFindCommand(const TArray<FString>& Tokens, FString& OutCommandName, const FConsoleCommand*& OutCommand, int32& OutConsumedTokens) const
{
	OutCommand = nullptr;
	OutConsumedTokens = 0;

	for (const auto& Pair : Commands)
	{
		const TArray<FString> CommandTokens = Tokenize(Pair.first);
		if (CommandTokens.size() > Tokens.size() || static_cast<int32>(CommandTokens.size()) <= OutConsumedTokens)
		{
			continue;
		}

		bool bMatches = true;
		for (size_t i = 0; i < CommandTokens.size(); ++i)
		{
			if (CommandTokens[i] != Tokens[i])
			{
				bMatches = false;
				break;
			}
		}

		if (bMatches)
		{
			OutCommandName = Pair.first;
			OutCommand = &Pair.second;
			OutConsumedTokens = static_cast<int32>(CommandTokens.size());
		}
	}

	return OutCommand != nullptr;
}

void FEditorConsoleWidget::ExecCommand(const char* CommandLine)
{
	AddLog("# %s\n", CommandLine);
	History.push_back(_strdup(CommandLine));
	HistoryPos = -1;

	TArray<FString> Tokens = Tokenize(CommandLine);
	if (Tokens.empty()) return;

	FString CommandName;
	const FConsoleCommand* Command = nullptr;
	int32 ConsumedTokens = 0;
	if (TryFindCommand(Tokens, CommandName, Command, ConsumedTokens))
	{
		TArray<FString> Args;
		for (size_t i = static_cast<size_t>(ConsumedTokens); i < Tokens.size(); ++i)
		{
			Args.push_back(Tokens[i]);
		}
		Command->Fn(Args);
	}
	else
	{
		AddLog("[ERROR] Unknown command: '%s'\n", JoinTokens(Tokens, 0).c_str());
	}
}

void FEditorConsoleWidget::HandleHelp(const TArray<FString>& Args)
{
	if (!Args.empty())
	{
		const FString Query = JoinTokens(Args, 0);
		auto CommandIt = Commands.find(Query);
		if (CommandIt != Commands.end())
		{
			AddLog("%s\n", Query.c_str());
			AddLog("  Usage: %s\n", CommandIt->second.Usage.c_str());
			AddLog("  Description: %s\n", CommandIt->second.Description.c_str());
			return;
		}

		bool bFoundCategory = false;
		for (const auto& Pair : Commands)
		{
			if (ToLower(Pair.second.Category) == Query)
			{
				if (!bFoundCategory)
				{
					AddLog("[%s]\n", Pair.second.Category.c_str());
					bFoundCategory = true;
				}
				AddLog("  %-24s %s\n", Pair.first.c_str(), Pair.second.Description.c_str());
			}
		}
		if (bFoundCategory)
		{
			return;
		}

		AddLog("[ERROR] Unknown help topic: '%s'\n", Query.c_str());
		return;
	}

	std::set<FString> Categories;
	for (const auto& Pair : Commands)
	{
		Categories.insert(Pair.second.Category);
	}

	for (const FString& Category : Categories)
	{
		AddLog("[%s]\n", Category.c_str());
		TArray<FString> Names;
		for (const auto& Pair : Commands)
		{
			if (Pair.second.Category == Category)
			{
				Names.push_back(Pair.first);
			}
		}
		std::sort(Names.begin(), Names.end());
		for (const FString& Name : Names)
		{
			const FConsoleCommand& Command = Commands.find(Name)->second;
			AddLog("  %-24s %s\n", Name.c_str(), Command.Description.c_str());
		}
	}
	AddLog("Use: help <category> or help <command>\n");
}

void FEditorConsoleWidget::HandleContentBrowserRefresh(const TArray<FString>& Args)
{
	(void)Args;
	if (!EditorEngine)
	{
		AddLog("[ERROR] EditorEngine is null.\n");
		return;
	}

	EditorEngine->RefreshContentBrowser();
	AddLog("Content browser refreshed.\n");
}

void FEditorConsoleWidget::HandleContentBrowserIconSize(const TArray<FString>& Args)
{
	if (!EditorEngine)
	{
		AddLog("[ERROR] EditorEngine is null.\n");
		return;
	}

	if (Args.empty())
	{
		AddLog("cb icon size: %.0f\n", EditorEngine->GetContentBrowserIconSize());
		AddLog("Usage: cb icon size <20-100>\n");
		return;
	}

	const float Size = static_cast<float>(std::atof(Args[0].c_str()));
	if (Size < 20.0f || Size > 100.0f)
	{
		AddLog("[ERROR] Icon size must be 20~100.\n");
		return;
	}

	EditorEngine->SetContentBrowserIconSize(Size);
	AddLog("Content browser icon size set to %.0f.\n", Size);
}

void FEditorConsoleWidget::HandleObjList(const TArray<FString>& Args)
{
	const FString ClassFilter = (!Args.empty()) ? Args[0] : "";

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
			FString Name = ToLower(ClassName);
			FString Filter = ToLower(ClassFilter);
			if (Name.find(Filter) == FString::npos)
				continue;
		}

		auto It = ClassMap.find(ClassName);
		if (It == ClassMap.end())
			ClassMap[ClassName] = { ClassName, Cls->GetSize(), 1 };
		else
			It->second.Count++;
	}

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

void FEditorConsoleWidget::HandleStatFPS(const TArray<FString>& Args)
{
	(void)Args;
	if (!EditorEngine)
	{
		AddLog("[ERROR] EditorEngine is null.\n");
		return;
	}
	EditorEngine->GetOverlayStatSystem().ShowFPS(true);
	AddLog("Overlay stat enabled: fps\n");
}

void FEditorConsoleWidget::HandleStatMemory(const TArray<FString>& Args)
{
	(void)Args;
	if (!EditorEngine)
	{
		AddLog("[ERROR] EditorEngine is null.\n");
		return;
	}
	EditorEngine->GetOverlayStatSystem().ShowMemory(true);
	AddLog("Overlay stat enabled: memory\n");
}

void FEditorConsoleWidget::HandleStatShadow(const TArray<FString>& Args)
{
	(void)Args;
	if (!EditorEngine)
	{
		AddLog("[ERROR] EditorEngine is null.\n");
		return;
	}
	EditorEngine->GetOverlayStatSystem().ShowShadow(true);
	AddLog("Overlay stat enabled: shadow\n");
}

void FEditorConsoleWidget::HandleStatNone(const TArray<FString>& Args)
{
	(void)Args;
	if (!EditorEngine)
	{
		AddLog("[ERROR] EditorEngine is null.\n");
		return;
	}
	EditorEngine->GetOverlayStatSystem().HideAll();
	AddLog("Overlay stat disabled: all\n");
}

void FEditorConsoleWidget::HandleCSMResolution(const TArray<FString>& Args)
{
	FShadowSettings& Settings = FShadowSettings::Get();

	if (Args.empty())
	{
		auto Cur = Settings.GetResolution();
		if (Cur.has_value())
			AddLog("csm resolution: %u\n", Cur.value());
		else
			AddLog("csm resolution: default (%u)\n", FShadowSettings::kDefaultCSMResolution);
		AddLog("Usage: csm resolution <size>|reset\n");
		return;
	}

	if (Args[0] == "reset")
	{
		Settings.ResetResolution();
		AddLog("CSM shadow resolution override reset to default.\n");
	}
	else
	{
		uint32 Res = static_cast<uint32>(std::atoi(Args[0].c_str()));
		if (Res < 64 || Res > 8192) { AddLog("[ERROR] Resolution must be 64~8192.\n"); return; }
		Settings.SetResolution(Res);
		AddLog("CSM shadow resolution override set to %u.\n", Res);
	}
}

void FEditorConsoleWidget::PrintCSMCascadeRanges()
{
	FShadowSettings& Settings = FShadowSettings::Get();
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

	AddLog("csm split: %.1f (0=linear, 100=log)\n", Lambda * 100.0f);
	for (int32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
	{
		AddLog("  C%d: %.3f ~ %.3f\n", i, CascadeRanges[i].NearZ, CascadeRanges[i].FarZ);
	}
}

void FEditorConsoleWidget::HandleCSMSplit(const TArray<FString>& Args)
{
	FShadowSettings& Settings = FShadowSettings::Get();

	if (Args.empty())
	{
		auto Cur = Settings.GetCSMCascadeLambda();
		if (Cur.has_value())
			AddLog("csm split: %.1f\n", Cur.value() * 100.0f);
		else
			AddLog("csm split: default (%.1f)\n", FShadowSettings::kDefaultCSMSplitLambda * 100.0f);

		PrintCSMCascadeRanges();
		AddLog("Usage: csm split <0-100>|reset\n");
		return;
	}

	if (Args[0] == "reset")
	{
		Settings.ResetCSMCascadeLambda();
		AddLog("CSM split override reset to default.\n");
		PrintCSMCascadeRanges();
	}
	else
	{
		float LambdaPercent = static_cast<float>(std::atof(Args[0].c_str()));
		if (LambdaPercent < 0.0f || LambdaPercent > 100.0f)
		{
			AddLog("[ERROR] Split percent must be in range 0 ~ 100.\n");
			return;
		}

		const float Lambda = LambdaPercent * 0.01f;
		Settings.SetCSMCascadeLambda(Lambda);
		AddLog("CSM split override set to %.1f (lambda=%.3f).\n", LambdaPercent, Lambda);
		PrintCSMCascadeRanges();
	}
}

void FEditorConsoleWidget::HandleCSMDistance(const TArray<FString>& Args)
{
	FShadowSettings& Settings = FShadowSettings::Get();

	if (Args.empty())
	{
		auto Cur = Settings.GetCSMShadowDistance();
		if (Cur.has_value())
			AddLog("csm distance: %.3f\n", Cur.value());
		else
			AddLog("csm distance: default (%.3f)\n", FShadowSettings::kDefaultDirectionalShadowDistance);
		AddLog("Usage: csm distance <distance>|reset\n");
		return;
	}

	if (Args[0] == "reset")
	{
		Settings.ResetCSMShadowDistance();
		AddLog("CSM shadow distance override reset to default.\n");
	}
	else
	{
		float Distance = static_cast<float>(std::atof(Args[0].c_str()));
		if (Distance <= 0.0f) { AddLog("[ERROR] Shadow distance must be greater than 0.\n"); return; }
		Settings.SetCSMShadowDistance(Distance);
		AddLog("CSM shadow distance override set to %.3f.\n", Distance);
	}
}

void FEditorConsoleWidget::HandleCSMCastingDistance(const TArray<FString>& Args)
{
	FShadowSettings& Settings = FShadowSettings::Get();

	if (Args.empty())
	{
		auto Cur = Settings.GetCSMShadowCasterDistance();
		if (Cur.has_value())
			AddLog("csm casting distance: %.3f\n", Cur.value());
		else
			AddLog("csm casting distance: default (%.3f)\n", FShadowSettings::kDefaultDirectionalShadowCasterDistance);

		AddLog("Usage: csm casting distance <distance>|reset\n");
		return;
	}

	if (Args[0] == "reset")
	{
		Settings.ResetCSMShadowCasterDistance();
		AddLog("CSM casting distance override reset to default.\n");
	}
	else
	{
		float Distance = static_cast<float>(std::atof(Args[0].c_str()));
		if (Distance <= 0.0f) { AddLog("[ERROR] Casting distance must be greater than 0.\n"); return; }
		Settings.SetCSMShadowCasterDistance(Distance);
		AddLog("CSM casting distance override set to %.3f.\n", Distance);
	}
}

void FEditorConsoleWidget::HandleCSMBlend(const TArray<FString>& Args)
{
	FShadowSettings& Settings = FShadowSettings::Get();

	if (Args.empty())
	{
		auto Cur = Settings.GetCSMBlendEnabled();
		if (Cur.has_value())
			AddLog("csm blend: %s\n", Cur.value() ? "on" : "off");
		else
			AddLog("csm blend: default (%s)\n", FShadowSettings::kDefaultCSMBlendEnabled ? "on" : "off");
		AddLog("Usage: csm blend on|off|reset\n");
		return;
	}

	const FString Arg = ToLower(Args[0]);
	if (Arg == "reset")
	{
		Settings.ResetCSMBlendEnabled();
		AddLog("CSM blend override reset to default (%s).\n", FShadowSettings::kDefaultCSMBlendEnabled ? "on" : "off");
	}
	else if (Arg == "on")
	{
		Settings.SetCSMBlendEnabled(true);
		AddLog("CSM blend set to on.\n");
	}
	else if (Arg == "off")
	{
		Settings.SetCSMBlendEnabled(false);
		AddLog("CSM blend set to off.\n");
	}
	else
	{
		AddLog("[ERROR] Unknown csm blend value: '%s'\n", Args[0].c_str());
		AddLog("Usage: csm blend on|off|reset\n");
	}
}

void FEditorConsoleWidget::HandleCSMBlendRange(const TArray<FString>& Args)
{
	FShadowSettings& Settings = FShadowSettings::Get();

	if (Args.empty())
	{
		auto Cur = Settings.GetCSMBlendRange();
		if (Cur.has_value())
			AddLog("csm blend range: %.3f\n", Cur.value());
		else
			AddLog("csm blend range: default (%.3f)\n", FShadowSettings::kDefaultCSMBlendRange);
		AddLog("Usage: csm blend range <range>|reset\n");
		return;
	}

	if (Args[0] == "reset")
	{
		Settings.ResetCSMBlendRange();
		AddLog("CSM blend range override reset to default.\n");
	}
	else
	{
		float Range = static_cast<float>(std::atof(Args[0].c_str()));
		if (Range < 0.0f)
		{
			AddLog("[ERROR] Blend range must be greater than or equal to 0.\n");
			return;
		}
		Settings.SetCSMBlendRange(Range);
		AddLog("CSM blend range override set to %.3f.\n", Range);
	}
}

void FEditorConsoleWidget::HandleShadowBias(const TArray<FString>& Args)
{
	FShadowSettings& Settings = FShadowSettings::Get();

	if (Args.empty())
	{
		auto Bias = Settings.GetBias();
		auto Slope = Settings.GetSlopeBias();
		const FString BiasText = Bias.has_value() ? std::to_string(Bias.value()) : "per-light";
		const FString SlopeText = Slope.has_value() ? std::to_string(Slope.value()) : "per-light";
		AddLog("shadow bias: %s  slope: %s\n", BiasText.c_str(), SlopeText.c_str());
		AddLog("Usage: shadow bias <bias> [slope_bias]|reset\n");
		return;
	}

	if (Args[0] == "reset")
	{
		Settings.ResetBias();
		Settings.ResetSlopeBias();
		AddLog("Shadow bias override reset to per-light values.\n");
	}
	else
	{
		float Bias = static_cast<float>(std::atof(Args[0].c_str()));
		Settings.SetBias(Bias);
		AddLog("Shadow bias override set to %.6f.\n", Bias);

		if (Args.size() >= 2)
		{
			float Slope = static_cast<float>(std::atof(Args[1].c_str()));
			Settings.SetSlopeBias(Slope);
			AddLog("Shadow slope bias override set to %.6f.\n", Slope);
		}
	}
}

void FEditorConsoleWidget::HandleShadowFilter(const TArray<FString>& Args)
{
	FShadowSettings& Settings = FShadowSettings::Get();

	if (Args.empty())
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
		AddLog("shadow filter: %s\n", Name);
		AddLog("Usage: shadow filter hard|pcf|vsm|reset\n");
		return;
	}

	const FString Arg = ToLower(Args[0]);
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
		AddLog("[ERROR] Unknown filter mode: '%s'\n", Args[0].c_str());
		AddLog("Usage: shadow filter hard|pcf|vsm|reset\n");
	}
}

// History & Tab-Completion Callback____________________________________________________________
int32 FEditorConsoleWidget::TextEditCallback(ImGuiInputTextCallbackData* Data)
{
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
		const TArray<FString> Candidates = Console->GetCompletionCandidates(Data->Buf);
		if (Candidates.empty())
		{
			return 0;
		}

		FString LowerInput = ToLower(Data->Buf);
		while (!LowerInput.empty() && std::isspace(static_cast<unsigned char>(LowerInput.front())))
		{
			LowerInput.erase(LowerInput.begin());
		}

		if (Candidates.size() == 1)
		{
			Data->DeleteChars(0, Data->BufTextLen);
			Data->InsertChars(0, Candidates[0].c_str());
			Data->InsertChars(Data->CursorPos, " ");
			return 0;
		}

		FString Common = Candidates[0];
		for (size_t i = 1; i < Candidates.size(); ++i)
		{
			size_t CommonLen = 0;
			while (CommonLen < Common.size() && CommonLen < Candidates[i].size() && Common[CommonLen] == Candidates[i][CommonLen])
			{
				++CommonLen;
			}
			Common.resize(CommonLen);
		}

		if (Common.size() > LowerInput.size())
		{
			Data->DeleteChars(0, Data->BufTextLen);
			Data->InsertChars(0, Common.c_str());
		}
	}

	return 0;
}

ImVector<char*> FEditorConsoleWidget::History;
