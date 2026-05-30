#include "Editor/Subsystem/AssetFactory.h"

#include "Animation/Graph/AnimGraphAsset.h"
#include "Animation/Graph/AnimGraphManager.h"
#include "CameraShake/CameraShakeAsset.h"
#include "CameraShake/CameraShakeManager.h"
#include "FloatCurve/FloatCurveManager.h"
#include "FloatCurve/FloatCurveAsset.h"
#include "Particles/ParticleSystemManager.h"
#include "Particles/ParticleSystem.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Platform/Paths.h"
#include "Core/Logging/Log.h"

#include "Mesh/MeshManager.h"
#include "Mesh/Skeletal/SkeletalMesh.h"  
#include "Animation/Skeleton/Skeleton.h"
#include "Physics/PhysicsAsset.h"  
#include "Physics/PhysicsAssetManager.h"
#include "Editor/Subsystem/PhysicsAssetGenerator.h"

#include <filesystem>

namespace
{
	FString SanitizeAssetStem(const FString& AssetName)
	{
		return AssetName.empty() ? FString("NewFloatCurve") : AssetName;
	}

	std::filesystem::path BuildUniqueAssetPath(const std::filesystem::path& Directory, const FString& AssetName, const wchar_t* Extension)
	{
		const FString BaseStem = SanitizeAssetStem(AssetName);

		int32 Suffix = 0;
		for (;;)
		{
			FString CandidateStem = BaseStem;
			if (Suffix > 0)
			{
				CandidateStem += "_";
				CandidateStem += std::to_string(Suffix);
			}

			std::filesystem::path CandidatePath = Directory / (FPaths::ToWide(CandidateStem) + Extension);
			if (!std::filesystem::exists(CandidatePath))
			{
				return CandidatePath;
			}

			++Suffix;
		}
	}
}

bool FAssetFactory::CreateFloatCurve(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UFloatCurveAsset* NewAsset = UObjectManager::Get().CreateObject<UFloatCurveAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));

	FFloatCurve& Curve = NewAsset->GetCurve();
	Curve.Reset();
	Curve.AddKey(0.0f, 0.0f);
	Curve.AddKey(1.0f, 1.0f);
	Curve.SortKeys();

	bool bSaved = FFloatCurveManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateCameraShake(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UCameraShakeAsset* NewAsset = UObjectManager::Get().CreateObject<UCameraShakeAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));
	NewAsset->Version = 1;
	NewAsset->ShakeType = ECameraShakeType::Sequence;

	bool bSaved = FCameraShakeManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateAnimGraph(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UAnimGraphAsset* NewAsset = UObjectManager::Get().CreateObject<UAnimGraphAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));
	NewAsset->InitializeDefault(); // SequencePlayer → OutputPose 기본 그래프.

	bool bSaved = FAnimGraphManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreateParticleSystem(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UParticleSystem* NewAsset = UObjectManager::Get().CreateObject<UParticleSystem>();
	NewAsset->InitializeDefaultEmitters();
	NewAsset->SetAssetPathFileName(FPaths::ToUtf8(AssetPath.wstring()));

	bool bSaved = FParticleSystemManager::Get().Save(NewAsset);
	UObjectManager::Get().DestroyObject(NewAsset);

	if (!bSaved)
	{
		return false;
	}

	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	return true;
}

bool FAssetFactory::CreatePhysicsAsset(const FString& DirectoryPath, const FString& AssetName, USkeletalMesh* SourceMesh, const FPhysicsAssetCreationParams& Params, FString& OutCreatedPath)
{
	OutCreatedPath.clear();

	if (!SourceMesh)
	{
		UE_LOG("PhysicsAsset creation failed: source skeletal mesh is null. Directory=%s AssetName=%s", DirectoryPath.c_str(), AssetName.c_str());
		return false;
	}

	FSkeletalMesh* MeshAsset = SourceMesh->GetSkeletalMeshAsset();
	if (!MeshAsset || MeshAsset->Bones.empty() || MeshAsset->Vertices.empty())
	{
		UE_LOG(
			"PhysicsAsset creation failed: source mesh has no usable skeletal data. Mesh=%s Bones=%llu Vertices=%llu",
			SourceMesh->GetAssetPathFileName().c_str(),
			static_cast<unsigned long long>(MeshAsset ? MeshAsset->Bones.size() : 0),
			static_cast<unsigned long long>(MeshAsset ? MeshAsset->Vertices.size() : 0)
		);
		return false;
	}

	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		UE_LOG("PhysicsAsset creation failed: target directory is invalid. Directory=%s AssetName=%s", DirectoryPath.c_str(), AssetName.c_str());
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UPhysicsAsset* NewAsset = UObjectManager::Get().CreateObject<UPhysicsAsset>();
	NewAsset->SetSourcePath(FPaths::ToUtf8(AssetPath.wstring()));

	GeneratePhysicsAssetBodies(*NewAsset, *MeshAsset, Params);

	if (NewAsset->GetBodySetups().empty())
	{
		UE_LOG(
			"PhysicsAsset creation failed: generator produced no body setups. Mesh=%s AssetName=%s MinBoneSize=%.2f CreateAllBones=%s",
			SourceMesh->GetAssetPathFileName().c_str(),
			AssetName.c_str(),
			static_cast<double>(Params.MinBoneSize),
			Params.bCreateBodyForAllBones ? "true" : "false"
		);
		UObjectManager::Get().DestroyObject(NewAsset);
		return false;
	}
	
	GeneratePhysicsAssetConstraints(*NewAsset, *MeshAsset, Params);
	
	const bool bSaved = FPhysicsAssetManager::Get().Save(NewAsset);
	if (!bSaved)
	{
		UE_LOG("PhysicsAsset creation failed: save failed. Path=%s", FPaths::ToUtf8(AssetPath.wstring()).c_str());
		UObjectManager::Get().DestroyObject(NewAsset);
		return false;
	}
	
	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	UE_LOG(
		"PhysicsAsset created. Path=%s Bodies=%llu",
		OutCreatedPath.c_str(),
		static_cast<unsigned long long>(NewAsset->GetBodySetups().size())
	);

	// 메시가 자기 PhysicsAsset 경로를 기억하게 한다.
	// 같은 세션에서 메시는 동일 인스턴스(매니저 캐시)라 뷰포트가 즉시 본다.
	SourceMesh->SetPhysicsAssetPath(OutCreatedPath);
	if (!FMeshManager::SaveSkeletalMeshPackage(SourceMesh))
	{
		UE_LOG("PhysicsAsset created but source mesh link save failed. Mesh=%s PhysicsAsset=%s",
			SourceMesh->GetAssetPathFileName().c_str(),
			OutCreatedPath.c_str());
	}

	return true;
}
