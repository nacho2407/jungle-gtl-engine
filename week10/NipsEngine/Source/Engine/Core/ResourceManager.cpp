#include "Core/ResourceManager.h"

#include "Core/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>
#include "Asset/FileUtils.h"

#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "UI/EditorConsoleWidget.h"
#include "Settings/EditorSettings.h"
#include "Asset/BinarySerializer.h"
#include "Asset/StaticMeshTypes.h"
#include "Asset/StaticMeshSimplifier.h"
#include "Render/Scene/RenderCommand.h"
#include "Render/Resource/ObjMtlLoader.h"

#pragma region __BINARY__

uint64 FResourceManager::GetFileWriteTimeTicks(const FString& Path) const
{
	namespace fs = std::filesystem;

	fs::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(Path)));
	if (!fs::exists(FilePath))
	{
		return 0;
	}

	auto WriteTime = fs::last_write_time(FilePath);
	auto Duration = WriteTime.time_since_epoch();

	return static_cast<uint64>(
		std::chrono::duration_cast<std::chrono::seconds>(Duration).count());
}

FString FResourceManager::MakeStaticMeshBinaryPath(const FString& SourcePath) const
{
	namespace fs = std::filesystem;

	fs::path SourceFsPath(FPaths::ToWide(SourcePath));

	//	Root/Asset/Mesh/Bin
	fs::path BinDir = fs::path(FPaths::RootDir()) / "Asset" / "Mesh" / "Bin";

	if (!fs::exists(BinDir))
	{
		fs::create_directories(BinDir);
	}

	//	파일명만 따와서 .bin 으로 저장
	fs::path BinaryFileName = SourceFsPath.stem();
	BinaryFileName += ".bin";

	fs::path BinaryPath = BinDir / BinaryFileName;
	return FPaths::ToString(BinaryPath.wstring());
}

bool FResourceManager::IsStaticMeshBinaryValid(const FString& SourcePath, const FString& BinaryPath) const
{
	FStaticMeshBinaryHeader Header;
	if (!BinarySerializer.ReadStaticMeshHeader(BinaryPath, Header))
	{
		return false;
	}

	const uint64 SourceWriteTime = GetFileWriteTimeTicks(SourcePath);
	if (SourceWriteTime == 0)
	{
		return false;
	}

	return Header.SourceFileWriteTime == SourceWriteTime;
}

void FResourceManager::PreloadStaticMeshes()
{
	for (const auto& [Key, Resource] : StaticMeshRegistry)
	{
		if (!Resource.bPreload)
		{
			continue;
		}

		if (LoadStaticMesh(Resource.Path) == nullptr)
		{
			UE_LOG("Failed to load static mesh from Resource.ini: %s", Resource.Path.c_str());
		}
	}
}

#pragma endregion


namespace ResourceKey
{
	constexpr const char* Font = "Font";
	constexpr const char* Particle = "Particle";
	constexpr const char* Material = "Material";
	constexpr const char* StaticMesh = "StaticMesh";
	constexpr const char* Path = "Path";
	constexpr const char* Columns = "Columns";
	constexpr const char* Rows = "Rows";
	constexpr const char* Preload = "Preload";
	constexpr const char* NormalizeToUnitCube = "NormalizeToUnitCube";
	constexpr const char* Type = "Type";
}

//	RootPath 하위에 있는 모든 사용 가능 Asset에 대하여 초기화 및 재추적하는 함수
void FResourceManager::LoadFromAssetDirectory(const FString& Path)
{
	//	초기화
	ObjFilePaths.clear();
	FontFilePaths.clear();
	TextureFilePaths.clear();
	MaterialFilePaths.clear();
	ParticleFilePaths.clear();
	StaticMeshRegistry.clear();

	InitializeDefaultResources(CachedDevice.Get());

	namespace fs = std::filesystem;
	
	const fs::path RootPath = fs::path(FPaths::RootDir()) / FPaths::ToWide(Path);
	
	const fs::path ProjectRootPath = fs::path(FPaths::RootDir());

	if (!fs::exists(RootPath) || !fs::is_directory(RootPath))
	{
		UE_LOG("[ResourceManager] Fatal Error : Root Directory Error");
		return;
	}

	TArray<FString> MaterialFiles;
	TArray<FString> MaterialInstanceFiles;

	for (const auto& Entry : fs::recursive_directory_iterator(RootPath))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		const fs::path& FilePath = Entry.path();
		const std::wstring Extension = FilePath.extension().wstring();

		if (Extension == L".meta")
		{
			continue;
		}
		
		const FString RelativePath = FPaths::ToString(fs::relative(FilePath, ProjectRootPath));

		if (Extension == L".obj")
		{
			ObjFilePaths.push_back(RelativePath);

			FStaticMeshResource Resource;
			Resource.Name = RelativePath;
			Resource.Path = RelativePath;
			Resource.bPreload = false;
			Resource.bNormalizeToUnitCube = false;
			StaticMeshRegistry[Resource.Name] = Resource;
		}
		else if (Extension == L".mtl")
		{
			// TODO: 현재는 임의로 static mesh shader로 로드하도록 설정
			MaterialFilePaths.push_back(RelativePath);
			LoadMaterial(RelativePath, "Shaders/ShaderStaticMesh.hlsl", CachedDevice.Get());
		}
		else if (Extension == L".mat")
		{
			MaterialFilePaths.push_back(RelativePath);
			MaterialFiles.push_back(RelativePath);
		}
		else if (Extension == L".matinst")
		{
			MaterialInstanceFiles.push_back(RelativePath);
		}
		else if (	Extension == L".png" ||	Extension == L".dds" ||	Extension == L".jpg" ||	Extension == L".jpeg")
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
				LoadTexture(RelativePath, CachedDevice.Get());
			}
			//else
			//{
			//	TextureFilePaths.push_back(RelativePath);
			//}
		}
	}

	for (const FString& MaterialPath : MaterialFiles)
	{
		DeserializeMaterial(MaterialPath);
	}

	for (const FString& MaterialInstancePath : MaterialInstanceFiles)
	{
		DeserializeMaterial(MaterialInstancePath);
	}

	//	TODO : Material, Texture Load

	InitializeDefaultResources(CachedDevice.Get());

	PreloadStaticMeshes();

	if (LoadGPUResources(CachedDevice.Get()))
	{
		UE_LOG("Complete Load Resources!");
	}
	else
	{
		UE_LOG("Failed to Load Resources...");
	}
}

