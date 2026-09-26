#include "Renderer/Feature/BillboardRenderFeature.h"

#include "Renderer/Renderer.h"

bool FBillboardRenderFeature::Initialize(FRenderer& Renderer)
{
	return BillboardRenderer.Initialize(Renderer);
}

void FBillboardRenderFeature::Release()
{
	BillboardRenderer.Release();
}

FMaterial* FBillboardRenderFeature::GetBaseMaterial() const
{
	return BillboardRenderer.GetBaseMaterial();
}

bool FBillboardRenderFeature::BuildMesh(const FVector2& Size, FRenderMesh& OutMesh) const
{
	return BillboardRenderer.BuildMesh(Size, OutMesh);
}

FMaterial* FBillboardRenderFeature::GetOrCreateMaterial(const UBillboardComponent& Component)
{
	return BillboardRenderer.GetOrCreateMaterial(Component);
}

void FBillboardRenderFeature::PruneMaterials(const TArray<const UBillboardComponent*>& ActiveComponents)
{
	BillboardRenderer.PruneMaterials(ActiveComponents);
}
