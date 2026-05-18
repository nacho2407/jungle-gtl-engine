#include "Core/ResourceManager.h"

#include "Core/Paths.h"
#include "Core/AssetPathPolicy.h"
#include "Core/ImportedMaterialPolicy.h"
#include "Core/MaterialLoadService.h"
#include "Core/MaterialSerializationService.h"
#include "Core/ResourceMemoryReporter.h"
#include "Core/SkeletalMeshLoadService.h"
#include "Core/StaticMeshLoadService.h"
#include "Object/Object.h"

#include <algorithm>
#include <filesystem>
#include <chrono>
#include <cwctype>
#include <cstdio>
#include <fstream>
#include "Asset/FileUtils.h"
#include "Animation/AnimSequence.h"

#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "Core/Logging/Log.h"

#if WITH_EDITOR
#include "Settings/EditorSettings.h"
#endif

#include "Asset/BinarySerializer.h"
#include "Asset/StaticMeshTypes.h"
#include "Asset/StaticMeshSimplifier.h"
#include "Render/Scene/RenderCommand.h"

namespace
{
	bool ShouldBuildStaticMeshLODs()
	{
#if WITH_EDITOR
		return FEditorSettings::Get().ShowFlags.bEnableLOD;
#else
		return true;
#endif
	}

	bool IsFbxSourcePath(const FString& Path)
	{
		std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
		std::wstring Extension = FsPath.extension().wstring();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
		return Extension == L".fbx";
	}

	bool HasAnimSequenceTrackKeys(const UAnimSequence* Sequence)
	{
		const UAnimDataModel* DataModel = Sequence ? Sequence->GetDataModel() : nullptr;
		if (!DataModel)
		{
			return false;
		}

		for (const FBoneAnimationTrack& Track : DataModel->GetBoneAnimationTracks())
		{
			if (!Track.InternalTrack.PosKeys.empty() ||
				!Track.InternalTrack.RotKeys.empty() ||
				!Track.InternalTrack.ScaleKeys.empty())
			{
				return true;
			}
		}
		return false;
	}

	UAnimSequence* ReimportAnimSequenceFromSource(
		FFbxImporter& FbxImporter,
		const FString& AnimSequenceAssetPath,
		const UAnimSequence* StaleSequence)
	{
		if (!StaleSequence)
		{
			return nullptr;
		}

		const FString SourceFilePath = FPaths::Normalize(StaleSequence->GetSourceFilePath());
		if (SourceFilePath.empty() || !IsFbxSourcePath(SourceFilePath) || !FAssetPathPolicy::FileExists(SourceFilePath))
		{
			return nullptr;
		}

		FFbxAnimImportOptions ImportOptions;
		ImportOptions.StackName = StaleSequence->GetSourceStackName();
		ImportOptions.PreviewMeshPath = StaleSequence->GetPreviewMeshPath().empty()
			? SourceFilePath
			: StaleSequence->GetPreviewMeshPath();

		UAnimSequence* ReimportedSequence = FbxImporter.LoadAnimSequence(SourceFilePath, ImportOptions);
		if (!ReimportedSequence || !HasAnimSequenceTrackKeys(ReimportedSequence))
		{
			return nullptr;
		}

		ReimportedSequence->SetAssetPath(AnimSequenceAssetPath);
		ReimportedSequence->SetSourceFilePath(SourceFilePath);
		if (ReimportedSequence->GetPreviewMeshPath().empty())
		{
			ReimportedSequence->SetPreviewMeshPath(ImportOptions.PreviewMeshPath);
		}
		return ReimportedSequence;
	}
}

#pragma region __BINARY__

namespace fs = std::filesystem;

uint64 FResourceManager::GetFileWriteTimeTicks(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	fs::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));
	std::error_code ErrorCode;
	if (!fs::exists(FilePath, ErrorCode) || ErrorCode)
	{
		return 0;
	}

	auto WriteTime = fs::last_write_time(FilePath, ErrorCode);
	if (ErrorCode)
	{
		return 0;
	}

	auto Duration = WriteTime.time_since_epoch();

	return static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::seconds>(Duration).count());
}

uint64 FResourceManager::GetFileSizeBytes(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	fs::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));

	std::error_code ErrorCode;
	const uintmax_t FileSize = fs::file_size(FilePath, ErrorCode);
	if (ErrorCode)
	{
		return 0;
	}

	return static_cast<uint64>(FileSize);
}

FString FResourceManager::ComputeFileContentHashString(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	std::ifstream In(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)), std::ios::binary);
	if (!In.is_open())
	{
		return "";
	}

	constexpr uint64 FnvOffsetBasis = 14695981039346656037ull;
	constexpr uint64 FnvPrime = 1099511628211ull;

	uint64 Hash = FnvOffsetBasis;
	char Buffer[64 * 1024];
	while (In.good())
	{
		In.read(Buffer, sizeof(Buffer));
		const std::streamsize BytesRead = In.gcount();
		for (std::streamsize Index = 0; Index < BytesRead; ++Index)
		{
			Hash ^= static_cast<unsigned char>(Buffer[Index]);
			Hash *= FnvPrime;
		}
	}

	char HashText[32] = {};
	std::snprintf(HashText, sizeof(HashText), "fnv1a64:%016llx", static_cast<unsigned long long>(Hash));
	return FString(HashText);
}

bool FResourceManager::IsStaticMeshBinaryValid(const FString& SourcePath, const FString& BinaryPath) const
{
	FStaticMeshBinaryHeader Header;
	const FString NormalizedBinaryPath = FPaths::Normalize(BinaryPath);
	if (!BinarySerializer.ReadStaticMeshHeader(NormalizedBinaryPath, Header))
	{
		return false;
	}

	const uint64 SourceWriteTime = GetFileWriteTimeTicks(FPaths::Normalize(SourcePath));
	if (SourceWriteTime == 0)
	{
		return false;
	}

	return Header.SourceFileWriteTime == SourceWriteTime;
}

bool FResourceManager::IsSkeletalMeshBinaryValid(const FString& SourcePath, const FString& BinaryPath) const
{
	FSkeletalMeshBinaryHeader Header;
	const FString NormalizedBinaryPath = FPaths::Normalize(BinaryPath);
	if (!BinarySerializer.ReadSkeletalMeshHeader(NormalizedBinaryPath, Header))
	{
		return false;
	}

	const uint64 SourceWriteTime = GetFileWriteTimeTicks(FPaths::Normalize(SourcePath));
	if (SourceWriteTime == 0)
	{
		return false;
	}

	return Header.SourceFileWriteTime == SourceWriteTime;
}

void FResourceManager::PreloadStaticMeshes()
{
	for (const auto& [Key, Resource] : StaticMeshCache.GetRegistry())
	{
		if (!Resource.bPreload)
		{
			continue;
		}

		if (LoadStaticMesh(Resource.Path) == nullptr)
		{
			UE_LOG_WARNING("Failed to load static mesh from Resource.ini: %s", Resource.Path.c_str());
		}
	}
}

