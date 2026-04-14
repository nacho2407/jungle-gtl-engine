#include "Materials/Material.h"
#include "Serialization/Archive.h"
#include "Render/Resource/Shader.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
IMPLEMENT_CLASS(UMaterial, UObject)

// ─── FMaterialTemplate ───

void FMaterialTemplate::Create(FShader* InShader, ERenderPass InRenderPass)
{
	ParameterLayout = InShader->GetParameterLayout(); // 셰이더에서 리플렉션된 파라미터 레이아웃 정보 확보
	Shader = InShader;
	RenderPass = InRenderPass;
}

ERenderPass FMaterialTemplate::GetRenderPass() const
{
	return RenderPass;
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

//bool FMaterialConstantBuffer::Create(ID3D11Device* Device, uint32 InSize)
//{
//	//Release();
//
//
//	//Size = (InSize + 15) & ~15;
//	//CPUData = new uint8[Size];
//	//memset(CPUData, 0, Size);
//
//	//D3D11_BUFFER_DESC Desc = {};
//	//Desc.ByteWidth = Size;
//	//Desc.Usage = D3D11_USAGE_DYNAMIC;
//	//Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
//	//Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
//
//	//HRESULT Hr = Device->CreateBuffer(&Desc, nullptr, &GPUBuffer);
//	//if (FAILED(Hr))
//	//{
//	//	Release();
//	//	return false;
//	//}
//
//	//bDirty = true; // 초기 데이터(0)도 업로드 필요
//	return true;
//}

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
	It->second->bDirty = true; // 데이터가 변경되었음을 표시

	
	It->second->Upload(GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext()); // 변경된 데이터 즉시 GPU에 업로드 (옵션)
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
	/*기존 GetVector4Parameter 패턴이랑 동일하고 reinterpret_cast 대신 memcpy 쓴 건 __m128 유니온 때문에 alignment 문제 생길 수 있어서*/
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

ERenderPass UMaterial::GetRenderPass() const
{
	return Template ? Template->GetRenderPass() : ERenderPass::Opaque;
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

	Ar << PathFileName;

	// 3. 상수 버퍼 CPU 데이터 저장
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
				// 이미 버퍼가 있으면 CPU 데이터만 덮어씀
				Ar.Serialize(It->second->CPUData, Size);
				It->second->bDirty = true;
			}
			else
			{
				// 없으면 스킵 (버퍼 구조가 바뀐 경우 대비)
				TArray<uint8> Dummy(Size);
				Ar.Serialize(Dummy.data(), Size);
			}
		}
	}
	Ar << DiffuseTextureFilePath;

	Ar << DiffuseColor;
	
	// 4. 텍스처 슬롯 저장 (경로로 저장)
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

				TextureParameters[SlotName] = UTexture2D::LoadFromFile(TexturePath, Device);
			}
		}
	}
}



