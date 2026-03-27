#include "Material.h"
#include "Shader.h"
#include <cstring>


FMaterialTexture::~FMaterialTexture()
{
	Release();
}

void FMaterialTexture::Release()
{
	if (TextureSRV)
	{
		TextureSRV->Release();
		TextureSRV = nullptr;
	}

	if (SamplerState)
	{
		SamplerState->Release();
		SamplerState = nullptr;
	}
}

void FMaterialTexture::Bind(ID3D11DeviceContext* DeviceContext)
{
	DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);
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
	if (!bDirty || !GPUBuffer)
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped;
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
	return ShaderId;
}

int32 FMaterial::CreateConstantBuffer(ID3D11Device* Device, uint32 InSize)
{
	FMaterialConstantBuffer CB;
	if (!CB.Create(Device, InSize))
	{
		return -1;
	}
	ConstantBuffers.push_back(std::move(CB));
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
	if (DataSize > Info.Size)
	{
		return false;
	}

	FMaterialConstantBuffer* CB = GetConstantBuffer(Info.BufferIndex);
	if (!CB)
	{
		return false;
	}

	CB->SetData(Data, DataSize, Info.Offset);
	return true;
}

std::unique_ptr<FDynamicMaterial> FMaterial::CreateDynamicMaterial() const
{
	ID3D11Device* Device = nullptr;
	for (const auto& CB : ConstantBuffers)
	{
		if (CB.GPUBuffer)
		{
			CB.GPUBuffer->GetDevice(&Device);
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
	Dynamic->RasterizerOption = RasterizerOption;
	Dynamic->DepthStencilOption = DepthStencilOption;
	Dynamic->BlendOption = BlendOption;
	Dynamic->RasterizerState = RasterizerState;
	Dynamic->DepthStencilState = DepthStencilState;
	Dynamic->BlendState = BlendState;
	Dynamic->SetMaterialTexture(MaterialTexture);

	for (const auto& CB : ConstantBuffers)
	{
		FMaterialConstantBuffer NewCB;
		if (NewCB.Create(Device, CB.Size))
		{
			if (CB.CPUData && NewCB.CPUData)
			{
				memcpy(NewCB.CPUData, CB.CPUData, CB.Size);
				NewCB.bDirty = true;
			}
		}
		Dynamic->ConstantBuffers.push_back(std::move(NewCB));
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
	if (VertexShader) VertexShader->Bind(DeviceContext);
	if (PixelShader) PixelShader->Bind(DeviceContext);
	if (MaterialTexture) MaterialTexture->Bind(DeviceContext);

	for (int32 i = 0; i < static_cast<int32>(ConstantBuffers.size()); ++i)
	{
		ConstantBuffers[i].Upload(DeviceContext);
		UINT Slot = MaterialCBStartSlot + static_cast<UINT>(i);
		ID3D11Buffer* Buf = ConstantBuffers[i].GPUBuffer;
		DeviceContext->VSSetConstantBuffers(Slot, 1, &Buf);
		DeviceContext->PSSetConstantBuffers(Slot, 1, &Buf);
	}
}

void FMaterial::Release()
{
	VertexShader.reset();
	PixelShader.reset();
	RasterizerState.reset();
	DepthStencilState.reset();
	BlendState.reset();
	for (auto& CB : ConstantBuffers)
	{
		CB.Release();
	}
	ConstantBuffers.clear();
}
