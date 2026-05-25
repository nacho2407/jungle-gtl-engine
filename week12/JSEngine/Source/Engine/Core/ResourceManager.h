#pragma once

#include "Asset/AnimSequenceAssetLoader.h"
#include "Asset/BinarySerializer.h"
#include "Asset/CurveFloatAsset.h"
#include "Asset/FBX/FbxImporter.h"
#include "Asset/ObjLoader.h"
#include "Asset/ParticleSystemAssetLoader.h"
#include "Asset/SkeletalMesh.h"
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
#include "Core/TextureAtlasAssetService.h"
#include "Core/TextureResourceCache.h"
#include "Object/FName.h"
#include "Render/Resource/ComputeShader.h"
#include "Render/Resource/ShaderTypes.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/Texture.h"
#include "Render/Resource/RenderResources.h"
#include <d3d11.h>

#include "Animation/AnimGraphAsset.h"


class FMaterialLoadService;
class FMaterialSerializationService;
class FStaticMeshLoadService;
class FSkeletalMeshLoadService;
class FFbxMaterialLoadService;
class UAnimSequence;
class UCurveColorAsset;
class UCurveVectorAsset;
class UParticleSystem;

// 리소스를 관리하는 싱글턴: 텍스처, 쉐이더, 머티리얼, 메시 등 다양한 리소스의 로드/캐싱/관리 기능을 제공합니다.
class FResourceManager : public TSingleton<FResourceManager>
{
	friend class TSingleton<FResourceManager>;
	friend class FMaterialLoadService;
	friend class FMaterialSerializationService;
	friend class FStaticMeshLoadService;
	friend class FSkeletalMeshLoadService;
	friend class FFbxMaterialLoadService;

public:
	void SetCachedDevice(ID3D11Device* Device) { CachedDevice = Device; }

	void LoadFromAssetDirectory(const FString& Path);
	void RefreshFromAssetDirectory(const FString& Path);

	void InitializeDefaultResources(ID3D11Device* Device);
	ID3D11ShaderResourceView* GetDefaultWhiteSRV() const { return TextureCache.GetDefaultWhiteSRV(); }

	bool LoadGPUResources(ID3D11Device* Device);
	void ReleaseGPUResources();

	// Texture
	UTexture* GetTexture(const FString& Path) const;
	UTexture* LoadTexture(const FString& Path, ID3D11Device* Device = nullptr);
	const TArray<FString>& GetTextureFilePath() const;

	// Shader ── VS / PS Stage 단위로 가져오고, Draw 시점에는 Program(VS + PS)을 바인딩합니다.
	FVertexShader* GetOrCreateVertexShader(const FShaderStageKey& Key, const D3D_SHADER_MACRO* Defines = nullptr, const FVertexLayoutDesc* VertexLayout = nullptr);
	FPixelShader* GetOrCreatePixelShader(const FShaderStageKey& Key, const D3D_SHADER_MACRO* Defines = nullptr);
	FShaderProgram* GetOrCreateShaderProgram(const FShaderStageKey& VSKey, const FShaderStageKey& PSKey, const D3D_SHADER_MACRO* VSDefines = nullptr, const D3D_SHADER_MACRO* PSDefines = nullptr, const FVertexLayoutDesc* VertexLayout = nullptr);
	FComputeShader* GetComputeShader(const FString& Key) const;
	bool LoadComputeShader(const FString& FilePath, const FString& CSEntryPoint, const D3D_SHADER_MACRO* Defines = nullptr, const FString& Key = "");
	void InvalidateShaderFile(const FString& Path); // Shader 변경 시 관련 Stage/Program 캐시만 비웁니다. 다음 사용 시 Lazy Compile됩니다.

	// Material
	UMaterial* GetMaterial(const FString& Path) const;
	UMaterial* GetOrCreateMaterial(const FString& Path, EMaterialShaderType ShaderType = EMaterialShaderType::SurfaceLit);
	UMaterial* GetOrCreateMaterial(const FString& Name, const FString& Path, EMaterialShaderType ShaderType = EMaterialShaderType::SurfaceLit);
	bool LoadMaterial(const FString& Path, EMaterialShaderType ShaderType = EMaterialShaderType::SurfaceLit, ID3D11Device* Device = nullptr);
	bool ImportMaterialFromFbx(const FString& Path, EMaterialShaderType ShaderType = EMaterialShaderType::SurfaceLit, ID3D11Device* Device = nullptr);

