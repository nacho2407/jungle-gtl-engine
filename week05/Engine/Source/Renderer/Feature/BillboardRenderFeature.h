#pragma once

#include "CoreMinimal.h"
#include "Renderer/BillboardRenderer.h"
#include "Renderer/RenderFeatureInterfaces.h"

class FRenderer;

class ENGINE_API FBillboardRenderFeature final : public ISceneBillboardFeature
{
public:
	bool Initialize(FRenderer& Renderer);
	void Release();

	FMaterial* GetBaseMaterial() const override;
	bool BuildMesh(const FVector2& Size, FRenderMesh& OutMesh) const override;
	FMaterial* GetOrCreateMaterial(const UBillboardComponent& Component) override;
	void PruneMaterials(const TArray<const UBillboardComponent*>& ActiveComponents) override;

	FBillboardRenderer& GetRenderer() { return BillboardRenderer; }
	const FBillboardRenderer& GetRenderer() const { return BillboardRenderer; }

private:
	FBillboardRenderer BillboardRenderer;
};
