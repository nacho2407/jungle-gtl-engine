#include "Materials/Material.h"
#include "Serialization/Archive.h"
#include "Render/Resource/Shader.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(UMaterial, UObject)

// ─── FMaterialTemplate ───

void FMaterialTemplate::Create(FShader* InShader, ERenderPass InRenderPass)
{
	ParameterLayout = InShader->GetParameterLayout(); // 셰이더에서 리플렉션된 파라미터 레이아웃 정보 확보
	Shader = InShader;
	RenderPass = InRenderPass;
}

bool FMaterialTemplate::GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const
{
	auto it = ParameterLayout.find(Name);
	if (it != ParameterLayout.end())
	{
		OutInfo = it->second;
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

bool FMaterialConstantBuffer::Create(ID3D11Device* Device, uint32 InSize)
{
	Release();


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

void FMaterialConstantBuffer::Init(ID3D11Buffer* InGPUBuffer, uint32 InSize, uint32 InSlot)
{
	Release();

	Size = (InSize + 15) & ~15;
	CPUData = new uint8[Size];
	memset(CPUData, 0, Size);
	GPUBuffer = InGPUBuffer;
	SlotIndex = InSlot;

	bDirty = true; // 초기 데이터(0)도 업로드 필요

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
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped;
	HRESULT Hr = DeviceContext->Map(GPUBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	if (SUCCEEDED(Hr))
	{
		memcpy(Mapped.pData, CPUData, Size);
		DeviceContext->Unmap(GPUBuffer, 0);
		bDirty = false;
	}

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

// ─── UMaterial ───

void UMaterial::Create(const FString& InPathFileName,
FMaterialTemplate* InTemplate,
TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers)
{
	PathFileName = InPathFileName;
	Template = InTemplate;

	ConstantBufferMap = std::move(InBuffers);
}

bool UMaterial::SetParameter(const FString& Name, const void* Data, uint32 Size)
{
	FMaterialParameterInfo Info;
	if (!Template->GetParameterInfo(Name, Info)) {
		return false;
	}
	// BufferName으로 바로 접근
	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	It->second->SetData(Data, Size, Info.Offset);
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
	return true;
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

void UMaterial::Bind(ID3D11DeviceContext* Context)
{
	FShader* Shader = Template->GetShader();
	if (!Shader)
	{
		return;
	}

	Shader->Bind(Context);

	//Map 전용으로 slot 매핑
	for (auto& Pair : ConstantBufferMap)
	{
		FMaterialConstantBuffer& Buffer = *Pair.second;

		// 데이터가 변경(Dirty)되었다면 
		Buffer.Upload(Context);

		ID3D11Buffer* Buf = Buffer.GPUBuffer;
		UINT Slot = static_cast<UINT>(Buffer.SlotIndex); // 캐싱된 슬롯 번호 사용

		// VS, PS 등에 바인딩 (현재 패스에 맞게 세팅)
		Context->VSSetConstantBuffers(Slot, 1, &Buf);
		Context->PSSetConstantBuffers(Slot, 1, &Buf);
	}

	//텍스쳐 바인딩 로직
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

void UMaterial::Serialize(FArchive& Ar)
{
	/*
	
	Ar << PathFileName;
	Ar << DiffuseTextureFilePath;
	Ar << DiffuseColor;
	
	*/
}



