#pragma once

#include "Render/Scene/Proxies/Primitive/PrimitiveSceneProxy.h"

class UDecalComponent;

// ============================================================
// FDecalSceneProxy — UDecalComponent 전용 프록시
// ============================================================
// 월드에 투영되는 데칼 정보를 관리한다.
// OBB(Oriented Bounding Box)를 기반으로 중첩되는 프리미티브에 텍스처를 투영
class FDecalSceneProxy : public FPrimitiveSceneProxy
{
public:
	FDecalSceneProxy(UDecalComponent* InComponent);
	~FDecalSceneProxy() override;

	void UpdateMaterial() override;
	void UpdateMesh() override;

private:
	UDecalComponent* GetDecalComponent() const;

	FConstantBuffer* DecalCB;
	class UMaterial* DecalMaterial = nullptr;
};
