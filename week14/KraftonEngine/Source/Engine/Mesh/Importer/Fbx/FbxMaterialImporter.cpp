#include "Mesh/Importer/Fbx/FbxMaterialImporter.h"
#include "Materials/MaterialManager.h"
#include "Materials/Material.h"
#include "Platform/Paths.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace
{
	// 실제 파일을 찾아 프로젝트 Content/Texture/Auto/<FBX이름>/ 아래로 복사하고
	// 프로젝트 상대경로를 돌려준다. 못 찾으면 기존 동작(경로 정리)만 수행한다.
	FString ImportTextureToProject(const FString& RawTexturePath, const FString& FbxSourcePath)
	{
		if (RawTexturePath.empty())
		{
			return FString();
		}

		namespace fs = std::filesystem;

		const fs::path RawPath(FPaths::ToWide(RawTexturePath));
		const std::wstring FileName = RawPath.filename().wstring();
		if (FileName.empty())
		{
			return FPaths::MakeProjectRelative(RawTexturePath);
		}

		const fs::path FbxPath(FPaths::ToWide(FbxSourcePath));
		const fs::path FbxDir = FbxPath.parent_path();

		// 후보 경로: 원본 경로 → FBX 옆 → FBX 옆 textures/ (대소문자 변형 포함)
		TArray<fs::path> Candidates;
		Candidates.push_back(RawPath);
		if (!FbxDir.empty())
		{
			Candidates.push_back(FbxDir / FileName);
			Candidates.push_back(FbxDir / L"textures" / FileName);
			Candidates.push_back(FbxDir / L"Textures" / FileName);
		}

		fs::path FoundPath;
		for (const fs::path& Candidate : Candidates)
		{
			std::error_code Ec;
			if (fs::exists(Candidate, Ec) && fs::is_regular_file(Candidate, Ec))
			{
				FoundPath = Candidate;
				break;
			}
		}

		if (FoundPath.empty())
		{
			// 실제 파일을 못 찾으면 기존 동작 유지 (경로만 정리)
			return FPaths::MakeProjectRelative(RawTexturePath);
		}

		const std::wstring SubFolder = FbxPath.stem().wstring();
		const fs::path DestRelDir = fs::path(L"Content") / L"Texture" / L"Auto" / SubFolder;
		const fs::path DestAbsDir = fs::path(FPaths::RootDir()) / DestRelDir;

		std::error_code Ec;
		fs::create_directories(DestAbsDir, Ec);

		const fs::path DestAbsPath = DestAbsDir / FileName;
		fs::copy_file(FoundPath, DestAbsPath, fs::copy_options::overwrite_existing, Ec);
		if (Ec)
		{
			// 복사 실패 시에도 깨지지 않게 기존 동작으로 폴백
			return FPaths::MakeProjectRelative(RawTexturePath);
		}

		const fs::path DestRelPath = DestRelDir / FileName;
		return FPaths::ToUtf8(DestRelPath.generic_wstring());
	}
}

void FFbxMaterialImporter::CollectMaterials(FbxScene* Scene, FFbxImportContext& Context)
{
	Context.Materials.clear();
	Context.MaterialToSlotIndex.clear();

	if (!Scene)
	{
		return;
	}

	const int32 MaterialCount = Scene->GetMaterialCount();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		FbxSurfaceMaterial* Material = Scene->GetMaterial(MaterialIndex);
		if (!Material)
		{
			continue;
		}

		FFbxImportedMaterialInfo MaterialInfo;
		MaterialInfo.Name = Material->GetName();
		MaterialInfo.DiffuseColor = FVector(1.0f, 1.0f, 1.0f);

		FbxProperty DiffuseProp = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
		if (DiffuseProp.IsValid())
		{
			FbxDouble3 Color = DiffuseProp.Get<FbxDouble3>();
			MaterialInfo.DiffuseColor = FVector(static_cast<float>(Color[0]), static_cast<float>(Color[1]), static_cast<float>(Color[2]));

			const int32 TextureCount = DiffuseProp.GetSrcObjectCount<FbxTexture>();
			if (TextureCount > 0)
			{
				FbxFileTexture* Texture = DiffuseProp.GetSrcObject<FbxFileTexture>(0);
				if (Texture)
				{
					MaterialInfo.DiffuseTexturePath = ImportTextureToProject(Texture->GetFileName(), Context.SourcePath);
				}
			}
		}

		auto ReadTexturePath = [&Context](const FbxProperty& Property) -> FString
		{
			if (!Property.IsValid())
			{
				return "";
			}

			const int32 TextureCount = Property.GetSrcObjectCount<FbxTexture>();
			for (int32 TextureIndex = 0; TextureIndex < TextureCount; ++TextureIndex)
			{
				FbxFileTexture* Texture = Property.GetSrcObject<FbxFileTexture>(TextureIndex);
				if (Texture)
				{
					return ImportTextureToProject(Texture->GetFileName(), Context.SourcePath);
				}
			}

			return "";
		};

		FbxProperty NormalProp = Material->FindProperty(FbxSurfaceMaterial::sNormalMap);
		MaterialInfo.NormalTexturePath = ReadTexturePath(NormalProp);

		if (MaterialInfo.NormalTexturePath.empty())
		{
			FbxProperty BumpProp = Material->FindProperty(FbxSurfaceMaterial::sBump);
			MaterialInfo.NormalTexturePath = ReadTexturePath(BumpProp);
		}

		const int32 GlobalIndex = static_cast<int32>(Context.Materials.size());
		Context.Materials.push_back(MaterialInfo);
		Context.MaterialToSlotIndex[Material] = GlobalIndex;
	}
}

