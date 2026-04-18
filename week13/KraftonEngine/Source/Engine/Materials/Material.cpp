#include "Materials/Material.h"
#include "Serialization/Archive.h"
#include "Render/Resource/Shader.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Types/MaterialTextureSlot.h"

IMPLEMENT_CLASS(UMaterial, UObject)

// ─── FMaterialTemplate ───

void FMaterialTemplate::Create(FShader* InShader)
{
	ParameterLayout = InShader->GetParameterLayout(); // 셰이더에서 리플렉션된 파라미터 레이아웃 정보 확보
	Shader = InShader;
}

bool FMaterialTemplate::GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const
{
	auto it = ParameterLayout.find(Name);
	if (it != ParameterLayout.end())
	{
		OutInfo = *(it->second);
		return true;
	}
	else
	{
		return false;
	}
}

// ─── FMaterialConstantBuffer ───

FMaterialConstantBuffer::~FMaterialConstantBuffer()
{
	Release();
}

void FMaterialConstantBuffer::Init(ID3D11Device* InDevice, uint32 InSize, uint32 InSlot)
{
	Release();

	uint32 AlignedSize = (InSize + 15) & ~15;
	GPUBuffer.Create(InDevice, AlignedSize);
	CPUData = new uint8[AlignedSize]();
	Size = AlignedSize;
	SlotIndex = InSlot;
	bDirty = true;
}

void FMaterialConstantBuffer::SetData(const void* Data, uint32 InSize, uint32 Offset)
{
	if (!CPUData || Offset + InSize > Size)
	{
		return;
	}
	memcpy(CPUData + Offset, Data, InSize);
	bDirty = true;
}

void FMaterialConstantBuffer::Upload(ID3D11DeviceContext* DeviceContext)
{
	if (!bDirty)
		return;

	GPUBuffer.Update(DeviceContext, CPUData, Size);
	bDirty = false;
}

void FMaterialConstantBuffer::Release()
{
	GPUBuffer.Release();
	delete[] CPUData;
	CPUData = nullptr;
	Size = 0;
	bDirty = false;
}

// ─── UMaterial ───

UMaterial::~UMaterial()
{
	for (auto& Pair : ConstantBufferMap)
	{
		Pair.second->Release();
	}
	ConstantBufferMap.clear();

	for (auto& Pair : TextureParameters)
	{
		Pair.second = nullptr;
	}
}

void UMaterial::Create(const FString& InPathFileName, FMaterialTemplate* InTemplate,
	ERenderPass InRenderPass,
	EBlendState InBlend,
	EDepthStencilState InDepth,
	ERasterizerState InRaster,
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers)
{
	PathFileName = InPathFileName;
	Template = InTemplate;
	RenderPass = InRenderPass;
	BlendState = InBlend;
	DepthStencilState = InDepth;
	RasterizerState = InRaster;

	ConstantBufferMap = std::move(InBuffers);
}

bool UMaterial::SetParameter(const FString& Name, const void* Data, uint32 Size)
{
	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(Name, Info)) {
		return false;
	}
	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	It->second->SetData(Data, Size, Info.Offset);
	It->second->bDirty = true;

	It->second->Upload(GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext());
	return true;
}


bool UMaterial::SetScalarParameter(const FString& ParamName, float Value)
{
	return SetParameter(ParamName, &Value, sizeof(float));
}

bool UMaterial::SetVector3Parameter(const FString& ParamName, const FVector& Value)
{
	float Data[3] = { Value.X, Value.Y, Value.Z };
	return SetParameter(ParamName, Data, sizeof(Data));
}

bool UMaterial::SetVector4Parameter(const FString& ParamName, const FVector4& Value)
{
	float Data[4] = { Value.X, Value.Y, Value.Z, Value.W };
	return SetParameter(ParamName, Data, sizeof(Data));
}

bool UMaterial::SetTextureParameter(const FString& ParamName, UTexture2D* Texture)
{
	TextureParameters[ParamName] = Texture;

	// CachedSRVs 갱신 — 슬롯 이름과 매칭되면 즉시 반영
	for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
	{
		FString SlotName = MaterialTextureSlot::ToString(s) + "Texture";
		if (ParamName == SlotName)
		{
			CachedSRVs[s] = (Texture && Texture->GetSRV()) ? Texture->GetSRV() : nullptr;
			break;
		}
	}

	return true;
}

