#pragma once

#include "Core/Types/CoreTypes.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Resource/Buffer.h"

class UClothComponent;
class UMaterial;
class FReferenceCollector;
struct FDrawCommandBuffer;

/**
 * @brief Cloth component 전용 scene proxy
 */
class FClothSceneProxy : public FPrimitiveSceneProxy
{
public:
	/**
	 * @brief Cloth component의 render proxy를 생성합니다
	 *
	 * @param InComponent proxy owner cloth component
	 */
	explicit FClothSceneProxy(UClothComponent* InComponent);
	~FClothSceneProxy() override = default;

	/**
	 * @brief material 변경 사항을 section draw에 반영합니다
	 */
	void UpdateMaterial() override;

	/**
	 * @brief cloth render data 변경 사항을 proxy cache에 반영합니다
	 */
	void UpdateMesh() override;

	/**
	 * @brief cloth CPU render data를 dynamic GPU buffer로 준비합니다
	 *
	 * @param Device dynamic buffer 생성에 사용할 D3D device
	 *
	 * @param Context dynamic buffer upload에 사용할 D3D context
	 *
	 * @param OutBuffer draw command에 전달할 buffer 정보
	 *
	 * @return draw buffer 준비 성공 여부
	 */
	bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const override;

	/**
	 * @brief GC reference collector에 proxy material 참조를 추가합니다
	 *
	 * @param Collector object reference collector
	 */
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	/**
	 * @brief owner를 cloth component로 반환합니다
	 *
	 * @return owner cloth component
	 */
	UClothComponent* GetClothComponent() const;

	/**
	 * @brief cloth render section을 draw section으로 다시 구성합니다
	 */
	void RebuildSectionDraws();

private:
	mutable FDynamicVertexBuffer DynamicVertexBuffer;
	mutable FDynamicIndexBuffer DynamicIndexBuffer;
	mutable uint64 UploadedRevision = 0;
	mutable bool bDynamicBuffersNeedCreate = true;
	uint32 CachedVertexCount = 0;
	uint32 CachedIndexCount = 0;
	UMaterial* CachedMaterial = nullptr;
};
