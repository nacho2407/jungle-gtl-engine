#include "MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Importer/ObjImporter.h"
#include "Mesh/Importer/FbxImporter.h"
#include "Mesh/MeshBinary.h"
#include "Materials/Material.h"
#include "Core/Log.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include "Asset/AssetPackage.h"

#include <algorithm>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <memory>
#include <utility>

#include "Animation/AnimationManager.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Skeleton/Skeleton.h"
#include "Animation/Skeleton/SkeletonManager.h"
#include "Animation/Skeleton/SkeletonTypes.h"

TMap<FString, UStaticMesh*> FMeshManager::StaticMeshCache;
TMap<FString, USkeletalMesh*> FMeshManager::SkeletalMeshCache;
TArray<FAssetListItem> FMeshManager::AvailableStaticMeshFiles;
TArray<FAssetListItem> FMeshManager::AvailableStaticMeshSourceFiles;
TArray<FAssetListItem> FMeshManager::AvailableSkeletalMeshFiles;
TArray<FAssetListItem> FMeshManager::AvailableFbxSourceFiles;

FMeshManager& FMeshManager::Get()
{
	static FMeshManager Instance;
	return Instance;
}

static void EnsureMeshCacheDirExists()
{
	static bool bCreated = false;
	if (!bCreated)
	{
		std::wstring CacheDir = FPaths::RootDir() + L"Content/MeshCache/";
		FPaths::CreateDir(CacheDir);
		bCreated = true;
	}
}

static std::wstring GetLowerExtension(const FString& Path)
{
	std::filesystem::path SrcPath(FPaths::ToWide(Path));
	std::wstring Ext = SrcPath.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
	return Ext;
}

static FString NormalizeProjectPath(const FString& Path)
{
	return FPaths::MakeProjectRelative(Path);
}

static FString GetMeshPackageFilePath(const FString& SourcePath, EAssetPackageType Type)
{
	std::filesystem::path SrcPath(FPaths::ToWide(SourcePath));
	std::wstring Ext = SrcPath.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

	if (Ext == MeshBinary::AssetPackageExtension)
	{
		return NormalizeProjectPath(SourcePath);
	}

	// 경로는 동일하지만, 확장자를 나눠서 Mesh 타입을 알 수 있게 한다.
	// SourcePath 가 이미 Content/ 하위면 (cleanup 이후 모든 source) 그대로 사용 — 그렇지
	// 않으면 (구 Data/ root 호환) Content/ prefix 박음. prefix 이중 적용 방지.
	std::filesystem::path ProjectRelative = std::filesystem::path(FPaths::ToWide(FPaths::MakeProjectRelative(SourcePath))).lexically_normal();

	std::filesystem::path AssetPath;
	if (!ProjectRelative.empty() && ProjectRelative.begin()->wstring() == L"Content")
	{
		AssetPath = ProjectRelative;
	}
	else
	{
		AssetPath = std::filesystem::path(L"Content") / ProjectRelative;
	}

	if (Type == EAssetPackageType::StaticMesh)
	{
		AssetPath.replace_filename(AssetPath.stem().wstring() + L"_StaticMesh" + L".uasset");
	}
	else if (Type == EAssetPackageType::SkeletalMesh)
	{
		AssetPath.replace_filename(AssetPath.stem().wstring() + L"_SkeletalMesh" + L".uasset");
	}
	else
	{
		UE_LOG("GetMeshPackageFilePath failed: unsupported asset package type. SourcePath=%s", SourcePath.c_str());
		return FString();
	}

	std::filesystem::path FullAssetPath = std::filesystem::path(FPaths::RootDir()) / AssetPath;

	FPaths::CreateDir(FullAssetPath.parent_path().wstring());

	return FPaths::ToUtf8(AssetPath.generic_wstring());
}

static std::filesystem::path ResolveProjectPath(const FString& Path)
{
	std::filesystem::path FullPath(FPaths::ToWide(Path));
	if (!FullPath.is_absolute())
	{
		FullPath = std::filesystem::path(FPaths::RootDir()) / FullPath;
	}
	return FullPath.lexically_normal();
}

static bool TryGetSourceFileState(const FString& SourcePath, uint64& OutTimestamp, uint64& OutFileSize)
{
	std::filesystem::path FullPath = ResolveProjectPath(SourcePath);

	if (!std::filesystem::exists(FullPath) || !std::filesystem::is_regular_file(FullPath)) return false;

	OutFileSize = static_cast<uint64>(std::filesystem::file_size(FullPath));

	const auto WriteTime = std::filesystem::last_write_time(FullPath);
	OutTimestamp = static_cast<uint64>(WriteTime.time_since_epoch().count());

	return true;
}

static FAssetImportMetadata MakeImportMetadata(const FString& SourcePath)
{
	FAssetImportMetadata Metadata;
	Metadata.SourcePath = NormalizeProjectPath(SourcePath);

	TryGetSourceFileState(SourcePath, Metadata.SourceTimestamp, Metadata.SourceFileSize);

	return Metadata;
}

static bool IsPackageSourceStale(const FString& BinaryPath, EAssetPackageType ExpectedType, bool& bOutMissingSource)
{
	bOutMissingSource = false;

	FAssetImportMetadata Metadata;
	if (!FAssetPackage::ReadMetadata(BinaryPath, ExpectedType, Metadata)) return true;

	uint64 CurrentTimestamp = 0;
	uint64 CurrentFileSize = 0;
	if (!TryGetSourceFileState(Metadata.SourcePath, CurrentTimestamp, CurrentFileSize))
	{
		bOutMissingSource = true;
		return true;
	}

	return !Metadata.MatchesSource(CurrentTimestamp, CurrentFileSize);
}

static bool IsSupportedStaticMeshSourcePath(const FString& Path)
{
	const std::wstring Ext = GetLowerExtension(Path);
	return Ext == L".obj" || Ext == L".fbx";
}

