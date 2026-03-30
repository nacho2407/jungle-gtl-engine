#include "Core/ResourceManager.h"

#include "Core/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>

#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "UI/EditorConsoleWidget.h"
#include "Asset/BinarySerializer.h"
#include "Asset/StaticMeshTypes.h"

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
}

void FResourceManager::LoadFromFile(const FString& Path, ID3D11Device* InDevice)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)),
	                std::istreambuf_iterator<char>());

	InitializeDefaultResources(InDevice);

	JSON Root = JSON::Load(Content);

	// Font — { "Name": { "Path": "...", "Columns": 16, "Rows": 16 } }
	if (Root.hasKey(ResourceKey::Font))
	{
		JSON FontSection = Root[ResourceKey::Font];
		for (auto& Pair : FontSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FFontResource Resource;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Entry[ResourceKey::Path].ToString();
			Resource.Columns = static_cast<uint32>(Entry[ResourceKey::Columns].ToInt());
			Resource.Rows = static_cast<uint32>(Entry[ResourceKey::Rows].ToInt());
			Resource.SRV = nullptr;
			FontResources[Pair.first] = Resource;
		}
	}

	// StaticMesh — { "Name": { "Path": "Asset/Mesh/xxx.obj" } }
	// StaticMesh — { "Name": { "Path": "...", "Preload": true, "NormalizeToUnitCube": true } }
	if (Root.hasKey(ResourceKey::StaticMesh))
	{
		JSON StaticMeshSection = Root[ResourceKey::StaticMesh];
		for (auto& Pair : StaticMeshSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			if (!Entry.hasKey(ResourceKey::Path))
			{
				continue;
			}

			FStaticMeshResource Resource;
			Resource.Name = Pair.first;
			Resource.Path = Entry[ResourceKey::Path].ToString();
			Resource.bPreload = Entry.hasKey(ResourceKey::Preload)
				? Entry[ResourceKey::Preload].ToBool()
				: false;
			Resource.bNormalizeToUnitCube = Entry.hasKey(ResourceKey::NormalizeToUnitCube)
				? Entry[ResourceKey::NormalizeToUnitCube].ToBool()
				: false;

			StaticMeshRegistry[Pair.first] = Resource;
		}
	}

	// Material — { "Name": { "Path": "Asset/Material/xxx.mtl" } }
	if (Root.hasKey(ResourceKey::Material))
	{
		JSON MaterialSection = Root[ResourceKey::Material];
		for (auto& Pair : MaterialSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			if (!Entry.hasKey(ResourceKey::Path))
			{
				continue;
			}

			FString MtlPath = Entry[ResourceKey::Path].ToString();
			if (!LoadMaterial(MtlPath))
			{
				UE_LOG("Failed to load material from Resource.ini: %s", MtlPath.c_str());
			}
		}
	}

	// Particle — { "Name": { "Path": "...", "Columns": 6, "Rows": 6 } }
	if (Root.hasKey(ResourceKey::Particle))
	{
		JSON ParticleSection = Root[ResourceKey::Particle];
		for (auto& Pair : ParticleSection.ObjectRange())
		{
			JSON Entry = Pair.second;
			FParticleResource Resource;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Entry[ResourceKey::Path].ToString();
			Resource.Columns = static_cast<uint32>(Entry[ResourceKey::Columns].ToInt());
			Resource.Rows = static_cast<uint32>(Entry[ResourceKey::Rows].ToInt());
			Resource.SRV = nullptr;
			ParticleResources[Pair.first] = Resource;
		}
	}

	// Material 로딩 이후 StaticMesh Preload를 수행해야 SlotName->Material resolve가 가능합니다.
	PreloadStaticMeshes();


	if (LoadGPUResources(InDevice))
	{
		UE_LOG("Complete Load Resources!");
	}
	else
	{
		UE_LOG("Failed to Load Resources...");
	}
}

bool FResourceManager::LoadGPUResources(ID3D11Device* Device)
{
	if (!Device)
	{
		return false;
	}

	for (auto& [Key, Resource] : FontResources)
	{
		if (Resource.SRV != nullptr)
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
		if (Resource.SRV != nullptr)
		{
			continue;
		}

		if (!ParticleLoader.Load(Resource.Name, Resource.Path, Resource.Columns, Resource.Rows, Device, Resource))
		{
			UE_LOG("Failed to load Particle atlas: %s", Resource.Path.c_str());
			return false;
		}
	}

	for (auto& [Name, Mat] : MaterialRegistry)
	{
		if (Mat.bHasDiffuseTexture  && !Mat.DiffuseTexPath.empty())  LoadTexture(Mat.DiffuseTexPath,  Device);
		if (Mat.bHasAmbientTexture  && !Mat.AmbientTexPath.empty())  LoadTexture(Mat.AmbientTexPath,  Device);
		if (Mat.bHasSpecularTexture && !Mat.SpecularTexPath.empty()) LoadTexture(Mat.SpecularTexPath, Device);
		if (Mat.bHasBumpTexture     && !Mat.BumpTexPath.empty())     LoadTexture(Mat.BumpTexPath,     Device);
	}

	return true;
}

