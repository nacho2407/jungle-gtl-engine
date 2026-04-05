#include "Material.h"
#include "Shader.h"
#include <cstring>

namespace
{
	uint64 HashCombine64(uint64 Seed, uint64 Value)
	{
		return Seed ^ (Value + 0x9e3779b97f4a7c15ull + (Seed << 6) + (Seed >> 2));
	}
}

FMaterialTexture::~FMaterialTexture()
{
	Release();
}

void FMaterialTexture::Release()
{
	if (bOwnsResources && TextureSRV)
	{
		TextureSRV->Release();
	}
	TextureSRV = nullptr;

	if (bOwnsResources && SamplerState)
	{
		SamplerState->Release();
	}
	SamplerState = nullptr;
	bOwnsResources = true;
}

void FMaterialTexture::Bind(ID3D11DeviceContext* DeviceContext)
{
	DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);
	DeviceContext->PSSetSamplers(0, 1, &SamplerState);
}

// ─── FMaterialConstantBuffer ───

FMaterialConstantBuffer::~FMaterialConstantBuffer()
{
	Release();
}

bool FMaterialConstantBuffer::Create(ID3D11Device* Device, uint32 InSize)
{
	Release();

	// D3D11 상수 버퍼는 ByteWidth가 16의 배수여야 함
	Size = (InSize + 15) & ~15;
	CPUData = new uint8[Size];
	memset(CPUData, 0, Size);

	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = Size;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT Hr = Device->CreateBuffer(&Desc, nullptr, &GPUBuffer);
	if (FAILED(Hr))
	{
		Release();
		return false;
	}

	bDirty = true; // 초기 데이터(0)도 업로드 필요
	return true;
}

bool FMaterialConstantBuffer::SetData(const void* Data, uint32 InSize, uint32 Offset)
{
	if (!CPUData || !Data || Offset + InSize > Size)
	{
		return false;
	}

	uint8* Dest = CPUData + Offset;
	if (memcmp(Dest, Data, InSize) == 0)
	{
		return false;
	}

	memcpy(Dest, Data, InSize);
	bDirty = true;
	return true;
}