int32 FFbxMaterialImporter::GetMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex)
{
	FbxLayerElementMaterial* LayerElementMaterial = Mesh ? Mesh->GetElementMaterial() : nullptr;
	if (!LayerElementMaterial)
	{
		return -1;
	}

	FbxLayerElementArrayTemplate<int32>& MaterialIndices = LayerElementMaterial->GetIndexArray();
	switch (LayerElementMaterial->GetMappingMode())
	{
	case FbxLayerElement::eAllSame:
		return MaterialIndices[0];
	case FbxLayerElement::eByPolygon:
		return MaterialIndices[PolygonIndex];
	default:
		return 0;
	}
}

void FFbxMaterialImporter::BuildStaticMaterials(const FFbxImportContext& Context, TArray<FStaticMaterial>& OutMaterials)
{
	OutMaterials.clear();
	OutMaterials.reserve(Context.Materials.size());

	for (const FFbxImportedMaterialInfo& MaterialInfo : Context.Materials)
	{
		FStaticMaterial NewMaterial;
		NewMaterial.MaterialSlotName = MaterialInfo.Name;
		NewMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterialInterface(CreateOrUpdateMaterialAsset(MaterialInfo));
		OutMaterials.push_back(NewMaterial);
	}
}

void FFbxMaterialImporter::BuildSkeletalMaterials(const FFbxImportContext& Context, const TArray<FSkeletalMeshSection>& Sections, TArray<FSkeletalMaterial>& OutMaterials, TArray<FSkeletalMeshSection>& InOutSections)
{
	OutMaterials.clear();
	OutMaterials.reserve(Context.Materials.size());

	for (const FFbxImportedMaterialInfo& MaterialInfo : Context.Materials)
	{
		const FString MaterialPath = CreateOrUpdateMaterialAsset(MaterialInfo);
		UMaterial* MaterialObject = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);

		FSkeletalMaterial NewMaterial;
		NewMaterial.MaterialInterface = MaterialObject;
		NewMaterial.MaterialSlotName = MaterialInfo.Name;
		NewMaterial.MaterialPath = MaterialPath;
		OutMaterials.push_back(NewMaterial);
	}

	bool bNeedsNoneSlot = OutMaterials.empty();
	for (const FSkeletalMeshSection& Section : Sections)
	{
		if (Section.MaterialSlotName == "None")
		{
			bNeedsNoneSlot = true;
			break;
		}
	}

	if (bNeedsNoneSlot)
	{
		FSkeletalMaterial DefaultMaterial;
		DefaultMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterialInterface("None");
		DefaultMaterial.MaterialSlotName = "None";
		DefaultMaterial.MaterialPath = DefaultMaterial.MaterialInterface
			? DefaultMaterial.MaterialInterface->GetAssetPathFileName()
			: FString();
		OutMaterials.push_back(DefaultMaterial);

		const int32 NoneMaterialIndex = static_cast<int32>(OutMaterials.size()) - 1;
		for (FSkeletalMeshSection& Section : InOutSections)
		{
			if (Section.MaterialSlotName == "None")
			{
				Section.MaterialIndex = NoneMaterialIndex;
			}
		}
	}
}

FString FFbxMaterialImporter::CreateOrUpdateMaterialAsset(const FFbxImportedMaterialInfo& MaterialInfo)
{
	const FString MatPath = "Content/Material/Auto/" + MaterialInfo.Name + ".mat";

	if (std::filesystem::exists(FPaths::ToWide(MatPath)))
	{
		return MatPath;
	}

	std::filesystem::create_directories(FPaths::ToWide("Content/Material/Auto"));

	json::JSON JsonData;
	JsonData["PathFileName"] = MatPath;
	JsonData["Origin"] = "FbxImport";
	JsonData["ShaderPath"] = "Shaders/Geometry/UberLit.hlsl";
	JsonData["RenderPass"] = "Opaque";

	if (!MaterialInfo.DiffuseTexturePath.empty())
	{
		JsonData["Textures"]["DiffuseTexture"] = FPaths::MakeProjectRelative(MaterialInfo.DiffuseTexturePath);
		JsonData["Parameters"]["SectionColor"][0] = 1.0f;
		JsonData["Parameters"]["SectionColor"][1] = 1.0f;
		JsonData["Parameters"]["SectionColor"][2] = 1.0f;
		JsonData["Parameters"]["SectionColor"][3] = 1.0f;
	}
	else
	{
		JsonData["Parameters"]["SectionColor"][0] = MaterialInfo.DiffuseColor.X;
		JsonData["Parameters"]["SectionColor"][1] = MaterialInfo.DiffuseColor.Y;
		JsonData["Parameters"]["SectionColor"][2] = MaterialInfo.DiffuseColor.Z;
		JsonData["Parameters"]["SectionColor"][3] = 1.0f;
	}

	if (!MaterialInfo.NormalTexturePath.empty())
	{
		JsonData["Textures"]["NormalTexture"] = FPaths::MakeProjectRelative(MaterialInfo.NormalTexturePath);
		JsonData["Parameters"]["HasNormalMap"] = 1.0f;
	}
	else
	{
		JsonData["Parameters"]["HasNormalMap"] = 0.0f;
	}

	std::ofstream File(FPaths::ToWide(MatPath));
	File << JsonData.dump();

	return MatPath;
}