static bool IsSupportedSkeletalMeshSourcePath(const FString& Path)
{
	const std::wstring Ext = GetLowerExtension(Path);
	return Ext == L".fbx";
}

static bool ImportStaticMeshByExtension(const FString& PathFileName, const FImportOptions* Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	const std::wstring Ext = GetLowerExtension(PathFileName);
	if (Ext == L".obj")
	{
		return Options
			? FObjImporter::Import(PathFileName, *Options, OutMesh, OutMaterials)
			: FObjImporter::Import(PathFileName, OutMesh, OutMaterials);
	}

	if (Ext == L".fbx")
	{
		FFbxStaticMeshImportResult ImportResult;
		if (!FFbxImporter::ImportStaticMesh(PathFileName, Options, ImportResult))
		{
			return false;
		}

		OutMesh = std::move(ImportResult.Mesh);
		OutMaterials = std::move(ImportResult.Materials);
		return true;
	}

	UE_LOG("StaticMesh import failed: unsupported source extension. Path=%s", PathFileName.c_str());
	return false;
}

static bool LoadStaticMeshBinary(UStaticMesh* StaticMesh, const FString& BinaryPath)
{
	FWindowsBinReader Reader(BinaryPath);
	if (!Reader.IsValid())
	{
		UE_LOG("StaticMesh binary open failed. Path=%s", BinaryPath.c_str());
		return false;
	}

	try
	{
		FAssetPackageHeader Header;
		Reader << Header;

		if (!Header.IsValid(EAssetPackageType::StaticMesh))
		{
			UE_LOG("StaticMesh binary read failed: invalid file header. Path=%s", BinaryPath.c_str());
			return false;
		}

		FAssetImportMetadata Metadata;
		Reader << Metadata;

		StaticMesh->Serialize(Reader);
	}
	catch (const std::exception&)
	{
		UE_LOG("StaticMesh binary read failed: serialization threw an exception. Path=%s", BinaryPath.c_str());
		return false;
	}

	if (!Reader.IsValid())
	{
		UE_LOG("StaticMesh binary read failed: file data is incomplete or corrupted. Path=%s", BinaryPath.c_str());
		return false;
	}

	return true;
}

static bool SaveStaticMeshBinary(UStaticMesh* StaticMesh, const FString& BinaryPath, const FString& SourcePath)
{
	FWindowsBinWriter Writer(BinaryPath);
	if (!Writer.IsValid())
	{
		UE_LOG("StaticMesh binary save failed: could not open file. Path=%s", BinaryPath.c_str());
		return false;
	}

	try
	{
		FAssetPackageHeader Header;
		Header.Type = static_cast<uint32>(EAssetPackageType::StaticMesh);
		Writer << Header;

		FAssetImportMetadata Metadata = MakeImportMetadata(SourcePath);
		Writer << Metadata;

		StaticMesh->Serialize(Writer);
	}
	catch (const std::exception&)
	{
		UE_LOG("StaticMesh binary save failed: serialization threw an exception. Path=%s", BinaryPath.c_str());
		return false;
	}

	return Writer.IsValid();
}

static bool LoadSkeletalMeshBinary(USkeletalMesh* SkeletalMesh, const FString& BinaryPath)
{
	FWindowsBinReader Reader(BinaryPath);
	if (!Reader.IsValid())
	{
		UE_LOG("SkeletalMesh binary open failed. Path=%s", BinaryPath.c_str());
		return false;
	}

	try
	{
		FAssetPackageHeader Header;
		Reader << Header;

		if (!Header.IsValid(EAssetPackageType::SkeletalMesh))
		{
			UE_LOG("SkeletalMesh binary read failed: invalid file header. Path=%s", BinaryPath.c_str());
			return false;
		}

		FAssetImportMetadata Metadata;
		Reader << Metadata;

		SkeletalMesh->Serialize(Reader);
	}
	catch (const std::exception&)
	{
		UE_LOG("SkeletalMesh binary read failed: serialization threw an exception. Path=%s", BinaryPath.c_str());
		return false;
	}

	if (!Reader.IsValid())
	{
		UE_LOG("SkeletalMesh binary read failed: file data is incomplete or corrupted. Path=%s", BinaryPath.c_str());
		return false;
	}

	return true;
}

