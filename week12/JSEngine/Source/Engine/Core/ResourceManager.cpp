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
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

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

	FString MakeProjectRelativePath(const FString& Path)
	{
		const FString NormalizedPath = FPaths::Normalize(Path);
		if (NormalizedPath.empty())
		{
			return {};
		}

		std::filesystem::path FsPath(FPaths::ToWide(NormalizedPath));
		if (!FsPath.is_absolute())
		{
			return NormalizedPath;
		}

		std::error_code ErrorCode;
		const std::filesystem::path RelativePath = std::filesystem::relative(FsPath, std::filesystem::path(FPaths::RootDir()), ErrorCode);
		if (ErrorCode || RelativePath.empty())
		{
			return NormalizedPath;
		}

		const std::wstring RelativeText = RelativePath.generic_wstring();
		if (RelativeText == L".." || RelativeText.rfind(L"../", 0) == 0)
		{
			return NormalizedPath;
		}

		return FPaths::Normalize(FPaths::ToUtf8(RelativeText));
	}

	bool IsImageExtension(const std::wstring& Extension)
	{
		return Extension == L".png"
			|| Extension == L".dds"
			|| Extension == L".jpg"
			|| Extension == L".jpeg"
			|| Extension == L".bmp"
			|| Extension == L".tga";
	}

	bool IsUnderAssetDirectory(const FString& RelativePath, const FString& DirectoryName)
	{
		std::filesystem::path RelativeFsPath(FPaths::ToWide(FPaths::Normalize(RelativePath)));
		std::wstring GenericPath = RelativeFsPath.generic_wstring();
		std::transform(GenericPath.begin(), GenericPath.end(), GenericPath.begin(), ::towlower);

		std::wstring Prefix = L"asset/";
		Prefix += FPaths::ToWide(DirectoryName);
		std::transform(Prefix.begin(), Prefix.end(), Prefix.begin(), ::towlower);
		Prefix += L"/";

		return GenericPath == Prefix.substr(0, Prefix.size() - 1)
			|| GenericPath.rfind(Prefix, 0) == 0;
	}

	void CopyAnimSequenceNotifies(const UAnimSequence* Source, UAnimSequence* Destination)
	{
		if (!Source || !Destination)
		{
			return;
		}

		Destination->ClearNotifies();
		for (const FAnimNotifyStateEvent& Notify : Source->GetNotifies())
		{
			Destination->AddNotify(Notify.TriggerTime, Notify.NotifyName, Notify.Duration, Notify.NotifyClassName);
		}
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

FString FResourceManager::GetCachedFileContentHashString(const FString& Path, uint64 WriteTimeTicks, uint64 FileSizeBytes)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	const FString CacheKey =
		NormalizedPath + "#" + std::to_string(WriteTimeTicks) + ":" + std::to_string(FileSizeBytes);

	auto Found = FileContentHashCache.find(CacheKey);
	if (Found != FileContentHashCache.end())
	{
		return Found->second;
	}

	const FString Hash = ComputeFileContentHashString(NormalizedPath);
	if (!Hash.empty())
	{
		FileContentHashCache[CacheKey] = Hash;
	}
	return Hash;
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
	SubUVFilePaths.clear();
	CurveFilePaths.clear();
	SkeletalMeshFilePaths.clear();
	AnimSequenceFilePaths.clear();
	AnimationFbxSourceFilePaths.clear();
	FileContentHashCache.clear();
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
			std::wstring RelativeGenericPath = std::filesystem::path(FPaths::ToWide(RelativePath)).generic_wstring();
			std::transform(RelativeGenericPath.begin(), RelativeGenericPath.end(), RelativeGenericPath.begin(), ::towlower);

			const bool bUnderSkeletalMeshRoot = RelativeGenericPath.rfind(L"asset/skeletalmesh/", 0) == 0;
			const FString SkeletalBinaryPath = FAssetPathPolicy::MakeWritableSkeletalMeshCacheBinaryPath(RelativePath);
			const bool bHasValidSkeletalBinary = IsSkeletalMeshBinaryValid(RelativePath, SkeletalBinaryPath);
			if (bUnderSkeletalMeshRoot || bHasValidSkeletalBinary)
			{
				SkeletalMeshFilePaths.push_back(RelativePath);
			}
		}
	}
	else if (Extension == L".mtl" || Extension == L".mat" || Extension == L".matinst")
	{
		MaterialFilePaths.push_back(RelativePath);
	}
	else if (Extension == L".font")
	{
		FTextureAtlasAsset Asset;
		if (LoadTextureAtlasAsset(FilePath, ETextureAtlasAssetType::Font, ProjectRootPath, Asset))
		{
			FontFilePaths.push_back(RelativePath);
			RegisterFont(FName(RelativePath.c_str()), Asset.ImagePath, Asset.Columns, Asset.Rows);
		}
	}
	else if (Extension == L".subuv")
	{
		FTextureAtlasAsset Asset;
		if (LoadTextureAtlasAsset(FilePath, ETextureAtlasAssetType::SubUV, ProjectRootPath, Asset))
		{
			SubUVFilePaths.push_back(RelativePath);
			RegisterSubUV(FName(RelativePath.c_str()), Asset.ImagePath, Asset.Columns, Asset.Rows);
		}
	}
	else if (IsImageExtension(Extension)
		&& (IsUnderAssetDirectory(RelativePath, "Texture")
			|| IsUnderAssetDirectory(RelativePath, "Font")
			|| IsUnderAssetDirectory(RelativePath, "SubUV")))
	{
		TextureFilePaths.push_back(RelativePath);
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

	// Startup should only discover assets. FBX animation import/binary cache generation
	// is intentionally deferred until an animation asset is explicitly opened or requested.
	
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

bool FResourceManager::LoadTextureAtlasAsset(
	const std::filesystem::path& AssetFilePath,
	ETextureAtlasAssetType Type,
	const std::filesystem::path& ProjectRootPath,
	FTextureAtlasAsset& OutAsset) const
{
	return FTextureAtlasAssetService::Load(AssetFilePath, Type, ProjectRootPath, OutAsset);
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

// --- SubUV ---
FSubUVResource* FResourceManager::FindSubUV(const FName& SubUVName)
{
	return AtlasCache.FindSubUV(SubUVName);
}

const FSubUVResource* FResourceManager::FindSubUV(const FName& SubUVName) const
{
	return AtlasCache.FindSubUV(SubUVName);
}

void FResourceManager::RegisterSubUV(const FName& SubUVName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	AtlasCache.RegisterSubUV(SubUVName, InPath, Columns, Rows);
}

TArray<FString> FResourceManager::GetFontNames() const
{
	return FontFilePaths;
}

TArray<FString> FResourceManager::GetSubUVNames() const
{
	return SubUVFilePaths;
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

void FResourceManager::WarmUpAnimationPreviewMeshCaches(const TArray<FString>& AnimSequenceAssetPaths)
{
	TArray<FString> PreviewMeshPaths;
	PreviewMeshPaths.reserve(AnimSequenceAssetPaths.size());

	for (const FString& AnimSequenceAssetPath : AnimSequenceAssetPaths)
	{
		FAnimSequenceAssetMetadata Metadata;
		if (!AnimSequenceAssetLoader.LoadMetadata(AnimSequenceAssetPath, Metadata))
		{
			continue;
		}

		FString PreviewMeshPath = FPaths::Normalize(Metadata.PreviewMeshPath);
		if (PreviewMeshPath.empty())
		{
			PreviewMeshPath = FPaths::Normalize(Metadata.SourceFilePath);
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

TArray<FString> FResourceManager::ImportAnimationStacksFromFbx(const FString& Path)
{
	TArray<FString> ImportedAssetPaths;
	TMap<FString, FString> ExistingAssetPathByStackName;

	const FString NormalizedPath = FPaths::Normalize(Path);
	const FString StableSourcePath = MakeProjectRelativePath(NormalizedPath);
	if (!IsFbxSourcePath(NormalizedPath))
	{
		return ImportedAssetPaths;
	}

	// 1. 이미 임포트된 에셋이 있는지 확인 (메타데이터를 로드해 FBX 원본 경로 대조)
	bool bAllExistingCachesFresh = true;
	for (const FString& AssetPath : AnimSequenceFilePaths)
	{
		FAnimSequenceAssetMetadata Metadata;
		if (AnimSequenceAssetLoader.LoadMetadata(AssetPath, Metadata))
		{
			if (MakeProjectRelativePath(Metadata.SourceFilePath) == StableSourcePath)
			{
				ImportedAssetPaths.push_back(AssetPath);
				if (!Metadata.SourceStackName.empty())
				{
					ExistingAssetPathByStackName[Metadata.SourceStackName] = AssetPath;
				}
				if (!AnimSequenceAssetLoader.HasValidBinaryCache(AssetPath))
				{
					bAllExistingCachesFresh = false;
				}
			}
		}
	}
	
	// 이미 에셋과 바이너리 캐시가 모두 존재한다면 즉시 반환한다.
	// descriptor만 있고 Bin/*.bin이 삭제된 제출 상태라면 아래 batch import로 한 번에 재생성한다.
	if (!ImportedAssetPaths.empty() && bAllExistingCachesFresh)
	{
		// 구 호환성: FBX 경로로 맵에 등록 (첫 번째 스택 기준)
		if (UAnimSequence* FirstSequence = FindAnimSequence(ImportedAssetPaths.front()))
		{
			AnimSequenceMap[NormalizedPath] = FirstSequence;
		}
		return ImportedAssetPaths;
	}
	
	// 2. 임포트된 에셋이 하나도 없는 경우에만 FBX 파싱 (최초 1회)
	FFbxAnimImportOptions ImportOptions;
	ImportOptions.PreviewMeshPath = NormalizedPath;

	// LoadAnimSequences가 FBX를 한 번 열어서 stack 순회와 sequence 생성을 같이 처리한다.
	TArray<FFbxAnimStackImportResult> ImportResults = FbxImporter.LoadAnimSequences(NormalizedPath, ImportOptions);
	
	for (const FFbxAnimStackImportResult& Result : ImportResults)
	{
		if (!Result.Sequence || Result.StackName.empty())
		{
			continue;
		}

		bool bUsingExistingAssetPath = false;
		FString ImportedAssetPath = FAssetPathPolicy::MakeImportedAnimSequenceAssetPath(NormalizedPath, Result.StackName);
		auto ExistingPathIt = ExistingAssetPathByStackName.find(Result.StackName);
		if (ExistingPathIt != ExistingAssetPathByStackName.end())
		{
			ImportedAssetPath = ExistingPathIt->second;
			bUsingExistingAssetPath = true;
		}
		
		Result.Sequence->SetAssetPath(ImportedAssetPath);
		Result.Sequence->SetPreviewMeshPath(NormalizedPath);

		if (bUsingExistingAssetPath)
		{
			if (UAnimSequence* ExistingDescriptor = AnimSequenceAssetLoader.Load(ImportedAssetPath))
			{
				CopyAnimSequenceNotifies(ExistingDescriptor, Result.Sequence);
			}
		}

		if (AnimSequenceAssetLoader.Save(ImportedAssetPath, Result.Sequence))
		{
			AnimSequenceMap[ImportedAssetPath] = Result.Sequence;
			
			if (std::find(AnimSequenceFilePaths.begin(), AnimSequenceFilePaths.end(), ImportedAssetPath) == AnimSequenceFilePaths.end())
			{
				AnimSequenceFilePaths.push_back(ImportedAssetPath);
			}

			if (std::find(ImportedAssetPaths.begin(), ImportedAssetPaths.end(), ImportedAssetPath) == ImportedAssetPaths.end())
			{
				ImportedAssetPaths.push_back(ImportedAssetPath);
			}
			
			UE_LOG("[AnimSequenceImport] Imported FBX animation stack: %s | Stack=%s | Asset=%s",
				NormalizedPath.c_str(),
				Result.StackName.c_str(),
				ImportedAssetPath.c_str());
		}
		else
		{
			UE_LOG_WARNING("[AnimSequenceImport] Failed to save imported animation stack: %s -> %s",
				NormalizedPath.c_str(),
				ImportedAssetPath.c_str());
		}
	}

	if (!ImportedAssetPaths.empty())
	{
		EnsureSkeletalMeshCacheForAnimationPreview(NormalizedPath);

		if (UAnimSequence* FirstSequence = FindAnimSequence(ImportedAssetPaths.front()))
		{
			AnimSequenceMap[NormalizedPath] = FirstSequence;
		}
	}

	return ImportedAssetPaths;
}

UAnimSequence* FResourceManager::LoadAnimSequence(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);

	// 1. 메모리 캐시를 일단 확인해
	if (UAnimSequence* FoundSequence = FindAnimSequence(NormalizedPath))
	{
		return FoundSequence;
	}

	UAnimSequence* LoadedSequence = nullptr;

	// 2. 바이너리 에셋 경로라면 즉시 로드해
	if (FAssetPathPolicy::IsAnimSequenceAssetPath(NormalizedPath))
	{
		LoadedSequence = AnimSequenceAssetLoader.Load(NormalizedPath);
		if (LoadedSequence &&
			LoadedSequence->GetDataModel() &&
			LoadedSequence->GetDataModel()->GetBoneAnimationTracks().empty() &&
			!LoadedSequence->AreJsonTracksEmbedded())
		{
			const FString SourceFilePath = MakeProjectRelativePath(LoadedSequence->GetSourceFilePath());
			if (IsFbxSourcePath(SourceFilePath))
			{
				const FString SourceStackName = LoadedSequence->GetSourceStackName();
				const FString PreviewMeshPath = MakeProjectRelativePath(
					LoadedSequence->GetPreviewMeshPath().empty()
						? SourceFilePath
						: LoadedSequence->GetPreviewMeshPath());
				ImportAnimationStacksFromFbx(SourceFilePath);
				EnsureSkeletalMeshCacheForAnimationPreview(PreviewMeshPath.empty() ? SourceFilePath : PreviewMeshPath);

				if (UAnimSequence* RebuiltSequence = AnimSequenceAssetLoader.Load(NormalizedPath))
				{
					RebuiltSequence->SetAssetPath(NormalizedPath);
					RebuiltSequence->SetSourceFilePath(SourceFilePath);
					RebuiltSequence->SetSourceStackName(SourceStackName);
					RebuiltSequence->SetPreviewMeshPath(PreviewMeshPath);
					LoadedSequence = RebuiltSequence;
				}
			}
		}
	}
	// 3. FBX 소스 경로인 경우라면 진짜 일을 시작함
	else if (IsFbxSourcePath(NormalizedPath))
	{
		//순회하며 animstack을 싹 다 가져옴
		const TArray<FString> ImportedAssetPaths = ImportAnimationStacksFromFbx(NormalizedPath);
		
		if (!ImportedAssetPaths.empty())
		{
			const FString& FirstAssetPath = ImportedAssetPaths.front();
			LoadedSequence = FindAnimSequence(FirstAssetPath);
			if (!LoadedSequence)
			{
				LoadedSequence = AnimSequenceAssetLoader.Load(FirstAssetPath);
				if (LoadedSequence)
				{
					// FBX 경로로 요청해서 이미 생성된 .animseq를 읽은 경우,
					// source FBX key와 asset key 둘 다 같은 객체를 가리키게 해서 다음 요청에서 재로드하지 않는다.
					AnimSequenceMap[FirstAssetPath] = LoadedSequence;
				}
			}
		}
	}

	if (!LoadedSequence)
	{
		UE_LOG_WARNING("[AnimSequenceLoad] Failed to load anim sequence: %s", NormalizedPath.c_str());
		return nullptr;
	}

	// 4. 로드된 결과를 메모리 캐시에 등록
	AnimSequenceMap[NormalizedPath] = LoadedSequence;
	
	// 원래의 Asset Path 정보 보정
	if (LoadedSequence->GetAssetPath().empty())
	{
		LoadedSequence->SetAssetPath(NormalizedPath);
	}

	// 5. 관리 목록(FilePaths) 등록 최적화 (std::find 중복 방지를 위한 단순화)
	// 보통 엔진 시작 시 폴더를 훑어서 AnimSequenceFilePaths를 다 채워놓으므로, 
	// 런타임에 동적으로 파일이 생기는 게 아니라면 이 과정은 이미 되어있을 확률이 높습니다.
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

namespace
{
	int32 GetJsonInt(json::JSON& Object, const char* Key, int32 DefaultValue = 0)
	{
		return Object.hasKey(Key) ? static_cast<int32>(Object[Key].ToInt()) : DefaultValue;
	}

	float GetJsonFloat(json::JSON& Object, const char* Key, float DefaultValue = 0.0f)
	{
		return Object.hasKey(Key) ? static_cast<float>(Object[Key].ToFloat()) : DefaultValue;
	}

	bool GetJsonBool(json::JSON& Object, const char* Key, bool DefaultValue = false)
	{
		return Object.hasKey(Key) ? Object[Key].ToBool() : DefaultValue;
	}

	FString GetJsonString(json::JSON& Object, const char* Key, const FString& DefaultValue = "")
	{
		return Object.hasKey(Key) ? Object[Key].ToString() : DefaultValue;
	}

	FVector2 GetJsonVector2(json::JSON& Object, const char* Key, const FVector2& DefaultValue = FVector2(0.0f, 0.0f))
	{
		if (!Object.hasKey(Key))
		{
			return DefaultValue;
		}

		json::JSON& Value = Object[Key];
		if (Value.JSONType() == json::JSON::Class::Array && Value.length() >= 2)
		{
			return FVector2(
				static_cast<float>(Value[0].ToFloat()),
				static_cast<float>(Value[1].ToFloat()));
		}

		if (Value.JSONType() == json::JSON::Class::Object)
		{
			return FVector2(
				Value.hasKey("X") ? static_cast<float>(Value["X"].ToFloat()) : DefaultValue.X,
				Value.hasKey("Y") ? static_cast<float>(Value["Y"].ToFloat()) : DefaultValue.Y);
		}

		return DefaultValue;
	}

	FAnimTransitionConditionDesc ParseAnimTransitionCondition(json::JSON& Object)
	{
		FAnimTransitionConditionDesc Condition;
		Condition.Type = static_cast<EAnimTransitionConditionType>(
			GetJsonInt(Object, "Type", static_cast<int32>(EAnimTransitionConditionType::AlwaysTrue)));
		Condition.ParameterName = GetJsonString(Object, "ParameterName");
		Condition.BoolValue = GetJsonBool(Object, "BoolValue", true);
		Condition.Threshold = GetJsonFloat(Object, "Threshold", 0.0f);
		Condition.IntValue = GetJsonInt(Object, "IntValue", 0);
		Condition.LuaFunctionName = GetJsonString(Object, "LuaFunctionName");
		return Condition;
	}

	FAnimStateTransitionDesc ParseAnimStateTransition(json::JSON& Object)
	{
		FAnimStateTransitionDesc Transition;
		Transition.FromStateId = GetJsonInt(Object, "FromStateId", -1);
		Transition.ToStateId = GetJsonInt(Object, "ToStateId", -1);
		Transition.BlendTime = GetJsonFloat(Object, "BlendTime", 0.2f);
		Transition.Priority = GetJsonInt(Object, "Priority", 0);
		if (Object.hasKey("Condition") && Object["Condition"].JSONType() == json::JSON::Class::Object)
		{
			Transition.Condition = ParseAnimTransitionCondition(Object["Condition"]);
		}
		return Transition;
	}

	FAnimStateDesc ParseAnimState(json::JSON& Object)
	{
		FAnimStateDesc State;
		State.StateId = GetJsonInt(Object, "StateId", -1);
		State.Name = GetJsonString(Object, "Name");
		State.AnimationPath = GetJsonString(Object, "AnimationPath");
		State.Position = GetJsonVector2(Object, "Position");
		State.PlayRate = GetJsonFloat(Object, "PlayRate", 1.0f);
		State.bLoop = GetJsonBool(Object, "bLoop", true);
		State.bAutoAdvanceOnEnd = GetJsonBool(Object, "bAutoAdvanceOnEnd", true);
		return State;
	}

	FAnimStateMachineDesc ParseAnimStateMachine(json::JSON& Object)
	{
		FAnimStateMachineDesc Machine;
		Machine.EntryStateId = GetJsonInt(Object, "EntryStateId", -1);

		if (Object.hasKey("States") && Object["States"].JSONType() == json::JSON::Class::Array)
		{
			json::JSON& States = Object["States"];
			for (int32 Index = 0; Index < static_cast<int32>(States.length()); ++Index)
			{
				if (States[Index].JSONType() == json::JSON::Class::Object)
				{
					Machine.States.push_back(ParseAnimState(States[Index]));
				}
			}
		}

		if (Object.hasKey("Transitions") && Object["Transitions"].JSONType() == json::JSON::Class::Array)
		{
			json::JSON& Transitions = Object["Transitions"];
			for (int32 Index = 0; Index < static_cast<int32>(Transitions.length()); ++Index)
			{
				if (Transitions[Index].JSONType() == json::JSON::Class::Object)
				{
					Machine.Transitions.push_back(ParseAnimStateTransition(Transitions[Index]));
				}
			}
		}

		return Machine;
	}

	FAnimGraphNodeDesc ParseAnimGraphNode(json::JSON& Object)
	{
		FAnimGraphNodeDesc Node;
		Node.NodeId = GetJsonInt(Object, "NodeId", -1);
		Node.Type = static_cast<EAnimGraphNodeType>(
			GetJsonInt(Object, "Type", static_cast<int32>(EAnimGraphNodeType::SequencePlayer)));
		Node.Name = GetJsonString(Object, "Name");
		Node.Position = GetJsonVector2(Object, "Position");
		Node.AnimationPath = GetJsonString(Object, "AnimationPath");
		Node.PlayRate = GetJsonFloat(Object, "PlayRate", 1.0f);
		Node.bLoop = GetJsonBool(Object, "bLoop", true);
		Node.InputPoseNodeId = GetJsonInt(Object, "InputPoseNodeId", -1);
		if (Object.hasKey("StateMachine") && Object["StateMachine"].JSONType() == json::JSON::Class::Object)
		{
			Node.StateMachine = ParseAnimStateMachine(Object["StateMachine"]);
		}
		return Node;
	}

	void LoadAnimGraphNodesFromJson(UAnimGraphAsset* Asset, json::JSON& JsonData)
	{
		if (!Asset)
		{
			return;
		}

		if (JsonData.hasKey("RootNodeId"))
		{
			Asset->RootNodeId = GetJsonInt(JsonData, "RootNodeId", -1);
		}

		if (!JsonData.hasKey("Nodes") || JsonData["Nodes"].JSONType() != json::JSON::Class::Array)
		{
			return;
		}

		Asset->Nodes.clear();
		json::JSON& Nodes = JsonData["Nodes"];
		for (int32 Index = 0; Index < static_cast<int32>(Nodes.length()); ++Index)
		{
			if (Nodes[Index].JSONType() == json::JSON::Class::Object)
			{
				Asset->Nodes.push_back(ParseAnimGraphNode(Nodes[Index]));
			}
		}

		Asset->ValidateAndRepairGraph();
	}
}

UAnimGraphAsset* FResourceManager::LoadAnimGraph(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	const std::filesystem::path FilePath =
		std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));

	std::ifstream In(FilePath);
	if (!In.is_open())
	{
		UE_LOG_ERROR("[AnimGraphAsset] Failed to open: %s", NormalizedPath.c_str());
		return nullptr;
	}

	std::string JsonStr(
		(std::istreambuf_iterator<char>(In)),
		std::istreambuf_iterator<char>()
	);

	json::JSON JsonData = json::JSON::Load(JsonStr);
	if (JsonData.JSONType() != json::JSON::Class::Object)
	{
		UE_LOG_ERROR("[AnimGraphAsset] Invalid json: %s", NormalizedPath.c_str());
		return nullptr;
	}

	UAnimGraphAsset* Asset = UObjectManager::Get().CreateObject<UAnimGraphAsset>();
	if (!Asset)
	{
		UE_LOG_ERROR("[AnimGraphAsset] Failed to create asset object: %s", NormalizedPath.c_str());
		return nullptr;
	}

	FJsonReader Reader(JsonData);
	Asset->Serialize(Reader);
	LoadAnimGraphNodesFromJson(Asset, JsonData);

	return Asset;
}

bool FResourceManager::SaveAnimGraph(UAnimGraphAsset* Asset, const FString& Path)
{
	if (!Asset)
	{
		return false;
	}

	const FString NormalizedPath = FPaths::Normalize(Path);
	const std::filesystem::path FilePath =
		std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));

	json::JSON JsonData = json::JSON::Make(json::JSON::Class::Object);

	FJsonWriter Writer(JsonData);
	Asset->Serialize(Writer);

	std::error_code ErrorCode;
	std::filesystem::create_directories(FilePath.parent_path(), ErrorCode);
	if (ErrorCode)
	{
		UE_LOG_ERROR(
			"[AnimGraphAsset] Failed to create directory: %s",
			NormalizedPath.c_str()
		);
		return false;
	}

	std::ofstream Out(FilePath);
	if (!Out.is_open())
	{
		UE_LOG_ERROR(
			"[AnimGraphAsset] Failed to open for writing: %s",
			NormalizedPath.c_str()
		);
		return false;
	}

	Out << JsonData.dump(4);
	return true;
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
