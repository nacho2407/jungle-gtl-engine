#pragma once

#include "Object/ObjectFactory.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Resource/Buffer.h"
#include <memory>
class UTexture2D;
class FArchive;
class FShader;

/*
{
	"ColorTint" : { BufferName = "PerFrame",SloatIndex = b2, Offset=0,  Size=16 }
	"Roughness" : { BufferName = "PerFrame",SloatIndex = b2, Offset=16, Size=4  }
	"Metallic"  : { BufferName = "PerFrame",SloatIndex = b2, Offset=20, Size=4  }
}
*/
// 파라미터 이름 → 상수 버퍼 내 위치 매핑
struct FMaterialParameterInfo
{
	FString BufferName;  // ConstantBuffers 이름 "PerMaterial""PerFrame"
	uint32 SlotIndex;    // ConstantBuffers 슬롯 인덱스 

	uint32 Offset;      // 버퍼 내 바이트 오프셋
	uint32 Size;        // 바이트 크기

	uint32 BufferSize;   //이 변수가 속한 상수 버퍼의 전체 크기 (16의 배수)
};


//셰이더 + 레이아웃 (불변, 공유)
//Template은 셰이더 파일이 있으면 언제든 재생성 가능
class FMaterialTemplate
{
private:
	uint32 MaterialTemplateID; // 고유 ID 
	FShader* Shader; // 어떤 셰이더를 사용하는지 
	TMap<FString, FMaterialParameterInfo*> ParameterLayout; // 리플렉션 결과 : 쉐이더 constant buffer 레이아웃 정보
	ERenderPass RenderPass; // 어떤 패스에서 렌더링되는지(Opaque)

public:
	const TMap<FString, FMaterialParameterInfo*>& GetParameterInfo() const { return ParameterLayout; }
	void Create(FShader* InShader,	ERenderPass InRenderPass);
	FShader* GetShader() const { return Shader; }
	bool GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const;
};


// 실제 데이터가 올라가는 버퍼
struct FMaterialConstantBuffer
{
	uint8* CPUData;   // CPU 메모리의 실제 값
	//ID3D11Buffer* GPUBuffer; // GPU에 올라간 버퍼
	FConstantBuffer GPUBuffer;
	uint32 Size = 0;
	UINT SlotIndex = 0;	//cbuffer 바인딩 슬롯 (b0, b1 등)
	bool bDirty = false;

	FMaterialConstantBuffer() = default;
	~FMaterialConstantBuffer();

	// 복사 금지
	FMaterialConstantBuffer(const FMaterialConstantBuffer&) = delete;
	FMaterialConstantBuffer& operator=(const FMaterialConstantBuffer&) = delete;

	// CPU 메모리 할당
	//bool Create(ID3D11Device* Device, uint32 InSize);
	void Init(ID3D11Device* InDevice, uint32 InSize, uint32 InSlot);

	// CPU 데이터의 특정 오프셋에 값 쓰기 (Dirty 마킹)
	void SetData(const void* Data, uint32 InSize, uint32 Offset = 0);

	void Upload(ID3D11DeviceContext* DeviceContext);

	void Release();

	FConstantBuffer* GetConstantBuffer() { return &GPUBuffer; }
};

//파라미터 값 + 텍스처 (런타임 데이터)
//JSON으로 직렬화되는 데이터
class UMaterial : public UObject
{
private:
	FString PathFileName;// 어떤 Material인지 판별하는 고유 이름
	uint32 MaterialInstanceID; // 고유 ID
	FMaterialTemplate* Template; // 공유

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> ConstantBufferMap; // 인스턴스 고유
	TMap<FString, UTexture2D*> TextureParameters;  //텍스처는 슬롯 이름으로 관리

	bool SetParameter(const FString& Name, const void* Data, uint32 Size);

	FString DiffuseTextureFilePath;
	FVector4 DiffuseColor = FVector4(1.0f, 0.0f, 1.0f, 1.0f);
public:
	DECLARE_CLASS(UMaterial, UObject)
	void Create(const FString& InPathFileName,FMaterialTemplate* InTemplate,
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers);

	const uint8* GetRawPtr(const FString& BufferName, uint32 Offset) const;
	bool SetScalarParameter(const FString& ParamName, float Value);
	bool SetVector3Parameter(const FString& ParamName, const FVector& Value);
	bool SetVector4Parameter(const FString& ParamName, const FVector4& Value);
	bool SetTextureParameter(const FString& ParamName, UTexture2D* Texture);
	bool SetMatrixParameter(const FString& ParamName, const FMatrix& Value);

	bool GetScalarParameter(const FString& ParamName, float& OutValue) const;
	bool GetVector3Parameter(const FString& ParamName, FVector& OutValue) const;
	bool GetVector4Parameter(const FString& ParamName, FVector4& OutValue) const;
	bool GetTextureParameter(const FString& ParamName, UTexture2D*& OutTexture) const;
	bool GetMatrixParameter(const FString& ParamName, FMatrix& Value) const;


	void Bind(ID3D11DeviceContext* Context);

	FShader* GetShader() const { return Template ? Template->GetShader() : nullptr; }

	const FString& GetTexturePathFileName(const FString& TextureName)const;

	const FString& GetAssetPathFileName() const { return PathFileName;}
	void SetAssetPathFileName(const FString& InPath) { PathFileName = InPath; }
	void Serialize(FArchive& Ar);//>>>>>Manager가 위임

	FConstantBuffer* GetGPUBufferBySlot(uint32 InSlot) const
	{
		for (const auto& Pair : ConstantBufferMap)
		{
			if (Pair.second->SlotIndex == InSlot)
				return Pair.second->GetConstantBuffer();
		}
		return nullptr;
	}
};

//// ─── 미래 확장용 구조 (현재 미사용) ───
//
////UMaterialInterface: UMaterial / UMaterialInstance 공통 베이스
//class UMaterialInterface : public UObject
//{
//public:
//	virtual UMaterial* GetRenderData() const = 0;
//	virtual bool SetScalarParameter(const FString& Name, float Value) = 0;
//	virtual bool SetVector4Parameter(const FString& Name, const FVector4& Value) = 0;
//	virtual bool SetTextureParameter(const FString& Name, UTexture2D* Texture) = 0;
//};
//
////UMaterialInstanceDynamic: UMaterial 원본을 공유하며 파라미터만 오버라이드
//class UMaterialInstanceDynamic : public UMaterialInterface
//{
//public:
//	static UMaterialInstanceDynamic* Create(UMaterial* InParent);
//
//	virtual UMaterial* GetRenderData() const override;
//	virtual bool SetScalarParameter(const FString& Name, float Value) override;
//	virtual bool SetVector4Parameter(const FString& Name, const FVector4& Value) override;
//	virtual bool SetTextureParameter(const FString& Name, UTexture2D* Texture) override;
//
//private:
//	UMaterial* Parent = nullptr; // 공유 원본
//	TMap<FString, float> ScalarOverrides;
//	TMap<FString, FVector4> VectorOverrides;
//	TMap<FString, UTexture2D*> TextureOverrides;
//};