void FMaterialConstantBuffer::Upload(ID3D11DeviceContext* DeviceContext)
{
	if (!bDirty || !GPUBuffer)
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	HRESULT Hr = DeviceContext->Map(GPUBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	if (SUCCEEDED(Hr))
	{
		memcpy(Mapped.pData, CPUData, Size);
		DeviceContext->Unmap(GPUBuffer, 0);
	}
	bDirty = false;
}

void FMaterialConstantBuffer::Release()
{
	if (GPUBuffer)
	{
		GPUBuffer->Release();
		GPUBuffer = nullptr;
	}
	delete[] CPUData;
	CPUData = nullptr;
	Size = 0;
	bDirty = false;
}

// ─── FMaterial ───

FMaterial::~FMaterial()
{
	Release();
}

uint64 FMaterial::GetSortId() const
{
	uint64 Key = SortGroupId;

	if (const std::shared_ptr<FMaterialTexture>& Texture = MaterialTexture)
	{
		Key = HashCombine64(Key, reinterpret_cast<uint64>(Texture->GetTextureSRV()));
		Key = HashCombine64(Key, reinterpret_cast<uint64>(Texture->GetSamplerState()));
	}

	return Key;
}

void FMaterial::SetVertexShader(const std::shared_ptr<FVertexShader>& InVS)
{
	VertexShader = InVS;
}

void FMaterial::SetPixelShader(const std::shared_ptr<FPixelShader>& InPS)
{
	PixelShader = InPS;
}

void FMaterial::SetMaterialTexture(const std::shared_ptr<FMaterialTexture>& InTexture)
{
	MaterialTexture = InTexture;
}

int32 FMaterial::CreateConstantBuffer(ID3D11Device* Device, uint32 InSize)
{
	FMaterialConstantBuffer ConstantBuffer;
	if (!ConstantBuffer.Create(Device, InSize))
	{
		return -1;
	}

	ConstantBuffers.push_back(std::move(ConstantBuffer));
	return static_cast<int32>(ConstantBuffers.size() - 1);
}

FMaterialConstantBuffer* FMaterial::GetConstantBuffer(int32 Index)
{
	if (Index < 0 || Index >= static_cast<int32>(ConstantBuffers.size()))
	{
		return nullptr;
	}
	return &ConstantBuffers[Index];
}

void FMaterial::RegisterParameter(const FString& ParamName, int32 BufferIndex, uint32 Offset, uint32 Size)
{
	ParameterMap[ParamName] = { BufferIndex, Offset, Size };
}

bool FMaterial::SetParameterData(const FString& ParamName, const void* Data, uint32 DataSize)
{
	auto It = ParameterMap.find(ParamName);
	if (It == ParameterMap.end())
	{
		return false;
	}

	const FMaterialParameterInfo& Info = It->second;
	const uint32 CopySize = (DataSize < Info.Size) ? DataSize : Info.Size;
	FMaterialConstantBuffer* ConstantBuffer = GetConstantBuffer(Info.BufferIndex);
	if (!ConstantBuffer)
	{
		return false;
	}

	ConstantBuffer->SetData(Data, CopySize, Info.Offset);
	return true;
}

bool FMaterial::GetParameterData(const FString& ParamName, void* OutData, uint32 DataSize) const
{
	auto It = ParameterMap.find(ParamName);
	if (It == ParameterMap.end())
	{
		return false;
	}

	const FMaterialParameterInfo& Info = It->second;
	if (Info.BufferIndex < 0 || Info.BufferIndex >= static_cast<int32>(ConstantBuffers.size()))
	{
		return false;
	}

	const FMaterialConstantBuffer& ConstantBuffer = ConstantBuffers[Info.BufferIndex];
	if (!ConstantBuffer.CPUData || Info.Offset + DataSize > ConstantBuffer.Size)
	{
		return false;
	}

	memcpy(OutData, ConstantBuffer.CPUData + Info.Offset, DataSize);
	return true;
}

FVector4 FMaterial::GetVectorParameter(const FString& ParamName) const
{
	FVector4 Result(1.0f, 1.0f, 1.0f, 1.0f);
	float Data[4] = { 0.0f };

	if (GetParameterData(ParamName, Data, sizeof(Data)))
	{
		Result = FVector4(Data[0], Data[1], Data[2], Data[3]);
	}

	return Result;
}

std::unique_ptr<FDynamicMaterial> FMaterial::CreateDynamicMaterial() const
{
	ID3D11Device* Device = nullptr;
	for (const FMaterialConstantBuffer& ConstantBuffer : ConstantBuffers)
	{
		if (ConstantBuffer.GPUBuffer)
		{
			ConstantBuffer.GPUBuffer->GetDevice(&Device);
			break;
		}
	}

	if (!Device)
	{
		return nullptr;
	}

	auto Dynamic = std::make_unique<FDynamicMaterial>();
	Dynamic->OriginName = OriginName;
	Dynamic->InstanceName = OriginName + "_Dynamic";
	Dynamic->VertexShader = VertexShader;
	Dynamic->PixelShader = PixelShader;
	Dynamic->ParameterMap = ParameterMap;
	Dynamic->MaterialTexture = MaterialTexture;
	Dynamic->SortGroupId = SortGroupId;

	for (const FMaterialConstantBuffer& ConstantBuffer : ConstantBuffers)
	{
		FMaterialConstantBuffer NewConstantBuffer;
		if (NewConstantBuffer.Create(Device, ConstantBuffer.Size))
		{
			if (ConstantBuffer.CPUData && NewConstantBuffer.CPUData)
			{
				memcpy(NewConstantBuffer.CPUData, ConstantBuffer.CPUData, ConstantBuffer.Size);
				NewConstantBuffer.bDirty = true;
			}
		}
		Dynamic->ConstantBuffers.push_back(std::move(NewConstantBuffer));
	}

	Device->Release();
	return Dynamic;
}

// ─── FDynamicMaterial ───

bool FDynamicMaterial::SetScalarParameter(const FString& ParamName, float Value)
{
	return SetParameterData(ParamName, &Value, sizeof(float));
}

bool FDynamicMaterial::SetVectorParameter(const FString& ParamName, const FVector4& Value)
{
	float Data[4] = { Value.X, Value.Y, Value.Z, Value.W };
	return SetParameterData(ParamName, Data, sizeof(Data));
}

bool FDynamicMaterial::SetVector3Parameter(const FString& ParamName, const FVector& Value)
{
	float Data[3] = { Value.X, Value.Y, Value.Z };
	return SetParameterData(ParamName, Data, sizeof(Data));
}

void FMaterial::Bind(ID3D11DeviceContext* DeviceContext)
{
	if (!DeviceContext)
	{
		return;
	}

	if (VertexShader)
	{
		VertexShader->Bind(DeviceContext);
	}
	else
	{
		DeviceContext->VSSetShader(nullptr, nullptr, 0);
	}

	if (PixelShader)
	{
		PixelShader->Bind(DeviceContext);
	}
	else
	{
		DeviceContext->PSSetShader(nullptr, nullptr, 0);
	}

	if (MaterialTexture)
	{
		MaterialTexture->Bind(DeviceContext);
	}
	else
	{
		ID3D11ShaderResourceView* NullTexture = nullptr;
		ID3D11SamplerState* NullSampler = nullptr;
		DeviceContext->PSSetShaderResources(0, 1, &NullTexture);
		DeviceContext->PSSetSamplers(0, 1, &NullSampler);
	}

	for (int32 Index = 0; Index < static_cast<int32>(ConstantBuffers.size()); ++Index)
	{
		ConstantBuffers[Index].Upload(DeviceContext);
		const UINT Slot = MaterialCBStartSlot + static_cast<UINT>(Index);
		ID3D11Buffer* Buffer = ConstantBuffers[Index].GPUBuffer;
		DeviceContext->VSSetConstantBuffers(Slot, 1, &Buffer);
		DeviceContext->PSSetConstantBuffers(Slot, 1, &Buffer);
	}
}

void FMaterial::Release()
{
	VertexShader.reset();
	PixelShader.reset();
	MaterialTexture.reset();

	for (FMaterialConstantBuffer& ConstantBuffer : ConstantBuffers)
	{
		ConstantBuffer.Release();
	}
	ConstantBuffers.clear();
}