void FResourceManager::RefreshFromAssetDirectory(const FString& Path)
{
	namespace fs = std::filesystem;

	ObjFilePaths.clear();
	FontFilePaths.clear();
	TextureFilePaths.clear();
	MaterialFilePaths.clear();
	ParticleFilePaths.clear();
	StaticMeshRegistry.clear();
	FontResources.clear();
	ParticleResources.clear();

	const fs::path RootPath = fs::path(FPaths::RootDir()) / FPaths::ToWide(Path);
	const fs::path ProjectRootPath = fs::path(FPaths::RootDir());

	if (!fs::exists(RootPath) || !fs::is_directory(RootPath))
	{
		UE_LOG("[ResourceManager] Refresh Failed : Root Directory Error");
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

			const fs::path& FilePath = Entry.path();
			const std::wstring Extension = FilePath.extension().wstring();

			if (Extension == L".meta" || Extension == L".bin")
			{
				continue;
			}

			const FString RelativePath = FPaths::ToString(fs::relative(FilePath, ProjectRootPath));

			if (Extension == L".obj")
			{
				ObjFilePaths.push_back(RelativePath);

				FStaticMeshResource Resource;
				Resource.Name = RelativePath;
				Resource.Path = RelativePath;
				Resource.bPreload = false;
				Resource.bNormalizeToUnitCube = false;
				StaticMeshRegistry[Resource.Name] = Resource;
			}
			else if (Extension == L".mtl")
			{
				MaterialFilePaths.push_back(RelativePath);
				LoadMaterial(RelativePath, "Shaders/ShaderStaticMesh.hlsl", CachedDevice.Get());
			}
			else if (Extension == L".mat")
			{
				MaterialFilePaths.push_back(RelativePath);
				DeserializeMaterial(RelativePath);
			}
			else if (
				Extension == L".png" ||
				Extension == L".dds" ||
				Extension == L".jpg" ||
				Extension == L".jpeg")
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
					LoadTexture(RelativePath, CachedDevice.Get());
				}
				//else
				//{
				//	TextureFilePaths.push_back(RelativePath);
				//}
			}
		}
	}
	catch (const std::exception& Ex)
	{
		UE_LOG("[ResourceManager] Refresh Exception: %s", Ex.what());
	}

	if (CachedDevice && !LoadGPUResources(CachedDevice.Get()))
	{
		UE_LOG("[ResourceManager] Refresh Failed : GPU Resource Reload Error");
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

	// 빈 디렉토리 정리
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
	namespace fs = std::filesystem;
	using namespace json;

	FTextureAssetMeta Meta;

	fs::path MetaPath = FilePath;
	MetaPath.replace_extension(L".meta");

	// 1. meta 파일 존재 → 로드
	if (fs::exists(MetaPath))
	{
		std::ifstream MetaFile(MetaPath);
		if (MetaFile.is_open())
		{
			FString Content((std::istreambuf_iterator<char>(MetaFile)),
			                std::istreambuf_iterator<char>());

			JSON Root = JSON::Load(Content);
			if (Root.JSONType() == JSON::Class::Object)
			{
				if (Root.hasKey(ResourceKey::Type))
				{
					const FString TypeStr = Root[ResourceKey::Type].ToString();
					if (TypeStr == "Font")
					{
						Meta.Type = EAssetMetaType::Font;
					}
					else if (TypeStr == "Particle")
					{
						Meta.Type = EAssetMetaType::Particle;
					}
					else if (TypeStr == "Texture")
					{
						Meta.Type = EAssetMetaType::Texture;
					}
				}

				if (Root.hasKey(ResourceKey::Columns))
				{
					Meta.Columns = std::max(1, static_cast<int32>(Root[ResourceKey::Columns].ToInt()));
				}

				if (Root.hasKey(ResourceKey::Rows))
				{
					Meta.Rows = std::max(1, static_cast<int32>(Root[ResourceKey::Rows].ToInt()));
				}
			}
		}

		return Meta;
	}

	// 2. 없으면 기본 생성
	const std::wstring ParentDir = FilePath.parent_path().filename().wstring();

	if (ParentDir == L"Font")
	{
		Meta.Type = EAssetMetaType::Font;
	}
	else if (ParentDir == L"Particle")
	{
		Meta.Type = EAssetMetaType::Particle;
	}
	else if (ParentDir == L"Texture")
	{
		Meta.Type = EAssetMetaType::Texture;
	}
	else
	{
		Meta.Type = EAssetMetaType::None;
	}

	Meta.Columns = 1;
	Meta.Rows = 1;

	// 3. meta 파일 생성
	JSON Root = JSON::Make(JSON::Class::Object);
	
	if (Meta.Type == EAssetMetaType::Font)
	{
		Root[ResourceKey::Type] = "Font";
	}
	else if (Meta.Type == EAssetMetaType::Particle)
	{
		Root[ResourceKey::Type] = "Particle";
	}
	else if (Meta.Type == EAssetMetaType::Texture)
	{
		Root[ResourceKey::Type] = "Texture";
	}
	else
	{
		Root[ResourceKey::Type] = "None";
	}
	
	Root[ResourceKey::Columns] = Meta.Columns;
	Root[ResourceKey::Rows] = Meta.Rows;

	std::ofstream OutFile(MetaPath);
	if (OutFile.is_open())
	{
		OutFile << Root.dump();
	}

	return Meta;
}

bool FResourceManager::LoadGPUResources(ID3D11Device* Device)
{
	if (!Device)
	{
		return false;
	}

	for (auto& [Key, Resource] : FontResources)
	{
		if (Resource.Texture != nullptr && Resource.Texture->GetSRV() != nullptr)
		{
			continue;
		}

		if (!FontLoader.Load(Resource.Name, Resource.Path, Resource.Columns, Resource.Rows, Device, Resource))
		{
			UE_LOG("Failed to load Font atlas: %s", Resource.Path.c_str());
			return false;
		}
	}

	for (auto& [Key, Resource] : ParticleResources)
	{
		if (Resource.Texture != nullptr && Resource.Texture->GetSRV() != nullptr)
		{
			continue;
		}

		if (!ParticleLoader.Load(Resource.Name, Resource.Path, Resource.Columns, Resource.Rows, Device, Resource))
		{
			UE_LOG("Failed to load Particle atlas: %s", Resource.Path.c_str());
			return false;
		}
	}

	return true;
}