static bool RemapSkeletalMeshToSkeleton(
	FSkeletalMesh&                Mesh,
	const FReferenceSkeleton&     SourceSkeleton,
	const USkeleton*              TargetSkeleton,
	const FSkeletonBoneRemap&     Remap,
	FSkeletonCompatibilityReport* OutReport = nullptr
	)
{
	if (!TargetSkeleton)
	{
		if (OutReport)
		{
			OutReport->Result = ESkeletonCompatibilityResult::Incompatible;
			OutReport->Reason = "null target skeleton";
		}
		return false;
	}

	const FReferenceSkeleton& TargetRef = TargetSkeleton->GetReferenceSkeleton();
	TArray<FBone>             RemappedBones;
	RemappedBones.resize(TargetRef.GetNumBones());

	for (int32 TargetIndex = 0; TargetIndex < TargetRef.GetNumBones(); ++TargetIndex)
	{
		const int32 SourceIndex = TargetIndex < static_cast<int32>(Remap.TargetToSourceBone.size()) ? Remap.TargetToSourceBone[TargetIndex] : -1;

		FBone Bone;
		if (SourceIndex >= 0 && SourceIndex < static_cast<int32>(Mesh.Bones.size()))
		{
			Bone = Mesh.Bones[SourceIndex];
		}
		else
		{
			const FReferenceBone& RefBone = TargetRef.Bones[TargetIndex];
			Bone.LocalMatrix              = RefBone.LocalBindPose;
			Bone.GlobalMatrix             = RefBone.GlobalBindPose;
			Bone.InverseBindPoseMatrix    = RefBone.InverseBindPose;
			Bone.SyncSeparatedPoseDataFromLegacy();
		}

		Bone.Name                  = TargetRef.Bones[TargetIndex].Name;
		Bone.ParentIndex           = TargetRef.Bones[TargetIndex].ParentIndex;
		RemappedBones[TargetIndex] = Bone;
	}

	for (FVertexPNCTBW& Vertex : Mesh.Vertices)
	{
		for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
		{
			const int32 SourceBoneIndex = Vertex.BoneIndices[InfluenceIndex];
			const float Weight          = Vertex.BoneWeights[InfluenceIndex];

			if (Weight <= 0.0f || SourceBoneIndex < 0)
			{
				Vertex.BoneIndices[InfluenceIndex] = -1;
				Vertex.BoneWeights[InfluenceIndex] = 0.0f;
				continue;
			}

			const int32 TargetBoneIndex = Remap.GetTargetBoneIndex(SourceBoneIndex);
			if (TargetBoneIndex < 0)
			{
				if (OutReport)
				{
					OutReport->Result = ESkeletonCompatibilityResult::Incompatible;
					OutReport->Reason = "vertex references an unmapped source bone";
					if (SourceBoneIndex >= 0 && SourceBoneIndex < SourceSkeleton.GetNumBones())
					{
						OutReport->MissingBones.push_back(SourceSkeleton.Bones[SourceBoneIndex].Name);
					}
				}
				return false;
			}

			Vertex.BoneIndices[InfluenceIndex] = TargetBoneIndex;
		}
	}

	Mesh.Bones = std::move(RemappedBones);

	const FSkeletonBinding Binding      = TargetSkeleton->GetSkeletonBinding();
	Mesh.SkeletonPath                   = Binding.SkeletonPath;
	Mesh.SkeletonAssetGuid              = Binding.SkeletonAssetGuid;
	Mesh.SkeletonCompatibilitySignature = Binding.CompatibilitySignature;
	return true;
}

bool FMeshManager::ReadSkeletalMeshBinding(const FString& PackagePath, FSkeletonBinding& OutBinding)
{
	OutBinding.Reset();

	USkeletalMesh* TempMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
	const bool bLoaded = LoadSkeletalMeshBinary(TempMesh, FPaths::MakeProjectRelative(PackagePath));
	if (bLoaded)
	{
		OutBinding = TempMesh->GetSkeletonBinding();
	}
	UObjectManager::Get().DestroyObject(TempMesh);
	return bLoaded;
}

static bool SaveSkeletalMeshBinary(USkeletalMesh* SkeletalMesh, const FString& BinaryPath, const FString& SourcePath)
{
	FWindowsBinWriter Writer(BinaryPath);
	if (!Writer.IsValid())
	{
		UE_LOG("SkeletalMesh binary save failed: could not open file. Path=%s", BinaryPath.c_str());
		return false;
	}

	try
	{
		FAssetPackageHeader Header;
		Header.Type = static_cast<uint32>(EAssetPackageType::SkeletalMesh);
		Writer << Header;

		FAssetImportMetadata Metadata = MakeImportMetadata(SourcePath);
		Writer << Metadata;

		SkeletalMesh->Serialize(Writer);
	}
	catch (const std::exception&)
	{
		UE_LOG("SkeletalMesh binary save failed: serialization threw an exception. Path=%s", BinaryPath.c_str());
		return false;
	}

	return Writer.IsValid();
}

FString FMeshManager::GetStaticMeshBinaryFilePath(const FString& SourcePath)
{
	return GetMeshPackageFilePath(SourcePath, EAssetPackageType::StaticMesh);
}

FString FMeshManager::GetSkeletalMeshBinaryFilePath(const FString& SourcePath)
{
	return GetMeshPackageFilePath(SourcePath, EAssetPackageType::SkeletalMesh);
}

bool FMeshManager::IsAssetPackagePath(const FString& Path)
{
	return FAssetPackage::IsAssetPackagePath(Path);
}

bool FMeshManager::ReimportStaticMesh(const FString& BinaryPath, ID3D11Device* Device, UStaticMesh*& OutStaticMesh)
{
	OutStaticMesh = nullptr;

	FAssetImportMetadata Metadata;
	if (!FAssetPackage::ReadMetadata(BinaryPath, EAssetPackageType::StaticMesh, Metadata)) return false;

	uint64 CurrentTimestamp = 0;
	uint64 CurrentFileSize = 0;
	if (!TryGetSourceFileState(Metadata.SourcePath, CurrentTimestamp, CurrentFileSize))
	{
		UE_LOG("StaticMesh reimport failed: source file is missing. Package=%s, Source=%s", BinaryPath.c_str(), Metadata.SourcePath.c_str());
		return false;
	}

	std::unique_ptr<FStaticMesh> NewMeshAsset = std::make_unique<FStaticMesh>();
	TArray<FStaticMaterial> ParsedMaterials;
	if (!ImportStaticMeshByExtension(Metadata.SourcePath, nullptr, *NewMeshAsset, ParsedMaterials))
	{
		return false;
	}

	const FString PackagePath = NormalizeProjectPath(BinaryPath);
	StaticMeshCache.erase(PackagePath);

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	NewMeshAsset->PathFileName = Metadata.SourcePath;
	StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
	StaticMesh->SetStaticMeshAsset(NewMeshAsset.release());

	if (!SaveStaticMeshBinary(StaticMesh, PackagePath, Metadata.SourcePath)) return false;

	StaticMesh->InitResources(Device);
	StaticMesh->SetAssetPathFileName(PackagePath);
	StaticMeshCache[PackagePath] = StaticMesh;
	OutStaticMesh = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return true;
}

