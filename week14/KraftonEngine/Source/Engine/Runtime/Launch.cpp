#include "Engine/Runtime/Launch.h"

#include "Animation/AnimationManager.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Engine/Runtime/EngineLoop.h"
#include "Engine/Platform/CrashDump.h"
#include "Object/FName.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Platform/Paths.h"
#include <objbase.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#pragma comment(lib, "ole32.lib")

// 빌드 변종에 맞는 UEngine 서브클래스 헤더만 포함. EngineLoop 자체는 구체 클래스를
// 모르고, 진입점인 이 파일이 팩토리를 만들어 주입한다 (Engine→Editor/Game 의존
// 끊기 위함).
#if IS_OBJ_VIEWER
#include "ObjViewer/ObjViewerEngine.h"
#elif WITH_EDITOR
#include "Editor/EditorEngine.h"
#elif WITH_STANDALONE
#include "Engine/Runtime/GameEngine.h"
#endif

namespace
{
	struct FFpsArmAnimationImportItem
	{
		const char* SourcePath;
		const char* Note;
	};

	bool HasCommandLineFlag(const char* CommandLine, const char* Flag)
	{
		if (!CommandLine || !Flag)
		{
			return false;
		}

		const std::string Args(CommandLine);
		return Args.find(Flag) != std::string::npos;
	}

	std::string GetCommandLineValue(const char* CommandLine, const char* Prefix)
	{
		if (!CommandLine || !Prefix)
		{
			return {};
		}

		const std::string Args(CommandLine);
		const std::string Key(Prefix);
		const size_t Begin = Args.find(Key);
		if (Begin == std::string::npos)
		{
			return {};
		}

		size_t ValueBegin = Begin + Key.size();
		if (ValueBegin < Args.size() && Args[ValueBegin] == '"')
		{
			++ValueBegin;
			const size_t ValueEnd = Args.find('"', ValueBegin);
			return Args.substr(ValueBegin, ValueEnd == std::string::npos ? std::string::npos : ValueEnd - ValueBegin);
		}

		const size_t ValueEnd = Args.find(' ', ValueBegin);
		return Args.substr(ValueBegin, ValueEnd == std::string::npos ? std::string::npos : ValueEnd - ValueBegin);
	}