UStaticMesh* FResourceManager::CreateStaticMeshFromLoadedData(FStaticMesh* LoadedMeshData, const FString& LogPath, bool bLogLodTiming, bool bLogLodSkipped) const
{
	UStaticMesh* LoadedMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	LoadedMesh->SetMeshData(LoadedMeshData);

	if (ShouldBuildStaticMeshLODs())
	{
		if (bLogLodTiming)
		{
			const auto LodStart = std::chrono::steady_clock::now();
			FStaticMeshSimplifier::BuildLODs(LoadedMesh);
			const auto LodEnd = std::chrono::steady_clock::now();
			double LodSec = std::chrono::duration<double>(LodEnd - LodStart).count();
			UE_LOG("[StaticMeshLoad] Generated %d LODs for %s in %.3f sec",
				   LoadedMesh->GetValidLODCount(), LogPath.c_str(), LodSec);
		}
		else
		{
			FStaticMeshSimplifier::BuildLODs(LoadedMesh);
		}
	}
	else if (bLogLodSkipped)
	{
		UE_LOG_WARNING("[StaticMeshLoad] LOD generation skipped for %s (Enable LOD is off)", LogPath.c_str());
	}

	return LoadedMesh;
}

#pragma endregion


void FResourceManager::ClearDiscoveredResourceLists(bool bClearAtlasCache)
{
	ObjFilePaths.clear();
	FontFilePaths.clear();
	TextureFilePaths.clear();
	MaterialFilePaths.clear();
	ParticleFilePaths.clear();
	CurveFilePaths.clear();
	SkeletalMeshFilePaths.clear();
	AnimSequenceFilePaths.clear();
	AnimationFbxSourceFilePaths.clear();
	StaticMeshCache.ClearRegistry();

	if (bClearAtlasCache)
	{
		AtlasCache.Clear();
	}
}

void FResourceManager::RegisterDiscoveredAssetFile(const std::filesystem::path& FilePath, const std::filesystem::path& ProjectRootPath)
{
	std::wstring Extension = FilePath.extension().wstring();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);

	if (Extension == L".meta" || Extension == L".bin")
	{
		return;
	}

	const FString RelativePath = FPaths::Normalize(FPaths::ToString(std::filesystem::relative(FilePath, ProjectRootPath)));

	if (FAssetPathPolicy::IsCurveAssetPath(FPaths::ToUtf8(FilePath.generic_wstring())))
	{
		CurveFilePaths.push_back(RelativePath);
	}
	else if (FAssetPathPolicy::IsAnimSequenceAssetPath(FPaths::ToUtf8(FilePath.generic_wstring())))
	{
		AnimSequenceFilePaths.push_back(RelativePath);
	}
	else if (Extension == L".obj" || Extension == L".fbx")
	{
		ObjFilePaths.push_back(RelativePath);

		FStaticMeshResource Resource;
		Resource.Name = RelativePath;
		Resource.Path = RelativePath;
		Resource.bPreload = false;
		Resource.bNormalizeToUnitCube = false;
		StaticMeshCache.RegisterResource(Resource);

		if (Extension == L".fbx")
		{
			const FString AbsolutePath = FPaths::Normalize(FPaths::ToUtf8(FilePath.wstring()));
			const FFbxMeshContentInfo ContentInfo = FbxImporter.InspectMeshContent(AbsolutePath);
			if (ContentInfo.bHasSkeletalMesh)
			{
				SkeletalMeshFilePaths.push_back(RelativePath);
			}
			if (ContentInfo.bHasAnimation)
			{
				if (std::find(AnimationFbxSourceFilePaths.begin(), AnimationFbxSourceFilePaths.end(), RelativePath)
					== AnimationFbxSourceFilePaths.end())
				{
					AnimationFbxSourceFilePaths.push_back(RelativePath);
				}
				if (std::find(AnimSequenceFilePaths.begin(), AnimSequenceFilePaths.end(), RelativePath) == AnimSequenceFilePaths.end())
				{
					AnimSequenceFilePaths.push_back(RelativePath);
				}
			}
		}
	}
	else if (Extension == L".mtl" || Extension == L".mat" || Extension == L".matinst")
	{
		MaterialFilePaths.push_back(RelativePath);
	}
	else if (Extension == L".png" || Extension == L".dds" || Extension == L".jpg" || Extension == L".jpeg")
	{
		const FTextureAssetMeta Meta = LoadOrCreateTextureMeta(FilePath);

		if (Meta.Type == EAssetMetaType::Font)
		{
			FontFilePaths.push_back(RelativePath);
			RegisterFont(FName(RelativePath.c_str()), RelativePath, Meta.Columns, Meta.Rows);
		}
		else if (Meta.Type == EAssetMetaType::Particle)
		{
			ParticleFilePaths.push_back(RelativePath);
			RegisterParticle(FName(RelativePath.c_str()), RelativePath, Meta.Columns, Meta.Rows);
		}
		else if (Meta.Type == EAssetMetaType::Texture)
		{
			TextureFilePaths.push_back(RelativePath);
		}
	}
}

void FResourceManager::InitializeDefaultWhiteTexture(ID3D11Device* Device)
{
	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = 1;
	Desc.Height = 1;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	constexpr uint32_t WhitePixel = 0xFFFFFFFF;
	D3D11_SUBRESOURCE_DATA InitData = {&WhitePixel, 4, 0};

	if (!TextureCache.Contains("DefaultWhite"))  {
		Device->CreateTexture2D(&Desc, &InitData, DefaultWhiteTexture.ReleaseAndGetAddressOf());
		if (DefaultWhiteTexture)
		{
			UTexture* DefaultTexture = UObjectManager::Get().CreateObject<UTexture>();
			Device->CreateShaderResourceView(DefaultWhiteTexture.Get(), nullptr, DefaultTexture->GetAddressOfSRV());
			TextureCache.Register("DefaultWhite", DefaultTexture);
		}
	}
}

