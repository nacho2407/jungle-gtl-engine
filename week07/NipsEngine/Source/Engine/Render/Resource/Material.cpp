#include "Material.h"
#include "Core/ResourceManager.h"

DEFINE_CLASS(UMaterialInterface, UObject)
DEFINE_CLASS(UMaterial, UMaterialInterface)
DEFINE_CLASS(UMaterialInstance, UMaterial)


void UMaterial::Bind(ID3D11DeviceContext* Context) const
{
	if (!Shader) return;
	Shader->Bind(Context);

	auto DSState = FResourceManager::Get().GetOrCreateDepthStencilState(DepthStencilType);
	auto BlendState = FResourceManager::Get().GetOrCreateBlendState(BlendType);
	auto RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(RasterizerType);
	auto Sampler = FResourceManager::Get().GetOrCreateSamplerState(SamplerType);
	
	// TODO: Render Pipeline Stalling 방지 추가 필요
	Context->OMSetDepthStencilState(DSState, 0);
	Context->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
	Context->RSSetState(RasterizerState);
	Context->PSSetSamplers(0, 1, &Sampler);

	ApplyParams(Context, MaterialParams);
}

void UMaterial::ApplyParams(ID3D11DeviceContext* Context, const TMap<FString, FMaterialParamValue>& Params) const
{
	TArray<uint8> CBufferData(Shader->GetCBufferSize());

	for (const auto& [Name, ParamValue] : Params)
	{
		FShaderVariableInfo VarInfo;
		if (Shader->GetShaderVariableInfo(Name, VarInfo))
		{
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
		else
		{
			if (ParamValue.Type == EMaterialParamType::Texture && std::holds_alternative<UTexture*>(ParamValue.Value))
			{
				int32 Slot = Shader->GetTextureBindSlot(Name);
				if (Slot >= 0)
				{
					UTexture* TextureAsset = std::get<UTexture*>(ParamValue.Value);
					if (TextureAsset)
					{
						ID3D11ShaderResourceView* SRV = TextureAsset->GetSRV();
						Context->PSSetShaderResources(Slot, 1, &SRV);
					}
				}
			}
		}
	}

	Shader->UpdateAndBindCBuffer(Context, CBufferData.data(), 2, CBufferData.size());
}

void UMaterialInstance::Bind(ID3D11DeviceContext* Context) const
{
	if (!Parent) return;

	Parent->Bind(Context);

	TMap<FString, FMaterialParamValue> CombinedParams;
	Parent->GatherAllParams(CombinedParams);
	for (const auto& [Name, Value] : OverridedParams)
	{
		CombinedParams[Name] = Value;
	}

	Parent->ApplyParams(Context, CombinedParams);
}
