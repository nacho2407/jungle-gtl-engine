#pragma once
#pragma once

#include "Asset/FontAtlasLoader.h"
#include "Asset/ObjLoader.h"
#include "Asset/ParticleAtlasLoader.h"
#include "Asset/StaticMesh.h"
#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

// 리소스를 관리하는 싱글턴.
// Resource.ini에서 리소스 경로/그리드 정보를 읽고, GPU 리소스를 로드/캐싱합니다.
// 컴포넌트는 소유하지 않고 포인터로 공유 데이터를 참조합니다.

struct ID3D11Device;

class FResourceManager : public TSingleton<FResourceManager>
{
	friend class TSingleton<FResourceManager>;

public:
	// Resource.ini에서 경로/그리드 정보 로드 후 GPU 리소스 생성
	void LoadFromFile(const FString& Path, ID3D11Device* InDevice);

	// GPU 리소스 로드 (Device 필요)
	bool LoadGPUResources(ID3D11Device* Device);

	// --- Material Texture ---
	// DDS / PNG 자동 분기
	FMaterialResource* GetOrLoadTexture(const FString& Path, ID3D11Device* Device);

	// 모든 GPU 리소스 해제
	void ReleaseGPUResources();

	// --- Font ---
	FFontResource* FindFont(const FName& FontName);
	const FFontResource* FindFont(const FName& FontName) const;
	void RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns = 16, uint32 Rows = 16);

	// --- Font names ---
	TArray<FString> GetFontNames() const;

	// --- Particle ---
	FParticleResource* FindParticle(const FName& ParticleName);
	const FParticleResource* FindParticle(const FName& ParticleName) const;
	void RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns = 1, uint32 Rows = 1);

	// --- Particle names ---
	TArray<FString> GetParticleNames() const;
	
	/* For StaticMeshes */
	UStaticMesh* LoadStaticMesh(const FString& Path);
	UStaticMesh* FindStaticMesh(const FString& Path) const;
	TArray<FString> GetStaticMeshPaths() const;

private:
	FResourceManager() = default;
	~FResourceManager() { ReleaseGPUResources(); }
	
	FObjLoader ObjLoader;
	FFontAtlasLoader FontLoader;
	FParticleAtlasLoader ParticleLoader;

	TMap<FString, FFontResource>     FontResources;
	TMap<FString, FParticleResource> ParticleResources;
	TMap<FString, FMaterialResource> MaterialTextureResources;
	
	TMap<FString, FStaticMeshResource> StaticMeshRegistry; // Resource.ini에 등록된 StaticMesh Path 목록
	TMap<FString, UStaticMesh*> StaticMeshMap;
};