void FResourceManager::InitializeDefaultResources(ID3D11Device* Device)
{
	if (!Device) return;

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

	UTexture* DefaultTexture = UObjectManager::Get().CreateObject<UTexture>();

	Device->CreateTexture2D(&Desc, &InitData, DefaultWhiteTexture.ReleaseAndGetAddressOf());
	if (DefaultWhiteTexture)
	{
		Device->CreateShaderResourceView(DefaultWhiteTexture.Get(), nullptr, DefaultTexture->GetAddressOfSRV());
		Textures["DefaultWhite"] = DefaultTexture;
	}

	UMaterial* DefaultMat = GetOrCreateMaterial("DefaultWhite", "Shaders/ShaderStaticMesh.hlsl");
	DefaultMat->MaterialParams["AmbientColor"] = FMaterialParamValue(DefaultMat->MaterialData.AmbientColor);
	DefaultMat->MaterialParams["DiffuseColor"] = FMaterialParamValue(DefaultMat->MaterialData.DiffuseColor);
	DefaultMat->MaterialParams["SpecularColor"] = FMaterialParamValue(DefaultMat->MaterialData.SpecularColor);
	DefaultMat->MaterialParams["EmissiveColor"] = FMaterialParamValue(DefaultMat->MaterialData.EmissiveColor);
	DefaultMat->MaterialParams["Shininess"] = FMaterialParamValue(DefaultMat->MaterialData.Shininess);
	DefaultMat->MaterialParams["Opacity"] = FMaterialParamValue(DefaultMat->MaterialData.Opacity);

	UTexture* DefaultWhite = FResourceManager::Get().GetTexture("DefaultWhite");

	if (DefaultMat->MaterialData.bHasDiffuseTexture)
		DefaultMat->MaterialParams["DiffuseMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(DefaultMat->MaterialData.DiffuseTexPath, Device));
	else
		DefaultMat->MaterialParams["DiffuseMap"] = FMaterialParamValue(DefaultWhite);

	if (DefaultMat->MaterialData.bHasAmbientTexture)
		DefaultMat->MaterialParams["AmbientMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(DefaultMat->MaterialData.AmbientTexPath, Device));
	else
		DefaultMat->MaterialParams["AmbientMap"] = FMaterialParamValue(DefaultWhite);

	if (DefaultMat->MaterialData.bHasSpecularTexture)
		DefaultMat->MaterialParams["SpecularMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(DefaultMat->MaterialData.SpecularTexPath, Device));
	else
		DefaultMat->MaterialParams["SpecularMap"] = FMaterialParamValue(DefaultWhite);

	if (DefaultMat->MaterialData.bHasBumpTexture)
		DefaultMat->MaterialParams["BumpMap"] = FMaterialParamValue(FResourceManager::Get().LoadTexture(DefaultMat->MaterialData.BumpTexPath, Device));
	else
		DefaultMat->MaterialParams["BumpMap"] = FMaterialParamValue(DefaultWhite);

	DefaultMat->MaterialParams["bHasDiffuseMap"] = FMaterialParamValue(DefaultMat->MaterialData.bHasDiffuseTexture);
	DefaultMat->MaterialParams["bHasSpecularMap"] = FMaterialParamValue(DefaultMat->MaterialData.bHasSpecularTexture);
	DefaultMat->MaterialParams["bHasAmbientMap"] = FMaterialParamValue(DefaultMat->MaterialData.bHasAmbientTexture);
	DefaultMat->MaterialParams["bHasBumpMap"] = FMaterialParamValue(DefaultMat->MaterialData.bHasBumpTexture);

	DefaultMat->MaterialParams["ScrollUV"] = FMaterialParamValue(FVector2(0.0f, 0.0f));
	
	// Outline Material
	UMaterial* OutlineMat = GetOrCreateMaterial("OutlineMaterial", "Shaders/OutlinePostProcess.hlsl");
	OutlineMat->SetParam("OutlineColor", FMaterialParamValue(FVector4(1.0f, 0.5f, 0.0f, 1.0f)));
	OutlineMat->SetParam("OutlineThicknessPixels", FMaterialParamValue(5.0f));
	OutlineMat->SetParam("OutlineViewportSize", FMaterialParamValue(FVector2(800.0f, 600.0f)));
    OutlineMat->SetParam("OutlineViewportOrigin", FMaterialParamValue(FVector2(0.0f, 0.0f)));
}

void FResourceManager::ReleaseGPUResources()
{
	for (auto& [Key, Texture] : Textures)
	{
		if (Texture)
		{
			UObjectManager::Get().DestroyObject(Texture);
		}
	}
	Textures.clear();

	for (auto& [Key, Material] : Materials)
	{
		if (Material)
		{
			UObjectManager::Get().DestroyObject(Material);
		}
	}
	Materials.clear();

	for (auto& [Key, MaterialInst] : MaterialInstances)
	{
		if (MaterialInst)
		{
			UObjectManager::Get().DestroyObject(MaterialInst);
		}
	}
	MaterialInstances.clear();

	for (auto& [Key, Shader] : Shaders)
	{
		if (Shader)
		{
			UObjectManager::Get().DestroyObject(Shader);
		}
	}
	Shaders.clear();

	for (auto& [Key, Font] : FontResources)
	{
		if (Font.Texture)
		{
			UObjectManager::Get().DestroyObject(Font.Texture);
		}
	}
	FontResources.clear();

	for (auto& [Key, Particle] : ParticleResources)
	{
		if (Particle.Texture)
		{
			UObjectManager::Get().DestroyObject(Particle.Texture);
		}
	}
	ParticleResources.clear();

	for (auto& [Path, StaticMeshAsset] : StaticMeshes)
	{
		UObjectManager::Get().DestroyObject(StaticMeshAsset);
	}
	StaticMeshes.clear();
	StaticMeshRegistry.clear();

	// D3D state object caches
	SamplerStates.clear();
	DepthStencilStates.clear();
	BlendStates.clear();
	RasterizerStates.clear();

	DefaultWhiteTexture.Reset();
	CachedDevice.Reset();
}

bool FResourceManager::LoadShader(const FString& FilePath, const FString& VSEntryPoint, const FString& PSEntryPoint,
                                  const D3D11_INPUT_ELEMENT_DESC* InputElements, UINT InputElementCount, const D3D_SHADER_MACRO* Defines)
{
	UShader* Shader = UObjectManager::Get().CreateObject<UShader>();
	Shader->FilePath = FilePath;

	TComPtr<ID3DBlob> VSBlob;
	TComPtr<ID3DBlob> PSBlob;
	TComPtr<ID3DBlob> ErrorBlob;

	HRESULT hr = D3DCompileFromFile(FPaths::ToWide(FilePath).c_str(), Defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		VSEntryPoint.c_str(), "vs_5_0", 0, 0, &VSBlob, &ErrorBlob);
	if (FAILED(hr))
	{
		if (ErrorBlob)
		{
			UE_LOG("Vertex Shader Compile Error (%s): %s", FilePath.c_str(), static_cast<const char*>(ErrorBlob->GetBufferPointer()));
		}
		else
		{
			UE_LOG("Failed to compile vertex shader: %s", FilePath.c_str());
		}
		return false;
	}
	Shader->ReflectShader(VSBlob.Get(), CachedDevice.Get());
	ErrorBlob.Reset();

	hr = D3DCompileFromFile(FPaths::ToWide(FilePath).c_str(), Defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		PSEntryPoint.c_str(), "ps_5_0", 0, 0, &PSBlob, &ErrorBlob);
	if (FAILED(hr))
	{
		if (ErrorBlob)
		{
			UE_LOG("Pixel Shader Compile Error (%s): %s", FilePath.c_str(), static_cast<const char*>(ErrorBlob->GetBufferPointer()));
		}
		else
		{
			UE_LOG("Failed to compile pixel shader: %s", FilePath.c_str());
		}
		return false;
	}
	Shader->ReflectShader(PSBlob.Get(), CachedDevice.Get());

	hr = CachedDevice->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr,
		&Shader->ShaderData.VS);
	if (FAILED(hr))
	{
		UE_LOG("Failed to create vertex shader: %s", FilePath.c_str());
		return false;
	}

	hr = CachedDevice->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr,
		&Shader->ShaderData.PS);
	if (FAILED(hr))
	{
		UE_LOG("Failed to create pixel shader: %s", FilePath.c_str());
		return false;
	}

	if (InputElements != nullptr && InputElementCount > 0)
	{
		hr = CachedDevice->CreateInputLayout(InputElements, InputElementCount, VSBlob->GetBufferPointer(),
			VSBlob->GetBufferSize(), &Shader->ShaderData.InputLayout);
		if (FAILED(hr))
		{
			UE_LOG("Failed to create input layout: %s", FilePath.c_str());
			return false;
		}
	}

	Shaders[FilePath] = Shader;

	return true;
}

//ID3DBlob* CompileShaderWithDefines(const WCHAR* filename,
//                                   const D3D_SHADER_MACRO* defines,
//                                   const char* entryPoint,
//                                   const char* shaderModel)
//{
//    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
//
//#if defined(DEBUG) || defined(_DEBUG)
//    flags |= D3DCOMPILE_DEBUG;
//#endif
//
//    ID3DBlob* byteCode = nullptr;
//    ID3DBlob* errors = nullptr;
//
//    HRESULT hr = D3DCompileFromFile(
//        filename,
//        defines,
//        D3D_COMPILE_STANDARD_FILE_INCLUDE,
//        entryPoint,
//        shaderModel,
//        flags,
//        0,
//        &byteCode,
//        &errors);
//
//    if (FAILED(hr))
//    {
//        if (errors)
//        {
//            MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Shader Compile Error", MB_OK);
//            errors->Release();
//        }
//        return nullptr;
//    }
//
//    return byteCode;
//}

UShader* FResourceManager::GetShader(const FString& FilePath) const
{
	auto It = Shaders.find(FilePath);
	return (It != Shaders.end()) ? It->second : nullptr;
}

TArray<FString> FResourceManager::GetMaterialNames() const
{
	TArray<FString> Names;
	Names.reserve(Materials.size());
	for (const auto& [Name, Mat] : Materials)
	{
		Names.push_back(Name);
	}
	return Names;
}

UMaterial* FResourceManager::GetMaterial(const FString& MaterialName) const
{
	auto It = Materials.find(MaterialName);
	return (It != Materials.end()) ? It->second : nullptr;
}

// 매개변수 없이 가장 간단한 Material을 생성
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

	Materials[Path] = Material;

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

	Materials[Name] = Material;

	return Material;
}

