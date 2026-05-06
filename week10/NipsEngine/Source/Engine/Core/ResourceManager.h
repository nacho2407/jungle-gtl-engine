#pragma once

#include "Asset/BinarySerializer.h"
#include "Asset/CurveFloatAsset.h"
#include "Asset/ObjLoader.h"
#include "Asset/StaticMesh.h"
#include "Core/AtlasResourceCache.h"
#include "Core/CurveResourceCache.h"
#include "Core/CoreTypes.h"
#include "Core/MaterialResourceCache.h"
#include "Core/RenderStateResourceCache.h"
#include "Core/Singleton.h"
#include "Core/ResourceTypes.h"
#include "Core/ShaderResourceCache.h"
#include "Core/StaticMeshResourceCache.h"
#include "Core/TextureResourceCache.h"
#include "Object/FName.h"
#include "Render/Resource/ComputeShader.h"
#include "Render/Resource/Shader.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/Texture.h"
#include "Render/Resource/RenderResources.h"
#include <d3d11.h>


// Asset에 붙는 Meta 정보
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
	case EAssetMetaType::Texture: return "Texture";
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


// 리소스를 관리하는 싱글턴.
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
		return TextureCache.GetDefaultWhiteSRV();
	}

	bool LoadGPUResources(ID3D11Device* Device);
	void ReleaseGPUResources();

	UTexture* GetTexture(const FString& Path) const;
	UTexture* LoadTexture(const FString& Path, ID3D11Device* Device = nullptr);
	const TArray<FString>& GetTextureFilePath() const;

	UShader* GetShader(const FString& FilePath) const;
	bool LoadShader(const FString& FilePath, const FString& VSEntryPoint, const FString& PSEntryPoint,
					const D3D_SHADER_MACRO* Defines = nullptr, uint32 PermutationKey = 0);
	bool EnsureShaderPermutation(const FString& FilePath, uint32 PermutationKey);
	void ReloadShader(const FString& Path);

	FComputeShader* GetComputeShader(const FString& Key) const;
    bool LoadComputeShader(const FString& FilePath, const FString& CSEntryPoint,
                           const D3D_SHADER_MACRO* Defines = nullptr, const FString& Key = "");

	UMaterial* GetMaterial(const FString& Path) const;
	UMaterial* GetOrCreateMaterial(const FString& Path, const FString& ShaderName);
	UMaterial* GetOrCreateMaterial(const FString& Name, const FString& Path, const FString& ShaderName);
	bool LoadMaterial(const FString& Path, const FString& ShaderName, ID3D11Device* Device = nullptr);

	bool SerializeMaterial(const FString& Path, const UMaterial* Material);
	bool SerializeMaterialInstance(const FString& Path, const UMaterialInstance* MaterialInstance);
	bool DeserializeMaterial(const FString& Path);
	TArray<FString> GetMaterialNames() const;
	TArray<FString> GetMaterialInterfaceNames() const;

	UMaterialInstance* CreateMaterialInstance(const FString& Path, UMaterial* Parent);
	UMaterialInstance* GetMaterialInstance(const FString& Path) const;
	UMaterialInterface* GetMaterialInterface(const FString& Path);

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

	UCurveFloatAsset* LoadCurve(const FString& Path);
	UCurveFloatAsset* FindCurve(const FString& Path) const;
	bool SaveCurve(const FString& Path, const UCurveFloatAsset* Curve);
	TArray<FString> GetCurvePaths() const;

	ID3D11SamplerState* GetOrCreateSamplerState(ESamplerType Type, ID3D11Device* Device = nullptr);
	ID3D11DepthStencilState* GetOrCreateDepthStencilState(EDepthStencilType Type, ID3D11Device* Device = nullptr);
	ID3D11BlendState* GetOrCreateBlendState(EBlendType Type, ID3D11Device* Device = nullptr);
	ID3D11RasterizerState* GetOrCreateRasterizerState(ERasterizerType Type, ID3D11Device* Device = nullptr);

	size_t GetMaterialMemorySize() const;
	
	//	Binary 전체 삭제
	void DeleteAllCacheFiles();

private:
	uint64 GetFileWriteTimeTicks(const FString& Path) const;
	bool IsStaticMeshBinaryValid(const FString& SourcePath, const FString& BinaryPath) const;
	void PreloadStaticMeshes();
	
	FTextureAssetMeta LoadOrCreateTextureMeta(const std::filesystem::path& FilePath) const;
	void RegisterObjMaterialSlotAliases(const FString& ObjPath, const FString& MtlPath);
	UMaterial* GetMaterialForStaticMeshSlot(const FString& SourcePath, const FString& SlotName) const;
	void ResolveStaticMeshMaterialSlots(const FString& SourcePath, FStaticMesh* StaticMesh) const;

	FResourceManager() = default;
	~FResourceManager() { ReleaseGPUResources(); }

	TComPtr<ID3D11Device> CachedDevice;

	FObjLoader ObjLoader;
	FBinarySerializer BinarySerializer;

	TComPtr<ID3D11Texture2D>          DefaultWhiteTexture;

	FCurveResourceCache CurveCache;
	FShaderResourceCache ShaderCache;
	FTextureResourceCache TextureCache;
	FMaterialResourceCache MaterialCache;
	FRenderStateResourceCache RenderStateCache;
	FStaticMeshResourceCache StaticMeshCache;
	FAtlasResourceCache AtlasCache;

	/* Paths */
	TArray<FString> ObjFilePaths;
	TArray<FString> MaterialFilePaths;
	TArray<FString> ParticleFilePaths;
	TArray<FString> FontFilePaths;
	TArray<FString> TextureFilePaths;
	TArray<FString> CurveFilePaths;
};
