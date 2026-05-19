#include "Mesh/Fbx/FbxSceneLoader.h"
#include "Platform/Paths.h"
#include "Core/Log.h"

#include <chrono>

namespace
{
	using FFbxClock = std::chrono::steady_clock;

	static double GetElapsedSeconds(const FFbxClock::time_point& Start)
	{
		return std::chrono::duration<double>(FFbxClock::now() - Start).count();
	}

	struct FFbxSharedSdkContext
	{
		FbxManager* Manager = nullptr;
		FbxIOSettings* IoSettings = nullptr;

		FFbxSharedSdkContext()
		{
			Manager = FbxManager::Create();
			if (!Manager)
			{
				return;
			}

			IoSettings = FbxIOSettings::Create(Manager, IOSROOT);
			if (!IoSettings)
			{
				Manager->Destroy();
				Manager = nullptr;
				return;
			}

			Manager->SetIOSettings(IoSettings);
		}

		~FFbxSharedSdkContext()
		{
			if (Manager)
			{
				Manager->Destroy();
				Manager = nullptr;
				IoSettings = nullptr;
			}
		}

		bool IsValid() const
		{
			return Manager != nullptr && IoSettings != nullptr;
		}
	};

	static FFbxSharedSdkContext& GetSharedSdkContext()
	{
		static FFbxSharedSdkContext Context;
		return Context;
	}
}

FFbxSceneHandle::~FFbxSceneHandle()
{
	Reset();
}

void FFbxSceneHandle::Reset()
{
	if (Scene)
	{
		const auto Start = FFbxClock::now();
		Scene->Destroy(true);
		Scene = nullptr;

		UE_LOG("FBX scene cleanup timing: SceneDestroy=%.3fs", GetElapsedSeconds(Start));
	}

	// Manager is a shared process-lifetime SDK context owned by FbxSceneLoader.cpp.
	Manager = nullptr;
}

namespace
{
	static void ApplyFbxImportOptions(FbxIOSettings* IoSettings, const FFbxSceneLoadOptions& Options)
	{
		if (!IoSettings)
		{
			return;
		}

		IoSettings->SetBoolProp(IMP_FBX_MATERIAL, Options.bImportMaterials);
		IoSettings->SetBoolProp(IMP_FBX_TEXTURE, Options.bImportTextures);
		IoSettings->SetBoolProp(IMP_FBX_LINK, Options.bImportLinks);
		IoSettings->SetBoolProp(IMP_FBX_SHAPE, Options.bImportShapes);
		IoSettings->SetBoolProp(IMP_FBX_GOBO, Options.bImportGobos);
		IoSettings->SetBoolProp(IMP_FBX_ANIMATION, Options.bImportAnimations);
		IoSettings->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, Options.bImportGlobalSettings);
	}

	static void ApplyFbxAnimationStackSelection(FbxImporter* Importer, const FFbxSceneLoadOptions& Options)
	{
		if (!Importer || !Options.bImportAnimations || Options.SelectedAnimationStackIndices.empty())
		{
			return;
		}

		const int32 AnimStackCount = Importer->GetAnimStackCount();
		for (int32 StackIndex = 0; StackIndex < AnimStackCount; ++StackIndex)
		{
			if (FbxTakeInfo* TakeInfo = Importer->GetTakeInfo(StackIndex))
			{
				TakeInfo->mSelect = Options.SelectedAnimationStackIndices.find(StackIndex) != Options.
				SelectedAnimationStackIndices.end();
			}
		}
	}
}

bool FFbxSceneLoader::Load(const FString& SourcePath, FFbxSceneHandle& OutScene, FString* OutMessage)
{
	return Load(SourcePath, FFbxSceneLoadOptions(), OutScene, OutMessage);
}

bool FFbxSceneLoader::Load(
	const FString&              SourcePath,
	const FFbxSceneLoadOptions& Options,
	FFbxSceneHandle&            OutScene,
	FString*                    OutMessage
	)
{
	OutScene.Reset();

	FFbxSharedSdkContext& SdkContext = GetSharedSdkContext();
	if (!SdkContext.IsValid())
	{
		if (OutMessage) *OutMessage = "FBX SDK manager creation failed.";
		return false;
	}

	FbxManager* SdkManager = SdkContext.Manager;
	FbxIOSettings* IoSettings = SdkContext.IoSettings;
	ApplyFbxImportOptions(IoSettings, Options);

	FbxScene* Scene = FbxScene::Create(SdkManager, "FBX Scene");
	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
	if (!Scene || !Importer)
	{
		if (Importer) Importer->Destroy();
		if (Scene) Scene->Destroy(true);
		if (OutMessage) *OutMessage = "FBX scene/importer creation failed.";
		return false;
	}

	const FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(SourcePath)));
	if (!Importer->Initialize(FullPath.c_str(), -1, SdkManager->GetIOSettings()))
	{
		Importer->Destroy();
		Scene->Destroy(true);
		if (OutMessage) *OutMessage = FString("FBX importer initialize failed: ") + SourcePath;
		return false;
	}

	ApplyFbxAnimationStackSelection(Importer, Options);

	if (!Importer->Import(Scene))
	{
		Importer->Destroy();
		Scene->Destroy(true);
		if (OutMessage) *OutMessage = FString("FBX scene import failed: ") + SourcePath;
		return false;
	}

	Importer->Destroy();
	OutScene.Manager = SdkManager;
	OutScene.Scene = Scene;
	return true;
}

void FFbxSceneLoader::NormalizeScene(FbxScene* Scene)
{
	if (!Scene)
	{
		return;
	}

	FbxSystemUnit::m.ConvertScene(Scene);

	FbxAxisSystem UnrealAxisSystem(
		FbxAxisSystem::eZAxis,
		FbxAxisSystem::eParityEven,
		FbxAxisSystem::eLeftHanded
	);
	UnrealAxisSystem.DeepConvertScene(Scene);
}

void FFbxSceneLoader::Triangulate(FbxManager* Manager, FbxScene* Scene)
{
	if (!Manager || !Scene)
	{
		return;
	}

	FbxGeometryConverter Converter(Manager);
	Converter.Triangulate(Scene, true);
}