void FResourceManager::InitializeDefaultMaterial(ID3D11Device* Device)
{
	UMaterial* DefaultMat = GetOrCreateMaterial("DefaultWhite", EMaterialShaderType::SurfaceLit);
	DefaultMat->MaterialParams["AmbientColor"] = FMaterialParamValue(DefaultMat->MaterialData.AmbientColor);
	DefaultMat->MaterialParams["DiffuseColor"] = FMaterialParamValue(DefaultMat->MaterialData.DiffuseColor);
	DefaultMat->MaterialParams["SpecularColor"] = FMaterialParamValue(DefaultMat->MaterialData.SpecularColor);
	DefaultMat->MaterialParams["EmissiveColor"] = FMaterialParamValue(DefaultMat->MaterialData.EmissiveColor);
	DefaultMat->MaterialParams["Shininess"] = FMaterialParamValue(DefaultMat->MaterialData.Shininess);
	DefaultMat->MaterialParams["Opacity"] = FMaterialParamValue(DefaultMat->MaterialData.Opacity);

	UTexture* DefaultWhite = GetTexture("DefaultWhite");

	if (DefaultMat->MaterialData.bHasDiffuseTexture)
		DefaultMat->MaterialParams["DiffuseMap"] = FMaterialParamValue(LoadTexture(DefaultMat->MaterialData.DiffuseTexPath, Device));
	else
		DefaultMat->MaterialParams["DiffuseMap"] = FMaterialParamValue(DefaultWhite);

	if (DefaultMat->MaterialData.bHasAmbientTexture)
		DefaultMat->MaterialParams["AmbientMap"] = FMaterialParamValue(LoadTexture(DefaultMat->MaterialData.AmbientTexPath, Device));
	else
		DefaultMat->MaterialParams["AmbientMap"] = FMaterialParamValue(DefaultWhite);

	if (DefaultMat->MaterialData.bHasSpecularTexture)
		DefaultMat->MaterialParams["SpecularMap"] = FMaterialParamValue(LoadTexture(DefaultMat->MaterialData.SpecularTexPath, Device));
	else
		DefaultMat->MaterialParams["SpecularMap"] = FMaterialParamValue(DefaultWhite);

	if (DefaultMat->MaterialData.bHasEmissiveTexture)
		DefaultMat->MaterialParams["EmissiveMap"] = FMaterialParamValue(LoadTexture(DefaultMat->MaterialData.EmissiveTexPath, Device));
	else
		DefaultMat->MaterialParams["EmissiveMap"] = FMaterialParamValue(DefaultWhite);

	if (DefaultMat->MaterialData.bHasBumpTexture)
		DefaultMat->MaterialParams["BumpMap"] = FMaterialParamValue(LoadTexture(DefaultMat->MaterialData.BumpTexPath, Device));
	else
		DefaultMat->MaterialParams["BumpMap"] = FMaterialParamValue(DefaultWhite);

	DefaultMat->MaterialParams["bHasDiffuseMap"] = FMaterialParamValue(DefaultMat->MaterialData.bHasDiffuseTexture);
	DefaultMat->MaterialParams["bHasSpecularMap"] = FMaterialParamValue(DefaultMat->MaterialData.bHasSpecularTexture);
	DefaultMat->MaterialParams["bHasAmbientMap"] = FMaterialParamValue(DefaultMat->MaterialData.bHasAmbientTexture);
	DefaultMat->MaterialParams["bHasEmissiveMap"] = FMaterialParamValue(DefaultMat->MaterialData.bHasEmissiveTexture);
	DefaultMat->MaterialParams["bHasBumpMap"] = FMaterialParamValue(DefaultMat->MaterialData.bHasBumpTexture);
	DefaultMat->MaterialParams["ScrollUV"] = FMaterialParamValue(FVector2(0.0f, 0.0f));
}

void FResourceManager::InitializeOutlineMaterial()
{
	UMaterial* OutlineMat = GetOrCreateMaterial("OutlineMaterial", EMaterialShaderType::EditorOutline);
	OutlineMat->SetParam("OutlineColor", FMaterialParamValue(FVector4(1.0f, 0.5f, 0.0f, 1.0f)));
	OutlineMat->SetParam("OutlineThicknessPixels", FMaterialParamValue(5.0f));
	OutlineMat->SetParam("OutlineViewportSize", FMaterialParamValue(FVector2(800.0f, 600.0f)));
	OutlineMat->SetParam("OutlineViewportOrigin", FMaterialParamValue(FVector2(0.0f, 0.0f)));
}

//	RootPath ??瑜곷쭊?????덈츎 嶺뚮ㅄ維獄??????띠럾???Asset??????琉우뿰 ?貫?껆뵳?????????⑤갭由????貫??
void FResourceManager::LoadFromAssetDirectory(const FString& Path)
{
	//	?貫?껆뵳??
	ClearDiscoveredResourceLists(false);

	InitializeDefaultResources(CachedDevice.Get());

	namespace fs = std::filesystem;
	
	const fs::path RootPath = fs::path(FPaths::RootDir()) / FPaths::ToWide(Path);
	
	const fs::path ProjectRootPath = fs::path(FPaths::RootDir());

	if (!fs::exists(RootPath) || !fs::is_directory(RootPath))
	{
		UE_LOG_ERROR("[ResourceManager] Fatal Error : Root Directory Error");
		return;
	}

	for (const auto& Entry : fs::recursive_directory_iterator(RootPath))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		RegisterDiscoveredAssetFile(Entry.path(), ProjectRootPath);
	}

	SyncDiscoveredFbxAnimationAssets();

	PreloadStaticMeshes();

	if (LoadGPUResources(CachedDevice.Get()))
	{
		UE_LOG("Complete Load Resources!");
	}
	else
	{
		UE_LOG_ERROR("Failed to Load Resources...");
	}
}

void FResourceManager::RefreshFromAssetDirectory(const FString& Path)
{
	namespace fs = std::filesystem;

	ClearDiscoveredResourceLists(true);

	const fs::path RootPath = fs::path(FPaths::RootDir()) / FPaths::ToWide(Path);
	const fs::path ProjectRootPath = fs::path(FPaths::RootDir());

	if (!fs::exists(RootPath) || !fs::is_directory(RootPath))
	{
		UE_LOG_ERROR("[ResourceManager] Refresh Failed : Root Directory Error");
		return;
	}

	try
	{
		for (const auto& Entry : fs::recursive_directory_iterator(RootPath, fs::directory_options::skip_permission_denied))
		{
			if (!Entry.is_regular_file())
			{
				continue;
			}

			RegisterDiscoveredAssetFile(Entry.path(), ProjectRootPath);
		}
	}
	catch (const std::exception& Ex)
	{
		UE_LOG_ERROR("[ResourceManager] Refresh Exception: %s", Ex.what());
	}

	if (CachedDevice && !LoadGPUResources(CachedDevice.Get()))
	{
		UE_LOG_ERROR("[ResourceManager] Refresh Failed : GPU Resource Reload Error");
	}

	UE_LOG("[ResourceManager] Asset Refresh Complete");
}

void FResourceManager::DeleteAllCacheFiles()
{
	namespace fs = std::filesystem;

	const fs::path BinRootPath = fs::path(FPaths::RootDir()) / "Asset" / "Mesh" / "Bin";

	if (!fs::exists(BinRootPath) || !fs::is_directory(BinRootPath))
	{
		return;
	}

	for (const auto& Entry : fs::recursive_directory_iterator(BinRootPath))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		const fs::path& FilePath = Entry.path();
		if (FilePath.extension() == L".bin")
		{
			std::error_code Ec;
			fs::remove(FilePath, Ec);
		}
	}

	// ????븐뼚???ル벣遊??筌먲퐘遊?
	for (auto It = fs::recursive_directory_iterator(BinRootPath);
		 It != fs::recursive_directory_iterator();
		 ++It)
	{
		std::error_code Ec;
		if (It->is_directory(Ec) && fs::is_empty(It->path(), Ec))
		{
			fs::remove(It->path(), Ec);
		}
	}

	UE_LOG("[ResourceManager] All mesh cache files removed");
}

