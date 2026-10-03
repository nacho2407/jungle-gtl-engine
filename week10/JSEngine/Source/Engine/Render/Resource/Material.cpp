#include "Material.h"
#include "Core/ResourceManager.h"

#include <cstring>

DEFINE_CLASS(UMaterialInterface, UObject)
DEFINE_CLASS(UMaterial, UMaterialInterface)
DEFINE_CLASS(UMaterialInstance, UMaterialInterface)

namespace
{
	uint32 AlignConstantBufferSize(uint32 Size)
	{
		return (Size + 15u) & ~15u;
	}

    uint32 GetMaterialConstantBufferSlot(const FShaderReflectionInfo& Reflection)
	{
	    return 2;
	}

	void CopyMaterialParamToBuffer(TArray<uint8>& CBufferData, const FShaderVariableReflection& VarInfo, const FMaterialParamValue& ParamValue)
	{
		if (VarInfo.Offset + VarInfo.Size > CBufferData.size())
		{
			return;
		}

		switch (ParamValue.Type)
		{
		case EMaterialParamType::Bool:
		{
			uint32 BoolVal = std::get<bool>(ParamValue.Value) ? 1 : 0;
			std::memcpy(CBufferData.data() + VarInfo.Offset, &BoolVal, sizeof(uint32));
			break;
		}
		case EMaterialParamType::Int:
		{
			int32 Val = std::get<int32>(ParamValue.Value);
			std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(int32));
			break;
		}
		case EMaterialParamType::UInt:
		{
			uint32 UIntVal = std::get<uint32>(ParamValue.Value);
			std::memcpy(CBufferData.data() + VarInfo.Offset, &UIntVal, sizeof(uint32));
			break;
		}
		case EMaterialParamType::Float:
		{
			float Val = std::get<float>(ParamValue.Value);
			std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(float));
			break;
		}
		case EMaterialParamType::Vector2:
		{
			FVector2 Val = std::get<FVector2>(ParamValue.Value);
			std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(FVector2));
			break;
		}
		case EMaterialParamType::Vector3:
		{
			FVector Val = std::get<FVector>(ParamValue.Value);
			std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(FVector));
			break;
		}
		case EMaterialParamType::Vector4:
		{
			FVector4 Val = std::get<FVector4>(ParamValue.Value);
			std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(FVector4));
			break;
		}
		case EMaterialParamType::Matrix4:
		{
			FMatrix Val = std::get<FMatrix>(ParamValue.Value);
			std::memcpy(CBufferData.data() + VarInfo.Offset, &Val, sizeof(FMatrix));
			break;
		}
		default:
			break;
		}
	}
}

UMaterial::~UMaterial()
{
	ReleaseMaterialConstantBuffer();
}

void UMaterial::ReleaseMaterialConstantBuffer() const
{
	if (MaterialConstantBuffer)
	{
		MaterialConstantBuffer->Release();
		MaterialConstantBuffer = nullptr;
	}
	MaterialConstantBufferSize = 0;
}

void UMaterial::BindRenderStates(ID3D11DeviceContext* Context) const
{
	if (!Context)
	{
		return;
	}

	auto DSState = FResourceManager::Get().GetOrCreateDepthStencilState(DepthStencilType);
	auto BlendState = FResourceManager::Get().GetOrCreateBlendState(BlendType);
	auto RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(RasterizerType);
	auto Sampler = FResourceManager::Get().GetOrCreateSamplerState(SamplerType);
	
	// TODO: Render Pipeline Stalling 방지 추가 필요
	Context->OMSetDepthStencilState(DSState, 0);
	Context->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
	Context->RSSetState(RasterizerState);
	Context->PSSetSamplers(0, 1, &Sampler);
}

void UMaterial::BindParameters(ID3D11DeviceContext* Context, const FPixelShader* PixelShader) const
{
	ApplyParams(Context, MaterialParams, PixelShader);
}

void UMaterial::ApplyParams(ID3D11DeviceContext* Context, const TMap<FString, FMaterialParamValue>& Params, const FPixelShader* PixelShader) const
{
	if (!Context || !PixelShader)
	{
		return;
	}

	const FShaderReflectionInfo& Reflection = PixelShader->Reflection;
	const uint32 RawCBufferSize = Reflection.ConstantBufferSize;
	if (RawCBufferSize > 0)
	{
		const uint32 CBufferSize = AlignConstantBufferSize(RawCBufferSize);
		TArray<uint8> CBufferData(CBufferSize);

		for (const auto& [Name, ParamValue] : Params)
		{
			auto VarIt = Reflection.Variables.find(Name);
			if (VarIt != Reflection.Variables.end())
			{
				CopyMaterialParamToBuffer(CBufferData, VarIt->second, ParamValue);
			}
		}

		if (!MaterialConstantBuffer || MaterialConstantBufferSize != CBufferSize)
		{
			ReleaseMaterialConstantBuffer();

			ID3D11Device* Device = nullptr;
			Context->GetDevice(&Device);
			if (Device)
			{
				D3D11_BUFFER_DESC Desc = {};
				Desc.Usage = D3D11_USAGE_DEFAULT;
				Desc.ByteWidth = CBufferSize;
				Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

				if (SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &MaterialConstantBuffer)))
				{
					MaterialConstantBufferSize = CBufferSize;
				}
				Device->Release();
			}
		}

		if (MaterialConstantBuffer)
		{
			const uint32 Slot = GetMaterialConstantBufferSlot(Reflection);
			Context->UpdateSubresource(MaterialConstantBuffer, 0, nullptr, CBufferData.data(), 0, 0);
			Context->VSSetConstantBuffers(Slot, 1, &MaterialConstantBuffer);
			Context->PSSetConstantBuffers(Slot, 1, &MaterialConstantBuffer);
		}
	}

	for (const auto& [Name, ParamValue] : Params)
	{
		if (ParamValue.Type != EMaterialParamType::Texture || !std::holds_alternative<UTexture*>(ParamValue.Value))
		{
			continue;
		}

		auto SlotIt = Reflection.TextureBindSlots.find(Name);
		if (SlotIt == Reflection.TextureBindSlots.end())
		{
			continue;
		}

		UTexture* TextureAsset = std::get<UTexture*>(ParamValue.Value);
		if (TextureAsset)
		{
			ID3D11ShaderResourceView* SRV = TextureAsset->GetSRV();
			const uint32 Slot = SlotIt->second;
			Context->PSSetShaderResources(Slot, 1, &SRV);
		}
	}
}

void UMaterialInstance::BindRenderStates(ID3D11DeviceContext* Context) const
{
	if (Parent)
	{
		Parent->BindRenderStates(Context);
	}
}

void UMaterialInstance::BindParameters(ID3D11DeviceContext* Context, const FPixelShader* PixelShader) const
{
	if (!Parent)
	{
		return;
	}

	TMap<FString, FMaterialParamValue> CombinedParams;
	Parent->GatherAllParams(CombinedParams);
	for (const auto& [Name, Value] : OverridedParams)
	{
		CombinedParams[Name] = Value;
	}

	Parent->ApplyParams(Context, CombinedParams, PixelShader);
}
