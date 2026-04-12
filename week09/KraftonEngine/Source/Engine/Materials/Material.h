#pragma once

#include "Object/ObjectFactory.h"
#include "Math/Vector.h"
#include "Render/Types/RenderTypes.h"

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
	TMap<FString, FMaterialParameterInfo> ParameterLayout; // 리플렉션 결과 : 쉐이더 constant buffer 레이아웃 정보, 텍스처 슬롯 정보 등등
	ERenderPass RenderPass; // 어떤 패스에서 렌더링되는지(Opaque)

public:
	const TMap<FString, FMaterialParameterInfo>& GetParameterInfo() const { return ParameterLayout; }
	FMaterialTemplate Create(FShader* InShader,	ERenderPass InRenderPass);
	FShader* GetShader() const { return Shader; }
	void GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const;
};


// 실제 데이터가 올라가는 버퍼
struct FMaterialConstantBuffer
{
	uint8* CPUData;   // CPU 메모리의 실제 값
	ID3D11Buffer* GPUBuffer; // GPU에 올라간 버퍼, Shader의 FConstatnbufferpool이 소유
	uint32 Size = 0;
	UINT SlotIndex = 0;	//cbuffer 바인딩 슬롯 (b0, b1 등)
	bool bDirty = false;

	FMaterialConstantBuffer() = default;
	~FMaterialConstantBuffer();

	// 복사 금지
	FMaterialConstantBuffer(const FMaterialConstantBuffer&) = delete;
	FMaterialConstantBuffer& operator=(const FMaterialConstantBuffer&) = delete;

	// CPU 메모리 할당
	bool Create(ID3D11Device* Device, uint32 InSize);
	void Init(ID3D11Buffer* InGPUBuffer, uint32 InSize, uint32 InSlot);	// 변경안

	// CPU 데이터의 특정 오프셋에 값 쓰기 (Dirty 마킹)
	void SetData(const void* Data, uint32 InSize, uint32 Offset = 0);

	void Upload(ID3D11DeviceContext* DeviceContext);

	void Release();

};

//파라미터 값 + 텍스처 (런타임 데이터)
//JSON으로 직렬화되는 데이터
class UMaterial
{
private:
	FString PathFileName;// 어떤 Material인지 판별하는 고유 이름
	uint32 MaterialInstanceID; // 고유 ID
	FMaterialTemplate* Template; // 공유

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> ConstantBufferMap; // 인스턴스 고유
	TMap<FString, UTexture2D*> TextureParameters;  //텍스처는 슬롯 이름으로 관리

	bool SetParameter(const FString& Name, const void* Data, uint32 Size);
public:
	void Create(const FString& InPathFileName,
		FMaterialTemplate* InTemplate,
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers);

	bool SetScalarParameter(const FString& ParamName, float Value);
	bool SetVector3Parameter(const FString& ParamName, const FVector& Value);
	bool SetVector4Parameter(const FString& ParamName, const FVector4& Value);
	const uint8* GetRawPtr(const FString& BufferName, uint32 Offset) const;
	bool SetTextureParameter(const FString& ParamName, UTexture2D* Texture);

	void Bind(ID3D11DeviceContext* Context);

	const FString& GetTexturePathFileName(const FString& SlotName)const;

	const FString& GetAssetPathFileName() const { return PathFileName;}
};

//에디터/게임플레이 인터페이스/래퍼 (직렬화, GC 대상)
class UMaterialInterface : public UObject // : public UMaterialInterface
{
public:
	DECLARE_CLASS(UMaterialInterface, UObject)
	~UMaterialInterface() noexcept override = default;

	UMaterial* Material;

	//레거시
	FString PathFileName;					// 어떤 Material인지 판별하는 고유 이름
	FString DiffuseTextureFilePath;
	FVector4 DiffuseColor = FVector4(1.0f, 0.0f, 1.0f, 1.0f);
	UTexture2D* DiffuseTexture = nullptr;	// UObjectManager 소유, 여기선 참조만
	//레거시 끝

	// 편의 함수 (내부적으로 UMaterial에 위임)
	void SetDiffuseColor(const FVector4& Color)
	{
		Material->SetVector4Parameter("DiffuseColor", Color);
	}
	void SetDiffuseTexture(UTexture2D* Texture)
	{
		Material->SetTextureParameter("DiffuseMap", Texture);
	}

public:

	//초기화?
	void Serialize(FArchive& Ar);

	const FString& GetAssetPathFileName() const{ return Material->GetAssetPathFileName(); }

	UMaterial* GetRenderData() const
	{
		return Material;
	}
};
