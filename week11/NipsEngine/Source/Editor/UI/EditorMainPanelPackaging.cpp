#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Packaging/GamePackager.h"
#include "Editor/Settings/ProjectSettings.h"
#include "Editor/UI/EditorMainPanelPackagingHelpers.h"
#include "Engine/Core/Paths.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Math/Utils.h"
#include "Object/ObjectFactory.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <future>

namespace
{
TArray<const FTypeInfo*> GetPackagingTypesAssignableTo(const FTypeInfo* BaseType)
{
    TArray<const FTypeInfo*> Types;
    if (!BaseType)
    {
        return Types;
    }

    TArray<const FTypeInfo*> RegisteredTypes;
    FObjectFactory::Get().GetRegisteredTypeInfos(RegisteredTypes);
    for (const FTypeInfo* Type : RegisteredTypes)
    {
        if (Type && Type->IsA(BaseType))
        {
            Types.push_back(Type);
        }
    }

    std::sort(
        Types.begin(),
        Types.end(),
        [](const FTypeInfo* A, const FTypeInfo* B)
        {
            const char* AName = A ? A->name : "";
            const char* BName = B ? B->name : "";
            return std::strcmp(AName, BName) < 0;
        });
    return Types;
}

bool DrawPackagingClassCombo(const char* Id, char* Buffer, size_t BufferSize, const FTypeInfo* BaseType)
{
    bool bChanged = false;
    const TArray<const FTypeInfo*> Types = GetPackagingTypesAssignableTo(BaseType);
    const char* CurrentLabel = Buffer && Buffer[0] != '\0' ? Buffer : "None";

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo(Id, CurrentLabel))
    {
        for (const FTypeInfo* Type : Types)
        {
            if (!Type || !Type->name)
            {
                continue;
            }

            const bool bSelected = Buffer && std::strcmp(Buffer, Type->name) == 0;
            if (ImGui::Selectable(Type->name, bSelected))
            {
                strncpy_s(Buffer, BufferSize, Type->name, _TRUNCATE);
                bChanged = true;
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return bChanged;
}
} // namespace

void FEditorMainPanel::RequestBuildGame()
{
    if (BuildGameState.bInProgress)
    {
        PushFooterLog("Packaging already in progress");
        return;
    }

    if (EditorEngine && EditorEngine->GetSceneService().IsDirty() && !EditorEngine->GetSceneService().PromptSaveIfDirty())
    {
        PushFooterLog("Packaging canceled");
        return;
    }

    FProjectSettings& ProjectSettings = FProjectSettings::Get();
    ProjectSettings.LoadFromFile(FProjectSettings::GetDefaultSettingsPath());
    BuildGameState.PendingSettings = ProjectSettings.BuildSettings;
    if (BuildGameState.PendingSettings.GameName.empty())
    {
        BuildGameState.PendingSettings.GameName = "NipsGame";
    }
    if (BuildGameState.PendingSettings.PlayerControllerClass.empty())
    {
        BuildGameState.PendingSettings.PlayerControllerClass = "APlayerController";
    }
    if (BuildGameState.PendingSettings.DefaultPawnClass.empty())
    {
        BuildGameState.PendingSettings.DefaultPawnClass = "ADefaultPawn";
    }
    if (BuildGameState.PendingSettings.OutputDirectory.empty())
    {
        BuildGameState.PendingSettings.OutputDirectory =
            FEditorMainPanelPackagingHelpers::MakeDefaultPackageOutputDirectory(BuildGameState.PendingSettings.GameName);
    }
    if (EditorEngine && EditorEngine->GetSceneService().HasCurrentScenePath())
    {
        BuildGameState.PendingSettings.StartupScene = EditorEngine->GetSceneService().GetCurrentScenePath();
        ProjectSettings.SetLastScenePath(BuildGameState.PendingSettings.StartupScene);
        ProjectSettings.SaveToFile(FProjectSettings::GetDefaultSettingsPath());
    }
    else if (BuildGameState.PendingSettings.StartupScene.empty() && ProjectSettings.HasSavedLastScenePath())
    {
        BuildGameState.PendingSettings.StartupScene = ProjectSettings.LastScenePath;
    }
    FEditorMainPanelPackagingHelpers::AddUniquePackagingScene(
        BuildGameState.PendingSettings.IncludedScenes,
        BuildGameState.PendingSettings.StartupScene
    );

    strncpy_s(BuildGameState.GameNameBuffer, BuildGameState.PendingSettings.GameName.c_str(), _TRUNCATE);
    strncpy_s(BuildGameState.StartupSceneBuffer, BuildGameState.PendingSettings.StartupScene.c_str(), _TRUNCATE);
    strncpy_s(BuildGameState.SceneListAddBuffer, "", _TRUNCATE);
    strncpy_s(BuildGameState.PlayerControllerClassBuffer, BuildGameState.PendingSettings.PlayerControllerClass.c_str(), _TRUNCATE);
    strncpy_s(BuildGameState.DefaultPawnClassBuffer, BuildGameState.PendingSettings.DefaultPawnClass.c_str(), _TRUNCATE);
    strncpy_s(BuildGameState.DefaultPawnPrefabPathBuffer, BuildGameState.PendingSettings.DefaultPawnPrefabPath.c_str(), _TRUNCATE);
    strncpy_s(BuildGameState.OutputDirectoryBuffer, BuildGameState.PendingSettings.OutputDirectory.c_str(), _TRUNCATE);
    strncpy_s(BuildGameState.IconPathBuffer, BuildGameState.PendingSettings.IconPath.c_str(), _TRUNCATE);
    strncpy_s(BuildGameState.SplashImagePathBuffer, BuildGameState.PendingSettings.SplashImagePath.c_str(), _TRUNCATE);

    BuildGameState.bOpenModal = true;
}

void FEditorMainPanel::TickBuildGameTask()
{
    if (!BuildGameState.bInProgress || !BuildGameState.Future.valid())
    {
        return;
    }

    if (BuildGameState.Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        return;
    }

    const FGamePackageResult Result = BuildGameState.Future.get();
    BuildGameState.bInProgress = false;
    PushFooterLog(Result.bSucceeded ? "Game package created" : Result.Message);
}

void FEditorMainPanel::RenderBuildGameModal()
{
    if (BuildGameState.bOpenModal)
    {
        ImGui::OpenPopup(FEditorMainPanelPackagingHelpers::GetPopupName());
        BuildGameState.bOpenModal = false;
    }

    ImGui::SetNextWindowSize(ImVec2(760.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(FEditorMainPanelPackagingHelpers::GetPopupName(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    if (ImGui::BeginTable("##PackagingSettingsTable", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 132.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Game Name");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const FString PreviousGameName = BuildGameState.PendingSettings.GameName;
        const FString PreviousOutputDirectory = FPaths::Normalize(BuildGameState.OutputDirectoryBuffer);
        if (ImGui::InputText("##PackageGameName", BuildGameState.GameNameBuffer, IM_ARRAYSIZE(BuildGameState.GameNameBuffer)))
        {
            const FString EditedGameName = BuildGameState.GameNameBuffer;
            BuildGameState.PendingSettings.GameName = EditedGameName.empty() ? FString("NipsGame") : EditedGameName;
            if (FEditorMainPanelPackagingHelpers::IsDefaultPackageOutputDirectory(PreviousOutputDirectory, PreviousGameName))
            {
                const FString NewOutputDirectory =
                    FEditorMainPanelPackagingHelpers::MakeDefaultPackageOutputDirectory(
                        BuildGameState.PendingSettings.GameName
                    );
                strncpy_s(BuildGameState.OutputDirectoryBuffer, NewOutputDirectory.c_str(), _TRUNCATE);
            }
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Startup Scene");
        ImGui::TableSetColumnIndex(1);
        const float BrowseButtonWidth = 76.0f;
        ImGui::SetNextItemWidth(-(BrowseButtonWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputText("##PackageStartupScene", BuildGameState.StartupSceneBuffer, IM_ARRAYSIZE(BuildGameState.StartupSceneBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Browse##StartupScene", ImVec2(BrowseButtonWidth, 0.0f)))
        {
            FString PickedPath;
            if (Widgets.ToolbarWidget.OpenSceneFileDialog(PickedPath))
            {
                const FString RelativePath = FPaths::ToRelativeString(FPaths::ToWide(PickedPath));
                strncpy_s(BuildGameState.StartupSceneBuffer, RelativePath.c_str(), _TRUNCATE);
            }
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Player Controller");
        ImGui::TableSetColumnIndex(1);
        DrawPackagingClassCombo(
            "##PackagePlayerController",
            BuildGameState.PlayerControllerClassBuffer,
            IM_ARRAYSIZE(BuildGameState.PlayerControllerClassBuffer),
            &APlayerController::s_TypeInfo
        );

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Default Pawn");
        ImGui::TableSetColumnIndex(1);
        DrawPackagingClassCombo(
            "##PackageDefaultPawnClass",
            BuildGameState.DefaultPawnClassBuffer,
            IM_ARRAYSIZE(BuildGameState.DefaultPawnClassBuffer),
            &APawn::s_TypeInfo
        );

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Pawn Prefab");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText(
            "##PackageDefaultPawnPrefab",
            BuildGameState.DefaultPawnPrefabPathBuffer,
            IM_ARRAYSIZE(BuildGameState.DefaultPawnPrefabPathBuffer)
        );

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Output Directory");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText(
            "##PackageOutputDirectory",
            BuildGameState.OutputDirectoryBuffer,
            IM_ARRAYSIZE(BuildGameState.OutputDirectoryBuffer)
        );

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Game Icon");
        ImGui::TableSetColumnIndex(1);
        const float BrandingBrowseWidth = 76.0f;
        ImGui::SetNextItemWidth(-(BrandingBrowseWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputText("##PackageGameIcon", BuildGameState.IconPathBuffer, IM_ARRAYSIZE(BuildGameState.IconPathBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Browse##PackageGameIcon", ImVec2(BrandingBrowseWidth, 0.0f)))
        {
            FString PickedPath;
            if (FEditorMainPanelPackagingHelpers::OpenPackagingAssetFileDialog(
                L"Icon Files (*.ico)\0*.ico\0All Files (*.*)\0*.*\0",
                PickedPath
            ))
            {
                strncpy_s(BuildGameState.IconPathBuffer, PickedPath.c_str(), _TRUNCATE);
            }
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Splash Image");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-(BrandingBrowseWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputText(
            "##PackageSplashImage",
            BuildGameState.SplashImagePathBuffer,
            IM_ARRAYSIZE(BuildGameState.SplashImagePathBuffer)
        );
        ImGui::SameLine();
        if (ImGui::Button("Browse##PackageSplashImage", ImVec2(BrandingBrowseWidth, 0.0f)))
        {
            FString PickedPath;
            if (FEditorMainPanelPackagingHelpers::OpenPackagingAssetFileDialog(
                L"Image Files (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All Files (*.*)\0*.*\0",
                PickedPath
            ))
            {
                strncpy_s(BuildGameState.SplashImagePathBuffer, PickedPath.c_str(), _TRUNCATE);
            }
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Splash Seconds");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::DragFloat(
            "##PackageSplashSeconds",
            &BuildGameState.PendingSettings.SplashMinSeconds,
            0.05f,
            3.0f,
            10.0f,
            "%.2f"
        );
        BuildGameState.PendingSettings.SplashMinSeconds =
            MathUtil::Clamp(BuildGameState.PendingSettings.SplashMinSeconds, 3.0f, 10.0f);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Configuration");
        ImGui::TableSetColumnIndex(1);
        int BuildConfigIndex = BuildGameState.PendingSettings.Configuration == EGameBuildConfiguration::Development
            ? 0
            : 1;
        const char* ConfigItems[] = { "Development (GameClientDebug)", "Shipping (GameClientRelease)" };
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::Combo("##PackageConfiguration", &BuildConfigIndex, ConfigItems, IM_ARRAYSIZE(ConfigItems)))
        {
            BuildGameState.PendingSettings.Configuration = BuildConfigIndex == 0
                ? EGameBuildConfiguration::Development
                : EGameBuildConfiguration::Shipping;
        }

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Scenes to Copy");
    ImGui::TextDisabled("Startup scene is always included. Add extra scenes that should be copied with the package.");
    const float SceneButtonWidth = 76.0f;
    const float SceneAddButtonWidth = 52.0f;
    ImGui::SetNextItemWidth(-(SceneButtonWidth + SceneAddButtonWidth + ImGui::GetStyle().ItemSpacing.x * 2.0f));
    ImGui::InputText("##PackageSceneToAdd", BuildGameState.SceneListAddBuffer, IM_ARRAYSIZE(BuildGameState.SceneListAddBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse##PackageSceneToAdd", ImVec2(SceneButtonWidth, 0.0f)))
    {
        FString PickedPath;
        if (Widgets.ToolbarWidget.OpenSceneFileDialog(PickedPath))
        {
            const FString RelativePath = FPaths::ToRelativeString(FPaths::ToWide(PickedPath));
            strncpy_s(BuildGameState.SceneListAddBuffer, RelativePath.c_str(), _TRUNCATE);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Add##PackageSceneToAdd", ImVec2(SceneAddButtonWidth, 0.0f)))
    {
        if (FEditorMainPanelPackagingHelpers::AddUniquePackagingScene(
            BuildGameState.PendingSettings.IncludedScenes,
            BuildGameState.SceneListAddBuffer
        ))
        {
            strncpy_s(BuildGameState.SceneListAddBuffer, "", _TRUNCATE);
        }
    }
    if (ImGui::Button("Add Startup Scene"))
    {
        FEditorMainPanelPackagingHelpers::AddUniquePackagingScene(
            BuildGameState.PendingSettings.IncludedScenes,
            BuildGameState.StartupSceneBuffer
        );
    }

    const float SceneListHeight = 118.0f;
    if (ImGui::BeginChild("##PackageSceneList", ImVec2(0.0f, SceneListHeight), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (BuildGameState.PendingSettings.IncludedScenes.empty())
        {
            ImGui::TextDisabled("No extra scenes added.");
        }
        for (int32 SceneIndex = 0; SceneIndex < static_cast<int32>(BuildGameState.PendingSettings.IncludedScenes.size()); ++SceneIndex)
        {
            ImGui::PushID(SceneIndex);
            ImGui::TextUnformatted(BuildGameState.PendingSettings.IncludedScenes[SceneIndex].c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 58.0f);
            if (ImGui::SmallButton("Remove"))
            {
                BuildGameState.PendingSettings.IncludedScenes.erase(BuildGameState.PendingSettings.IncludedScenes.begin() + SceneIndex);
                --SceneIndex;
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SeparatorText("Options");
    ImGui::Checkbox("Clean Output", &BuildGameState.PendingSettings.bCleanOutput);
    ImGui::SameLine();
    ImGui::Checkbox("Run After Packaging", &BuildGameState.PendingSettings.bRunAfterBuild);

    const FString GameName = BuildGameState.GameNameBuffer;
    const FString StartupScene = FPaths::Normalize(BuildGameState.StartupSceneBuffer);
    const FString PlayerControllerClass = FPaths::Normalize(BuildGameState.PlayerControllerClassBuffer);
    const FString DefaultPawnClass = FPaths::Normalize(BuildGameState.DefaultPawnClassBuffer);
    const FString DefaultPawnPrefabPath = FPaths::Normalize(BuildGameState.DefaultPawnPrefabPathBuffer);
    const FString OutputDirectory = FPaths::Normalize(BuildGameState.OutputDirectoryBuffer);
    const FString IconPath = FPaths::Normalize(BuildGameState.IconPathBuffer);
    const FString SplashImagePath = FPaths::Normalize(BuildGameState.SplashImagePathBuffer);
    const bool bValidGameName = !GameName.empty();
    const bool bValidScene =
        !StartupScene.empty() && std::filesystem::exists(FPaths::ToAbsolute(FPaths::ToWide(StartupScene)));
    const bool bValidPlayerController = !PlayerControllerClass.empty();
    const bool bValidDefaultPawn = !DefaultPawnClass.empty();
    const bool bValidDefaultPawnPrefab =
        DefaultPawnPrefabPath.empty() ||
        std::filesystem::exists(FPaths::ToAbsolute(FPaths::ToWide(DefaultPawnPrefabPath)));
    const bool bValidOutput = !OutputDirectory.empty();
    const std::wstring IconExt = FEditorMainPanelPackagingHelpers::ToLowerPathExtension(IconPath);
    const std::wstring SplashExt = FEditorMainPanelPackagingHelpers::ToLowerPathExtension(SplashImagePath);
    const bool bValidIcon =
        IconPath.empty() ||
        (std::filesystem::exists(FPaths::ToAbsolute(FPaths::ToWide(IconPath))) && IconExt == L".ico");
    const bool bValidSplashExt =
        SplashImagePath.empty() ||
        SplashExt == L".png" ||
        SplashExt == L".jpg" ||
        SplashExt == L".jpeg" ||
        SplashExt == L".bmp";
    const bool bValidSplash =
        SplashImagePath.empty() ||
        (std::filesystem::exists(FPaths::ToAbsolute(FPaths::ToWide(SplashImagePath))) && bValidSplashExt);
    bool bValidIncludedScenes = true;
    for (const FString& IncludedScene : BuildGameState.PendingSettings.IncludedScenes)
    {
        if (IncludedScene.empty() || !std::filesystem::exists(FPaths::ToAbsolute(FPaths::ToWide(IncludedScene))))
        {
            bValidIncludedScenes = false;
            break;
        }
    }

    ImGui::SeparatorText("Validation");
    ImGui::Text(
        "Configuration: %s",
        BuildGameState.PendingSettings.Configuration == EGameBuildConfiguration::Development
            ? "Development -> GameClientDebug|x64"
            : "Shipping -> GameClientRelease|x64"
    );
    const FString OutputExeName = FEditorMainPanelPackagingHelpers::SanitizePackageNameForPath(GameName) + ".exe";
    ImGui::Text("Output Exe: %s", OutputExeName.c_str());
    ImGui::Text("Scenes to Copy: %d", static_cast<int32>(BuildGameState.PendingSettings.IncludedScenes.size()));
    ImGui::Text("Icon: %s", IconPath.empty() ? "(none)" : IconPath.c_str());
    ImGui::Text("Splash: %s", SplashImagePath.empty() ? "(none)" : SplashImagePath.c_str());
    ImGui::Text("Default Pawn: %s", DefaultPawnClass.c_str());
    ImGui::Text("Pawn Prefab: %s", DefaultPawnPrefabPath.empty() ? "(none)" : DefaultPawnPrefabPath.c_str());
    ImGui::TextDisabled("Pawn prefab root must derive from APawn. If set, it overrides Default Pawn Class.");

    if (bValidScene)
    {
        ImGui::TextColored(ImVec4(0.42f, 0.78f, 0.48f, 1.0f), "Startup scene found.");
    }
    if (!bValidScene)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Startup scene does not exist.");
    }
    if (!bValidGameName)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Game name is empty.");
    }
    if (!bValidPlayerController)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Player controller class is empty.");
    }
    if (!bValidDefaultPawn)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Default pawn class is empty.");
    }
    if (!bValidDefaultPawnPrefab)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Pawn prefab path does not exist.");
    }
    if (!bValidOutput)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Output directory is empty.");
    }
    if (!bValidIcon)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Game icon must be an existing .ico file.");
    }
    if (!bValidSplash)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "Splash must be an existing png, jpg, jpeg, or bmp file.");
    }
    if (!bValidIncludedScenes)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.35f, 1.0f), "One or more scenes to copy do not exist.");
    }
    if (BuildGameState.bInProgress)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.58f, 0.18f, 1.0f), "Packaging is running. Check Console for live output.");
    }

    ImGui::Separator();
    const bool bCanBuild =
        bValidGameName &&
        bValidScene &&
        bValidPlayerController &&
        bValidDefaultPawn &&
        bValidDefaultPawnPrefab &&
        bValidOutput &&
        bValidIcon &&
        bValidSplash &&
        bValidIncludedScenes;
    const bool bDisablePackageButton = !bCanBuild || BuildGameState.bInProgress;
    if (bDisablePackageButton)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Package", ImVec2(120.0f, 0.0f)))
    {
        BuildGameState.PendingSettings.GameName = GameName;
        BuildGameState.PendingSettings.StartupScene = StartupScene;
        BuildGameState.PendingSettings.PlayerControllerClass = PlayerControllerClass;
        BuildGameState.PendingSettings.DefaultPawnClass = DefaultPawnClass;
        BuildGameState.PendingSettings.DefaultPawnPrefabPath = DefaultPawnPrefabPath;
        BuildGameState.PendingSettings.OutputDirectory = OutputDirectory;
        BuildGameState.PendingSettings.IconPath = IconPath;
        BuildGameState.PendingSettings.SplashImagePath = SplashImagePath;
        BuildGameState.PendingSettings.SplashMinSeconds =
            MathUtil::Clamp(BuildGameState.PendingSettings.SplashMinSeconds, 3.0f, 10.0f);
        FEditorMainPanelPackagingHelpers::AddUniquePackagingScene(
            BuildGameState.PendingSettings.IncludedScenes,
            StartupScene
        );

        FProjectSettings& ProjectSettings = FProjectSettings::Get();
        ProjectSettings.BuildSettings = BuildGameState.PendingSettings;
        if (bValidScene)
        {
            ProjectSettings.SetLastScenePath(StartupScene);
        }
        ProjectSettings.SaveToFile(FProjectSettings::GetDefaultSettingsPath());

        PushFooterLog("Packaging game...");
        BuildGameState.bInProgress = true;
        BuildGameState.Future = std::async(std::launch::async, [Settings = BuildGameState.PendingSettings]()
        {
            return FGamePackager::BuildAndPackage(Settings);
        });
        ImGui::CloseCurrentPopup();
    }
    if (bDisablePackageButton)
    {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}