	int RunFpsArmAnimationImportCommandlet(const char* CommandLine)
	{
		FNamePool::Get();
		FObjectFactory::Get();

		const FString TargetSkeletonPath = "Content/Data/FirstPerson/Body/TitanHuman_Body_Skeleton.uasset";
		const FString DestinationDirectory = "Content/Data/FirstPerson/Animations/FPSARM";
		const std::string SingleSourcePath = GetCommandLineValue(CommandLine, "--fpsarm-source=");

		std::vector<FFpsArmAnimationImportItem> Items;
		if (!SingleSourcePath.empty())
		{
			Items.push_back({ SingleSourcePath.c_str(), "single source" });
		}
		else
		{
			Items = {
				{ "Content/Data/FirstPerson/Animations/a_Idle.fbx", "stand idle" },
				{ "Content/Data/FirstPerson/Animations/pt_Walk_Forward.fbx", "walk forward" },
				{ "Content/Data/FirstPerson/Animations/pt_Run_Forward.fbx", "run forward" },
				{ "Content/Data/FirstPerson/Animations/a_Idle_ADS.fbx", "stand ADS idle" },
				{ "Content/Data/FirstPerson/Animations/pt_ADS_Walk_Forward.fbx", "ADS walk forward" },
				{ "Content/Data/FirstPerson/Animations/pt_Run_Forward_ADS.fbx", "ADS run forward" },
				{ "Content/Data/FirstPerson/Animations/a_Idle_Crouch.fbx", "crouch idle" },
				{ "Content/Data/FirstPerson/Animations/a_CrouchWalkN.fbx", "crouch walk forward" },
				{ "Content/Data/FirstPerson/Animations/a_Idle_Crouch_ADS.fbx", "crouch ADS idle" },
				{ "Content/Data/FirstPerson/Animations/a_CrouchWalkAdsN.fbx", "crouch ADS walk forward" },
				{ "Content/Data/FirstPerson/Animations/a_stand_2_crouch.fbx", "stand to crouch" },
				{ "Content/Data/FirstPerson/Animations/a_crouch_2_stand.fbx", "crouch to stand" },
				{ "Content/Data/FirstPerson/Animations/a_MP_Jump_start.fbx", "jump start" },
				{ "Content/Data/FirstPerson/Animations/a_MP_Jump_float.fbx", "jump float" },
				{ "Content/Data/FirstPerson/Animations/a_MP_Jump_end.fbx", "jump land" },
				{ "Content/Data/FirstPerson/Animations/pt_Slide_Forward.fbx", "slide forward" },
				{ "Content/Data/FirstPerson/Animations/a_pt_wallrun_left.fbx", "wall run left" },
				{ "Content/Data/FirstPerson/Animations/a_pt_wallrun_right.fbx", "wall run right" },
				{ "Content/Data/FirstPerson/Animations/a_Fire.fbx", "hip fire" },
				{ "Content/Data/FirstPerson/Animations/a_shootGeneric.fbx", "ADS fire fallback" },
				{ "Content/Data/FirstPerson/Animations/a_CrouchFire.fbx", "crouch fire" },
				{ "Content/Data/FirstPerson/Animations/a_Rifle_Reload.fbx", "rifle reload" },
				{ "Content/Data/FirstPerson/Animations/a_RifleReload_Crouch.fbx", "crouch rifle reload" },
			};
		}

		const std::filesystem::path LogPath =
			std::filesystem::path(FPaths::RootDir()) / L"Saves" / L"Logs" / L"FpsArmAnimationImport.log";
		FPaths::CreateDir(LogPath.parent_path().wstring());

		std::ofstream Log(LogPath, std::ios::out | std::ios::trunc);
		auto WriteLog = [&Log](const std::string& Text)
		{
			if (Log)
			{
				Log << Text << "\n";
				Log.flush();
			}
		};

		WriteLog("FPSARM animation import");
		WriteLog("TargetSkeleton=" + TargetSkeletonPath);
		WriteLog("Destination=" + DestinationDirectory);

		int32 FailureCount = 0;
		int32 SavedCount = 0;

		for (const FFpsArmAnimationImportItem& Item : Items)
		{
			FAnimationImportRequest Request;
			Request.SourceFbxPath = Item.SourcePath;
			Request.TargetSkeletonPath = TargetSkeletonPath;
			Request.DestinationDirectory = DestinationDirectory;
			Request.bAllowTargetExtraBones = true;
			Request.bOverwriteExistingAssets = true;

			TArray<UAnimSequence*> ImportedSequences;
			const bool bImported = FAnimationManager::Get().ImportAnimationForSkeleton(Request, &ImportedSequences);
			if (!bImported || ImportedSequences.empty())
			{
				++FailureCount;
				WriteLog(std::string("[FAIL] ") + Item.SourcePath + " (" + Item.Note + ")");
				continue;
			}

			for (UAnimSequence* Sequence : ImportedSequences)
			{
				if (!Sequence)
				{
					continue;
				}

				++SavedCount;
				WriteLog(std::string("[OK] ") + Item.SourcePath + " -> " + Sequence->GetAssetPathFileName() + " (" + Item.Note + ")");
			}
		}

		WriteLog("SavedCount=" + std::to_string(SavedCount));
		WriteLog("FailureCount=" + std::to_string(FailureCount));
		return FailureCount == 0 ? 0 : 1;
	}

	UEngine* CreateConcreteEngine()
	{
#if IS_OBJ_VIEWER
		return UObjectManager::Get().CreateObject<UObjViewerEngine>();
#elif WITH_EDITOR
		return UObjectManager::Get().CreateObject<UEditorEngine>();
#elif WITH_STANDALONE
		return UObjectManager::Get().CreateObject<UGameEngine>();
#else
		return UObjectManager::Get().CreateObject<UEngine>();
#endif
	}

	int GuardedMain(HINSTANCE hInstance, int nShowCmd)
	{
		const HRESULT ComInitResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		const bool bDidInitializeCOM = SUCCEEDED(ComInitResult);

		FEngineLoop EngineLoop(&CreateConcreteEngine);
		if (!EngineLoop.Init(hInstance, nShowCmd))
		{
			if (bDidInitializeCOM)
			{
				CoUninitialize();
			}
			return -1;
		}

		const int ExitCode = EngineLoop.Run();
		EngineLoop.Shutdown();
		if (bDidInitializeCOM)
		{
			CoUninitialize();
		}
		return ExitCode;
	}
}

int Launch(HINSTANCE hInstance, LPSTR lpCmdLine, int nShowCmd)
{
	__try
	{
		if (HasCommandLineFlag(lpCmdLine, "--import-fpsarm-animations"))
		{
			return RunFpsArmAnimationImportCommandlet(lpCmdLine);
		}

		return GuardedMain(hInstance, nShowCmd);
	}
	__except (WriteCrashDump(GetExceptionInformation()))
	{
		return static_cast<int>(GetExceptionCode());
	}
}
