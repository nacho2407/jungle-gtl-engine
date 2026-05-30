#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Geometry/DebugGeometryTypes.h"
#include "Math/Transform.h"
#include "Object/FName.h"

class USkeletalMeshComponent;
struct FDrawCommandBuffer;
struct FFrameContext;

class FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent);
	~FSkeletalMeshSceneProxy() override;

	void UpdateMaterial() override;
	void UpdateMesh() override;

	bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const override;
	bool PrepareGpuSkinningDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const;
	ID3D11ShaderResourceView* GetSkinMatrixSRV(ID3D11Device* Device, ID3D11DeviceContext* Context) const;

	void BuildPhysicsAssetSolidMesh(const FFrameContext& Frame, FPhysicsDebugSolidMesh& OutMesh) const;
	
private:
	void RebuildSectionDraws();
	USkeletalMeshComponent* GetSkeletalMeshComponent() const;
	bool GetPhysicsAssetBoneWorldTransform(const FName& BoneName, FTransform& OutBoneWorldTM) const;
	void ReleaseSkinMatrixBuffer() const;
	bool UpdateSkinMatrixBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context) const;

private:
	mutable FDynamicVertexBuffer DynamicVertexBuffer;
	mutable uint64 UploadedSkinnedRevision = 0;
	uint32 CachedDynamicVertexCount = 0;
	mutable bool bDynamicBufferNeedsCreate = true;

	mutable ID3D11Buffer* SkinMatrixBuffer = nullptr;
	mutable ID3D11ShaderResourceView* SkinMatrixSRV = nullptr;
	mutable uint32 SkinMatrixCapacity = 0;
	mutable uint64 UploadedSkinMatrixRevision = 0;
};