bool FMeshManager::ReimportSkeletalMesh(const FString& BinaryPath, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh)
{
	OutSkeletalMesh = nullptr;

	FAssetImportMetadata Metadata;
	if (!FAssetPackage::ReadMetadata(BinaryPath, EAssetPackageType::SkeletalMesh, Metadata)) return false;

	uint64 CurrentTimestamp = 0;
	uint64 CurrentFileSize = 0;
	if (!TryGetSourceFileState(Metadata.SourcePath, CurrentTimestamp, CurrentFileSize))
	{
		UE_LOG("SkeletalMesh reimport failed: source file is missing. Package=%s, Source=%s", BinaryPath.c_str(), Metadata.SourcePath.c_str());
		return false;
	}

	const FString    PackagePath = NormalizeProjectPath(BinaryPath);
	FSkeletonBinding ExistingBinding;
	ReadSkeletalMeshBinding(PackagePath, ExistingBinding);

	const FString DefaultSkeletonPath  = FSkeletonManager::GetSkeletonPackagePath(Metadata.SourcePath);
	const bool    bUseExistingSkeleton = ExistingBinding.HasSkeletonPath() && ExistingBinding.SkeletonPath != DefaultSkeletonPath;

	if (bUseExistingSkeleton)
	{
		FSkeletalMeshImportRequest Request;
		Request.SourceFbxPath            = Metadata.SourcePath;
		Request.TargetSkeletonPath       = ExistingBinding.SkeletonPath;
		Request.DestinationPackagePath   = PackagePath;
		Request.bOverwriteExistingAssets = true;
		return ImportSkeletalMesh(Request, Device, OutSkeletalMesh);
	}

	return ImportSkeletalMeshAsNew(Metadata.SourcePath, Device, OutSkeletalMesh);
}

bool FMeshManager::IsStaticMeshPackage(const FString& Path)
{
	FAssetImportMetadata Metadata;
	return FAssetPackage::ReadMetadata(Path, EAssetPackageType::StaticMesh, Metadata);
}

bool FMeshManager::IsSkeletalMeshPackage(const FString& Path)
{
	FAssetImportMetadata Metadata;
	return FAssetPackage::ReadMetadata(Path, EAssetPackageType::SkeletalMesh, Metadata);
}

void FMeshManager::ScanMeshAssets()
{
	AvailableStaticMeshFiles.clear();
	AvailableSkeletalMeshFiles.clear();

	const std::filesystem::path MeshCacheRoot = FPaths::RootDir() + L"Content\\";
	if (!std::filesystem::exists(MeshCacheRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(MeshCacheRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		std::wstring Ext = Path.extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

		if (Ext != MeshBinary::StaticMeshBinaryExtension) continue;	

		// MeshCache 목록은 새 확장자만 보여준다.
		// Static은 .statbin, Skeletal은 .sketbin으로 분리해서 수집한다.
		TArray<FAssetListItem>* TargetList = nullptr;

		FString RelPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());

		FAssetImportMetadata Metadata;
		if (FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::StaticMesh, Metadata))
		{
			TargetList = &AvailableStaticMeshFiles;
		}
		else if (FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::SkeletalMesh, Metadata))
		{
			TargetList = &AvailableSkeletalMeshFiles;
		}
		else
		{
			continue;
		}

		FAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = RelPath;
		TargetList->push_back(std::move(Item));
	}
}

void FMeshManager::ScanMeshSourceFiles()
{
	AvailableStaticMeshSourceFiles.clear();

	const std::filesystem::path DataRoot = FPaths::RootDir() + L"Content/Data/";

	if (!std::filesystem::exists(DataRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		std::wstring Ext = Path.extension().wstring();

		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".obj" && Ext != L".fbx") continue;

		FAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableStaticMeshSourceFiles.push_back(std::move(Item));
	}
}