FTextureAssetMeta FResourceManager::LoadOrCreateTextureMeta(const std::filesystem::path& FilePath) const
{
	return FTextureAssetMetaService::LoadOrCreate(FilePath);
}

bool FResourceManager::LoadGPUResources(ID3D11Device* Device)
{
	return AtlasCache.LoadGPUResources(Device);
}

void FResourceManager::InitializeDefaultResources(ID3D11Device* Device)
{
	if (!Device) return;

	InitializeDefaultWhiteTexture(Device);
	InitializeDefaultMaterial(Device);
	InitializeOutlineMaterial();
}

void FResourceManager::ReleaseGPUResources()
{
	TextureCache.Release();

	MaterialCache.Release();

	ShaderCache.Release();

	AtlasCache.Release();

	StaticMeshCache.Release();

	CurveCache.Release();

	RenderStateCache.Release();

	for (auto& [Path, Mesh] : SkeletalMeshMap)
	{
		UObjectManager::Get().DestroyObject(Mesh);
	}
	SkeletalMeshMap.clear();

	DefaultWhiteTexture.Reset();
	CachedDevice.Reset();
}

FVertexShader* FResourceManager::GetOrCreateVertexShader(
	const FShaderStageKey& Key,
	const D3D_SHADER_MACRO* Defines,
	const FVertexLayoutDesc* VertexLayout)
{
	return ShaderCache.GetOrCreateVertexShader(Key, Defines, CachedDevice.Get(), VertexLayout);
}

FPixelShader* FResourceManager::GetOrCreatePixelShader(const FShaderStageKey& Key, const D3D_SHADER_MACRO* Defines)
{
	return ShaderCache.GetOrCreatePixelShader(Key, Defines, CachedDevice.Get());
}

FShaderProgram* FResourceManager::GetOrCreateShaderProgram(
	const FShaderStageKey& VSKey,
	const FShaderStageKey& PSKey,
	const D3D_SHADER_MACRO* VSDefines,
	const D3D_SHADER_MACRO* PSDefines,
	const FVertexLayoutDesc* VertexLayout)
{
	return ShaderCache.GetOrCreateProgram(VSKey, PSKey, VSDefines, PSDefines, CachedDevice.Get(), VertexLayout);
}

bool FResourceManager::LoadComputeShader(const FString& FilePath, const FString& EntryPoint,
										 const D3D_SHADER_MACRO* Defines, const FString& Key)
{
	return ShaderCache.LoadComputeShader(FilePath, EntryPoint, Defines, Key, CachedDevice.Get());
}

void FResourceManager::InvalidateShaderFile(const FString& FilePath)
{
	ShaderCache.InvalidateShaderFile(FilePath);
}

FComputeShader* FResourceManager::GetComputeShader(const FString& Key) const
{
	return ShaderCache.GetComputeShader(Key);
}

TArray<FString> FResourceManager::GetMaterialNames() const
{
	return MaterialCache.GetMaterialNames();
}

TArray<FString> FResourceManager::GetMaterialInterfaceNames() const
{
	return MaterialCache.GetMaterialInterfaceNames(MaterialFilePaths);
}

UMaterial* FResourceManager::GetMaterial(const FString& MaterialName) const
{
	return MaterialCache.GetMaterial(MaterialName);
}

// 嶺뚮씞?녻뚯궘??????怨몃턄 ?띠럾????띠룄????Material????諛댁뎽
UMaterial* FResourceManager::GetOrCreateMaterial(const FString& Path, EMaterialShaderType ShaderType)
{
	UMaterial* Material = GetMaterial(Path);
	if (Material)
	{
		return Material;
	}

	Material = UObjectManager::Get().CreateObject<UMaterial>();
	Material->Name = Path;
	Material->FilePath = Path;

	Material->SetShaderType(ShaderType);

	MaterialCache.RegisterMaterial(Path, Material);

	return Material;
}

UMaterial* FResourceManager::GetOrCreateMaterial(const FString& Name, const FString& Path, EMaterialShaderType ShaderType)
{
	UMaterial* Material = GetMaterial(Name);
	if (Material)
	{
		return Material;
	}

	Material = UObjectManager::Get().CreateObject<UMaterial>();
	Material->Name = Name;
	Material->FilePath = Path;

	Material->SetShaderType(ShaderType);

	MaterialCache.RegisterMaterial(Name, Material);

	return Material;
}

bool FResourceManager::LoadMaterial(const FString& MtlFilePath, EMaterialShaderType ShaderType, ID3D11Device* Device)
{
	return FMaterialLoadService(*this).Load(MtlFilePath, ShaderType, Device);
}

void FResourceManager::RegisterObjMaterialSlotAliases(const FString& ObjPath, const FString& MtlPath)
{
	const FString NormalizedObjPath = FPaths::Normalize(ObjPath);
	const FString NormalizedMtlPath = FPaths::Normalize(MtlPath);
	const TArray<FString> SlotNames = FImportedMaterialPolicy::CollectObjMaterialSlotNames(NormalizedObjPath);

	for (const FString& SlotName : SlotNames)
	{
		const FString* MtlAlias = MaterialCache.FindMaterialSlotAlias(FImportedMaterialPolicy::MakeMaterialSlotAliasKey(NormalizedMtlPath, SlotName));
		if (MtlAlias)
		{
			MaterialCache.SetMaterialSlotAlias(FImportedMaterialPolicy::MakeMaterialSlotAliasKey(NormalizedObjPath, SlotName), *MtlAlias);
		}
	}
}

UMaterial* FResourceManager::GetMaterialForStaticMeshSlot(const FString& SourcePath, const FString& SlotName) const
{
	if (!SourcePath.empty())
	{
		const FString* Alias = MaterialCache.FindMaterialSlotAlias(FImportedMaterialPolicy::MakeMaterialSlotAliasKey(SourcePath, SlotName));
		if (Alias)
		{
			if (UMaterial* Material = GetMaterial(*Alias))
			{
				return Material;
			}
		}
	}

	return GetMaterial(SlotName);
}

void FResourceManager::ResolveStaticMeshMaterialSlots(const FString& SourcePath, FStaticMesh* StaticMesh) const
{
	if (!StaticMesh)
	{
		return;
	}

	for (FStaticMeshMaterialSlot& Slot : StaticMesh->Slots)
	{
		if (!SourcePath.empty())
		{
			const FString* Alias = MaterialCache.FindMaterialSlotAlias(FImportedMaterialPolicy::MakeMaterialSlotAliasKey(SourcePath, Slot.SlotName));
			if (Alias)
			{
				Slot.SlotName = *Alias;
			}
		}

		Slot.Material = GetMaterialForStaticMeshSlot(SourcePath, Slot.SlotName);
		if (Slot.Material == nullptr)
		{
			Slot.Material = GetMaterial("DefaultWhite");
		}
	}
}