bool UMaterial::SetMatrixParameter(const FString& ParamName, const FMatrix& Value)
{
	return SetParameter(ParamName, Value.Data, sizeof(float) * 16);
}

bool UMaterial::GetScalarParameter(const FString& ParamName, float& OutValue) const
{
	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(ParamName, Info)) return false;

	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	const uint8* Ptr = It->second->CPUData + Info.Offset;
	OutValue = *reinterpret_cast<const float*>(Ptr);
	return true;
}

bool UMaterial::GetVector3Parameter(const FString& ParamName, FVector& OutValue) const
{
	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(ParamName, Info)) return false;

	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	const uint8* Ptr = It->second->CPUData + Info.Offset;
	OutValue = *reinterpret_cast<const FVector*>(Ptr);
	return true;
}

bool UMaterial::GetVector4Parameter(const FString& ParamName, FVector4& OutValue) const
{
	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(ParamName, Info)) return false;

	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	const uint8* Ptr = It->second->CPUData + Info.Offset;
	OutValue = *reinterpret_cast<const FVector4*>(Ptr);
	return true;
}

bool UMaterial::GetTextureParameter(const FString& ParamName, UTexture2D*& OutTexture) const
{
	auto It = TextureParameters.find(ParamName);
	if (It == TextureParameters.end()) return false;

	OutTexture = It->second;
	return true;
}

bool UMaterial::GetMatrixParameter(const FString& ParamName, FMatrix& Value) const
{
	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(ParamName, Info)) return false;

	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	const uint8* Ptr = It->second->CPUData + Info.Offset;
	memcpy(Value.Data, Ptr, sizeof(float) * 16);
	return true;
}

void UMaterial::Bind(ID3D11DeviceContext* Context)
{
}

const FString& UMaterial::GetTexturePathFileName(const FString& TextureName)const
{
	auto it = TextureParameters.find(TextureName);
	if (it != TextureParameters.end())
	{
		UTexture2D* Texture = it->second;
		if(Texture)
		{
			return Texture->GetSourcePath();
		}
	}
	static const FString EmptyString;
	return EmptyString;
}

void UMaterial::RebuildCachedSRVs()
{
	for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
	{
		CachedSRVs[s] = nullptr;
		UTexture2D* Tex = nullptr;
		FString SlotName = MaterialTextureSlot::ToString(s) + "Texture";
		if (GetTextureParameter(SlotName, Tex) && Tex && Tex->GetSRV())
			CachedSRVs[s] = Tex->GetSRV();
	}
}

void UMaterial::Serialize(FArchive& Ar)
{
	Ar << PathFileName;

	uint32 BufferCount = static_cast<uint32>(ConstantBufferMap.size());
	Ar << BufferCount;

	if (Ar.IsSaving())
	{
		for (auto& Pair : ConstantBufferMap)
		{
			FString BufferName = Pair.first;
			uint32 Size = Pair.second->Size;

			Ar << BufferName;
			Ar << Size;
			Ar.Serialize(Pair.second->CPUData, Size);
		}
	}

	if (Ar.IsLoading())
	{
		for (uint32 i = 0; i < BufferCount; ++i)
		{
			FString BufferName;
			uint32 Size = 0;

			Ar << BufferName;
			Ar << Size;

			auto It = ConstantBufferMap.find(BufferName);
			if (It != ConstantBufferMap.end())
			{
				Ar.Serialize(It->second->CPUData, Size);
				It->second->bDirty = true;
				It->second->Upload(GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext());
			}
			else
			{
				TArray<uint8> Dummy(Size);
				Ar.Serialize(Dummy.data(), Size);
			}
		}
	}
	
	uint32 TextureCount = static_cast<uint32>(TextureParameters.size());
	Ar << TextureCount;

	if (Ar.IsSaving())
	{
		for (auto& Pair : TextureParameters)
		{
			FString SlotName = Pair.first;
			FString TexturePath = Pair.second ? Pair.second->GetSourcePath() : FString();

			Ar << SlotName;
			Ar << TexturePath;
		}
	}
	else // IsLoading
	{
		for (uint32 i = 0; i < TextureCount; ++i)
		{
			FString SlotName;
			FString TexturePath;

			Ar << SlotName;
			Ar << TexturePath;

			if (!TexturePath.empty())
			{
				ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
				UTexture2D* Loaded = UTexture2D::LoadFromFile(TexturePath, Device);
				if (Loaded)
				{
					TextureParameters[SlotName] = Loaded;
				}
			}
		}

		RebuildCachedSRVs();
	}
}
