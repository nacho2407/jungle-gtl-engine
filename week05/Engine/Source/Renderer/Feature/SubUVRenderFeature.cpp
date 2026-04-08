#include "Renderer/Feature/SubUVRenderFeature.h"

#include "Renderer/Renderer.h"

bool FSubUVRenderFeature::Initialize(FRenderer& Renderer, const std::wstring& TexturePath)
{
	return SubUVRenderer.Initialize(&Renderer, TexturePath);
}

void FSubUVRenderFeature::Release()
{
	SubUVRenderer.Release();
}

FMaterial* FSubUVRenderFeature::GetBaseMaterial() const
{
	return SubUVRenderer.GetSubUVMaterial();
}

bool FSubUVRenderFeature::BuildMesh(const FVector2& Size, FRenderMesh& OutMesh) const
{
	return SubUVRenderer.BuildSubUVMesh(Size, OutMesh);
}