bool FResourceManager::LoadMaterial(const FString& MtlFilePath, const FString& ShaderName, ID3D11Device* Device)
{
	if (MtlFilePath.empty())
	{
		return false;
	}

	UShader* Shader = FResourceManager::Get().GetShader(ShaderName);
	if (!Shader)
	{
		UE_LOG("Shader not found for material: %s, using default shader", ShaderName.c_str());
		return false;
	}

	TMap<FString, UMaterial*> Parsed;
	if (!FObjMtlLoader::Load(MtlFilePath, Parsed, CachedDevice.Get()))
	{
		UE_LOG("Failed to load MTL: %s", MtlFilePath.c_str());
		return false;
	}

	if (Parsed.empty())
	{
		Parsed["DefaultWhite"] = GetMaterial("DefaultWhite");
	}

	for (auto& [Name, Mat] : Parsed)
	{
		Mat->FilePath = MtlFilePath;
		Mat->SetShader(Shader);

		if (Materials.find(Name) != Materials.end())
		{
			UE_LOG("Warning: Material with name '%s' already exists. Overwriting.", Name.c_str());
		}
		else
		{
			Materials[Name] = Mat;
		}
	}

	// TODO: 개선 필요해보임
	for (auto& [Name, Mat] : Parsed)
	{
		FMaterial& MaterialData = Mat->MaterialData;

		if (MaterialData.bHasDiffuseTexture && !MaterialData.DiffuseTexPath.empty())  LoadTexture(MaterialData.DiffuseTexPath, CachedDevice.Get());
		if (MaterialData.bHasAmbientTexture && !MaterialData.AmbientTexPath.empty())  LoadTexture(MaterialData.AmbientTexPath, CachedDevice.Get());
		if (MaterialData.bHasSpecularTexture && !MaterialData.SpecularTexPath.empty()) LoadTexture(MaterialData.SpecularTexPath, CachedDevice.Get());
		if (MaterialData.bHasBumpTexture && !MaterialData.BumpTexPath.empty())     LoadTexture(MaterialData.BumpTexPath, CachedDevice.Get());
	}

	UE_LOG("Loaded MTL: %s", MtlFilePath.c_str());
	return true;
}

UMaterialInstance* FResourceManager::CreateMaterialInstance(const FString& Path, UMaterial* Parent)
{
	UMaterialInstance* Instance = UObjectManager::Get().CreateObject<UMaterialInstance>();
	Instance->Parent = Parent;
	Instance->Name = Path;
	Instance->FilePath = Path;
	MaterialInstances[Path] = Instance;
	return Instance;
}

UMaterialInstance* FResourceManager::GetMaterialInstance(const FString& Path) const
{
	auto It = MaterialInstances.find(Path);
	return (It != MaterialInstances.end()) ? It->second : nullptr;
}

UMaterialInterface* FResourceManager::GetMaterialInterface(const FString& Name) const
{
	UMaterial* Mat = GetMaterial(Name);
	if (Mat)
	{
		return Mat;
	}
	return GetMaterialInstance(Name);
}

