#include "Editor/UI/Panel/EditorGameBuildWidget.h"

#include "Core/ProjectSettings.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/Util/EditorFileUtils.h"
#include "Engine/Platform/Paths.h"
#include "Engine/Platform/WindowsWindow.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace
{
bool InputFStringSetting(const char* Label, FString& Value, size_t BufferSize)
{
	char Buffer[1024] = {};
	const size_t SafeSize = (std::min)(BufferSize, sizeof(Buffer));
	std::snprintf(Buffer, SafeSize, "%s", Value.c_str());
	if (ImGui::InputText(Label, Buffer, SafeSize))
	{
		Value = Buffer;
		return true;
	}
	return false;
}

std::filesystem::path GetDefaultSolutionPath()
{
	// 현재 엔진 루트 추정값 기준의 기존 fallback solution 위치
	const std::filesystem::path EngineRoot(FPaths::RootDir());
	return (EngineRoot.lexically_normal().parent_path() / L"KraftonEngine.sln").lexically_normal();
}

std::filesystem::path ResolveInitialSolutionDirectory(const FString& CurrentSolutionPath)
{
	// 이미 입력된 solution path가 있으면 해당 폴더에서 파일 선택을 시작
	if (!CurrentSolutionPath.empty())
	{
		std::filesystem::path CurrentPath(FPaths::ToWide(CurrentSolutionPath));
		if (CurrentPath.has_filename())
		{
			CurrentPath = CurrentPath.parent_path();
		}

		if (!CurrentPath.empty() && std::filesystem::exists(CurrentPath))
		{
			return CurrentPath.lexically_normal();
		}
	}

	// 입력값이 비어 있거나 유효하지 않으면 기존 추정 solution 폴더를 기본값으로 사용
	const std::filesystem::path DefaultDirectory = GetDefaultSolutionPath().parent_path();
	if (!DefaultDirectory.empty() && std::filesystem::exists(DefaultDirectory))
	{
		return DefaultDirectory.lexically_normal();
	}

	return std::filesystem::current_path().lexically_normal();
}

FString OpenSolutionFileDialog(UEditorEngine* EditorEngine, const FString& CurrentSolutionPath)
{
	// 파일 다이얼로그 호출 중 참조할 초기 폴더 문자열 수명 유지
	const std::filesystem::path InitialDirectory = ResolveInitialSolutionDirectory(CurrentSolutionPath);
	const std::wstring InitialDirectoryText = InitialDirectory.wstring();

	return FEditorFileUtils::OpenFileDialog({
		.Filter = L"Solution Files (*.sln)\0*.sln\0All Files (*.*)\0*.*\0",
		.Title = L"Select Solution File",
		.DefaultExtension = L"sln",
		.InitialDirectory = InitialDirectoryText.c_str(),
		.OwnerWindowHandle = EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr,
		.bFileMustExist = true,
		.bPathMustExist = true,
		.bReturnRelativeToProjectRoot = false,
	});
}
}

void FEditorGameBuildWidget::Initialize(UEditorEngine* InEditorEngine)
{
	EditorEngine = InEditorEngine;
}

void FEditorGameBuildWidget::Shutdown()
{
	Builder.Shutdown();
}

void FEditorGameBuildWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!bOpen)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(680, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Game Build", &bOpen))
	{
		ImGui::End();
		return;
	}

	const bool bIsBuilding = Builder.IsBuilding();
	if (bIsBuilding)
	{
		ImGui::BeginDisabled();
	}

	ImGui::TextUnformatted("Solution File");
	ImGui::PushItemWidth(-96.0f);
	InputFStringSetting("##SolutionFile", Settings.SolutionPath, 1024);
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::Button("Browse##SolutionFile"))
	{
		const FString SelectedPath = OpenSolutionFileDialog(EditorEngine, Settings.SolutionPath);
		if (!SelectedPath.empty())
		{
			Settings.SolutionPath = SelectedPath;
		}
	}

	InputFStringSetting("Output Directory", Settings.OutputDirectory, 512);
	ImGui::Text("Configuration: %s", Settings.Configuration.c_str());
	ImGui::Text("Platform: %s", Settings.Platform.c_str());

	ImGui::Separator();
	ImGui::Checkbox("Auto Save Before Build", &Settings.bAutoSaveBeforeBuild);
	ImGui::Checkbox("Stop PIE Before Build", &Settings.bStopPIEBeforeBuild);
	ImGui::Checkbox("Clean Output Before Build", &Settings.bCleanOutput);
	ImGui::Checkbox("Build Executable", &Settings.bBuildExecutable);
	ImGui::Checkbox("Package Content", &Settings.bPackageContent);
	ImGui::Checkbox("Package Shaders", &Settings.bPackageShaders);
	ImGui::Checkbox("Package Settings", &Settings.bPackageSettings);
	ImGui::Checkbox("Package DLLs", &Settings.bPackageDlls);

	if (bIsBuilding)
	{
		ImGui::EndDisabled();
	}

	ImGui::Separator();
	if (bIsBuilding)
	{
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Build"))
	{
		StartBuildFromUI();
	}
	if (bIsBuilding)
	{
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	ImGui::Text("Status: %s", Builder.GetStatusText().c_str());

	const bool bCanUseOutput = Builder.HasFinished() && Builder.WasSuccessful();
	if (!bCanUseOutput)
	{
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Open Output Folder"))
	{
		Builder.OpenOutputFolder();
	}
	ImGui::SameLine();
	if (ImGui::Button("Run Game"))
	{
		Builder.RunGame();
	}
	if (!bCanUseOutput)
	{
		ImGui::EndDisabled();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Build Log");
	ImGui::BeginChild("##GameBuildLog", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
	const FString LogText = Builder.GetLogText();
	ImGui::TextUnformatted(LogText.c_str());
	if (bIsBuilding)
	{
		ImGui::SetScrollHereY(1.0f);
	}
	ImGui::EndChild();

	ImGui::End();
}

void FEditorGameBuildWidget::StartBuildFromUI()
{
	if (!EditorEngine || Builder.IsBuilding())
	{
		return;
	}

	if (EditorEngine->IsPlayingInEditor())
	{
		if (!Settings.bStopPIEBeforeBuild)
		{
			return;
		}
		EditorEngine->StopPlayInEditorImmediate();
	}

	if (Settings.bAutoSaveBeforeBuild)
	{
		if (EditorEngine->HasCurrentLevelFilePath())
		{
			EditorEngine->SaveScene();
		}
		FProjectSettings::Get().SaveToFile(FProjectSettings::GetDefaultPath());
	}

	Builder.StartBuild(Settings);
}