void FResourceManager::InitializeDefaultResources(ID3D11Device* Device)
{
	if (!Device || DefaultWhiteSRV) return;

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width            = 1;
	Desc.Height           = 1;
	Desc.MipLevels        = 1;
	Desc.ArraySize        = 1;
	Desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage            = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

	constexpr uint32_t WhitePixel = 0xFFFFFFFF;
	D3D11_SUBRESOURCE_DATA InitData = { &WhitePixel, 4, 0 };

	Device->CreateTexture2D(&Desc, &InitData, &DefaultWhiteTexture);
	if (DefaultWhiteTexture)
	{
		Device->CreateShaderResourceView(DefaultWhiteTexture, nullptr, &DefaultWhiteSRV);
	}
}

void FResourceManager::ReleaseGPUResources()
{
	for (auto& [Key, Resource] : FontResources)
	{
		if (Resource.SRV)
		{
			Resource.SRV->Release();
			Resource.SRV = nullptr;
		}
	}
	FontResources.clear();

	for (auto& [Key, Resource] : ParticleResources)
	{
		if (Resource.SRV)
		{
			Resource.SRV->Release();
			Resource.SRV = nullptr;
		}
	}
	ParticleResources.clear();

	for (auto& [Key, Resource] : MaterialTextureResources)
	{
		if (Resource.SRV)
		{
			Resource.SRV->Release();
			Resource.SRV = nullptr;
		}
	}
	MaterialTextureResources.clear();

	for (auto& [Path, StaticMeshAsset] : StaticMeshMap)
	{
		delete StaticMeshAsset;
	}
	StaticMeshMap.clear();
	StaticMeshRegistry.clear();

	if (DefaultWhiteSRV)     { DefaultWhiteSRV->Release();     DefaultWhiteSRV     = nullptr; }
	if (DefaultWhiteTexture) { DefaultWhiteTexture->Release();  DefaultWhiteTexture = nullptr; }
}

bool FResourceManager::LoadMaterial(const FString& MtlFilePath)
{
	if (MtlFilePath.empty())
	{
		return false;
	}

	TMap<FString, FMaterial> Parsed;
	if (!FObjMtlLoader::Load(MtlFilePath, Parsed))
	{
		UE_LOG("Failed to load MTL: %s", MtlFilePath.c_str());
		return false;
	}

	for (auto& [Name, Mat] : Parsed)
	{
		MaterialRegistry[Name] = Mat;
	}

	UE_LOG("Loaded MTL: %s", MtlFilePath.c_str());
	return true;
}

FMaterial* FResourceManager::FindMaterial(const FString& MaterialName)
{
	auto It = MaterialRegistry.find(MaterialName);
	return (It != MaterialRegistry.end()) ? &It->second : nullptr;
}

const FMaterial* FResourceManager::FindMaterial(const FString& MaterialName) const
{
	auto It = MaterialRegistry.find(MaterialName);
	return (It != MaterialRegistry.end()) ? &It->second : nullptr;
}


TArray<FString> FResourceManager::GetMaterialNames() const
{
	TArray<FString> Names;
	Names.reserve(MaterialRegistry.size());
	for (const auto& [Name, None] : MaterialRegistry)
	{
		Names.push_back(Name);
	}
	return Names;
}

FMaterialResource* FResourceManager::FindTexture(const FString& Path) const
{
	auto It = MaterialTextureResources.find(Path);
	return (It != MaterialTextureResources.end()) ? const_cast<FMaterialResource*>(&It->second) : nullptr;
}

FMaterialResource* FResourceManager::LoadTexture(const FString& Path, ID3D11Device* Device)
{
	if (FMaterialResource* Cached = FindTexture(Path))
	{
		return Cached;
	}

	if (!Device)
	{
		return nullptr;
	}

	std::wstring FullPath = FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Path));

	FMaterialResource Resource;
	Resource.Path = Path;

	HRESULT hr;
	if (FullPath.size() >= 4 && FullPath.substr(FullPath.size() - 4) == L".dds")
	{
		hr = DirectX::CreateDDSTextureFromFile(Device, FullPath.c_str(), nullptr, &Resource.SRV);
	}
	else
	{
		hr = DirectX::CreateWICTextureFromFile(Device, FullPath.c_str(), nullptr, &Resource.SRV);
	}

	if (FAILED(hr))
	{
		UE_LOG("Failed to load texture: %s", Path.c_str());
		return nullptr;
	}

	MaterialTextureResources[Path] = Resource;
	return &MaterialTextureResources[Path];
}