bool FResourceManager::SerializeMaterial(const FString& MatFilePath, const UMaterial* Material)
{
	using json::JSON;
	JSON Root = JSON::Make(JSON::Class::Object);
	Root["Name"] = Material->Name;
	Root["Shader"] = Material->Shader ? Material->Shader->FilePath : "";

	JSON Params = JSON::Make(JSON::Class::Array);
	for (const auto& [ParamName, ParamValue] : Material->MaterialParams)
	{
		JSON Param = JSON::Make(JSON::Class::Object);
		Param["Name"] = ParamName;
		if (std::holds_alternative<bool>(ParamValue.Value))
		{
			Param["Type"] = "Bool";
			Param["Value"] = std::get<bool>(ParamValue.Value);
		}
		else if (std::holds_alternative<int>(ParamValue.Value))
		{
			Param["Type"] = "Int";
			Param["Value"] = std::get<int>(ParamValue.Value);
		}
		else if (std::holds_alternative<uint32>(ParamValue.Value))
		{
			Param["Type"] = "UInt";
			Param["Value"] = std::get<uint32>(ParamValue.Value);
		}
		else if (std::holds_alternative<float>(ParamValue.Value))
		{
			Param["Type"] = "Float";
			Param["Value"] = std::get<float>(ParamValue.Value);
		}
		else if (std::holds_alternative<FVector2>(ParamValue.Value))
		{
			const FVector2& Vec = std::get<FVector2>(ParamValue.Value);
			Param["Type"] = "Vector2";
			Param["Value"] = {Vec.X, Vec.Y};
		}
		else if (std::holds_alternative<FVector>(ParamValue.Value))
		{
			const FVector& Vec = std::get<FVector>(ParamValue.Value);
			Param["Type"] = "Vector3";
			Param["Value"] = {Vec.X, Vec.Y, Vec.Z};
		}
		else if (std::holds_alternative<FVector4>(ParamValue.Value))
		{
			const FVector4& Vec = std::get<FVector4>(ParamValue.Value);
			Param["Type"] = "Vector4";
			Param["Value"] = { Vec.X, Vec.Y, Vec.Z, Vec.W };
		}
		else if (std::holds_alternative<FMatrix>(ParamValue.Value))
		{
			const FMatrix& Mat = std::get<FMatrix>(ParamValue.Value);
			Param["Type"] = "Matrix4";
			JSON MatArray = JSON::Make(JSON::Class::Array);
			for (int Row = 0; Row < 4; ++Row)
			{
				JSON RowArray = JSON::Make(JSON::Class::Array);
				for (int Col = 0; Col < 4; ++Col)
				{
					RowArray.append(Mat.M[Row][Col]);
				}
				MatArray.append(RowArray);
			}
			Param["Value"] = MatArray;
		}
		else if (std::holds_alternative<UTexture*>(ParamValue.Value))
		{
			UTexture* Texture = std::get<UTexture*>(ParamValue.Value);
			Param["Type"] = "Texture";
			Param["Value"] = Texture ? Texture->GetFilePath() : "";
		}
		Params.append(Param);
	}
	Root["Params"] = Params;
	std::ofstream OutFile(FPaths::ToWide(MatFilePath));
	if (!OutFile.is_open())
	{
		UE_LOG("Failed to open material file for writing: %s", MatFilePath.c_str());
		return false;
	}
	OutFile << Root.dump(4);
	return true;
}

bool FResourceManager::SerializeMaterialInstance(const FString& MatInstFilePath, const UMaterialInstance* MaterialInstance)
{
	using json::JSON;
	JSON Root = JSON::Make(JSON::Class::Object);
	Root["Name"] = MaterialInstance->GetName();
	Root["Parent"] = MaterialInstance->Parent->Name;
	JSON Params = JSON::Make(JSON::Class::Array);
	for (const auto& [ParamName, ParamValue] : MaterialInstance->OverridedParams)
	{
		JSON Param = JSON::Make(JSON::Class::Object);
		Param["Name"] = ParamName;
		if (std::holds_alternative<bool>(ParamValue.Value))
		{
			Param["Type"] = "Bool";
			Param["Value"] = std::get<bool>(ParamValue.Value);
		}
		else if (std::holds_alternative<int>(ParamValue.Value))
		{
			Param["Type"] = "Int";
			Param["Value"] = std::get<int>(ParamValue.Value);
		}
		else if (std::holds_alternative<uint32>(ParamValue.Value))
		{
			Param["Type"] = "UInt";
			Param["Value"] = std::get<uint32>(ParamValue.Value);
		}
		else if (std::holds_alternative<float>(ParamValue.Value))
		{
			Param["Type"] = "Float";
			Param["Value"] = std::get<float>(ParamValue.Value);
		}
		else if (std::holds_alternative<FVector2>(ParamValue.Value))
		{
			const FVector2& Vec = std::get<FVector2>(ParamValue.Value);
			Param["Type"] = "Vector2";
			Param["Value"] = JSON::Make(JSON::Class::Array);
			Param["Value"].append(Vec.X);
			Param["Value"].append(Vec.Y);
		}
		else if (std::holds_alternative<FVector>(ParamValue.Value))
		{
			const FVector& Vec = std::get<FVector>(ParamValue.Value);
			Param["Type"] = "Vector3";
			Param["Value"] = JSON::Make(JSON::Class::Array);
			Param["Value"].append(Vec.X);
			Param["Value"].append(Vec.Y);
			Param["Value"].append(Vec.Z);
		}
		else if (std::holds_alternative<FVector4>(ParamValue.Value))
		{
			const FVector4& Vec = std::get<FVector4>(ParamValue.Value);
			Param["Type"] = "Vector4";
			Param["Value"] = JSON::Make(JSON::Class::Array);
			Param["Value"].append(Vec.X);
			Param["Value"].append(Vec.Y);
			Param["Value"].append(Vec.Z);
			Param["Value"].append(Vec.W);
		}
		else if (std::holds_alternative<FMatrix>(ParamValue.Value))
		{
			const FMatrix& Mat = std::get<FMatrix>(ParamValue.Value);
			Param["Type"] = "Matrix4";
			JSON MatArray = JSON::Make(JSON::Class::Array);
			for (int Row = 0; Row < 4; ++Row)
			{
				JSON RowArray = JSON::Make(JSON::Class::Array);
				for (int Col = 0; Col < 4; ++Col)
				{
					RowArray.append(Mat.M[Row][Col]);
				}
				MatArray.append(RowArray);
			}
			Param["Value"] = MatArray;
		}
		else if (std::holds_alternative<UTexture*>(ParamValue.Value))
		{
			UTexture* Texture = std::get<UTexture*>(ParamValue.Value);
			Param["Type"] = "Texture";
			Param["Value"] = Texture ? Texture->GetFilePath() : "";
		}
		Params.append(Param);
	}
	Root["OverridedParams"] = Params;
	std::ofstream OutFile(FPaths::ToWide(MatInstFilePath));
	if (!OutFile.is_open())
	{
		UE_LOG("Failed to open material instance file for writing: %s", MatInstFilePath.c_str());
		return false;
	}
	OutFile << Root.dump(4);
	return true;
}

