#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>

class FMaterial;

class ENGINE_API FMaterialBindingCache
{
public:
	void Reset();
	void BindMaterial(ID3D11DeviceContext* DeviceContext, FMaterial* Material);

private:
	void BindTexture(ID3D11DeviceContext* DeviceContext, ID3D11ShaderResourceView* InTextureSRV, ID3D11SamplerState* InSamplerState);

private:
	FMaterial* BoundMaterial = nullptr;
	uint32 BoundMaterialRevision = 0;
	const class FVertexShader* BoundVertexShader = nullptr;
	const class FPixelShader* BoundPixelShader = nullptr;
	ID3D11ShaderResourceView* BoundTextureSRV = nullptr;
	ID3D11SamplerState* BoundSamplerState = nullptr;
	uint32 BoundMaterialConstantBufferCount = 0;
	TArray<ID3D11Buffer*> BoundVSConstantBuffers;
	TArray<ID3D11Buffer*> BoundPSConstantBuffers;
};