UStaticMesh* FMeshManager::LoadStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice)
{
	const bool bInputIsPackage = IsAssetPackagePath(PathFileName);
	if (bInputIsPackage)
	{
		UE_LOG("StaticMesh load failed: StaticMesh binary cannot be loaded as StaticMesh. Path=%s", PathFileName.c_str());
		return nullptr;
	}
	if (!IsSupportedStaticMeshSourcePath(PathFileName))
	{
		UE_LOG("StaticMesh import failed: option import only supports source .obj/.fbx paths. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	const FString CacheKey = GetStaticMeshBinaryFilePath(PathFileName);

	// import 옵션이 바뀌면 같은 원본도 다른 Mesh가 될 수 있다.
	// 그래서 기존 캐시를 지우고 새 .statbin을 만든다.
	StaticMeshCache.erase(CacheKey);

	std::unique_ptr<FStaticMesh> NewMeshAsset = std::make_unique<FStaticMesh>();
	TArray<FStaticMaterial> ParsedMaterials;
	if (!ImportStaticMeshByExtension(PathFileName, &Options, *NewMeshAsset, ParsedMaterials))
	{
		UE_LOG("StaticMesh import failed: empty mesh will not be added to cache. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	NewMeshAsset->PathFileName = NormalizeProjectPath(PathFileName);
	StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
	StaticMesh->SetStaticMeshAsset(NewMeshAsset.release());

	// import가 끝난 StaticMesh는 .statbin으로 저장한다.
	// 다음 로드부터는 무거운 원본 파싱을 건너뛸 수 있다.
	SaveStaticMeshBinary(StaticMesh, CacheKey, PathFileName);

	StaticMesh->InitResources(InDevice);
	StaticMesh->SetAssetPathFileName(CacheKey);
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}

UStaticMesh* FMeshManager::LoadStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	const bool bInputIsPackage = IsAssetPackagePath(PathFileName);
	if (!bInputIsPackage && !IsSupportedStaticMeshSourcePath(PathFileName))
	{
		UE_LOG("StaticMesh load failed: unsupported path. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	const FString CacheKey = GetStaticMeshBinaryFilePath(PathFileName);
	
	auto It = StaticMeshCache.find(CacheKey);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	const std::filesystem::path BinaryPath(FPaths::ToWide(CacheKey));
	if (std::filesystem::exists(BinaryPath))
	{
		UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
		if (LoadStaticMeshBinary(StaticMesh, CacheKey))
		{
			bool bMissingSource = false;
			if (IsPackageSourceStale(CacheKey, EAssetPackageType::StaticMesh, bMissingSource))
			{
				UE_LOG("StaticMesh package is stale. Package=%s MissingSource=%s", CacheKey.c_str(), bMissingSource ? "true" : "false");
			}

			StaticMesh->InitResources(InDevice);
			StaticMesh->SetAssetPathFileName(CacheKey);
			StaticMeshCache[CacheKey] = StaticMesh;
			return StaticMesh;
		}

		if (bInputIsPackage)
		{
			// Binary 경로만 받으면 원본 위치를 확실히 알 수 없다.
			// 이 경우에는 새 import를 시도하지 않고 실패로 끝낸다.
			return nullptr;
		}

		UE_LOG("StaticMesh binary load failed: source path is available, reimporting. Source=%s Binary=%s", PathFileName.c_str(), CacheKey.c_str());
	}
	else if (bInputIsPackage)
	{
		UE_LOG("StaticMesh load failed: StaticMesh binary file does not exist. Path=%s", CacheKey.c_str());
		return nullptr;
	}

	std::unique_ptr<FStaticMesh> NewMeshAsset = std::make_unique<FStaticMesh>();
	TArray<FStaticMaterial> ParsedMaterials;
	if (!ImportStaticMeshByExtension(PathFileName, nullptr, *NewMeshAsset, ParsedMaterials))
	{
		UE_LOG("StaticMesh import failed: empty mesh will not be added to cache. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	NewMeshAsset->PathFileName = NormalizeProjectPath(PathFileName);
	StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
	StaticMesh->SetStaticMeshAsset(NewMeshAsset.release());

	// .statbin이 없을 때만 원본 파일을 import한다.
	// import가 성공하면 바로 캐시 파일을 만들어 둔다.
	SaveStaticMeshBinary(StaticMesh, CacheKey, PathFileName);

	StaticMesh->InitResources(InDevice);
	StaticMesh->SetAssetPathFileName(CacheKey);
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}

void FMeshManager::ScanFbxSourceFiles()
{
	AvailableFbxSourceFiles.clear();

	const std::filesystem::path DataRoot = FPaths::RootDir() + L"Content/Data/";

	if (!std::filesystem::exists(DataRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		std::wstring Ext = Path.extension().wstring();

		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".fbx") continue;

		FAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableFbxSourceFiles.push_back(std::move(Item));
	}
}

void FMeshManager::ReleaseAllGPU()
{
	// Static Mesh
	for (auto& [Key, Mesh] : StaticMeshCache)
	{
		if (Mesh)
		{
			FStaticMesh* Asset = Mesh->GetStaticMeshAsset();
			if (Asset && Asset->RenderBuffer)
			{
				Asset->RenderBuffer->Release();
				Asset->RenderBuffer.reset();
			}
			// LOD 버퍼도 해제
			for (uint32 LOD = 1; LOD < UStaticMesh::MAX_LOD_COUNT; ++LOD)
			{
				FMeshBuffer* LODBuffer = Mesh->GetLODMeshBuffer(LOD);
				if (LODBuffer)
				{
					LODBuffer->Release();
				}
			}
		}
	}
	StaticMeshCache.clear();

	// Skeletal Mesh
	for (auto& [Key, Mesh] : SkeletalMeshCache)
	{
		if (Mesh)
		{
			FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
			if (Asset && Asset->RenderBuffer)
			{
				Asset->RenderBuffer->Release();
				Asset->RenderBuffer.reset();
			}
		}
	}
	SkeletalMeshCache.clear();
}

static bool SaveImportedSkeletonAsset(
	const FString&       SourceFbxPath,
	FReferenceSkeleton&& ImportedReferenceSkeleton,
	USkeleton*&          OutSkeleton
	)
{
	OutSkeleton = nullptr;

	if (ImportedReferenceSkeleton.GetNumBones() <= 0)
	{
		UE_LOG("Skeleton import failed: imported reference skeleton has no bones. Source=%s", SourceFbxPath.c_str());
		return false;
	}

	const FString SkeletonPath           = FSkeletonManager::GetSkeletonPackagePath(SourceFbxPath);
	const FString CompatibilitySignature = FSkeletonManager::BuildCompatibilitySignature(ImportedReferenceSkeleton);

	USkeleton* ExistingSkeleton = nullptr;
	if (std::filesystem::exists(ResolveProjectPath(SkeletonPath)))
	{
		ExistingSkeleton = FSkeletonManager::Get().LoadSkeleton(SkeletonPath);
	}

	USkeleton* Skeleton = UObjectManager::Get().CreateObject<USkeleton>();
	Skeleton->SetAssetPathFileName(SkeletonPath);
	Skeleton->GetMutableReferenceSkeleton() = std::move(ImportedReferenceSkeleton);
	Skeleton->SetCompatibilitySignature(CompatibilitySignature);

	if (ExistingSkeleton)
	{
		FSkeletonCompatibilityReport ExistingReport;
		const bool                   bSameStructure = FSkeletonManager::AreSkeletonsSameStructure(
			ExistingSkeleton->GetReferenceSkeleton(),
			Skeleton->GetReferenceSkeleton(),
			&ExistingReport
		);

		if (bSameStructure)
		{
			Skeleton->SetSkeletonAssetGuid(ExistingSkeleton->GetSkeletonAssetGuid());
		}
		else
		{
			UE_LOG(
				"Skeleton import: skeleton structure changed, issuing new SkeletonAssetGuid. Path=%s Reason=%s",
				SkeletonPath.c_str(),
				ExistingReport.Reason.c_str()
			);
			Skeleton->SetSkeletonAssetGuid(
				FSkeletonManager::MakeSkeletonAssetGuid(SkeletonPath + "#changed", CompatibilitySignature)
			);
		}
	}
	else
	{
		Skeleton->SetSkeletonAssetGuid(FSkeletonManager::MakeSkeletonAssetGuid(SkeletonPath, CompatibilitySignature));
	}

	Skeleton->RebuildBoneNameCache();

	if (!FSkeletonManager::Get().SaveSkeleton(Skeleton, SkeletonPath, SourceFbxPath))
	{
		UE_LOG("Skeleton import failed: skeleton save failed. Path=%s", SourceFbxPath.c_str());
		return false;
	}

	FSkeletonManager::Get().ScanSkeletonAssets();
	OutSkeleton = Skeleton;
	return true;
}

USkeletalMesh* FMeshManager::LoadSkeletalMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	const std::wstring Ext = GetLowerExtension(PathFileName);

	const bool bInputIsPackage = IsAssetPackagePath(PathFileName);
	if (!bInputIsPackage && !IsSupportedSkeletalMeshSourcePath(PathFileName))
	{
		UE_LOG("SkeletalMesh load failed: unsupported path. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	const FString CacheKey = GetSkeletalMeshBinaryFilePath(PathFileName);
	auto It = SkeletalMeshCache.find(CacheKey);
	if (It != SkeletalMeshCache.end())
	{
		return It->second;
	}

	const std::filesystem::path BinaryPath(FPaths::ToWide(CacheKey));
	if (std::filesystem::exists(BinaryPath))
	{
		USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
		if (LoadSkeletalMeshBinary(SkeletalMesh, CacheKey))
		{
			bool bMissingSource = false;
			if (IsPackageSourceStale(CacheKey, EAssetPackageType::SkeletalMesh, bMissingSource))
			{
				UE_LOG("SkeletalMesh package is stale. Package=%s MissingSource=%s", CacheKey.c_str(), bMissingSource ? "true" : "false");
			}

			const FSkeletonBinding& Binding = SkeletalMesh->GetSkeletonBinding();
			if (!Binding.SkeletonPath.empty() && Binding.SkeletonPath != "None")
			{
				USkeleton* Skeleton = FSkeletonManager::Get().LoadSkeleton(Binding.SkeletonPath);
				SkeletalMesh->SetSkeleton(Skeleton);
			}

			SkeletalMesh->InitResources(InDevice);
			SkeletalMesh->SetAssetPathFileName(CacheKey);
			SkeletalMeshCache[CacheKey] = SkeletalMesh;
			return SkeletalMesh;
		}

		if (bInputIsPackage)
		{
			return nullptr;
		}

		UE_LOG("SkeletalMesh binary load failed: source path is available, reimporting. Source=%s Binary=%s", PathFileName.c_str(), CacheKey.c_str());
	}
	else if (bInputIsPackage)
	{
		UE_LOG("SkeletalMesh load failed: SkeletalMesh binary file does not exist. Path=%s", CacheKey.c_str());
		return nullptr;
	}

	USkeletalMesh* ImportedSkeletalMesh = nullptr;
	if (!ImportSkeletalMeshAsNew(PathFileName, InDevice, ImportedSkeletalMesh))
	{
		UE_LOG("SkeletalMesh import failed: empty mesh will not be added to cache. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	return ImportedSkeletalMesh;
}

bool FMeshManager::ImportSkeletalMeshAsNew(const FString& SourceFbxPath, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh)
{
	OutSkeletalMesh = nullptr;

	if (!IsSupportedSkeletalMeshSourcePath(SourceFbxPath))
	{
		UE_LOG("SkeletalMesh import failed: only source FBX paths can be imported. Path=%s", SourceFbxPath.c_str());
		return false;
	}

	FFbxSkeletalMeshImportResult ImportResult;
	if (!FFbxImporter::ImportSkeletalMesh(SourceFbxPath, ImportResult))
	{
		return false;
	}

	USkeleton* Skeleton = nullptr;
	if (!SaveImportedSkeletonAsset(SourceFbxPath, std::move(ImportResult.Skeleton), Skeleton))
	{
		UE_LOG("SkeletalMesh import failed: skeleton save failed. Path=%s", SourceFbxPath.c_str());
		return false;
	}

	const FString          SkeletonPath    = Skeleton->GetAssetPathFileName();
	const FSkeletonBinding SkeletonBinding = Skeleton->GetSkeletonBinding();

	for (UAnimSequence* Sequence : ImportResult.AnimSequences)
	{
		if (!Sequence)
		{
			continue;
		}

		Sequence->SetSkeletonBinding(SkeletonBinding);

		const FString AnimPath = FAnimationManager::GetAnimationPathForSkeleton(SourceFbxPath, Sequence->GetName(), SkeletonPath);
		if (!FAnimationManager::Get().SaveAnimation(Sequence, AnimPath, SourceFbxPath))
		{
			UE_LOG("SkeletalMesh import failed: animation save failed. Source=%s Anim=%s", SourceFbxPath.c_str(), Sequence->GetName().c_str());
		}
	}

	std::unique_ptr<FSkeletalMesh> NewMesh  = std::make_unique<FSkeletalMesh>(std::move(ImportResult.Mesh));
	NewMesh->PathFileName                   = NormalizeProjectPath(SourceFbxPath);
	NewMesh->SkeletonPath                   = SkeletonBinding.SkeletonPath;
	NewMesh->SkeletonAssetGuid              = SkeletonBinding.SkeletonAssetGuid;
	NewMesh->SkeletonCompatibilitySignature = SkeletonBinding.CompatibilitySignature;

	const FString PackagePath = GetSkeletalMeshBinaryFilePath(SourceFbxPath);
	SkeletalMeshCache.erase(PackagePath);

	USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
	SkeletalMesh->SetSkeletalMaterials(std::move(ImportResult.Materials));
	SkeletalMesh->SetSkeletalMeshAsset(NewMesh.release());
	SkeletalMesh->SetSkeleton(Skeleton);

	if (!SaveSkeletalMeshBinary(SkeletalMesh, PackagePath, SourceFbxPath))
	{
		return false;
	}

	SkeletalMesh->InitResources(Device);
	SkeletalMesh->SetAssetPathFileName(PackagePath);
	SkeletalMeshCache[PackagePath] = SkeletalMesh;
	OutSkeletalMesh                = SkeletalMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();
	return true;
}

bool FMeshManager::ImportSkeletonAsNew(const FString& SourceFbxPath, USkeleton*& OutSkeleton)
{
	OutSkeleton = nullptr;

	if (!IsSupportedSkeletalMeshSourcePath(SourceFbxPath))
	{
		UE_LOG("Skeleton import failed: only source FBX paths can be imported. Path=%s", SourceFbxPath.c_str());
		return false;
	}

	FFbxSkeletonImportResult ImportResult;
	if (!FFbxImporter::ImportSkeletonOnly(SourceFbxPath, ImportResult))
	{
		return false;
	}

	return SaveImportedSkeletonAsset(SourceFbxPath, std::move(ImportResult.SourceSkeleton), OutSkeleton);
}

bool FMeshManager::ImportSkeletalMesh(const FSkeletalMeshImportRequest& Request, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh)
{
	OutSkeletalMesh = nullptr;

	if (!IsSupportedSkeletalMeshSourcePath(Request.SourceFbxPath))
	{
		UE_LOG("SkeletalMesh import failed: only source FBX paths can be imported. Path=%s", Request.SourceFbxPath.c_str());
		return false;
	}

	if (Request.TargetSkeletonPath.empty() || Request.TargetSkeletonPath == "None")
	{
		UE_LOG("SkeletalMesh import failed: target skeleton is required. Source=%s", Request.SourceFbxPath.c_str());
		return false;
	}

	USkeleton* TargetSkeleton = FSkeletonManager::Get().LoadSkeleton(Request.TargetSkeletonPath);
	if (!TargetSkeleton)
	{
		UE_LOG("SkeletalMesh import failed: target skeleton not found. Path=%s", Request.TargetSkeletonPath.c_str());
		return false;
	}

	FFbxSkeletalMeshOnlyImportResult ImportResult;
	if (!FFbxImporter::ImportSkeletalMeshOnly(Request.SourceFbxPath, ImportResult))
	{
		return false;
	}

	FSkeletonBoneRemap           Remap;
	FSkeletonCompatibilityReport Report;
	if (!FSkeletonManager::BuildBoneRemapByName(
		ImportResult.SourceSkeleton,
		TargetSkeleton->GetReferenceSkeleton(),
		Remap,
		&Report,
		!Request.bAllowTargetExtraBones
	))
	{
		UE_LOG(
			"SkeletalMesh import failed: skeleton remap failed. Source=%s Target=%s Reason=%s",
			Request.SourceFbxPath.c_str(),
			Request.TargetSkeletonPath.c_str(),
			Report.Reason.c_str()
		);
		return false;
	}

	if (!RemapSkeletalMeshToSkeleton(ImportResult.Mesh, ImportResult.SourceSkeleton, TargetSkeleton, Remap, &Report))
	{
		UE_LOG(
			"SkeletalMesh import failed: mesh remap failed. Source=%s Target=%s Reason=%s",
			Request.SourceFbxPath.c_str(),
			Request.TargetSkeletonPath.c_str(),
			Report.Reason.c_str()
		);
		return false;
	}

	const FString PackagePath = Request.DestinationPackagePath.empty() ? GetSkeletalMeshBinaryFilePath(Request.SourceFbxPath)
	: NormalizeProjectPath(Request.DestinationPackagePath);

	if (!Request.bOverwriteExistingAssets && std::filesystem::exists(ResolveProjectPath(PackagePath)))
	{
		UE_LOG("SkeletalMesh import skipped: destination exists. Path=%s", PackagePath.c_str());
		return false;
	}

	ImportResult.Mesh.PathFileName = NormalizeProjectPath(Request.SourceFbxPath);

	SkeletalMeshCache.erase(PackagePath);
	USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
	SkeletalMesh->SetSkeletalMaterials(std::move(ImportResult.Materials));
	SkeletalMesh->SetSkeletalMeshAsset(new FSkeletalMesh(std::move(ImportResult.Mesh)));
	SkeletalMesh->SetSkeleton(TargetSkeleton);

	if (!SaveSkeletalMeshBinary(SkeletalMesh, PackagePath, Request.SourceFbxPath))
	{
		return false;
	}

	SkeletalMesh->InitResources(Device);
	SkeletalMesh->SetAssetPathFileName(PackagePath);
	SkeletalMeshCache[PackagePath] = SkeletalMesh;
	OutSkeletalMesh                = SkeletalMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();
	return true;
}

bool FMeshManager::ImportFbxScene(
	const FFbxSceneImportRequest& Request,
	ID3D11Device*                 Device,
	FFbxSceneImportResult&        OutResult
	)
{
	OutResult = FFbxSceneImportResult();

	if (!IsSupportedSkeletalMeshSourcePath(Request.SourceFbxPath))
	{
		UE_LOG("FBX import failed: only source FBX paths can be imported. Path=%s", Request.SourceFbxPath.c_str());
		return false;
	}

	if (!Request.bImportSkeleton && !Request.bImportSkin && !Request.bImportAnimations)
	{
		UE_LOG("FBX import failed: no import part was selected. Source=%s", Request.SourceFbxPath.c_str());
		return false;
	}

	FFbxSkeletalSceneImportOptions ImportOptions;
	ImportOptions.bImportSkeleton                       = Request.bImportSkeleton;
	ImportOptions.bImportSkin                           = Request.bImportSkin;
	ImportOptions.bImportAnimations                     = Request.bImportAnimations;
	ImportOptions.AnimationOptions.SelectedStackIndices = Request.SelectedAnimationStackIndices;

	FFbxSkeletalSceneImportResult ImportResult;
	FString                       ImportMessage;
	if (!FFbxImporter::ImportSkeletalScene(Request.SourceFbxPath, ImportOptions, ImportResult, &ImportMessage))
	{
		UE_LOG(
			"FBX import failed: skeletal scene import failed. Source=%s Reason=%s",
			Request.SourceFbxPath.c_str(),
			ImportMessage.c_str()
		);
		return false;
	}

	FReferenceSkeleton SourceSkeleton = ImportResult.SourceSkeleton;
	if (SourceSkeleton.GetNumBones() <= 0)
	{
		UE_LOG("FBX import failed: source skeleton is empty. Source=%s", Request.SourceFbxPath.c_str());
		return false;
	}

	FString    EffectiveSkeletonPath = Request.TargetSkeletonPath;
	USkeleton* EffectiveSkeleton     = nullptr;

	if (Request.bImportSkeleton)
	{
		FReferenceSkeleton SkeletonToSave   = SourceSkeleton;
		USkeleton*         ImportedSkeleton = nullptr;
		if (!SaveImportedSkeletonAsset(Request.SourceFbxPath, std::move(SkeletonToSave), ImportedSkeleton))
		{
			UE_LOG("FBX import failed: skeleton save failed. Source=%s", Request.SourceFbxPath.c_str());
			return false;
		}

		OutResult.Skeleton = ImportedSkeleton;
		EffectiveSkeleton  = ImportedSkeleton;
		if (ImportedSkeleton)
		{
			EffectiveSkeletonPath = ImportedSkeleton->GetAssetPathFileName();
		}
	}

	const bool bNeedsTargetSkeleton = Request.bImportSkin || Request.bImportAnimations;
	if (bNeedsTargetSkeleton && (EffectiveSkeletonPath.empty() || EffectiveSkeletonPath == "None"))
	{
		UE_LOG(
			"FBX import failed: skin or animation import requires an imported or target skeleton. Source=%s",
			Request.SourceFbxPath.c_str()
		);
		return false;
	}

	if (bNeedsTargetSkeleton && !EffectiveSkeleton)
	{
		EffectiveSkeleton = FSkeletonManager::Get().LoadSkeleton(EffectiveSkeletonPath);
	}

	if (bNeedsTargetSkeleton && !EffectiveSkeleton)
	{
		UE_LOG("FBX import failed: target skeleton not found. Path=%s", EffectiveSkeletonPath.c_str());
		return false;
	}

	if (Request.bImportSkin)
	{
		FSkeletonBoneRemap           Remap;
		FSkeletonCompatibilityReport Report;
		if (!FSkeletonManager::BuildBoneRemapByName(
			SourceSkeleton,
			EffectiveSkeleton->GetReferenceSkeleton(),
			Remap,
			&Report,
			!Request.bAllowTargetExtraBones
		))
		{
			UE_LOG(
				"FBX import failed: skeletal mesh remap failed. Source=%s Target=%s Reason=%s",
				Request.SourceFbxPath.c_str(),
				EffectiveSkeletonPath.c_str(),
				Report.Reason.c_str()
			);
			return false;
		}

		if (!RemapSkeletalMeshToSkeleton(ImportResult.Mesh, SourceSkeleton, EffectiveSkeleton, Remap, &Report))
		{
			UE_LOG(
				"FBX import failed: skeletal mesh remap failed. Source=%s Target=%s Reason=%s",
				Request.SourceFbxPath.c_str(),
				EffectiveSkeletonPath.c_str(),
				Report.Reason.c_str()
			);
			return false;
		}

		const FString PackagePath = Request.DestinationPackagePath.empty()
		? GetSkeletalMeshBinaryFilePath(Request.SourceFbxPath) : NormalizeProjectPath(Request.DestinationPackagePath);

		if (!Request.bOverwriteExistingAssets && std::filesystem::exists(ResolveProjectPath(PackagePath)))
		{
			UE_LOG("FBX skeletal mesh import skipped: destination exists. Path=%s", PackagePath.c_str());
		}
		else
		{
			ImportResult.Mesh.PathFileName = NormalizeProjectPath(Request.SourceFbxPath);

			SkeletalMeshCache.erase(PackagePath);
			USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
			SkeletalMesh->SetSkeletalMaterials(std::move(ImportResult.Materials));
			SkeletalMesh->SetSkeletalMeshAsset(new FSkeletalMesh(std::move(ImportResult.Mesh)));
			SkeletalMesh->SetSkeleton(EffectiveSkeleton);

			if (!SaveSkeletalMeshBinary(SkeletalMesh, PackagePath, Request.SourceFbxPath))
			{
				return false;
			}

			SkeletalMesh->InitResources(Device);
			SkeletalMesh->SetAssetPathFileName(PackagePath);
			SkeletalMeshCache[PackagePath] = SkeletalMesh;
			OutResult.SkeletalMesh         = SkeletalMesh;
		}
	}

	if (Request.bImportAnimations)
	{
		if (!FAnimationManager::Get().SaveImportedAnimationsForSkeleton(
			Request.SourceFbxPath,
			SourceSkeleton,
			EffectiveSkeletonPath,
			Request.DestinationAnimationDirectory,
			Request.bAllowTargetExtraBones,
			Request.bOverwriteExistingAssets,
			ImportResult.AnimSequences,
			&OutResult.AnimSequences
		))
		{
			UE_LOG(
				"FBX import failed: animation save failed. Source=%s Target=%s",
				Request.SourceFbxPath.c_str(),
				EffectiveSkeletonPath.c_str()
			);
			return false;
		}
	}

	if (Request.bImportSkin)
	{
		ScanMeshAssets();
		FMaterialManager::Get().ScanMaterialAssets();
	}
	if (Request.bImportSkeleton)
	{
		FSkeletonManager::Get().ScanSkeletonAssets();
	}
	if (Request.bImportAnimations)
	{
		FAnimationManager::Get().RefreshAvailableAnimations();
	}

	return true;
}