bool FResourceManager::DeserializeMaterial(const FString& MatFilePath)
{
	using json::JSON;

	std::ifstream MatFile(FPaths::ToWide(MatFilePath));
	if (!MatFile.is_open())
	{
		UE_LOG("Failed to open material file: %s", MatFilePath.c_str());
		return false;
	}

	FString FileContent((std::istreambuf_iterator<char>(MatFile)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(FileContent);

	if (Root.hasKey("Parent"))
	{
		FString MatName = Root["Name"].ToString();
		UMaterial* ParentMat = GetMaterial(FPaths::Normalize(Root["Parent"].ToString()));

		if (!ParentMat)
		{
			UE_LOG("Parent material not found: %s", Root["Parent"].ToString().c_str());
			return false;
		}

		UMaterialInstance* MatInstance = CreateMaterialInstance(MatName, ParentMat);

		for (auto& Param : Root["OverridedParams"].ArrayRange())
		{
			FString ParamName = Param["Name"].ToString();
			FString Type = Param["Type"].ToString();
			if (Type == "Bool")
			{
				bool Value = Param["Value"].ToBool();
				MatInstance->SetParam(ParamName, FMaterialParamValue(Value));
			}
			else if (Type == "Int")
			{
				int Value = Param["Value"].ToInt();
				MatInstance->SetParam(ParamName, FMaterialParamValue(Value));
			}
			else if (Type == "UInt")
			{
				uint32 Value = static_cast<uint32>(Param["Value"].ToInt());
				MatInstance->SetParam(ParamName, FMaterialParamValue(Value));
			}
			else if (Type == "Float")
			{
				float Value = static_cast<float>(Param["Value"].ToFloat());
				MatInstance->SetParam(ParamName, FMaterialParamValue(Value));
			}
			else if (Type == "Vector2")
			{
				FVector2 Value(
					static_cast<float>(Param["Value"][0].ToFloat()),
					static_cast<float>(Param["Value"][1].ToFloat()));
				MatInstance->SetParam(ParamName, FMaterialParamValue(Value));
			}
			else if (Type == "Vector3")
			{
				FVector Value(
					static_cast<float>(Param["Value"][0].ToFloat()),
					static_cast<float>(Param["Value"][1].ToFloat()),
					static_cast<float>(Param["Value"][2].ToFloat()));
				MatInstance->SetParam(ParamName, FMaterialParamValue(Value));
			}
			else if (Type == "Vector4")
			{
				FVector4 Value(
					static_cast<float>(Param["Value"][0].ToFloat()),
					static_cast<float>(Param["Value"][1].ToFloat()),
					static_cast<float>(Param["Value"][2].ToFloat()),
					static_cast<float>(Param["Value"][3].ToFloat()));
				MatInstance->SetParam(ParamName, FMaterialParamValue(Value));
			}
			else if (Type == "Matrix4")
			{
				FMatrix Value;
				for (int Row = 0; Row < 4; ++Row)
				{
					for (int Col = 0; Col < 4; ++Col)
					{
						Value.M[Row][Col] = static_cast<float>(Param["Value"][Row][Col].ToFloat());
					}
				}
				MatInstance->SetParam(ParamName, FMaterialParamValue(Value));
			}
			else if (Type == "Texture")
			{
				FString TexPath = Param["Value"].ToString();
				UTexture* Texture = LoadTexture(TexPath, CachedDevice.Get());
				if (Texture)
				{
					MatInstance->SetParam(ParamName, FMaterialParamValue(Texture));
				}
			}
		}

		MaterialInstances[MatName] = MatInstance;
		return true;
	}

	FString MatName = Root["Name"].ToString();
	FString ShaderPath = Root["Shader"].ToString();
	UMaterial* Material = GetOrCreateMaterial(MatName, MatFilePath, ShaderPath);

	for (auto& Param : Root["Params"].ArrayRange())
	{
		FString ParamName = Param["Name"].ToString();
		FString Type = Param["Type"].ToString();

		if (Type == "Bool")
		{
			bool Value = Param["Value"].ToBool();
			Material->SetParam(ParamName, FMaterialParamValue(Value));
		}
		else if (Type == "Int")
		{
			int Value = Param["Value"].ToInt();
			Material->SetParam(ParamName, FMaterialParamValue(Value));
		}
		else if (Type == "UInt")
		{
			uint32 Value = static_cast<uint32>(Param["Value"].ToInt());
			Material->SetParam(ParamName, FMaterialParamValue(Value));
		}
		else if (Type == "Float")
		{
			float Value = static_cast<float>(Param["Value"].ToFloat());
			Material->SetParam(ParamName, FMaterialParamValue(Value));
		}
		else if (Type == "Vector2")
		{
			FVector2 Value(
				static_cast<float>(Param["Value"][0].ToFloat()),
				static_cast<float>(Param["Value"][1].ToFloat()));
			Material->SetParam(ParamName, FMaterialParamValue(Value));
		}
		else if (Type == "Vector3")
		{
			FVector Value(
				static_cast<float>(Param["Value"][0].ToFloat()),
				static_cast<float>(Param["Value"][1].ToFloat()),
				static_cast<float>(Param["Value"][2].ToFloat()));
			Material->SetParam(ParamName, FMaterialParamValue(Value));
		}
		else if (Type == "Vector4")
		{
			FVector4 Value(
				static_cast<float>(Param["Value"][0].ToFloat()),
				static_cast<float>(Param["Value"][1].ToFloat()),
				static_cast<float>(Param["Value"][2].ToFloat()),
				static_cast<float>(Param["Value"][3].ToFloat()));
			Material->SetParam(ParamName, FMaterialParamValue(Value));
		}
		else if (Type == "Matrix4")
		{
			FMatrix Value;
			for (int Row = 0; Row < 4; ++Row)
			{
				for (int Col = 0; Col < 4; ++Col)
				{
					Value.M[Row][Col] = static_cast<float>(Param["Value"][Row][Col].ToFloat());
				}
			}
			Material->SetParam(ParamName, FMaterialParamValue(Value));
		}
		else if (Type == "Texture")
		{
			FString TexPath = Param["Value"].ToString();
			UTexture* Texture = LoadTexture(TexPath, CachedDevice.Get());
			if (Texture)
			{
				Material->SetParam(ParamName, FMaterialParamValue(Texture));
			}
		}
	}

	Materials[MatName] = Material;

	return true;
}

UTexture* FResourceManager::GetTexture(const FString& Path) const
{
	auto It = Textures.find(Path);
	return (It != Textures.end()) ? It->second : nullptr;
}

UTexture* FResourceManager::LoadTexture(const FString& Path, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		Device = CachedDevice.Get();
	}

	if (UTexture* Cached = GetTexture(Path))
	{
		return Cached;
	}

	UTexture* Texture = UObjectManager::Get().CreateObject<UTexture>();
	if (!Texture->LoadFromFile(Path, Device))
	{
		return nullptr;
	}

	Textures[Path] = Texture;
	return Texture;
}

// --- Font ---
FFontResource* FResourceManager::FindFont(const FName& FontName)
{
	if (FontResources.empty())
	{
		return nullptr;
	}
	
	auto It = FontResources.find(FontName.ToString());
	return (It != FontResources.end()) ? &It->second : &FontResources.begin()->second;
}

const FFontResource* FResourceManager::FindFont(const FName& FontName) const
{
	if (FontResources.empty())
	{
		return nullptr;
	}
	
	//	Default인 경우 첫 Font Resource로 Fallback (반드시 하나 이상의 Font는 Upload
	auto It = FontResources.find(FontName.ToString());
	return (It != FontResources.end()) ? &It->second : &FontResources.begin()->second;
}

void FResourceManager::RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FFontResource Resource;
	Resource.Name = FontName;
	Resource.Path = InPath;
	Resource.Columns = Columns;
	Resource.Rows = Rows;
	Resource.Texture = UObjectManager::Get().CreateObject<UTexture>();
	FontResources[FontName.ToString()] = Resource;
}