	bool SerializeMaterial(const FString& Path, const UMaterial* Material);
	bool SerializeMaterialInstance(const FString& Path, const UMaterialInstance* MaterialInstance);
	bool DeserializeMaterial(const FString& Path);
	TArray<FString> GetMaterialNames() const;
	TArray<FString> GetMaterialInterfaceNames() const;

	size_t GetMaterialMemorySize() const;

	UMaterialInstance* CreateMaterialInstance(const FString& Path, UMaterial* Parent);
	UMaterialInstance* GetMaterialInstance(const FString& Path) const;
	UMaterialInterface* GetMaterialInterface(const FString& Path);

	// Font
	FFontResource* FindFont(const FName& FontName);
	const FFontResource* FindFont(const FName& FontName) const;
	void RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns = 16, uint32 Rows = 16);
	TArray<FString> GetFontNames() const;

	// SubUV
	FSubUVResource* FindSubUV(const FName& SubUVName);
	const FSubUVResource* FindSubUV(const FName& SubUVName) const;
	void RegisterSubUV(const FName& SubUVName, const FString& InPath, uint32 Columns = 1, uint32 Rows = 1);
	TArray<FString> GetSubUVNames() const;

	// Static Mesh
	UStaticMesh* LoadStaticMesh(const FString& Path);
	UStaticMesh* ImportStaticMeshFromFbx(const FString& Path);
	UStaticMesh* FindStaticMesh(const FString& Path) const;
	TArray<FString> GetStaticMeshPaths() const;

	// Skeletal Mesh
	USkeletalMesh* LoadSkeletalMesh(const FString& Path);
	USkeletalMesh* ImportSkeletalMeshFromFbx(const FString& Path);
	USkeletalMesh* FindSkeletalMesh(const FString& Path) const;

	TArray<FString> GetSkeletalMeshPaths() const;
	FFbxMeshContentInfo InspectFbxMeshContent(const FString& Path);
	bool SaveSkeletalMesh(USkeletalMesh* Mesh); // 에디터에서 socket 등 mesh data 변경 후 writable cache(.bin)에 저장.

	// Curve
	UCurveFloatAsset* LoadCurve(const FString& Path);
	UCurveFloatAsset* LoadFloatCurve(const FString& Path);
	UCurveVectorAsset* LoadVectorCurve(const FString& Path);
	UCurveColorAsset* LoadColorCurve(const FString& Path);

	UCurveFloatAsset* FindCurve(const FString& Path) const;
	UCurveFloatAsset* FindFloatCurve(const FString& Path) const;
	UCurveVectorAsset* FindVectorCurve(const FString& Path) const;
	UCurveColorAsset* FindColorCurve(const FString& Path) const;

	bool SaveCurve(const FString& Path, const UCurveFloatAsset* Curve);
	bool SaveCurve(const FString& Path, const UCurveVectorAsset* Curve);
	bool SaveCurve(const FString& Path, const UCurveColorAsset* Curve);
	TArray<FString> GetCurvePaths() const;

	// Animation Sequence
	UAnimSequence* LoadAnimSequence(const FString& Path);
	TArray<FString> ImportAnimationStacksFromFbx(const FString& Path);
	bool SaveAnimSequence(const FString& Path, const UAnimSequence* Sequence);
	UAnimSequence* FindAnimSequence(const FString& Path) const;
	TArray<FString> GetAnimSequencePaths() const;

	// Animation Graph
	UAnimGraphAsset* LoadAnimGraph(const FString& Path);
	bool SaveAnimGraph(UAnimGraphAsset* Asset, const FString& Path);

	// Particle System
	UParticleSystem* LoadParticleSystem(const FString& Path);
	UParticleSystem* FindParticleSystem(const FString& Path) const;

	bool SaveParticleSystem(const FString& Path, const UParticleSystem* ParticleSystem);
	TArray<FString> GetParticleSystemPaths() const;

	// Device State
	ID3D11SamplerState* GetOrCreateSamplerState(ESamplerType Type, ID3D11Device* Device = nullptr);
	ID3D11DepthStencilState* GetOrCreateDepthStencilState(EDepthStencilType Type, ID3D11Device* Device = nullptr);
	ID3D11BlendState* GetOrCreateBlendState(EBlendType Type, ID3D11Device* Device = nullptr);
	ID3D11BlendState* GetOrCreateBlendState(const FMaterialBlendStateDesc& Desc, ID3D11Device* Device = nullptr);
	ID3D11RasterizerState* GetOrCreateRasterizerState(ERasterizerType Type, ID3D11Device* Device = nullptr);

	// Binary Cache Deletion
	void DeleteAllCacheFiles();

