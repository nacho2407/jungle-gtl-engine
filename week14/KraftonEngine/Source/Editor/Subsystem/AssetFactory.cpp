#include "Editor/Subsystem/AssetFactory.h"

#include "Animation/Graph/AnimGraphAsset.h"
#include "Animation/Graph/AnimGraphManager.h"
#include "CameraShake/CameraShakeAsset.h"
#include "CameraShake/CameraShakeManager.h"
#include "FloatCurve/FloatCurveManager.h"
#include "FloatCurve/FloatCurveAsset.h"
#include "Particles/ParticleSystemManager.h"
#include "Particles/ParticleSystem.h"
#include "Physics/PhysicalMaterial.h"
#include "Physics/PhysicalMaterialManager.h"
#include "Materials/MaterialManager.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Platform/Paths.h"
#include "Core/Logging/Log.h"
#include "SimpleJSON/json.hpp"

#include "Mesh/Skeletal/SkeletalMesh.h"  
#include "Animation/Skeleton/Skeleton.h"
#include "Physics/PhysicsAsset.h"  
#include "Physics/PhysicsAssetManager.h"
#include "Editor/Subsystem/PhysicsAssetGenerator.h"

#include <filesystem>
#include <fstream>

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

bool FAssetFactory::CreateMaterial(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".mat");
	const std::filesystem::path ProjectRoot = FPaths::RootDir();
	const FString RelativePath = FPaths::ToUtf8(AssetPath.lexically_relative(ProjectRoot).generic_wstring());

	json::JSON JsonData;
	JsonData[MatKeys::PathFileName] = RelativePath.c_str();
	JsonData[MatKeys::ShaderPath] = "Shaders/Geometry/UberLit.hlsl";
	JsonData[MatKeys::RenderPass] = "Opaque";
	JsonData[MatKeys::BlendState] = "Opaque";
	JsonData[MatKeys::DepthStencilState] = "Default";
	JsonData[MatKeys::RasterizerState] = "SolidBackCull";
	JsonData[MatKeys::ShadowMode] = "Opaque";
	JsonData[MatKeys::EmissiveColor] = json::Array(1.0f, 1.0f, 1.0f, 1.0f);
	JsonData[MatKeys::EmissiveIntensity] = 0.0f;
	JsonData[MatKeys::bEnableBloom] = false;
	JsonData[MatKeys::Parameters] = json::JSON::Make(json::JSON::Class::Object);
	JsonData[MatKeys::Parameters]["HasNormalMap"] = 0.0f;
	JsonData[MatKeys::Parameters]["SectionColor"] = json::Array(1.0f, 1.0f, 1.0f, 1.0f);
	JsonData[MatKeys::Parameters]["_pad"] = json::Array(0.0f, 0.0f, 0.0f);
	JsonData[MatKeys::Textures] = json::JSON::Make(json::JSON::Class::Object);
	JsonData[MatKeys::Textures]["DiffuseTexture"] = "";

	std::ofstream File(AssetPath);
	if (!File.is_open())
	{
		return false;
	}

	File << JsonData.dump(4);
	if (!File.good())
	{
		return false;
	}

	OutCreatedPath = RelativePath;
	return true;
}

bool FAssetFactory::CreatePhysicalMaterial(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath)
{
	const std::filesystem::path Directory(FPaths::ToWide(DirectoryPath));
	if (!std::filesystem::exists(Directory) || !std::filesystem::is_directory(Directory))
	{
		return false;
	}

	const std::filesystem::path AssetPath = BuildUniqueAssetPath(Directory, AssetName, L".uasset");

	UPhysicalMaterial* NewAsset = UObjectManager::Get().CreateObject<UPhysicalMaterial>();
	NewAsset->SetAssetPathFileName(FPaths::ToUtf8(AssetPath.wstring()));

	bool bSaved = FPhysicalMaterialManager::Get().Save(NewAsset);
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
	NewAsset->SetPreviewSkeletalMeshPath(SourceMesh->GetAssetPathFileName());

	FPhysicsAssetCreationParams BodyGenerationParams = Params;
	bool bRetriedWithLowerMinBoneSize = false;
	GeneratePhysicsAssetBodies(*NewAsset, *MeshAsset, BodyGenerationParams);

	if (NewAsset->GetBodySetups().empty()
		&& !Params.bCreateBodyForAllBones
		&& Params.MinBoneSize > 1.0f)
	{
		BodyGenerationParams.MinBoneSize = 1.0f;
		bRetriedWithLowerMinBoneSize = true;
		GeneratePhysicsAssetBodies(*NewAsset, *MeshAsset, BodyGenerationParams);
	}

	if (NewAsset->GetBodySetups().empty())
	{
		UE_LOG(
			"PhysicsAsset creation failed: generator produced no body setups. Mesh=%s AssetName=%s MinBoneSize=%.2f MinWeldSize=%.4f CreateAllBones=%s RetriedMinBoneSize=%s",
			SourceMesh->GetAssetPathFileName().c_str(),
			AssetName.c_str(),
			static_cast<double>(Params.MinBoneSize),
			static_cast<double>(Params.MinWeldSize),
			Params.bCreateBodyForAllBones ? "true" : "false",
			bRetriedWithLowerMinBoneSize ? "1.0" : "false"
		);
		UObjectManager::Get().DestroyObject(NewAsset);
		return false;
	}
	
	GeneratePhysicsAssetConstraints(*NewAsset, *MeshAsset, BodyGenerationParams);
	
	const bool bSaved = FPhysicsAssetManager::Get().Save(NewAsset);
	if (!bSaved)
	{
		UE_LOG("PhysicsAsset creation failed: save failed. Path=%s", FPaths::ToUtf8(AssetPath.wstring()).c_str());
		UObjectManager::Get().DestroyObject(NewAsset);
		return false;
	}
	
	OutCreatedPath = FPaths::ToUtf8(AssetPath.wstring());
	UE_LOG(
		"PhysicsAsset created. Path=%s PreviewMesh=%s Bodies=%llu",
		OutCreatedPath.c_str(),
		SourceMesh->GetAssetPathFileName().c_str(),
		static_cast<unsigned long long>(NewAsset->GetBodySetups().size())
	);

	return true;
}