void FResourceManager::ResolveSkeletalMeshMaterialSlots(const FString& SourcePath, FSkeletalMesh* SkeletalMesh) const
{
	if (!SkeletalMesh)
	{
		return;
	}

	for (FStaticMeshMaterialSlot& Slot : SkeletalMesh->MaterialSlots)
	{
		if (!SourcePath.empty())
		{
			const FString* Alias = MaterialCache.FindMaterialSlotAlias(FImportedMaterialPolicy::MakeMaterialSlotAliasKey(SourcePath, Slot.SlotName));
			if (Alias)
			{
				Slot.SlotName = *Alias;
			}
		}

		Slot.Material = GetMaterialForStaticMeshSlot(SourcePath, Slot.SlotName);
		if (Slot.Material == nullptr)
		{
			Slot.Material = GetMaterial("DefaultWhite");
		}
	}
}

UMaterialInstance* FResourceManager::CreateMaterialInstance(const FString& Path, UMaterial* Parent)
{
	return MaterialCache.CreateMaterialInstance(Path, Parent);
}

UMaterialInstance* FResourceManager::GetMaterialInstance(const FString& Path) const
{
	return MaterialCache.GetMaterialInstance(Path);
}

UMaterialInterface* FResourceManager::GetMaterialInterface(const FString& Name)
{
	UMaterial* Mat = GetMaterial(Name);
	if (Mat)
	{
		return Mat;
	}
	else if (Mat = GetMaterial(FPaths::Normalize(Name)))
	{
		return Mat;
	}
	else if (UMaterialInstance* MatInst = GetMaterialInstance(Name))
	{
		return MatInst;
	}
	if (UMaterialInstance* MatInst = GetMaterialInstance(FPaths::Normalize(Name)))
	{
		return MatInst;
	}

	const FString NormalizedName = FPaths::Normalize(Name);
	if (FAssetPathPolicy::IsSerializedMaterialAssetPath(NormalizedName) && FAssetPathPolicy::FileExists(NormalizedName))
	{
		if (DeserializeMaterial(NormalizedName))
		{
			if (UMaterial* LoadedMat = GetMaterial(NormalizedName))
			{
				return LoadedMat;
			}
			if (UMaterialInstance* LoadedMatInst = GetMaterialInstance(NormalizedName))
			{
				return LoadedMatInst;
			}
		}
	}

	return nullptr;
}

bool FResourceManager::SerializeMaterial(const FString& MatFilePath, const UMaterial* Material)
{
	return FMaterialSerializationService(*this).SerializeMaterial(MatFilePath, Material);
}

bool FResourceManager::SerializeMaterialInstance(const FString& MatInstFilePath, const UMaterialInstance* MaterialInstance)
{
	return FMaterialSerializationService(*this).SerializeMaterialInstance(MatInstFilePath, MaterialInstance);
}

bool FResourceManager::DeserializeMaterial(const FString& MatFilePath)
{
	return FMaterialSerializationService(*this).DeserializeMaterial(MatFilePath);
}

UTexture* FResourceManager::GetTexture(const FString& Path) const
{
	return TextureCache.Get(Path);
}

UTexture* FResourceManager::LoadTexture(const FString& Path, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		Device = CachedDevice.Get();
	}

	return TextureCache.Load(Path, Device);
}

// --- Font ---
FFontResource* FResourceManager::FindFont(const FName& FontName)
{
	return AtlasCache.FindFont(FontName);
}

const FFontResource* FResourceManager::FindFont(const FName& FontName) const
{
	return AtlasCache.FindFont(FontName);
}

void FResourceManager::RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	AtlasCache.RegisterFont(FontName, InPath, Columns, Rows);
}

// --- Particle ---
FParticleResource* FResourceManager::FindParticle(const FName& ParticleName)
{
	return AtlasCache.FindParticle(ParticleName);
}

const FParticleResource* FResourceManager::FindParticle(const FName& ParticleName) const
{
	return AtlasCache.FindParticle(ParticleName);
}

void FResourceManager::RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	AtlasCache.RegisterParticle(ParticleName, InPath, Columns, Rows);
}

TArray<FString> FResourceManager::GetFontNames() const
{
	return FontFilePaths;
}

TArray<FString> FResourceManager::GetParticleNames() const
{
	return ParticleFilePaths;
}

UStaticMesh* FResourceManager::LoadStaticMesh(const FString& Path)
{
	return FStaticMeshLoadService(*this).Load(Path);
}

UStaticMesh* FResourceManager::FindStaticMesh(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	return StaticMeshCache.Find(NormalizedPath);
}

TArray<FString> FResourceManager::GetStaticMeshPaths() const
{
	return ObjFilePaths;
}

USkeletalMesh* FResourceManager::LoadSkeletalMesh(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	USkeletalMesh* Mesh = FSkeletalMeshLoadService(*this).Load(NormalizedPath);

	//일단 최적화를 위해 anime stack 훑어보는 과정은 LoadAnimSequence(FBX 경로) 에서만...
	//단순히 fbx 내부를 보는 것만으로도 오래 걸림.
	return Mesh;
}

USkeletalMesh* FResourceManager::FindSkeletalMesh(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);

	auto It = SkeletalMeshMap.find(NormalizedPath);
	if (It != SkeletalMeshMap.end())
	{
		return It->second;
	}

	return nullptr;
}

TArray<FString> FResourceManager::GetSkeletalMeshPaths() const
{
	return SkeletalMeshFilePaths;
}

FFbxMeshContentInfo FResourceManager::InspectFbxMeshContent(const FString& Path)
{
	return FbxImporter.InspectMeshContent(Path);
}

bool FResourceManager::SaveSkeletalMesh(USkeletalMesh* Mesh)
{
	if (!Mesh) return false;
	FSkeletalMesh* Data = Mesh->GetMeshData();
	if (!Data) return false;

	const FString FbxPath = Mesh->GetAssetPathFileName();
	if (FbxPath.empty()) return false;

	const FString BinPath = FAssetPathPolicy::MakeWritableSkeletalMeshCacheBinaryPath(FbxPath);
	return BinarySerializer.SaveSkeletalMesh(BinPath, FbxPath, *Data);
}

UCurveFloatAsset* FResourceManager::LoadCurve(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	UCurveFloatAsset* Curve = CurveCache.Load(NormalizedPath);
	if (!Curve)
	{
		return nullptr;
	}

	if (std::find(CurveFilePaths.begin(), CurveFilePaths.end(), NormalizedPath) == CurveFilePaths.end())
	{
		CurveFilePaths.push_back(NormalizedPath);
	}

	return Curve;
}

