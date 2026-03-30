#pragma once

#include "Asset/FontAtlasLoader.h"
#include "Asset/ObjLoader.h"
#include "Asset/ParticleAtlasLoader.h"
#include "Asset/StaticMesh.h"
#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"
#include "Render/Resource/Material.h"

// 리소스를 관리하는 싱글턴.
// Resource.ini에서 리소스 경로/그리드 정보를 읽고, GPU 리소스를 로드/캐싱합니다.
// 컴포넌트는 소유하지 않고 포인터로 공유 데이터를 참조합니다.

struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

class FResourceManager : public TSingleton<FResourceManager>
{
	friend class TSingleton<FResourceManager>;

public:
	// Resource.ini에서 경로/그리드 정보 로드 후 GPU 리소스 생성
	void LoadFromFile(const FString& Path, ID3D11Device* InDevice);

	// GPU 리소스 로드 (Device 필요)
	bool LoadGPUResources(ID3D11Device* Device);

	// --- Default Resources ---
	void InitializeDefaultResources(ID3D11Device* Device);
	ID3D11ShaderResourceView* GetDefaultWhiteSRV() const { return DefaultWhiteSRV; }

	// --- Material Texture (SRV) ---
	FMaterialResource* FindTexture(const FString& Path) const;
	FMaterialResource* LoadTexture(const FString& Path, ID3D11Device* Device);
	// 모든 GPU 리소스 해제
	void ReleaseGPUResources();

	// --- Material ---
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

private:
	FResourceManager() = default;
	~FResourceManager() { ReleaseGPUResources(); }

	FObjLoader ObjLoader;
	FFontAtlasLoader FontLoader;
	FParticleAtlasLoader ParticleLoader;

	TMap<FString, FFontResource>     FontResources;
	TMap<FString, FParticleResource> ParticleResources;
	
	TMap<FString, FMaterialResource> MaterialTextureResources;
	TMap<FString, FMaterial>         MaterialRegistry;   

	TMap<FString, FStaticMeshResource> StaticMeshRegistry;
	TMap<FString, UStaticMesh*>        StaticMeshMap;

	ID3D11Texture2D*          DefaultWhiteTexture = nullptr;
	ID3D11ShaderResourceView* DefaultWhiteSRV     = nullptr;
};
