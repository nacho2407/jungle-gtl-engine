#include "ConsolePanel.h"

#include "Core/Logging/LogMacros.h"
#include "Core/Path.h"
#include "Editor/Editor.h"
#include "Editor/EditorContext.h"
#include "Editor/Logging/EditorLogBuffer.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/EngineStatics.h"
#include "Engine/Game/Actor.h"
#include "Engine/Game/UnknownActor.h"
#include "Engine/Game/StaticMeshActor.h" // 통합된 액터 사용
#include "Engine/Scene.h"
#include "Content/EditorContentIndex.h"
#include "Renderer/Types/ViewMode.h"
#include "Viewport/EditorViewportClient.h"
#include "Viewport/Navigation/ViewportNavigationController.h"
#include "Viewport/RenderSetting/ViewportRenderSetting.h"
#include "Viewport/Selection/ViewportSelectionController.h"
#include "imgui.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

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

    FString ToLowerAsciiCopy(const FString& Value)
    {
        FString Lower = Value;
        std::transform(
            Lower.begin(), Lower.end(), Lower.begin(),
            [](char Character)
            {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
            });
        return Lower;
    }

    bool EqualsIgnoreCase(const FString& Left, const FString& Right)
    {
        return ToLowerAsciiCopy(Left) == ToLowerAsciiCopy(Right);
    }

    TArray<FString> TokenizeCommandLine(const FString& CommandLine)
    {
        TArray<FString> Tokens;
        FString         CurrentToken;
        bool            bInQuotes = false;

        for (char Character : CommandLine)
        {
            if (Character == '"')
            {
                bInQuotes = !bInQuotes;
                continue;
            }

            if (!bInQuotes &&
                (Character == ' ' || Character == '\t' || Character == '\r' || Character == '\n'))
            {
                if (!CurrentToken.empty())
                {
                    Tokens.push_back(CurrentToken);
                    CurrentToken.clear();
                }
                continue;
            }

            CurrentToken.push_back(Character);
        }

        if (!CurrentToken.empty())
        {
            Tokens.push_back(CurrentToken);
        }

        return Tokens;
    }

    bool TryParseInt32(const FString& Value, int32& OutValue)
    {
        char* EndPtr = nullptr;
        const long ParsedValue = std::strtol(Value.c_str(), &EndPtr, 10);
        if (EndPtr == Value.c_str() || (EndPtr != nullptr && *EndPtr != '\0'))
        {
            return false;
        }

        OutValue = static_cast<int32>(ParsedValue);
        return true;
    }

    bool TryParseUInt32(const FString& Value, uint32& OutValue)
    {
        char* EndPtr = nullptr;
        const unsigned long ParsedValue = std::strtoul(Value.c_str(), &EndPtr, 10);
        if (EndPtr == Value.c_str() || (EndPtr != nullptr && *EndPtr != '\0'))
        {
            return false;
        }

        OutValue = static_cast<uint32>(ParsedValue);
        return true;
    }

    bool TryParseFloat(const FString& Value, float& OutValue)
    {
        char* EndPtr = nullptr;
        OutValue = std::strtof(Value.c_str(), &EndPtr);
        if (EndPtr == Value.c_str() || (EndPtr != nullptr && *EndPtr != '\0'))
        {
            return false;
        }

        return true;
    }

    bool TryParseToggle(const FString& Value, bool& OutEnabled)
    {
        const FString LowerValue = ToLowerAsciiCopy(Value);
        if (LowerValue == "on" || LowerValue == "true" || LowerValue == "1")
        {
            OutEnabled = true;
            return true;
        }

        if (LowerValue == "off" || LowerValue == "false" || LowerValue == "0")
        {
            OutEnabled = false;
            return true;
        }

        return false;
    }

    FString PathToUtf8String(const std::filesystem::path& Path)
    {
        const std::u8string Utf8Path = Path.u8string();
        return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
    }

    std::filesystem::path Utf8PathToFilesystemPath(const FString& Value)
    {
        std::u8string Utf8Value;
        Utf8Value.reserve(Value.size());
        for (char Character : Value)
        {
            Utf8Value.push_back(static_cast<char8_t>(Character));
        }

        return std::filesystem::path(Utf8Value);
    }

    std::filesystem::path NormalizeScenePath(const std::filesystem::path& InPath)
    {
        if (InPath.empty())
        {
            return {};
        }

        if (InPath.has_extension() && _wcsicmp(InPath.extension().c_str(), L".Scene") == 0)
        {
            return InPath;
        }

        std::filesystem::path NormalizedPath = InPath;
        NormalizedPath.replace_extension(L".Scene");
        return NormalizedPath;
    }

    std::filesystem::path ResolveVirtualPathToAbsolute(const FString& Input)
    {
        if (Input.rfind("/Game/", 0) != 0)
        {
            return {};
        }

        std::filesystem::path RelativePath = Utf8PathToFilesystemPath(Input.substr(6));
        return FPaths::Combine(FPaths::AppContentDir(), RelativePath);
    }

    std::filesystem::path ResolveSceneCommandPath(const FString& Input, const FEditor* Editor)
    {
        if (Input.empty())
        {
            return {};
        }

        std::filesystem::path Path = ResolveVirtualPathToAbsolute(Input);
        if (Path.empty())
        {
            Path = Utf8PathToFilesystemPath(Input);
            if (Path.is_relative())
            {
                const std::filesystem::path BaseDirectory =
                    Editor != nullptr ? Editor->GetDefaultSceneDirectory() : FPaths::AppRoot();
                Path = FPaths::Combine(BaseDirectory, Path);
            }
        }

        return NormalizeScenePath(Path);
    }

    FString GetObjectName(const UObject* Object)
    {
        if (Object == nullptr)
        {
            return "<null>";
        }

        FString Name = Object->Name.ToFString();
        if (Name.empty())
        {
            Name = Object->GetTypeName();
        }
        return Name;
    }

    const char* ViewModeToString(EViewModeIndex ViewMode)
    {
        switch (ViewMode)
        {
        case EViewModeIndex::VMI_Unlit:
            return "unlit";
        case EViewModeIndex::VMI_Wireframe:
            return "wireframe";
        case EViewModeIndex::VMI_Lit:
        default:
            return "lit";
        }
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

    const char* ContentItemTypeToString(EContentBrowserItemType ItemType)
    {
        switch (ItemType)
        {
        case EContentBrowserItemType::Folder:
            return "Folder";
        case EContentBrowserItemType::Scene:
            return "Scene";
        case EContentBrowserItemType::Texture:
            return "Texture";
        case EContentBrowserItemType::Font:
            return "Font";
        case EContentBrowserItemType::UnknownFile:
        default:
            return "Unknown";
        }
    }

    bool ActorMatchesToken(const AActor* Actor, const FString& Token)
    {
        if (Actor == nullptr)
        {
            return false;
        }

        uint32 ParsedUUID = 0;
        if (TryParseUInt32(Token, ParsedUUID) && Actor->UUID == ParsedUUID)
        {
            return true;
        }

        return EqualsIgnoreCase(GetObjectName(Actor), Token) ||
               EqualsIgnoreCase(Actor->GetTypeName(), Token);
    }

    bool ComponentMatchesToken(const Engine::Component::USceneComponent* Component,
                               const FString& Token)
    {
        if (Component == nullptr)
        {
            return false;
        }

        uint32 ParsedUUID = 0;
        if (TryParseUInt32(Token, ParsedUUID) && Component->UUID == ParsedUUID)
        {
            return true;
        }

        return EqualsIgnoreCase(GetObjectName(Component), Token) ||
               EqualsIgnoreCase(Component->GetTypeName(), Token);
    }

    void LogActorSummary(const AActor* Actor)
    {
        if (Actor == nullptr)
        {
            return;
        }

        UE_LOG(Console, ELogVerbosity::Log, "[Actor] name=%s type=%s uuid=%u",
               GetObjectName(Actor).c_str(), Actor->GetTypeName(), Actor->UUID);
        UE_LOG(Console, ELogVerbosity::Log,
               "        location=(%.2f, %.2f, %.2f) rotation=(%.2f, %.2f, %.2f) scale=(%.2f, %.2f, %.2f)",
               Actor->GetLocation().X, Actor->GetLocation().Y, Actor->GetLocation().Z,
               Actor->GetRotation().Rotator().Euler().X, Actor->GetRotation().Rotator().Euler().Y,
               Actor->GetRotation().Rotator().Euler().Z, Actor->GetScale().X, Actor->GetScale().Y,
               Actor->GetScale().Z);
    }

    void LogComponentSummary(const Engine::Component::USceneComponent* Component,
                             const AActor* OwnerActor)
    {
        if (Component == nullptr)
        {
            return;
        }

        UE_LOG(Console, ELogVerbosity::Log,
               "[Component] name=%s type=%s uuid=%u owner=%s",
               GetObjectName(Component).c_str(), Component->GetTypeName(), Component->UUID,
               OwnerActor != nullptr ? GetObjectName(OwnerActor).c_str() : "<none>");
        UE_LOG(Console, ELogVerbosity::Log,
               "            location=(%.2f, %.2f, %.2f) rotation=(%.2f, %.2f, %.2f) scale=(%.2f, %.2f, %.2f)",
               Component->GetRelativeLocation().X, Component->GetRelativeLocation().Y,
               Component->GetRelativeLocation().Z,
               Component->GetRelativeRotation().Euler().X, Component->GetRelativeRotation().Euler().Y,
               Component->GetRelativeRotation().Euler().Z, Component->GetRelativeScale3D().X,
               Component->GetRelativeScale3D().Y, Component->GetRelativeScale3D().Z);
    }

    void CollectContentMatches(const FContentBrowserFolderNode& Folder, const FString& QueryLower,
                               TArray<FString>& OutMatches)
    {
        const FString FolderNameLower = ToLowerAsciiCopy(Folder.DisplayName);
        const FString FolderVirtualLower = ToLowerAsciiCopy(Folder.VirtualPath);
        if ((!Folder.DisplayName.empty() && FolderNameLower.find(QueryLower) != FString::npos) ||
            (!Folder.VirtualPath.empty() && FolderVirtualLower.find(QueryLower) != FString::npos))
        {
            OutMatches.push_back(FString("[Folder] ") + Folder.VirtualPath);
        }

        for (const FContentBrowserItem& File : Folder.Files)
        {
            const FString FileNameLower = ToLowerAsciiCopy(File.DisplayName);
            const FString FileVirtualLower = ToLowerAsciiCopy(File.VirtualPath);
            if ((!File.DisplayName.empty() && FileNameLower.find(QueryLower) != FString::npos) ||
                (!File.VirtualPath.empty() && FileVirtualLower.find(QueryLower) != FString::npos))
            {
                OutMatches.push_back(FString("[") + ContentItemTypeToString(File.ItemType) + "] " +
                                     File.VirtualPath);
            }
        }

        for (const FContentBrowserFolderNode& ChildFolder : Folder.ChildFolders)
        {
            CollectContentMatches(ChildFolder, QueryLower, OutMatches);
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
        UE_LOG(Console, ELogVerbosity::Log, "Commands:");
        UE_LOG(Console, ELogVerbosity::Log, "  help, clear, log <text>, warn <text>, error <text>");
        UE_LOG(Console, ELogVerbosity::Log,
               "  scene.new, scene.open <path>, scene.save, scene.saveas <path>, scene.clear, scene.list, scene.summary");
        UE_LOG(Console, ELogVerbosity::Log,
               "  actor.spawn <mesh> [count], actor.delete_selected, actor.list_selected, actor.inspect <name|uuid>");
        UE_LOG(Console, ELogVerbosity::Log,
               "  component.inspect <name|uuid>, select.clear, select.focus, selection.dump");
        UE_LOG(Console, ELogVerbosity::Log,
               "  camera.reset, camera.speed [value], camera.rot_speed [value], grid.spacing [value], viewmode <lit|unlit|wireframe>");
        UE_LOG(Console, ELogVerbosity::Log,
               "  show.bounds <on|off>, show.grid <on|off>, show.outline <on|off>");
        UE_LOG(Console, ELogVerbosity::Log,
               "  stats.fps, stats.memory, stats.gpu, content.refresh, content.find <keyword>");
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

    FEditorContext* Context = GetContext();
    FEditor*        Editor = Context != nullptr ? Context->Editor : nullptr;
    FScene*         Scene = Context != nullptr ? Context->Scene : nullptr;
    const TArray<FString> Tokens = TokenizeCommandLine(TrimmedCommand);
    if (Tokens.empty())
    {
        return;
    }

    const FString CommandName = ToLowerAsciiCopy(Tokens[0]);

    auto RequireEditor = [&]() -> bool
    {
        if (Editor != nullptr)
        {
            return true;
        }

        UE_LOG(Console, ELogVerbosity::Warning, "Editor is unavailable.");
        return false;
    };

    auto RequireScene = [&]() -> bool
    {
        if (Scene != nullptr)
        {
            return true;
        }

        UE_LOG(Console, ELogVerbosity::Warning, "Scene is unavailable.");
        return false;
    };

    if (CommandName == "scene.new")
    {
        if (RequireEditor())
        {
            Editor->CreateNewScene();
            UE_LOG(Console, ELogVerbosity::Log, "Created a new scene.");
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "scene.open")
    {
        if (RequireEditor())
        {
            if (Tokens.size() < 2)
            {
                UE_LOG(Console, ELogVerbosity::Warning, "Usage: scene.open <path>");
            }
            else
            {
                Editor->OpenSceneFromPath(ResolveSceneCommandPath(Tokens[1], Editor));
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "scene.save")
    {
        if (RequireEditor() && !Editor->SaveCurrentSceneToDisk())
        {
            UE_LOG(Console, ELogVerbosity::Warning, "Scene save failed.");
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "scene.saveas")
    {
        if (RequireEditor())
        {
            if (Tokens.size() < 2)
            {
                UE_LOG(Console, ELogVerbosity::Warning, "Usage: scene.saveas <path>");
            }
            else if (!Editor->SaveSceneAsPath(ResolveSceneCommandPath(Tokens[1], Editor)))
            {
                UE_LOG(Console, ELogVerbosity::Warning, "Scene saveas failed.");
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "scene.clear")
    {
        if (RequireEditor())
        {
            Editor->ClearScene();
            UE_LOG(Console, ELogVerbosity::Log, "Cleared the current scene.");
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "scene.list")
    {
        if (RequireScene())
        {
            const TArray<AActor*>* Actors = Scene->GetActors();
            if (Actors == nullptr || Actors->empty())
            {
                UE_LOG(Console, ELogVerbosity::Log, "Scene is empty.");
            }
            else
            {
                UE_LOG(Console, ELogVerbosity::Log, "Scene actors: %u",
                       static_cast<uint32>(Actors->size()));
                for (AActor* Actor : *Actors)
                {
                    LogActorSummary(Actor);
                }
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "scene.summary")
    {
        if (RequireScene())
        {
            uint32 ActorCount = 0;
            uint32 ComponentCount = 0;
            uint32 RenderableCount = 0;
            const TArray<AActor*>* Actors = Scene->GetActors();
            if (Actors != nullptr)
            {
                ActorCount = static_cast<uint32>(Actors->size());
                for (AActor* Actor : *Actors)
                {
                    if (Actor == nullptr)
                    {
                        continue;
                    }

                    ComponentCount += static_cast<uint32>(Actor->GetOwnedComponents().size());
                    if (Actor->IsRenderable())
                    {
                        ++RenderableCount;
                    }
                }
            }

            const uint32 SelectedCount =
                Context != nullptr ? static_cast<uint32>(Context->SelectedActors.size()) : 0u;
            UE_LOG(Console, ELogVerbosity::Log,
                   "Scene summary: actors=%u, components=%u, renderable=%u, selected=%u",
                   ActorCount, ComponentCount, RenderableCount, SelectedCount);
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "actor.spawn")
    {
        if (RequireEditor())
        {
            if (Tokens.size() < 2)
            {
                UE_LOG(Console, ELogVerbosity::Warning,
                       "Usage: actor.spawn <mesh> [count]");
            }
            else
            {
                int32 SpawnCount = 1;
                if (Tokens.size() >= 3 && !TryParseInt32(Tokens[2], SpawnCount))
                {
                    UE_LOG(Console, ELogVerbosity::Warning, "Invalid spawn count.");
                }
                else
                {
                    SpawnCount = FMath::Clamp(SpawnCount, 1, 256);
                    int32 CreatedCount = 0;
                    for (int32 Index = 0; Index < SpawnCount; ++Index)
                    {
                        // 모든 도형 스폰은 이제 StaticMeshActor 하나로 통합됩니다.
                        AActor* NewActor = new AStaticMeshActor();
                        Editor->AddActorToScene(NewActor, Index == SpawnCount - 1);
                        ++CreatedCount;
                    }

                    if (CreatedCount > 0)
                    {
                        UE_LOG(Console, ELogVerbosity::Log, "Spawned %d static mesh actor(s).",
                               CreatedCount);
                    }
                }
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "actor.delete_selected")
    {
        if (RequireEditor() && !Editor->DeleteSelectedActors())
        {
            UE_LOG(Console, ELogVerbosity::Warning, "No selected actors to delete.");
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "actor.list_selected")
    {
        if (Context == nullptr || Context->SelectedActors.empty())
        {
            UE_LOG(Console, ELogVerbosity::Log, "No selected actors.");
        }
        else
        {
            UE_LOG(Console, ELogVerbosity::Log, "Selected actors: %u",
                   static_cast<uint32>(Context->SelectedActors.size()));
            for (AActor* Actor : Context->SelectedActors)
            {
                LogActorSummary(Actor);
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "actor.inspect")
    {
        if (RequireScene())
        {
            if (Tokens.size() < 2)
            {
                UE_LOG(Console, ELogVerbosity::Warning, "Usage: actor.inspect <name|uuid>");
            }
            else
            {
                bool bFoundActor = false;
                for (AActor* Actor : *Scene->GetActors())
                {
                    if (!ActorMatchesToken(Actor, Tokens[1]))
                    {
                        continue;
                    }

                    bFoundActor = true;
                    LogActorSummary(Actor);
                    UE_LOG(Console, ELogVerbosity::Log, "        components=%u",
                           static_cast<uint32>(Actor->GetOwnedComponents().size()));
                    for (Engine::Component::USceneComponent* Component : Actor->GetOwnedComponents())
                    {
                        LogComponentSummary(Component, Actor);
                    }
                }

                if (!bFoundActor)
                {
                    UE_LOG(Console, ELogVerbosity::Warning, "Actor not found: %s",
                           Tokens[1].c_str());
                }
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "component.inspect")
    {
        if (RequireScene())
        {
            if (Tokens.size() < 2)
            {
                UE_LOG(Console, ELogVerbosity::Warning, "Usage: component.inspect <name|uuid>");
            }
            else
            {
                bool bFoundComponent = false;
                for (AActor* Actor : *Scene->GetActors())
                {
                    if (Actor == nullptr)
                    {
                        continue;
                    }

                    for (Engine::Component::USceneComponent* Component : Actor->GetOwnedComponents())
                    {
                        if (!ComponentMatchesToken(Component, Tokens[1]))
                        {
                            continue;
                        }

                        bFoundComponent = true;
                        LogComponentSummary(Component, Actor);
                    }
                }

                if (!bFoundComponent)
                {
                    UE_LOG(Console, ELogVerbosity::Warning, "Component not found: %s",
                           Tokens[1].c_str());
                }
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "select.clear")
    {
        if (RequireEditor())
        {
            Editor->GetViewportClient().GetSelectionController().ClearSelection();
            UE_LOG(Console, ELogVerbosity::Log, "Selection cleared.");
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "select.focus")
    {
        if (RequireEditor())
        {
            Editor->GetViewportClient().GetNavigationController().FocusActors();
            UE_LOG(Console, ELogVerbosity::Log, "Focused camera on current selection.");
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "selection.dump")
    {
        if (Context == nullptr)
        {
            UE_LOG(Console, ELogVerbosity::Warning, "Editor context is unavailable.");
        }
        else
        {
            UObject* SelectedObject = Context->SelectedObject;
            UE_LOG(Console, ELogVerbosity::Log, "Selection dump:");
            UE_LOG(Console, ELogVerbosity::Log, "  selected_object=%s (%s)",
                   GetObjectName(SelectedObject).c_str(),
                   SelectedObject != nullptr ? SelectedObject->GetTypeName() : "null");
            UE_LOG(Console, ELogVerbosity::Log, "  selected_actor_count=%u",
                   static_cast<uint32>(Context->SelectedActors.size()));
            for (AActor* Actor : Context->SelectedActors)
            {
                LogActorSummary(Actor);
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "camera.reset")
    {
        if (RequireEditor())
        {
            FViewportCamera& Camera = Editor->GetViewportClient().GetCamera();
            Camera.SetProjectionType(EViewportProjectionType::Perspective);
            Camera.SetFOV(3.141592f * 0.5f);
            Camera.SetNearPlane(0.1f);
            Camera.SetFarPlane(2000.0f);
            Camera.SetLocation(FVector(-20.0f, 1.0f, 10.0f));
            Camera.SetRotation(FRotator(0.0f, 0.0f, 0.0f));
            UE_LOG(Console, ELogVerbosity::Log, "Camera reset to default editor transform.");
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "camera.speed")
    {
        if (RequireEditor())
        {
            FViewportNavigationController& NavigationController =
                Editor->GetViewportClient().GetNavigationController();

            if (Tokens.size() < 2)
            {
                UE_LOG(Console, ELogVerbosity::Log, "Camera move speed = %.2f",
                       NavigationController.GetMoveSpeed());
            }
            else
            {
                float MoveSpeed = 0.0f;
                if (!TryParseFloat(Tokens[1], MoveSpeed))
                {
                    UE_LOG(Console, ELogVerbosity::Warning, "Usage: camera.speed [value]");
                }
                else
                {
                    NavigationController.SetMoveSpeed(MoveSpeed);
                    UE_LOG(Console, ELogVerbosity::Log, "Camera move speed = %.2f",
                           NavigationController.GetMoveSpeed());
                }
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "camera.rot_speed")
    {
        if (RequireEditor())
        {
            FViewportNavigationController& NavigationController =
                Editor->GetViewportClient().GetNavigationController();

            if (Tokens.size() < 2)
            {
                UE_LOG(Console, ELogVerbosity::Log, "Camera rotation speed = %.2f",
                       NavigationController.GetRotationSpeed());
            }
            else
            {
                float RotationSpeed = 0.0f;
                if (!TryParseFloat(Tokens[1], RotationSpeed))
                {
                    UE_LOG(Console, ELogVerbosity::Warning,
                           "Usage: camera.rot_speed [value]");
                }
                else
                {
                    NavigationController.SetRotationSpeed(RotationSpeed);
                    UE_LOG(Console, ELogVerbosity::Log, "Camera rotation speed = %.2f",
                           NavigationController.GetRotationSpeed());
                }
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "grid.spacing")
    {
        if (Tokens.size() < 2)
        {
            UE_LOG(Console, ELogVerbosity::Log, "Grid spacing = %.2f", UEngineStatics::GridSpacing);
        }
        else
        {
            float GridSpacing = 0.0f;
            if (!TryParseFloat(Tokens[1], GridSpacing))
            {
                UE_LOG(Console, ELogVerbosity::Warning, "Usage: grid.spacing [value]");
            }
            else
            {
                UEngineStatics::GridSpacing = FMath::Clamp(GridSpacing, 1.0f, 1000.0f);
                UE_LOG(Console, ELogVerbosity::Log, "Grid spacing = %.2f",
                       UEngineStatics::GridSpacing);
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "viewmode")
    {
        if (RequireEditor())
        {
            FViewportRenderSetting& RenderSetting = Editor->GetViewportClient().GetRenderSetting();
            if (Tokens.size() < 2)
            {
                UE_LOG(Console, ELogVerbosity::Log, "View mode = %s",
                       ViewModeToString(RenderSetting.GetViewMode()));
            }
            else
            {
                const FString Mode = ToLowerAsciiCopy(Tokens[1]);
                if (Mode == "lit")
                {
                    RenderSetting.SetViewMode(EViewModeIndex::VMI_Lit);
                }
                else if (Mode == "unlit")
                {
                    RenderSetting.SetViewMode(EViewModeIndex::VMI_Unlit);
                }
                else if (Mode == "wireframe")
                {
                    RenderSetting.SetViewMode(EViewModeIndex::VMI_Wireframe);
                }
                else
                {
                    UE_LOG(Console, ELogVerbosity::Warning,
                           "Usage: viewmode <lit|unlit|wireframe>");
                    bScrollToBottom = true;
                    return;
                }

                UE_LOG(Console, ELogVerbosity::Log, "View mode = %s",
                       ViewModeToString(RenderSetting.GetViewMode()));
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "show.bounds")
    {
        if (RequireScene())
        {
            if (Tokens.size() < 2)
            {
                uint32 VisibleBoundsCount = 0;
                for (AActor* Actor : *Scene->GetActors())
                {
                    if (Actor != nullptr && Actor->IsShowBounds())
                    {
                        ++VisibleBoundsCount;
                    }
                }

                UE_LOG(Console, ELogVerbosity::Log, "Bounds visible on %u actor(s).",
                       VisibleBoundsCount);
            }
            else
            {
                bool bShowBounds = false;
                if (!TryParseToggle(Tokens[1], bShowBounds))
                {
                    UE_LOG(Console, ELogVerbosity::Warning, "Usage: show.bounds <on|off>");
                }
                else
                {
                    uint32 UpdatedCount = 0;
                    for (AActor* Actor : *Scene->GetActors())
                    {
                        if (Actor == nullptr)
                        {
                            continue;
                        }

                        Actor->SetShowBounds(bShowBounds);
                        ++UpdatedCount;
                    }

                    UE_LOG(Console, ELogVerbosity::Log,
                           "Bounds visibility set to %s for %u actor(s).",
                           bShowBounds ? "on" : "off", UpdatedCount);
                }
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "show.grid")
    {
        if (RequireEditor())
        {
            FViewportRenderSetting& RenderSetting = Editor->GetViewportClient().GetRenderSetting();
            if (Tokens.size() < 2)
            {
                UE_LOG(Console, ELogVerbosity::Log, "Grid visibility = %s",
                       RenderSetting.IsGridVisible() ? "on" : "off");
            }
            else
            {
                bool bShowGrid = false;
                if (!TryParseToggle(Tokens[1], bShowGrid))
                {
                    UE_LOG(Console, ELogVerbosity::Warning, "Usage: show.grid <on|off>");
                }
                else
                {
                    RenderSetting.SetGridVisible(bShowGrid);
                    UE_LOG(Console, ELogVerbosity::Log, "Grid visibility = %s",
                           bShowGrid ? "on" : "off");
                }
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "show.outline")
    {
        if (RequireEditor())
        {
            FViewportRenderSetting& RenderSetting = Editor->GetViewportClient().GetRenderSetting();
            if (Tokens.size() < 2)
            {
                UE_LOG(Console, ELogVerbosity::Log, "Selection outline visibility = %s",
                       RenderSetting.IsSelectionOutlineVisible() ? "on" : "off");
            }
            else
            {
                bool bShowOutline = false;
                if (!TryParseToggle(Tokens[1], bShowOutline))
                {
                    UE_LOG(Console, ELogVerbosity::Warning, "Usage: show.outline <on|off>");
                }
                else
                {
                    RenderSetting.SetSelectionOutlineVisible(bShowOutline);
                    UE_LOG(Console, ELogVerbosity::Log,
                           "Selection outline visibility = %s",
                           bShowOutline ? "on" : "off");
                }
            }
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "stats.fps")
    {
        if (Context == nullptr)
        {
            UE_LOG(Console, ELogVerbosity::Warning, "Editor context is unavailable.");
        }
        else
        {
            UE_LOG(Console, ELogVerbosity::Log, "FPS = %.1f (%.3f ms)",
                   Context->CurrentFPS, Context->DeltaTime * 1000.0f);
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "stats.memory")
    {
        UE_LOG(Console, ELogVerbosity::Log, "TotalAllocationCount = %u",
               UEngineStatics::TotalAllocationCount);
        UE_LOG(Console, ELogVerbosity::Log, "Heap Usage = %.2f KB",
               UEngineStatics::TotalAllocatedBytes / 1024.0f);
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "stats.gpu")
    {
        UE_LOG(Console, ELogVerbosity::Warning,
               "GPU memory stats are not available in the current build.");
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "content.refresh")
    {
        if (Editor == nullptr || Context == nullptr || Context->ContentIndex == nullptr)
        {
            UE_LOG(Console, ELogVerbosity::Warning, "Content index is unavailable.");
        }
        else
        {
            Editor->RefreshContentIndex();
            const FContentIndexSnapshot& Snapshot = Context->ContentIndex->GetSnapshot();
            UE_LOG(Console, ELogVerbosity::Log,
                   "Content refreshed: folders=%d files=%d root=%s",
                   Snapshot.FolderCount, Snapshot.FileCount,
                   PathToUtf8String(Snapshot.ContentRootPath).c_str());
        }
        bScrollToBottom = true;
        return;
    }

    if (CommandName == "content.find")
    {
        if (Context == nullptr || Context->ContentIndex == nullptr)
        {
            UE_LOG(Console, ELogVerbosity::Warning, "Content index is unavailable.");
        }
        else if (Tokens.size() < 2)
        {
            UE_LOG(Console, ELogVerbosity::Warning, "Usage: content.find <keyword>");
        }
        else
        {
            const FContentIndexSnapshot& Snapshot = Context->ContentIndex->GetSnapshot();
            TArray<FString> Matches;
            CollectContentMatches(Snapshot.RootFolder, ToLowerAsciiCopy(Tokens[1]), Matches);

            if (Matches.empty())
            {
                UE_LOG(Console, ELogVerbosity::Log, "No content items matched '%s'.",
                       Tokens[1].c_str());
            }
            else
            {
                constexpr size_t MaxResultsToPrint = 64;
                UE_LOG(Console, ELogVerbosity::Log, "Content matches for '%s': %u",
                       Tokens[1].c_str(), static_cast<uint32>(Matches.size()));
                for (size_t Index = 0; Index < Matches.size() && Index < MaxResultsToPrint; ++Index)
                {
                    UE_LOG(Console, ELogVerbosity::Log, "  %s", Matches[Index].c_str());
                }

                if (Matches.size() > MaxResultsToPrint)
                {
                    UE_LOG(Console, ELogVerbosity::Log, "  ... %u more result(s)",
                           static_cast<uint32>(Matches.size() - MaxResultsToPrint));
                }
            }
        }
        bScrollToBottom = true;
        return;
    }

    UE_LOG(Console, ELogVerbosity::Warning, "Unknown command: %s", TrimmedCommand.c_str());
    bScrollToBottom = true;
}