// --- Particle ---
FParticleResource* FResourceManager::FindParticle(const FName& ParticleName)
{
	//	마찬가지로 하나 이상의 Particle을 무조건 가지고 있어야 함.
	if (ParticleResources.empty())
	{
		return nullptr;
	}
	
	auto It = ParticleResources.find(ParticleName.ToString());
	return (It != ParticleResources.end()) ? &It->second : &ParticleResources.begin()->second;
}

const FParticleResource* FResourceManager::FindParticle(const FName& ParticleName) const
{
	if (ParticleResources.empty())
	{
		return nullptr;
	}
	
	auto It = ParticleResources.find(ParticleName.ToString());
	return (It != ParticleResources.end()) ? &It->second : &ParticleResources.begin()->second;
}

void FResourceManager::RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FParticleResource Resource;
	Resource.Name = ParticleName;
	Resource.Path = InPath;
	Resource.Columns = Columns;
	Resource.Rows = Rows;
	Resource.Texture = UObjectManager::Get().CreateObject<UTexture>();
	ParticleResources[ParticleName.ToString()] = Resource;
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
	// 메모리 캐시 확인
	if (UStaticMesh* FoundMesh = FindStaticMesh(Path))
	{
		return FoundMesh;
	}

 	LoadMaterial(Path, "Shaders/ShaderStaticMesh.hlsl");

	FStaticMeshLoadOptions LoadOptions = {};

	for (const auto& [Key, Resource] : StaticMeshRegistry)
	{
		if (Resource.Path == Path)
		{
			LoadOptions.bNormalizeToUnitCube = Resource.bNormalizeToUnitCube;
			break;
		}
	}

	const FString BinaryPath = MakeStaticMeshBinaryPath(Path);

	FStaticMesh* LoadedMeshData = nullptr;
	double BinaryLoadSec = 0.0;
	double ObjLoadSec = 0.0;

	//	2. Binary Load 시도
	if (IsStaticMeshBinaryValid(Path, BinaryPath))
	{
		const auto BinaryStart = std::chrono::steady_clock::now();

		LoadedMeshData = new FStaticMesh();
		if (!BinarySerializer.LoadStaticMesh(BinaryPath, *LoadedMeshData))
		{
			delete LoadedMeshData;
			LoadedMeshData = nullptr;
		}

		const auto BinaryEnd = std::chrono::steady_clock::now();
		BinaryLoadSec = std::chrono::duration<double>(BinaryEnd - BinaryStart).count();
	}

	//	3. Binary 실패 시 OBJ Load
	if (LoadedMeshData == nullptr)
	{
		const auto ObjStart = std::chrono::steady_clock::now();
		LoadedMeshData = ObjLoader.Load(Path, LoadOptions);
		const auto ObjEnd = std::chrono::steady_clock::now();
		ObjLoadSec = std::chrono::duration<double>(ObjEnd - ObjStart).count();

		if (LoadedMeshData == nullptr)
		{
			UE_LOG("[StaticMeshLoad] Failed | Path=%s | BinarySec=%.6f | ObjSec=%.6f", Path.c_str(), BinaryLoadSec,
			       ObjLoadSec);
			return nullptr;
		}

		//	4. OBJ 로드 성공 시 Binary 저장
		const bool bSaveBinaryOk = BinarySerializer.SaveStaticMesh(BinaryPath, Path, *LoadedMeshData);
		UE_LOG(
			"[StaticMeshLoad] Source=OBJ | Path=%s | ObjSec=%.6f | BinarySave=%s | BinaryPath=%s",
			Path.c_str(),
			ObjLoadSec,
			bSaveBinaryOk ? "OK" : "FAIL",
			BinaryPath.c_str());
	}
	else
	{
		UE_LOG(
			"[StaticMeshLoad] Source=Binary | Path=%s | BinarySec=%.6f | BinaryPath=%s",
			Path.c_str(),
			BinaryLoadSec,
			BinaryPath.c_str());
	}

	// Material 연결
	for (FStaticMeshMaterialSlot& Slot : LoadedMeshData->Slots)
	{
		Slot.Material = GetMaterial(Slot.SlotName);

		if (Slot.Material == nullptr)
		{
			Slot.Material = GetMaterial("DefaultWhite");
		}
	}

    UStaticMesh* LoadedMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
    LoadedMesh->SetMeshData(LoadedMeshData);

    if (FEditorSettings::Get().ShowFlags.bEnableLOD)
    {
        const auto LodStart = std::chrono::steady_clock::now();
        FStaticMeshSimplifier::BuildLODs(LoadedMesh);
        const auto LodEnd = std::chrono::steady_clock::now();
        double LodSec = std::chrono::duration<double>(LodEnd - LodStart).count();
        UE_LOG("[StaticMeshLoad] Generated %d LODs for %s in %.3f sec",
               LoadedMesh->GetValidLODCount(), Path.c_str(), LodSec);
    }
    else
    {
        UE_LOG("[StaticMeshLoad] LOD generation skipped for %s (Enable LOD is off)", Path.c_str());
    }

    StaticMeshes.insert({Path, LoadedMesh});
    
    return LoadedMesh;
}