UCurveFloatAsset* FResourceManager::FindCurve(const FString& Path) const
{
	return CurveCache.Find(Path);
}

bool FResourceManager::SaveCurve(const FString& Path, const UCurveFloatAsset* Curve)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (!CurveCache.Save(NormalizedPath, Curve))
	{
		return false;
	}

	if (std::find(CurveFilePaths.begin(), CurveFilePaths.end(), NormalizedPath) == CurveFilePaths.end())
	{
		CurveFilePaths.push_back(NormalizedPath);
	}

	return true;
}

TArray<FString> FResourceManager::GetCurvePaths() const
{
	return CurveFilePaths;
}

void FResourceManager::SyncDiscoveredFbxAnimationAssets()
{
	if (AnimationFbxSourceFilePaths.empty())
	{
		return;
	}

	int32 ImportedCount = 0;
	int32 AssetCount = 0;
	TArray<FString> ImportedAnimSequenceAssetPaths;
	for (const FString& FbxPath : AnimationFbxSourceFilePaths)
	{
		const TArray<FString> ImportedPaths = ImportAnimationStacksFromFbx(FbxPath);
		AssetCount += static_cast<int32>(ImportedPaths.size());
		for (const FString& ImportedPath : ImportedPaths)
		{
			if (std::find(ImportedAnimSequenceAssetPaths.begin(), ImportedAnimSequenceAssetPaths.end(), ImportedPath)
				== ImportedAnimSequenceAssetPaths.end())
			{
				ImportedAnimSequenceAssetPaths.push_back(ImportedPath);
			}

			if (FindAnimSequence(ImportedPath))
			{
				++ImportedCount;
			}
		}
	}

	WarmUpAnimationPreviewMeshCaches(ImportedAnimSequenceAssetPaths);

	UE_LOG("[AnimSequenceStartupImport] Synced FBX animation sources: Sources=%d Assets=%d Loaded=%d",
		static_cast<int32>(AnimationFbxSourceFilePaths.size()),
		AssetCount,
		ImportedCount);
}

void FResourceManager::WarmUpAnimationPreviewMeshCaches(const TArray<FString>& AnimSequenceAssetPaths)
{
	TArray<FString> PreviewMeshPaths;
	PreviewMeshPaths.reserve(AnimSequenceAssetPaths.size());

	for (const FString& AnimSequenceAssetPath : AnimSequenceAssetPaths)
	{
		UAnimSequence* Sequence = FindAnimSequence(AnimSequenceAssetPath);
		if (!Sequence)
		{
			Sequence = AnimSequenceAssetLoader.Load(AnimSequenceAssetPath);
			if (Sequence)
			{
				AnimSequenceMap[AnimSequenceAssetPath] = Sequence;
			}
		}

		if (!Sequence)
		{
			continue;
		}

		FString PreviewMeshPath = FPaths::Normalize(Sequence->GetPreviewMeshPath());
		if (PreviewMeshPath.empty())
		{
			PreviewMeshPath = FPaths::Normalize(Sequence->GetSourceFilePath());
		}
		if (PreviewMeshPath.empty())
		{
			continue;
		}

		if (std::find(PreviewMeshPaths.begin(), PreviewMeshPaths.end(), PreviewMeshPath) == PreviewMeshPaths.end())
		{
			PreviewMeshPaths.push_back(PreviewMeshPath);
		}
	}

	int32 WarmedCount = 0;
	for (const FString& PreviewMeshPath : PreviewMeshPaths)
	{
		if (EnsureSkeletalMeshCacheForAnimationPreview(PreviewMeshPath))
		{
			++WarmedCount;
		}
	}

	if (!PreviewMeshPaths.empty())
	{
		UE_LOG("[AnimSequenceStartupImport] Warmed animation preview mesh caches: Requested=%d Ready=%d",
			static_cast<int32>(PreviewMeshPaths.size()),
			WarmedCount);
	}
}

bool FResourceManager::EnsureSkeletalMeshCacheForAnimationPreview(const FString& PreviewMeshPath)
{
	const FString NormalizedPreviewMeshPath = FPaths::Normalize(PreviewMeshPath);
	if (NormalizedPreviewMeshPath.empty() || !IsFbxSourcePath(NormalizedPreviewMeshPath))
	{
		return false;
	}

	if (FindSkeletalMesh(NormalizedPreviewMeshPath))
	{
		return true;
	}

	const FString BinaryPath = FAssetPathPolicy::MakeWritableSkeletalMeshCacheBinaryPath(NormalizedPreviewMeshPath);
	if (IsSkeletalMeshBinaryValid(NormalizedPreviewMeshPath, BinaryPath))
	{
		return true;
	}

	const FFbxMeshContentInfo ContentInfo = InspectFbxMeshContent(NormalizedPreviewMeshPath);
	if (!ContentInfo.bHasSkeletalMesh)
	{
		UE_LOG_WARNING("[AnimSequenceStartupImport] Preview mesh is not a skeletal FBX: %s",
			NormalizedPreviewMeshPath.c_str());
		return false;
	}

	return LoadSkeletalMesh(NormalizedPreviewMeshPath) != nullptr;
}

bool FResourceManager::IsImportedAnimSequenceFresh(
	const FString& SourcePath,
	const FString& StackName,
	const FString& AnimSequenceAssetPath,
	bool& bOutNeedsMetadataRefresh)
{
	bOutNeedsMetadataRefresh = false;

	UAnimSequence* ExistingSequence = AnimSequenceAssetLoader.Load(AnimSequenceAssetPath);
	if (!ExistingSequence || !HasAnimSequenceTrackKeys(ExistingSequence))
	{
		return false;
	}

	const FString NormalizedSourcePath = FPaths::Normalize(SourcePath);
	const FString ExistingSourcePath = FPaths::Normalize(ExistingSequence->GetSourceFilePath());
	if (ExistingSourcePath != NormalizedSourcePath ||
		ExistingSequence->GetSourceStackName() != StackName)
	{
		return false;
	}

	const uint64 SourceWriteTimeTicks = GetFileWriteTimeTicks(NormalizedSourcePath);
	const uint64 SourceFileSizeBytes = GetFileSizeBytes(NormalizedSourcePath);
	if (SourceWriteTimeTicks == 0 || SourceFileSizeBytes == 0)
	{
		return false;
	}

	const bool bFastMetadataMatches =
		ExistingSequence->GetSourceFileWriteTimeTicks() == SourceWriteTimeTicks &&
		ExistingSequence->GetSourceFileSizeBytes() == SourceFileSizeBytes;

	if (bFastMetadataMatches && !ExistingSequence->GetSourceFileContentHash().empty())
	{
		AnimSequenceMap[AnimSequenceAssetPath] = ExistingSequence;
		return true;
	}

	const FString SourceContentHash = ComputeFileContentHashString(NormalizedSourcePath);
	if (SourceContentHash.empty())
	{
		return false;
	}

	if (bFastMetadataMatches ||
		(!ExistingSequence->GetSourceFileContentHash().empty() &&
		 ExistingSequence->GetSourceFileContentHash() == SourceContentHash))
	{
		ExistingSequence->SetSourceFileWriteTimeTicks(SourceWriteTimeTicks);
		ExistingSequence->SetSourceFileSizeBytes(SourceFileSizeBytes);
		ExistingSequence->SetSourceFileContentHash(SourceContentHash);
		AnimSequenceMap[AnimSequenceAssetPath] = ExistingSequence;
		bOutNeedsMetadataRefresh = true;
		return true;
	}

	return false;
}