private:
	FResourceManager() = default;
	~FResourceManager() { ReleaseGPUResources(); }

	TComPtr<ID3D11Device> CachedDevice;

	FObjLoader ObjLoader;
	FFbxImporter FbxImporter;
	FAnimSequenceAssetLoader AnimSequenceAssetLoader;
	FParticleSystemAssetLoader ParticleSystemAssetLoader;
	FBinarySerializer BinarySerializer;

	TComPtr<ID3D11Texture2D> DefaultWhiteTexture;

	FCurveResourceCache CurveCache;
	FShaderResourceCache ShaderCache;
	FTextureResourceCache TextureCache;
	FMaterialResourceCache MaterialCache;
	FRenderStateResourceCache RenderStateCache;
	FStaticMeshResourceCache StaticMeshCache;
	FAtlasResourceCache AtlasCache;

	TMap<FString, USkeletalMesh*> SkeletalMeshMap;
	TMap<FString, UAnimSequence*> AnimSequenceMap;
	TMap<FString, UParticleSystem*> ParticleSystemMap;
	TMap<FString, FString> FileContentHashCache;

	/* Paths */
	TArray<FString> ObjFilePaths;
	TArray<FString> MaterialFilePaths;
	TArray<FString> SubUVFilePaths;
	TArray<FString> FontFilePaths;
	TArray<FString> TextureFilePaths;
	TArray<FString> SkeletalMeshFilePaths;
	TArray<FString> AnimSequenceFilePaths;
	TArray<FString> AnimationFbxSourceFilePaths;
	TArray<FString> CurveFilePaths;
	TArray<FString> ParticleSystemFilePaths;

private:
	// Helper Functions
	void ClearDiscoveredResourceLists(bool bClearAtlasCache);
	void RegisterDiscoveredAssetFile(const std::filesystem::path& FilePath, const std::filesystem::path& ProjectRootPath);
	void WarmUpAnimationPreviewMeshCaches(const TArray<FString>& AnimSequenceAssetPaths);
	bool EnsureSkeletalMeshCacheForAnimationPreview(const FString& PreviewMeshPath);

	void InitializeDefaultWhiteTexture(ID3D11Device* Device);
	void InitializeDefaultMaterial(ID3D11Device* Device);
	void InitializeOutlineMaterial();

	// File MetaData & Hash
	uint64 GetFileWriteTimeTicks(const FString& Path) const;
	uint64 GetFileSizeBytes(const FString& Path) const;
	FString ComputeFileContentHashString(const FString& Path) const;
	FString GetCachedFileContentHashString(const FString& Path, uint64 WriteTimeTicks, uint64 FileSizeBytes);

	// Validity Check
	bool IsImportedStaticMeshAssetFresh(const FString& SourcePath, const FString& BinaryPath) const;
	bool IsImportedSkeletalMeshAssetFresh(const FString& SourcePath, const FString& BinaryPath) const;
	bool IsStaticMeshBinaryValid(const FString& SourcePath, const FString& BinaryPath) const;
	bool IsSkeletalMeshBinaryValid(const FString& SourcePath, const FString& BinaryPath) const;

	// Preload & Creation
	void PreloadStaticMeshes();
	bool LoadTextureAtlasAsset(const std::filesystem::path& AssetFilePath, ETextureAtlasAssetType Type, const std::filesystem::path& ProjectRootPath, FTextureAtlasAsset& OutAsset) const;
	UStaticMesh* CreateStaticMeshFromLoadedData(FStaticMesh* LoadedMeshData, const FString& LogPath, bool bLogLodTiming, bool bLogLodSkipped) const;

	// Slot Aliases
	void RegisterObjMaterialSlotAliases(const FString& ObjPath, const FString& MtlPath);
	UMaterial* GetMaterialForStaticMeshSlot(const FString& SourcePath, const FString& SlotName) const;
	void ResolveStaticMeshMaterialSlots(const FString& SourcePath, FStaticMesh* StaticMesh) const;
	void ResolveSkeletalMeshMaterialSlots(const FString& SourcePath, FSkeletalMesh* SkeletalMesh) const;
};