UStaticMesh* FResourceManager::FindStaticMesh(const FString& Path) const
{
	auto It = StaticMeshes.find(Path);
	if (It == StaticMeshes.end())
	{
		return nullptr;
	}

	return It->second;
}

TArray<FString> FResourceManager::GetStaticMeshPaths() const
{
	return ObjFilePaths;
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

	auto It = SamplerStates.find(Type);
	if (It != SamplerStates.end())
	{
		return It->second.Get();
	}

	D3D11_SAMPLER_DESC Desc = {};
	Desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	Desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	Desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	Desc.MinLOD = 0;
	Desc.MaxLOD = D3D11_FLOAT32_MAX;
	switch (Type)
	{
	case ESamplerType::EST_Point:
		Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		break;
	case ESamplerType::EST_Linear:
		Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		break;
	case ESamplerType::EST_Anisotropic:
		Desc.Filter = D3D11_FILTER_ANISOTROPIC;
		Desc.MaxAnisotropy = 16;
		break;
	default:
		Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		break;
	}

	TComPtr<ID3D11SamplerState> SamplerState;
	HRESULT hr = CachedDevice->CreateSamplerState(&Desc, &SamplerState);
	if (FAILED(hr))
	{
		UE_LOG("Failed to create sampler state");
		return nullptr;
	}

	SamplerStates[Type] = SamplerState;
	return SamplerState.Get();
}

ID3D11DepthStencilState* FResourceManager::GetOrCreateDepthStencilState(EDepthStencilType Type, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		Device = CachedDevice.Get();
	}
	auto It = DepthStencilStates.find(Type);
	if (It != DepthStencilStates.end())
	{
		return It->second.Get();
	}

	D3D11_DEPTH_STENCIL_DESC Desc = {};
	switch (Type)
	{
	case EDepthStencilType::Default:
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		Desc.DepthFunc = D3D11_COMPARISON_LESS;
		Desc.StencilEnable = FALSE;
		break;
	case EDepthStencilType::DepthReadOnly:
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		Desc.StencilEnable = FALSE;
		break;
	case EDepthStencilType::StencilWrite:
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		Desc.StencilEnable = TRUE;
		Desc.StencilWriteMask = 0xFF;
		Desc.StencilWriteMask = 0xFF;
		Desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		Desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.BackFace = Desc.FrontFace;
		break;
	case EDepthStencilType::GizmoInside:
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		Desc.StencilEnable = TRUE;
		Desc.StencilReadMask = 0xFF;
		Desc.StencilWriteMask = 0x00;
		Desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.BackFace = Desc.FrontFace;
		break;
	case EDepthStencilType::GizmoOutside:
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		Desc.StencilEnable = TRUE;
		Desc.StencilReadMask = 0xFF;
		Desc.StencilWriteMask = 0x00;
		Desc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
		Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.BackFace = Desc.FrontFace;
		break;
	}

	TComPtr<ID3D11DepthStencilState> DepthStencilState;
	HRESULT hr = CachedDevice->CreateDepthStencilState(&Desc, &DepthStencilState);
	if (FAILED(hr))
	{
		UE_LOG("Failed to create depth stencil state");
		return nullptr;
	}

	DepthStencilStates[Type] = DepthStencilState;
	return DepthStencilState.Get();
}

ID3D11BlendState* FResourceManager::GetOrCreateBlendState(EBlendType Type, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		Device = CachedDevice.Get();
	}
	auto It = BlendStates.find(Type);
	if (It != BlendStates.end())
	{
		return It->second.Get();
	}

	D3D11_BLEND_DESC Desc = {};
	switch (Type)
	{
	case EBlendType::Opaque:
		Desc.RenderTarget[0].BlendEnable = FALSE;
		Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		break;
	case EBlendType::AlphaBlend:
		Desc.RenderTarget[0].BlendEnable = TRUE;
		Desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		Desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		break;
	case EBlendType::NoColor:
		Desc.RenderTarget[0].BlendEnable = FALSE;
		Desc.RenderTarget[0].RenderTargetWriteMask = 0;
		break;
	}

	TComPtr<ID3D11BlendState> BlendState;
	HRESULT hr = CachedDevice->CreateBlendState(&Desc, &BlendState);
	if (FAILED(hr))
	{
		UE_LOG("Failed to create blend state");
		return nullptr;
	}

	BlendStates[Type] = BlendState;
	return BlendState.Get();
}

ID3D11RasterizerState* FResourceManager::GetOrCreateRasterizerState(ERasterizerType Type, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		Device = CachedDevice.Get();
	}
	auto It = RasterizerStates.find(Type);
	if (It != RasterizerStates.end())
	{
		return It->second.Get();
	}

	D3D11_RASTERIZER_DESC Desc = {};
	switch (Type)
	{
	case ERasterizerType::SolidBackCull:
		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.CullMode = D3D11_CULL_BACK;
		break;
	case ERasterizerType::SolidFrontCull:
		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.CullMode = D3D11_CULL_FRONT;
		break;
	case ERasterizerType::SolidNoCull:
		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.CullMode = D3D11_CULL_NONE;
		break;
	case ERasterizerType::WireFrame:
		Desc.FillMode = D3D11_FILL_WIREFRAME;
		Desc.CullMode = D3D11_CULL_BACK;
		break;
	}

	TComPtr<ID3D11RasterizerState> RasterizerState;
	HRESULT hr = CachedDevice->CreateRasterizerState(&Desc, &RasterizerState);
	if (FAILED(hr))
	{
		UE_LOG("Failed to create rasterizer state");
		return nullptr;
	}
	RasterizerStates[Type] = RasterizerState;
	return RasterizerState.Get();
}

// TODO: 변경된 구조에 맞춰서 수정하기
size_t FResourceManager::GetMaterialMemorySize() const
{
	size_t TotalSize = 0;

	TotalSize += Materials.size() * sizeof(UMaterial);

	for (const auto& Pair : Materials)
	{
		const FMaterial& Mat = Pair.second->MaterialData;
		TotalSize += Mat.Name.capacity();
		TotalSize += Mat.DiffuseTexPath.capacity();
		TotalSize += Mat.AmbientTexPath.capacity();
		TotalSize += Mat.SpecularTexPath.capacity();
		TotalSize += Mat.BumpTexPath.capacity();
	}

	return TotalSize;
}
