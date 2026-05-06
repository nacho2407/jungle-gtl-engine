#include "Core/ResourceManager.h"

#include "Core/Paths.h"
#include "Core/AssetPathPolicy.h"
#include "Core/ImportedMaterialPolicy.h"
#include "Core/MaterialLoadService.h"
#include "Core/MaterialSerializationService.h"
#include "Core/ResourceMemoryReporter.h"
#include "Core/StaticMeshLoadService.h"

#include <algorithm>
#include <filesystem>
#include <chrono>
#include <cwctype>
#include "Asset/FileUtils.h"

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

}

#pragma region __BINARY__

namespace fs = std::filesystem;

uint64 FResourceManager::GetFileWriteTimeTicks(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	fs::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));
	if (!fs::exists(FilePath))
	{
		return 0;
	}

	auto WriteTime = fs::last_write_time(FilePath);
	auto Duration = WriteTime.time_since_epoch();

	return static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::seconds>(Duration).count());
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
	else if (Extension == L".obj")
	{
		ObjFilePaths.push_back(RelativePath);

		FStaticMeshResource Resource;
		Resource.Name = RelativePath;
		Resource.Path = RelativePath;
		Resource.bPreload = false;
		Resource.bNormalizeToUnitCube = false;
		StaticMeshCache.RegisterResource(Resource);
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
	UMaterial* DefaultMat = GetOrCreateMaterial("DefaultWhite", "Shaders/UberLit.hlsl");
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
	UMaterial* OutlineMat = GetOrCreateMaterial("OutlineMaterial", "Shaders/OutlinePostProcess.hlsl");
	OutlineMat->SetParam("OutlineColor", FMaterialParamValue(FVector4(1.0f, 0.5f, 0.0f, 1.0f)));
	OutlineMat->SetParam("OutlineThicknessPixels", FMaterialParamValue(5.0f));
	OutlineMat->SetParam("OutlineViewportSize", FMaterialParamValue(FVector2(800.0f, 600.0f)));
    OutlineMat->SetParam("OutlineViewportOrigin", FMaterialParamValue(FVector2(0.0f, 0.0f)));
}

//	RootPath ??륁맄????덈뮉 筌뤴뫀諭?????揶쎛??Asset??????뤿연 ?λ뜃由??獄?????怨밸릭????λ땾
void FResourceManager::LoadFromAssetDirectory(const FString& Path)
{
	//	?λ뜃由??
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

	// ???遺얠젂?醫듼봺 ?類ｂ봺
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

	DefaultWhiteTexture.Reset();
	CachedDevice.Reset();
}

bool FResourceManager::LoadShader(const FString& FilePath, const FString& VSEntryPoint, const FString& PSEntryPoint,
								  const D3D_SHADER_MACRO* Defines, uint32 PermutationKey)
{
	return ShaderCache.LoadShader(FilePath, VSEntryPoint, PSEntryPoint, Defines, PermutationKey, CachedDevice.Get());
}

bool FResourceManager::EnsureShaderPermutation(const FString& FilePath, uint32 PermutationKey)
{
	return ShaderCache.EnsureShaderPermutation(FilePath, PermutationKey, CachedDevice.Get());
}

void FResourceManager::ReloadShader(const FString& FilePath)
{
	ShaderCache.ReloadShader(FilePath, CachedDevice.Get());
}

UShader* FResourceManager::GetShader(const FString& FilePath) const
{
	return ShaderCache.GetShader(FilePath);
}

bool FResourceManager::LoadComputeShader(const FString& FilePath, const FString& EntryPoint,
                                         const D3D_SHADER_MACRO* Defines, const FString& Key)
{
	return ShaderCache.LoadComputeShader(FilePath, EntryPoint, Defines, Key, CachedDevice.Get());
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

// 筌띲끆而삭퉪?????곸뵠 揶쎛??揶쏄쑬???Material????밴쉐
UMaterial* FResourceManager::GetOrCreateMaterial(const FString& Path, const FString& ShaderName)
{
	UMaterial* Material = GetMaterial(Path);
	if (Material)
	{
		return Material;
	}

	Material = UObjectManager::Get().CreateObject<UMaterial>();
	Material->Name = Path;
	Material->FilePath = Path;

	UShader* Shader = GetShader(ShaderName);
	Material->SetShader(Shader);

	MaterialCache.RegisterMaterial(Path, Material);

	return Material;
}

UMaterial* FResourceManager::GetOrCreateMaterial(const FString& Name, const FString& Path, const FString& ShaderName)
{
	UMaterial* Material = GetMaterial(Name);
	if (Material)
	{
		return Material;
	}

	Material = UObjectManager::Get().CreateObject<UMaterial>();
	Material->Name = Name;
	Material->FilePath = Path;

	UShader* Shader = GetShader(ShaderName);
	Material->SetShader(Shader);

	MaterialCache.RegisterMaterial(Name, Material);

	return Material;
}

bool FResourceManager::LoadMaterial(const FString& MtlFilePath, const FString& ShaderName, ID3D11Device* Device)
{
	return FMaterialLoadService(*this).Load(MtlFilePath, ShaderName, Device);
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
