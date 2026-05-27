#pragma once

#include "GameFramework/WorldContext.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Common/ViewTypes.h"
#include "Render/Resource/VertexTypes.h"
#include "Spatial/WorldSpatialIndex.h"

class FRenderBus;
class FDecalCommandBuilder;
class FConstantBuffer;
class FMeshBuffer;
class FPrimitiveDrawCommandBuilder;
class UStaticMesh;
class USkeletalMesh;
struct FBoneMatrixConstants;
struct ID3D11Device;
struct ID3D11DeviceContext;

enum class EPrimitiveRenderProxyCollectIntent : uint8
{
	Normal,
	ShadowOnly,
};

class FRenderResourceProvider
{
public:
	virtual ~FRenderResourceProvider() = default;
	virtual FMeshBuffer& GetMeshBuffer(EPrimitiveType InPrimitiveType) = 0;
	virtual FMeshBuffer* GetStaticMeshBuffer(const UStaticMesh* StaticMeshAsset, int32 LODLevel = 0) = 0;
	virtual FMeshBuffer* GetProcMeshBuffer(uint32 ProcMeshCompUUID, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices) = 0;
	virtual FMeshBuffer* GetCPUSkeletalMeshBuffer(uint32 SkeletalMeshCompUUID, const USkeletalMesh* SkeletalMeshAsset,
	                                              const TArray<FSkeletalMeshVertex>& Vertices, const TArray<uint32>& Indices,
	                                              bool bNeedsUpload) = 0;
	virtual FMeshBuffer* GetGPUSkeletalMeshBuffer(const USkeletalMesh* SkeletalMeshAsset) = 0;
	virtual FConstantBuffer* GetGPUSkeletalBoneMatrixBuffer(uint32 SkeletalMeshCompUUID,
	                                                        const FBoneMatrixConstants& Constants,
	                                                        bool bNeedsUpload) = 0;
};

struct FPrimitiveRenderProxyCommandServices
{
	const FPrimitiveDrawCommandBuilder& PrimitiveDrawCommandBuilder;
	FDecalCommandBuilder& DecalCommandBuilder;
	FWorldSpatialIndex::FPrimitiveOBBQueryScratch& OBBQueryScratch;
};

struct FPrimitiveRenderProxyCollectionContext
{
	FRenderBus& RenderBus;
	FRenderResourceProvider& ResourceProvider;
	FPrimitiveRenderProxyCommandServices& CommandServices;
	const FShowFlags& ShowFlags;
	EViewMode ViewMode = EViewMode::Lit_BlinnPhong;
	EWorldType WorldType = EWorldType::Editor;
	bool bIncludeEditorOnlyPrimitives = false;
	EPrimitiveRenderProxyCollectIntent Intent = EPrimitiveRenderProxyCollectIntent::Normal;
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
};

class FPrimitiveRenderProxy
{
public:
	virtual ~FPrimitiveRenderProxy() = default;
	virtual void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) = 0;
	virtual void ReleaseResources() {}
};
