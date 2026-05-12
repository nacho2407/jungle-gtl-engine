#pragma once

#include "Asset/BinarySerializer.h"
#include "Asset/CurveFloatAsset.h"
#include "Asset/FbxImporter.h"
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
#include "Core/TextureAssetMetaService.h"
#include "Core/TextureResourceCache.h"
#include "Object/FName.h"
#include "Render/Resource/ComputeShader.h"
#include "Render/Resource/ShaderTypes.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/Texture.h"
#include "Render/Resource/RenderResources.h"
#include <d3d11.h>


class FMaterialLoadService;
class FMaterialSerializationService;
class FStaticMeshLoadService;

// 리소스를 관리하는 싱글턴.
class FResourceManager : public TSingleton<FResourceManager>
{
	friend class TSingleton<FResourceManager>;
	friend class FMaterialLoadService;
	friend class FMaterialSerializationService;
	friend class FStaticMeshLoadService;

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

	// Shader는 이제 VS / PS Stage 단위로 가져오고, Draw 시점에는 Program(VS+PS 조합)을 바인딩합니다.
	FVertexShader* GetOrCreateVertexShader(
		const FShaderStageKey& Key,
		const D3D_SHADER_MACRO* Defines = nullptr,
		const FVertexLayoutDesc* VertexLayout = nullptr);
	FPixelShader* GetOrCreatePixelShader(const FShaderStageKey& Key, const D3D_SHADER_MACRO* Defines = nullptr);
	FShaderProgram* GetOrCreateShaderProgram(
		const FShaderStageKey& VSKey,
		const FShaderStageKey& PSKey,
		const D3D_SHADER_MACRO* VSDefines = nullptr,
		const D3D_SHADER_MACRO* PSDefines = nullptr,
		const FVertexLayoutDesc* VertexLayout = nullptr);

	FComputeShader* GetComputeShader(const FString& Key) const;
    bool LoadComputeShader(const FString& FilePath, const FString& CSEntryPoint,
                           const D3D_SHADER_MACRO* Defines = nullptr, const FString& Key = "");

	// Shader 파일 변경 시 관련 Stage/Program 캐시만 비웁니다. 다음 사용 시 Lazy Compile 됩니다.
	void InvalidateShaderFile(const FString& Path);

	UMaterial* GetMaterial(const FString& Path) const;
	UMaterial* GetOrCreateMaterial(const FString& Path, EMaterialShaderType ShaderType = EMaterialShaderType::SurfaceLit);
	UMaterial* GetOrCreateMaterial(const FString& Name, const FString& Path, EMaterialShaderType ShaderType = EMaterialShaderType::SurfaceLit);
	bool LoadMaterial(const FString& Path, EMaterialShaderType ShaderType = EMaterialShaderType::SurfaceLit, ID3D11Device* Device = nullptr);

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
	void ClearDiscoveredResourceLists(bool bClearAtlasCache);
	void RegisterDiscoveredAssetFile(const std::filesystem::path& FilePath, const std::filesystem::path& ProjectRootPath);
	void InitializeDefaultWhiteTexture(ID3D11Device* Device);
	void InitializeDefaultMaterial(ID3D11Device* Device);
	void InitializeOutlineMaterial();
	uint64 GetFileWriteTimeTicks(const FString& Path) const;
	bool IsStaticMeshBinaryValid(const FString& SourcePath, const FString& BinaryPath) const;
	void PreloadStaticMeshes();
	UStaticMesh* CreateStaticMeshFromLoadedData(FStaticMesh* LoadedMeshData, const FString& LogPath, bool bLogLodTiming, bool bLogLodSkipped) const;
	
	FTextureAssetMeta LoadOrCreateTextureMeta(const std::filesystem::path& FilePath) const;
	void RegisterObjMaterialSlotAliases(const FString& ObjPath, const FString& MtlPath);
	UMaterial* GetMaterialForStaticMeshSlot(const FString& SourcePath, const FString& SlotName) const;
	void ResolveStaticMeshMaterialSlots(const FString& SourcePath, FStaticMesh* StaticMesh) const;

	FResourceManager() = default;
	~FResourceManager() { ReleaseGPUResources(); }

	TComPtr<ID3D11Device> CachedDevice;

	FObjLoader ObjLoader;
	FFbxImporter FbxImporter;
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