// --- Font ---
FFontResource* FResourceManager::FindFont(const FName& FontName)
{
	auto It = FontResources.find(FontName.ToString());
	return (It != FontResources.end()) ? &It->second : nullptr;
}

const FFontResource* FResourceManager::FindFont(const FName& FontName) const
{
	auto It = FontResources.find(FontName.ToString());
	return (It != FontResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FFontResource Resource;
	Resource.Name = FontName;
	Resource.Path = InPath;
	Resource.Columns = Columns;
	Resource.Rows = Rows;
	Resource.SRV = nullptr;
	FontResources[FontName.ToString()] = Resource;
}

// --- Particle ---
FParticleResource* FResourceManager::FindParticle(const FName& ParticleName)
{
	auto It = ParticleResources.find(ParticleName.ToString());
	return (It != ParticleResources.end()) ? &It->second : nullptr;
}

const FParticleResource* FResourceManager::FindParticle(const FName& ParticleName) const
{
	auto It = ParticleResources.find(ParticleName.ToString());
	return (It != ParticleResources.end()) ? &It->second : nullptr;
}

void FResourceManager::RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FParticleResource Resource;
	Resource.Name = ParticleName;
	Resource.Path = InPath;
	Resource.Columns = Columns;
	Resource.Rows = Rows;
	Resource.SRV = nullptr;
	ParticleResources[ParticleName.ToString()] = Resource;
}

TArray<FString> FResourceManager::GetFontNames() const
{
	TArray<FString> Names;
	Names.reserve(FontResources.size());
	for (const auto& [Key, _] : FontResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

TArray<FString> FResourceManager::GetParticleNames() const
{
	TArray<FString> Names;
	Names.reserve(ParticleResources.size());
	for (const auto& [Key, _] : ParticleResources)
	{
		Names.push_back(Key);
	}
	return Names;
}

UStaticMesh* FResourceManager::LoadStaticMesh(const FString& Path)
{
	if (Path.empty())
	{
		return nullptr;
	}

	//	1. 메모리 캐시 확인
	if (UStaticMesh* FoundMesh = FindStaticMesh(Path))
	{
		return FoundMesh;
	}

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
			UE_LOG("[StaticMeshLoad] Failed | Path=%s | BinarySec=%.6f | ObjSec=%.6f", Path.c_str(), BinaryLoadSec, ObjLoadSec);
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

	TArray<FStaticMeshMaterialSlot> MaterialSlots;
	MaterialSlots.reserve(LoadedMeshData->SlotNames.size());
	for (const FString& SlotName : LoadedMeshData->SlotNames)
	{
		FStaticMeshMaterialSlot Slot = {};
		Slot.SlotName = SlotName;
		Slot.MaterialData = FindMaterial(SlotName);
		MaterialSlots.push_back(Slot);
	}

	UStaticMesh* LoadedMesh = new UStaticMesh();
	LoadedMesh->SetMeshData(LoadedMeshData, MaterialSlots);

	StaticMeshMap.insert({ Path, LoadedMesh });
	return LoadedMesh;
}

UStaticMesh* FResourceManager::FindStaticMesh(const FString& Path) const
{
	auto It = StaticMeshMap.find(Path);
	if (It == StaticMeshMap.end())
	{
		return nullptr;
	}

	return It->second;
}

TArray<FString> FResourceManager::GetStaticMeshPaths() const
{
	TArray<FString> Paths;
	Paths.reserve(StaticMeshRegistry.size() + StaticMeshMap.size());

	for (const auto& Pair : StaticMeshRegistry)
	{
		Paths.push_back(Pair.second.Path);
	}

	for (const auto& Pair : StaticMeshMap)
	{
		if (std::find(Paths.begin(), Paths.end(), Pair.first) == Paths.end())
		{
			Paths.push_back(Pair.first);
		}
	}

	return Paths;
}

size_t FResourceManager::GetMaterialMemorySize() const
{
	size_t TotalSize = 0;
	// 1. FMaterial 구조체 기본 크기
	TotalSize += MaterialRegistry.size() * sizeof(FMaterial);

	// 2. 내부 FString들이 힙(Heap)에 동적 할당한 문자열 길이까지 정밀하게 합산
	for (const auto& Pair : MaterialRegistry)
	{
		const FMaterial& Mat = Pair.second;
		TotalSize += Mat.Name.capacity();
		TotalSize += Mat.DiffuseTexPath.capacity();
		TotalSize += Mat.AmbientTexPath.capacity();
		TotalSize += Mat.SpecularTexPath.capacity();
		TotalSize += Mat.BumpTexPath.capacity();
	}

	return TotalSize;
}
