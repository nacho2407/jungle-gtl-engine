#pragma once

#include "Core/Types/CoreTypes.h"

#include <atomic>
#include <mutex>
#include <thread>

/**
 * @brief 에디터 게임 빌드 설정
 */
struct FEditorGameBuildSettings
{
	FString SolutionPath;
	FString OutputDirectory = "KraftonEngine/Bin/GameBuild";
	FString Configuration = "Game";
	FString Platform = "x64";

	bool bAutoSaveBeforeBuild = true;
	bool bStopPIEBeforeBuild = true;
	bool bCleanOutput = true;
	bool bBuildExecutable = true;
	bool bPackageContent = true;
	bool bPackageShaders = true;
	bool bPackageSettings = true;
	bool bPackageDlls = true;
};

/**
 * @brief 에디터 game configuration 빌드와 패키징 worker
 */
class FEditorGameBuilder
{
public:
	FEditorGameBuilder() = default;
	~FEditorGameBuilder();

	/**
	 * @brief 새 game build 작업을 시작합니다
	 *
	 * @param InSettings 빌드와 패키징에 사용할 설정
	 *
	 * @return 작업 시작 여부
	 */
	bool StartBuild(const FEditorGameBuildSettings& InSettings);

	/**
	 * @brief 진행 중인 worker thread가 있으면 종료를 기다립니다
	 */
	void Shutdown();

	bool IsBuilding() const { return bBuilding.load(); }
	bool HasFinished() const;
	bool WasSuccessful() const;
	FString GetStatusText() const;
	FString GetLogText() const;
	FString GetLastOutputDirectory() const;

	void OpenOutputFolder() const;
	void RunGame() const;

private:
	void BuildThreadProc(FEditorGameBuildSettings InSettings);
	void ResetStateForBuild();
	void AppendLog(const FString& Text);
	void SetStatus(const FString& Text);
	void SetFinished(bool bInSucceeded);

private:
	std::thread WorkerThread;
	std::atomic<bool> bBuilding = false;

	mutable std::mutex StateMutex;
	FString LogText;
	FString StatusText = "Idle";
	FString LastOutputDirectory;
	bool bFinished = false;
	bool bSucceeded = false;
};
