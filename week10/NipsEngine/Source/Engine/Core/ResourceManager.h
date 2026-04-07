#pragma once

#include "Asset/BinarySerializer.h"
#include "Asset/FontAtlasLoader.h"
#include "Asset/ObjLoader.h"
#include "Asset/ParticleAtlasLoader.h"
#include "Asset/StaticMesh.h"
#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"
#include "Render/Resource/Material.h"
#include <d3d11.h>

// 리소스를 관리하는 싱글턴.
// Resource.ini에서 리소스 경로/그리드 정보를 읽고, GPU 리소스를 로드/캐싱합니다.
// 컴포넌트는 소유하지 않고 포인터로 공유 데이터를 참조합니다.

#pragma region __ASSET_META__

constexpr const char* TextureMetaKey_Type = "Type";
constexpr const char* TextureMetaKey_Columns = "Columns";
constexpr const char* TextureMetaKey_Rows = "Rows";

enum class EAssetMetaType
{
	None,
	Font,
	Particle
};

struct FTextureAssetMeta
{
	EAssetMetaType Type = EAssetMetaType::None;
	int32 Columns = 1;
	int32 Rows = 1;
};

inline const char* ToString(EAssetMetaType Type)
{
	switch (Type)
	{
	case EAssetMetaType::Font: return "Font";
	case EAssetMetaType::Particle: return "Particle";
	default: return "None";
	}
}

inline EAssetMetaType ToAssetMetaType(const FString& Value)
{
	if (Value == "Font")
	{
		return EAssetMetaType::Font;
	}
	if (Value == "Particle")
	{
		return EAssetMetaType::Particle;
	}
	return EAssetMetaType::None;
}

#pragma endregion

class FResourceManager : public TSingleton<FResourceManager>
{
	friend class TSingleton<FResourceManager>;

public:
	//	초기 설정
	void LoadFromAssetDirectory(const FString & Path, ID3D11Device* Device);
	//	Refresh
	void RefreshFromAssetDirectory(const FString & Path);
	
	// Resource.ini에서 경로/그리드 정보 로드 후 GPU 리소스 생성
	// void LoadFromFile(const FString& Path, ID3D11Device* InDevice);

	// GPU 리소스 로드 (Device 필요)
	bool LoadGPUResources(ID3D11Device* Device);

	// --- Default Resources ---
	void InitializeDefaultResources(ID3D11Device* Device);
	ID3D11ShaderResourceView* GetDefaultWhiteSRV() const { return DefaultWhiteSRV.Get(); }

	// --- Material Texture (SRV) ---
	FMaterialResource* FindTexture(const FString& Path) const;
	FMaterialResource* LoadTexture(const FString& Path, ID3D11Device* Device);
	// 모든 GPU 리소스 해제
	void ReleaseGPUResources();

	// --- Material ---
	void LoadMaterialFromPath(const FString& FilePath);
	bool LoadMaterial(const FString& MtlFilePath);
	FMaterial* FindMaterial(const FString& MaterialName);
	const FMaterial* FindMaterial(const FString& MaterialName) const;
	TArray<FString> GetMaterialNames() const;

	// --- Font ---
	FFontResource* FindFont(const FName& FontName);
	const FFontResource* FindFont(const FName& FontName) const;
	void RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns = 16, uint32 Rows = 16);
	TArray<FString> GetFontNames() const;

	// --- Particle ---
	FParticleResource* FindParticle(const FName& ParticleName);
	const FParticleResource* FindParticle(const FName& ParticleName) const;
	void RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns = 1, uint32 Rows = 1);
	TArray<FString> GetParticleNames() const;

	// --- StaticMesh ---
	UStaticMesh* LoadStaticMesh(const FString& Path);
	UStaticMesh* FindStaticMesh(const FString& Path) const;
	TArray<FString> GetStaticMeshPaths() const;
	
	// --- Memory ---
	size_t GetMaterialMemorySize() const;
	
	//	Binary 전체 삭제
	void DeleteAllCacheFiles();

private:
	uint64 GetFileWriteTimeTicks(const FString& Path) const;
	FString MakeStaticMeshBinaryPath(const FString& SourcePath) const;
	bool IsStaticMeshBinaryValid(const FString& SourcePath, const FString& BinaryPath) const;
	void PreloadStaticMeshes();
	
	FTextureAssetMeta LoadOrCreateTextureMeta(const std::filesystem::path& FilePath) const;

	FResourceManager() = default;
	~FResourceManager() { ReleaseGPUResources(); }

	TComPtr<ID3D11Device> CachedDevice;

	FObjLoader ObjLoader;
	FFontAtlasLoader FontLoader;
	FParticleAtlasLoader ParticleLoader;
	
	FBinarySerializer BinarySerializer;

	TMap<FString, FFontResource>     FontResources;
	TMap<FString, FParticleResource> ParticleResources;
	
	TMap<FString, FMaterialResource> MaterialTextureResources;
	TMap<FString, FMaterial>         MaterialRegistry;   

	TMap<FString, FStaticMeshResource> StaticMeshRegistry;
	TMap<FString, UStaticMesh*>        StaticMeshMap;

	TComPtr<ID3D11Texture2D>          DefaultWhiteTexture;
	TComPtr<ID3D11ShaderResourceView> DefaultWhiteSRV;
	
	/* Paths */
	TArray<FString> ObjFilePaths;
	TArray<FString> MaterialFilePaths;
	TArray<FString> ParticleFilePaths;
	TArray<FString> FontFilePaths;
	TArray<FString> TextureFilePaths;
};
