#pragma once

#include "Render/Proxy/BillboardSceneProxy.h"

class USubUVComponent;

// ============================================================
// FSubUVSceneProxy — USubUVComponent 전용 프록시
// ============================================================
// SubUVBatcher 경유 렌더링 (bBatcherRendered=true).
class FSubUVSceneProxy : public FBillboardSceneProxy
{
public:
	FSubUVSceneProxy(USubUVComponent* InComponent);

	void UpdateMesh() override;
	void CollectEntries(FRenderBus& Bus) override;

private:
	USubUVComponent* GetSubUVComponent() const;
};
