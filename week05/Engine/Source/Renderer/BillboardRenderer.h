#pragma once
#include <d3d11.h>
#include "Renderer/Material.h"

struct FRenderMesh;
class FRenderer;

class ENGINE_API FBillboardRenderer
{
public:
	FBillboardRenderer() = default;
	~FBillboardRenderer();

	bool Initialize(FRenderer* InRenderer);
	void Release();

	bool BuildBillboardMesh(const FVector2& Size, FRenderMesh& OutMesh) const;

	std::shared_ptr<FMaterialTexture> GetOrLoadTexture(const std::wstring& Path);

	FMaterial* GetBillboardMeterial() const { return BillboardMaterial.get(); }

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;

	std::shared_ptr<FMaterial> BillboardMaterial = nullptr;

	TMap<std::wstring, std::shared_ptr<FMaterialTexture>> TextureCache;
};
