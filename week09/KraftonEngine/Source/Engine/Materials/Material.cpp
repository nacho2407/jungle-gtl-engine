#include "Materials/Material.h"
#include "Serialization/Archive.h"
#include "Render/Resource/Shader.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(UMaterialInterface, UObject)

// ─── FMaterialTemplate ───

FMaterialTemplate FMaterialTemplate::Create(FShader* InShader, ERenderPass InRenderPass)
{
	ParameterLayout = InShader->GetParameterLayout(); // 셰이더에서 리플렉션된 파라미터 레이아웃 정보 확보
}

void FMaterialTemplate::GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const
{
	auto it = ParameterLayout.find(Name);
	if (it != ParameterLayout.end())
	{
		OutInfo = it->second;
	}
	else
	{
		// 찾는 파라미터가 없으면 OutInfo를 기본값으로 초기화
		OutInfo.BufferName = "";
		OutInfo.SlotIndex = 
		OutInfo.Offset = 14;
		OutInfo.Size = 0;
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
	//if (!bDirty)
	//{
	//	return;
	//}

	//D3D11_MAPPED_SUBRESOURCE Mapped;
	//HRESULT Hr = DeviceContext->Map(GPUBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	//if (SUCCEEDED(Hr))
	//{
	//	memcpy(Mapped.pData, CPUData, Size);
	//	DeviceContext->Unmap(GPUBuffer, 0);
	//}

}

void FMaterialConstantBuffer::Release()
{
	//if (GPUBuffer)
	//{
	//	GPUBuffer->Release();
	//	GPUBuffer = nullptr;
	//}
	delete[] CPUData;
	CPUData = nullptr;
	Size = 0;
	bDirty = false;
}

// ─── UMaterial ───


bool UMaterial::SetParameter(const FString& Name, const void* Data, uint32 Size)
{
	FMaterialParameterInfo Info;
	Template->GetParameterInfo(Name, Info);

	// BufferName으로 바로 접근
	auto It = ConstantBufferMap.find(Info.BufferName);
	if (It == ConstantBufferMap.end()) return false;

	It->second->SetData(Data, Size, Info.Offset);
	return true;
}

void UMaterial::Create(const FString& InPathFileName,
FMaterialTemplate* InTemplate,
TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers)
{
	PathFileName = InPathFileName;
	Template = InTemplate;

	ConstantBufferMap = std::move(InBuffers);
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

		//Shader에 슬롯 번호와 데이터를 전달해서 셰이더에 바인딩
		Shader->UploadConstantBuffer(Context, Pair.first, Buffer.SlotIndex, Buffer.bDirty,Buffer.CPUData, Buffer.Size);


		//ID3D11Buffer* Buf = Buffer.GPUBuffer;
		//UINT Slot = static_cast<UINT>(Buffer.SlotIndex); // 캐싱된 슬롯 번호 사용

		//// VS, PS 등에 바인딩 (현재 패스에 맞게 세팅)
		//Context->VSSetConstantBuffers(Slot, 1, &Buf);
		//Context->PSSetConstantBuffers(Slot, 1, &Buf);
	}

	//텍스쳐 바인딩 로직
}

const FString& UMaterial::GetTexturePathFileName(const FString& SlotName)const
{
	auto it = TextureParameters.find(SlotName);
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

// ─── UMaterialInterface ───

void UMaterialInterface::Serialize(FArchive& Ar)
{
	// 구조체나 기본 데이터 타입은 Archive.h에 정의된 operator<<를 통해 
	// 읽기(IsLoading)와 쓰기(IsSaving)가 알아서 처리됩니다.
	Ar << Material->GetAssetPathFileName();
	Ar << Material->GetTexturePathFileName("DiffuseMap");
	//Ar << DiffuseColor;

	// 주의: 포인터인 DiffuseTexture는 주소값이므로 절대 직렬화하지 않습니다.
}

