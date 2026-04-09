#pragma once

#include <d3d11.h>

#include "Renderer/Material.h"

struct FRenderMesh;
class FRenderer;
class UBillboardComponent;

class ENGINE_API FBillboardRenderer
{
public:
	FBillboardRenderer() = default;
	~FBillboardRenderer();

	bool Initialize(FRenderer& InRenderer);
	void Release();

	FMaterial* GetBaseMaterial() const { return BillboardMaterial.get(); }
	bool BuildMesh(const FVector2& Size, FRenderMesh& OutMesh) const;
	FMaterial* GetOrCreateMaterial(const UBillboardComponent& Component);
	void PruneMaterials(const TArray<const UBillboardComponent*>& ActiveComponents);

	const TMap<std::wstring, std::shared_ptr<FMaterialTexture>>& GetTextureCache() const
	{
		return TextureCache;
	}

private:
	std::shared_ptr<FMaterialTexture> GetOrLoadTexture(const std::wstring& Path);

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	std::shared_ptr<FMaterial> BillboardMaterial = nullptr;
	TMap<std::wstring, std::shared_ptr<FMaterialTexture>> TextureCache;
	TMap<const UBillboardComponent*, std::shared_ptr<FDynamicMaterial>> MaterialsByComponent;
};
