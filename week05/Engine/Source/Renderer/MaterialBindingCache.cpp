#include "Renderer/MaterialBindingCache.h"

#include "Renderer/Material.h"
#include "Renderer/Shader.h"

void FMaterialBindingCache::Reset()
{
	BoundMaterial = nullptr;
	BoundMaterialRevision = 0;
	BoundVertexShader = nullptr;
	BoundPixelShader = nullptr;
	BoundTextureSRV = nullptr;
	BoundSamplerState = nullptr;
	BoundMaterialConstantBufferCount = 0;
	BoundVSConstantBuffers.clear();
	BoundPSConstantBuffers.clear();
}

void FMaterialBindingCache::BindMaterial(ID3D11DeviceContext* DeviceContext, FMaterial* Material)
{
	if (!DeviceContext || !Material)
	{
		return;
	}

	const uint32 MaterialRevision = Material->GetBindingRevision();
	if (BoundMaterial == Material && BoundMaterialRevision == MaterialRevision && !Material->HasDirtyConstantBuffers())
	{
		return;
	}

	const FVertexShader* VertexShader = Material->GetVertexShader();
	if (VertexShader != BoundVertexShader)
	{
		if (VertexShader)
		{
			VertexShader->Bind(DeviceContext);
		}
		else
		{
			DeviceContext->VSSetShader(nullptr, nullptr, 0);
		}
		BoundVertexShader = VertexShader;
	}

	const FPixelShader* PixelShader = Material->GetPixelShader();
	if (PixelShader != BoundPixelShader)
	{
		if (PixelShader)
		{
			PixelShader->Bind(DeviceContext);
		}
		else
		{
			DeviceContext->PSSetShader(nullptr, nullptr, 0);
		}
		BoundPixelShader = PixelShader;
	}

	ID3D11ShaderResourceView* TextureSRV = nullptr;
	ID3D11SamplerState* SamplerState = nullptr;
	if (const std::shared_ptr<FMaterialTexture>& MaterialTexture = Material->GetMaterialTexture())
	{
		TextureSRV = MaterialTexture->GetTextureSRV();
		SamplerState = MaterialTexture->GetSamplerState();
	}
	BindTexture(DeviceContext, TextureSRV, SamplerState);

	const TArray<FMaterialConstantBuffer>& ConstantBuffers = Material->GetConstantBuffers();
	const UINT NewMaterialConstantBufferCount = static_cast<UINT>(ConstantBuffers.size());
	if (BoundVSConstantBuffers.size() < ConstantBuffers.size())
	{
		BoundVSConstantBuffers.resize(ConstantBuffers.size(), nullptr);
	}
	if (BoundPSConstantBuffers.size() < ConstantBuffers.size())
	{
		BoundPSConstantBuffers.resize(ConstantBuffers.size(), nullptr);
	}

	for (int32 Index = 0; Index < static_cast<int32>(ConstantBuffers.size()); ++Index)
	{
		const FMaterialConstantBuffer& ConstantBuffer = ConstantBuffers[Index];
		const_cast<FMaterialConstantBuffer&>(ConstantBuffer).Upload(DeviceContext);

		const UINT Slot = FMaterial::MaterialCBStartSlot + static_cast<UINT>(Index);
		ID3D11Buffer* Buffer = ConstantBuffer.GPUBuffer;
		if (BoundVSConstantBuffers[Index] != Buffer)
		{
			DeviceContext->VSSetConstantBuffers(Slot, 1, &Buffer);
			BoundVSConstantBuffers[Index] = Buffer;
		}
		if (BoundPSConstantBuffers[Index] != Buffer)
		{
			DeviceContext->PSSetConstantBuffers(Slot, 1, &Buffer);
			BoundPSConstantBuffers[Index] = Buffer;
		}
	}

	if (NewMaterialConstantBufferCount < BoundMaterialConstantBufferCount)
	{
		ID3D11Buffer* NullBuffer = nullptr;
		for (UINT Index = NewMaterialConstantBufferCount; Index < BoundMaterialConstantBufferCount; ++Index)
		{
			const UINT Slot = FMaterial::MaterialCBStartSlot + Index;
			DeviceContext->VSSetConstantBuffers(Slot, 1, &NullBuffer);
			DeviceContext->PSSetConstantBuffers(Slot, 1, &NullBuffer);
			if (Index < BoundVSConstantBuffers.size())
			{
				BoundVSConstantBuffers[Index] = nullptr;
			}
			if (Index < BoundPSConstantBuffers.size())
			{
				BoundPSConstantBuffers[Index] = nullptr;
			}
		}
	}

	BoundMaterialConstantBufferCount = NewMaterialConstantBufferCount;
	BoundMaterial = Material;
	BoundMaterialRevision = MaterialRevision;
}

void FMaterialBindingCache::BindTexture(ID3D11DeviceContext* DeviceContext, ID3D11ShaderResourceView* InTextureSRV, ID3D11SamplerState* InSamplerState)
{
	if (BoundTextureSRV != InTextureSRV)
	{
		DeviceContext->PSSetShaderResources(0, 1, &InTextureSRV);
		BoundTextureSRV = InTextureSRV;
	}

	if (BoundSamplerState != InSamplerState)
	{
		DeviceContext->PSSetSamplers(0, 1, &InSamplerState);
		BoundSamplerState = InSamplerState;
	}
}