void FResourceManager::RefreshImportedAnimSequenceMetadata(
	UAnimSequence* Sequence,
	const FString& AnimSequenceAssetPath,
	const FString& SourcePath,
	const FString& StackName)
{
	if (!Sequence)
	{
		return;
	}

	const FString NormalizedSourcePath = FPaths::Normalize(SourcePath);
	Sequence->SetAssetPath(FPaths::Normalize(AnimSequenceAssetPath));
	Sequence->SetSourceFilePath(NormalizedSourcePath);
	Sequence->SetSourceStackName(StackName);
	if (Sequence->GetPreviewMeshPath().empty())
	{
		Sequence->SetPreviewMeshPath(NormalizedSourcePath);
	}

	if (!AnimSequenceAssetLoader.Save(AnimSequenceAssetPath, Sequence))
	{
		UE_LOG_WARNING("[AnimSequenceStartupImport] Failed to refresh anim sequence metadata: %s",
			AnimSequenceAssetPath.c_str());
	}
}

TArray<FString> FResourceManager::ImportAnimationStacksFromFbx(const FString& Path)
{
	TArray<FString> ImportedAssetPaths;

	const FString NormalizedPath = FPaths::Normalize(Path);
	if (!IsFbxSourcePath(NormalizedPath))
	{
		return ImportedAssetPaths;
	}

	const TArray<FString> StackNames = FbxImporter.GetAnimationStackNames(NormalizedPath);
	if (StackNames.empty())
	{
		return ImportedAssetPaths;
	}

	ImportedAssetPaths.reserve(StackNames.size());
	TArray<FString> StaleStackNames;
	StaleStackNames.reserve(StackNames.size());
	for (const FString& StackName : StackNames)
	{
		if (StackName.empty())
		{
			continue;
		}

		const FString ImportedAssetPath = FAssetPathPolicy::MakeImportedAnimSequenceAssetPath(NormalizedPath, StackName);

		if (FAssetPathPolicy::FileExists(ImportedAssetPath))
		{
			bool bNeedsMetadataRefresh = false;
			if (IsImportedAnimSequenceFresh(NormalizedPath, StackName, ImportedAssetPath, bNeedsMetadataRefresh))
			{
				if (bNeedsMetadataRefresh)
				{
					RefreshImportedAnimSequenceMetadata(
						FindAnimSequence(ImportedAssetPath),
						ImportedAssetPath,
						NormalizedPath,
						StackName);
				}

				if (std::find(AnimSequenceFilePaths.begin(), AnimSequenceFilePaths.end(), ImportedAssetPath) == AnimSequenceFilePaths.end())
				{
					AnimSequenceFilePaths.push_back(ImportedAssetPath);
				}
				ImportedAssetPaths.push_back(ImportedAssetPath);
				continue;
			}

			UE_LOG("[AnimSequenceImport] Reimport stale FBX animation stack: %s | Stack=%s | Asset=%s",
				NormalizedPath.c_str(),
				StackName.c_str(),
				ImportedAssetPath.c_str());
		}
		else
		{
			UE_LOG("[AnimSequenceImport] Import missing FBX animation stack: %s | Stack=%s | Asset=%s",
				NormalizedPath.c_str(),
				StackName.c_str(),
				ImportedAssetPath.c_str());
		}

		if (FAssetPathPolicy::FileExists(ImportedAssetPath))
		{
			if (std::find(AnimSequenceFilePaths.begin(), AnimSequenceFilePaths.end(), ImportedAssetPath) == AnimSequenceFilePaths.end())
			{
				AnimSequenceFilePaths.push_back(ImportedAssetPath);
			}
		}

		AnimSequenceMap.erase(ImportedAssetPath);
		StaleStackNames.push_back(StackName);
	}

	if (!StaleStackNames.empty())
	{
		FFbxAnimImportOptions ImportOptions;
		ImportOptions.PreviewMeshPath = NormalizedPath;

		TArray<FFbxAnimStackImportResult> ImportResults = FbxImporter.LoadAnimSequences(NormalizedPath, ImportOptions);
		if (ImportResults.empty())
		{
			UE_LOG_WARNING("[AnimSequenceImport] Bulk FBX animation import returned no stacks, falling back to per-stack import: %s",
				NormalizedPath.c_str());
		}

		for (const FString& StackName : StaleStackNames)
		{
			const FString ImportedAssetPath = FAssetPathPolicy::MakeImportedAnimSequenceAssetPath(NormalizedPath, StackName);
			UAnimSequence* ImportedSequence = nullptr;

			for (const FFbxAnimStackImportResult& Result : ImportResults)
			{
				if (Result.StackName == StackName)
				{
					ImportedSequence = Result.Sequence;
					break;
				}
			}

			if (!ImportedSequence)
			{
				FFbxAnimImportOptions FallbackImportOptions;
				FallbackImportOptions.StackName = StackName;
				FallbackImportOptions.PreviewMeshPath = NormalizedPath;
				ImportedSequence = FbxImporter.LoadAnimSequence(NormalizedPath, FallbackImportOptions);
			}

			if (!ImportedSequence)
			{
				UE_LOG_WARNING("[AnimSequenceImport] Failed to import FBX animation stack: %s | Stack=%s",
					NormalizedPath.c_str(),
					StackName.c_str());
				continue;
			}

			ImportedSequence->SetAssetPath(ImportedAssetPath);
			ImportedSequence->SetPreviewMeshPath(NormalizedPath);

			if (!AnimSequenceAssetLoader.Save(ImportedAssetPath, ImportedSequence))
			{
				UE_LOG_WARNING("[AnimSequenceImport] Failed to save imported animation stack: %s -> %s",
					NormalizedPath.c_str(),
					ImportedAssetPath.c_str());
				continue;
			}

			AnimSequenceMap[ImportedAssetPath] = ImportedSequence;
			if (std::find(AnimSequenceFilePaths.begin(), AnimSequenceFilePaths.end(), ImportedAssetPath) == AnimSequenceFilePaths.end())
			{
				AnimSequenceFilePaths.push_back(ImportedAssetPath);
			}

			ImportedAssetPaths.push_back(ImportedAssetPath);
			UE_LOG("[AnimSequenceImport] Imported FBX animation stack: %s | Stack=%s | Asset=%s",
				NormalizedPath.c_str(),
				StackName.c_str(),
				ImportedAssetPath.c_str());
		}
	}

	if (!ImportedAssetPaths.empty())
	{
		if (UAnimSequence* FirstSequence = FindAnimSequence(ImportedAssetPaths.front()))
		{
			// 기존 코드가 FBX 경로 자체를 LoadAnimSequence()에 넘겨도 첫 번째 stack을 반환하던 동작은 유지한다.
			AnimSequenceMap[NormalizedPath] = FirstSequence;
		}
	}

	return ImportedAssetPaths;
}

