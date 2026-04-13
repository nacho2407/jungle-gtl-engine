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
#include "Render/Resource/Shader.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/Texture.h"
#include "Render/Resource/RenderResources.h"
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
	Particle,
	Texture
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
	if (Value == "Texture") 
	{
		return EAssetMetaType::Texture;
	}
	return EAssetMetaType::None;
}

#pragma endregion

class FResourceManager : public TSingleton<FResourceManager>
{
	friend class TSingleton<FResourceManager>;

public:
	void SetCachedDevice(ID3D11Device* Device) { CachedDevice = Device; }

	void LoadFromAssetDirectory(const FString& Path);
	void RefreshFromAssetDirectory(const FString& Path);

	void InitializeDefaultResources(ID3D11Device* Device);
	ID3D11ShaderResourceView* GetDefaultWhiteSRV() const
	{
		auto it = Textures.find("DefaultWhite");
		if (it != Textures.end())
		{
			return it->second->GetSRV();
		}
		return nullptr;
	}

	bool LoadGPUResources(ID3D11Device* Device);
	void ReleaseGPUResources();

	UTexture* GetTexture(const FString& Path) const;
	UTexture* LoadTexture(const FString& Path, ID3D11Device* Device = nullptr);
	const TArray<FString>& GetTextureFilePath() const;

	UShader* GetShader(const FString& FilePath) const;
	bool LoadShader(const FString& FilePath, const FString& VSEntryPoint, const FString& PSEntryPoint,
		const D3D11_INPUT_ELEMENT_DESC* InputElements, UINT InputElementCount);

	UMaterial* GetMaterial(const FString& Path) const;
	UMaterial* GetOrCreateMaterial(const FString& Path, const FString& ShaderName);
	bool LoadMaterial(const FString& Path, const FString& ShaderName, ID3D11Device* Device = nullptr);
	TArray<FString> GetMaterialNames() const;

	FFontResource* FindFont(const FName& FontName);
	const FFontResource* FindFont(const FName& FontName) const;
	void RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns = 16, uint32 Rows = 16);
	TArray<FString> GetFontNames() const;

	FParticleResource* FindParticle(const FName& ParticleName);
	const FParticleResource* FindParticle(const FName& ParticleName) const;
	void RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns = 1, uint32 Rows = 1);
	TArray<FString> GetParticleNames() const;

	UStaticMesh* LoadStaticMesh(const FString& Path);
	UStaticMesh* FindStaticMesh(const FString& Path) const;
	TArray<FString> GetStaticMeshPaths() const;

	ID3D11SamplerState* GetOrCreateSamplerState(ESamplerType Type, ID3D11Device* Device = nullptr);
	ID3D11DepthStencilState* GetOrCreateDepthStencilState(EDepthStencilType Type, ID3D11Device* Device = nullptr);
	ID3D11BlendState* GetOrCreateBlendState(EBlendType Type, ID3D11Device* Device = nullptr);
	ID3D11RasterizerState* GetOrCreateRasterizerState(ERasterizerType Type, ID3D11Device* Device = nullptr);

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
	
	TMap<FString, FStaticMeshResource> StaticMeshRegistry;

	TComPtr<ID3D11Texture2D>          DefaultWhiteTexture;

	TMap<FString, UStaticMesh*> StaticMeshes;
	TMap<FString, UShader*> Shaders;
	TMap<FString, UTexture*> Textures;
	TMap<FString, UMaterial*> Materials;
	TMap<ESamplerType, TComPtr<ID3D11SamplerState>> SamplerStates;
	TMap<EDepthStencilType, TComPtr<ID3D11DepthStencilState>> DepthStencilStates;
	TMap<EBlendType, TComPtr<ID3D11BlendState>> BlendStates;
	TMap<ERasterizerType, TComPtr<ID3D11RasterizerState>> RasterizerStates;

	/* Paths */
	TArray<FString> ObjFilePaths;
	TArray<FString> MaterialFilePaths;
	TArray<FString> ParticleFilePaths;
	TArray<FString> FontFilePaths;
	TArray<FString> TextureFilePaths;
};