UAnimSequence* FResourceManager::LoadAnimSequence(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);

	if (UAnimSequence* FoundSequence = FindAnimSequence(NormalizedPath))
	{
		if (FAssetPathPolicy::IsAnimSequenceAssetPath(NormalizedPath) && !HasAnimSequenceTrackKeys(FoundSequence))
		{
			if (UAnimSequence* ReimportedSequence = ReimportAnimSequenceFromSource(FbxImporter, NormalizedPath, FoundSequence))
			{
				if (AnimSequenceAssetLoader.Save(NormalizedPath, ReimportedSequence))
				{
					UE_LOG("[AnimSequenceLoad] Rebuilt empty anim sequence asset from source: %s", NormalizedPath.c_str());
				}
				AnimSequenceMap[NormalizedPath] = ReimportedSequence;
				return ReimportedSequence;
			}
		}
		return FoundSequence;
	}

	UAnimSequence* LoadedSequence = nullptr;
	if (FAssetPathPolicy::IsAnimSequenceAssetPath(NormalizedPath))
	{
		LoadedSequence = AnimSequenceAssetLoader.Load(NormalizedPath);
		if (LoadedSequence && !HasAnimSequenceTrackKeys(LoadedSequence))
		{
			if (UAnimSequence* ReimportedSequence = ReimportAnimSequenceFromSource(FbxImporter, NormalizedPath, LoadedSequence))
			{
				if (AnimSequenceAssetLoader.Save(NormalizedPath, ReimportedSequence))
				{
					UE_LOG("[AnimSequenceLoad] Rebuilt empty anim sequence asset from source: %s", NormalizedPath.c_str());
				}
				LoadedSequence = ReimportedSequence;
			}
		}
	}
	else if (IsFbxSourcePath(NormalizedPath))
	{
		const TArray<FString> ImportedAssetPaths = ImportAnimationStacksFromFbx(NormalizedPath);
		if (!ImportedAssetPaths.empty())
		{
			LoadedSequence = FindAnimSequence(ImportedAssetPaths.front());
			if (!LoadedSequence)
			{
				LoadedSequence = AnimSequenceAssetLoader.Load(ImportedAssetPaths.front());
			}
		}

		// 안전망: stack 전체 import가 실패한 경우 기존 단일 stack import 경로를 한 번 더 시도한다.
		if (!LoadedSequence)
		{
			LoadedSequence = FbxImporter.LoadAnimSequence(NormalizedPath);
			if (LoadedSequence)
			{
				const FString ImportedAssetPath = FAssetPathPolicy::MakeImportedAnimSequenceAssetPath(
					NormalizedPath,
					LoadedSequence->GetSourceStackName());
				LoadedSequence->SetAssetPath(ImportedAssetPath);
				LoadedSequence->SetPreviewMeshPath(NormalizedPath);

				if (AnimSequenceAssetLoader.Save(ImportedAssetPath, LoadedSequence))
				{
					AnimSequenceMap[ImportedAssetPath] = LoadedSequence;
					if (std::find(AnimSequenceFilePaths.begin(), AnimSequenceFilePaths.end(), ImportedAssetPath) == AnimSequenceFilePaths.end())
					{
						AnimSequenceFilePaths.push_back(ImportedAssetPath);
					}
					UE_LOG("[AnimSequenceLoad] Imported FBX animation saved: %s -> %s",
						NormalizedPath.c_str(),
						ImportedAssetPath.c_str());
				}
				else
				{
					UE_LOG_WARNING("[AnimSequenceLoad] Imported FBX animation could not be saved as .animseq: %s",
						NormalizedPath.c_str());
				}
			}
		}
	}

	if (!LoadedSequence)
	{
		UE_LOG_ERROR("[AnimSequenceLoad] Failed | Path=%s", NormalizedPath.c_str());
		return nullptr;
	}

	AnimSequenceMap[NormalizedPath] = LoadedSequence;
	if (std::find(AnimSequenceFilePaths.begin(), AnimSequenceFilePaths.end(), NormalizedPath) == AnimSequenceFilePaths.end())
	{
		AnimSequenceFilePaths.push_back(NormalizedPath);
	}

	return LoadedSequence;
}

bool FResourceManager::SaveAnimSequence(const FString& Path, const UAnimSequence* Sequence)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (!AnimSequenceAssetLoader.Save(NormalizedPath, Sequence))
	{
		return false;
	}

	if (Sequence)
	{
		AnimSequenceMap[NormalizedPath] = const_cast<UAnimSequence*>(Sequence);
	}

	if (std::find(AnimSequenceFilePaths.begin(), AnimSequenceFilePaths.end(), NormalizedPath) == AnimSequenceFilePaths.end())
	{
		AnimSequenceFilePaths.push_back(NormalizedPath);
	}

	return true;
}

UAnimSequence* FResourceManager::FindAnimSequence(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	auto It = AnimSequenceMap.find(NormalizedPath);
	return It != AnimSequenceMap.end() ? It->second : nullptr;
}

TArray<FString> FResourceManager::GetAnimSequencePaths() const
{
	return AnimSequenceFilePaths;
}

const TArray<FString>& FResourceManager::GetTextureFilePath() const
{
	return TextureFilePaths;
}

ID3D11SamplerState* FResourceManager::GetOrCreateSamplerState(ESamplerType Type, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		Device = CachedDevice.Get();
	}

	return RenderStateCache.GetOrCreateSamplerState(Type, Device);
}

ID3D11DepthStencilState* FResourceManager::GetOrCreateDepthStencilState(EDepthStencilType Type, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		Device = CachedDevice.Get();
	}
	return RenderStateCache.GetOrCreateDepthStencilState(Type, Device);
}

ID3D11BlendState* FResourceManager::GetOrCreateBlendState(EBlendType Type, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		Device = CachedDevice.Get();
	}
	return RenderStateCache.GetOrCreateBlendState(Type, Device);
}

ID3D11RasterizerState* FResourceManager::GetOrCreateRasterizerState(ERasterizerType Type, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		Device = CachedDevice.Get();
	}
	return RenderStateCache.GetOrCreateRasterizerState(Type, Device);
}

size_t FResourceManager::GetMaterialMemorySize() const
{
	return FResourceMemoryReporter::GetMaterialMemorySize(MaterialCache);
}
