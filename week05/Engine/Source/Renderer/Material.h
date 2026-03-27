#pragma once

#include "CoreMinimal.h"
#include "RenderState.h"
#include <d3d11.h>
#include <memory>

class FVertexShader;
class FPixelShader;

struct FMaterialTexture
{
	ID3D11ShaderResourceView* TextureSRV = nullptr;
	ID3D11SamplerState* SamplerState = nullptr;

	FMaterialTexture() = default;
	~FMaterialTexture();

	FMaterialTexture(const FMaterialTexture&) = delete;
	FMaterialTexture& operator=(const FMaterialTexture&) = delete;

	void Release();
	void Bind(ID3D11DeviceContext* DeviceContext);
};

// 파라미터 이름 → 상수 버퍼 내 위치 매핑
struct FMaterialParameterInfo
{
	int32 BufferIndex;  // ConstantBuffers 배열 인덱스
	uint32 Offset;      // 버퍼 내 바이트 오프셋
	uint32 Size;        // 바이트 크기
};

// Material이 소유하는 상수 버퍼 슬롯 하나
// GPU 버퍼 생성, CPU 데이터 관리, Dirty 플래그 기반 업로드를 모두 담당
struct ENGINE_API FMaterialConstantBuffer
{
	ID3D11Buffer* GPUBuffer = nullptr;
	uint8* CPUData = nullptr; // CPU 쪽 shadow copy
	uint32 Size = 0;
	bool bDirty = false;

	FMaterialConstantBuffer() = default;
	~FMaterialConstantBuffer();

	// 복사 금지
	FMaterialConstantBuffer(const FMaterialConstantBuffer&) = delete;
	FMaterialConstantBuffer& operator=(const FMaterialConstantBuffer&) = delete;

	// Move 지원 (소유권 이전)
	FMaterialConstantBuffer(FMaterialConstantBuffer&& Other) noexcept
		: GPUBuffer(Other.GPUBuffer), CPUData(Other.CPUData), Size(Other.Size), bDirty(Other.bDirty)
	{
		Other.GPUBuffer = nullptr;
		Other.CPUData = nullptr;
		Other.Size = 0;
		Other.bDirty = false;
	}
	FMaterialConstantBuffer& operator=(FMaterialConstantBuffer&& Other) noexcept
	{
		if (this != &Other)
		{
			Release();
			GPUBuffer = Other.GPUBuffer;
			CPUData = Other.CPUData;
			Size = Other.Size;
			bDirty = Other.bDirty;
			Other.GPUBuffer = nullptr;
			Other.CPUData = nullptr;
			Other.Size = 0;
			Other.bDirty = false;
		}
		return *this;
	}

	// Device로 GPU 버퍼 생성 + CPU 메모리 할당
	bool Create(ID3D11Device* Device, uint32 InSize);

	// CPU 데이터의 특정 오프셋에 값 쓰기 (Dirty 마킹)
	void SetData(const void* Data, uint32 InSize, uint32 Offset = 0);

	// Dirty면 Map/Unmap으로 GPU에 업로드
	void Upload(ID3D11DeviceContext* DeviceContext);

	void Release();
};

// Material: VS/PS 셰이더 조합 + 추가 상수 버퍼 (b2+)
// 생성 후 파라미터 값 변경 불가 (읽기 전용). 런타임 변경이 필요하면 FDynamicMaterial 사용.
class ENGINE_API FMaterial
{
public:
	FMaterial() : ShaderId(NextShaderId++) {}
	virtual ~FMaterial();

	FMaterial(const FMaterial&) = delete;
	FMaterial& operator=(const FMaterial&) = delete;
	FMaterial(FMaterial&&) = default;
	FMaterial& operator=(FMaterial&&) = default;

	uint64 GetSortId() const;

	// 에셋 원본 이름 (JSON에서 로드된 이름, 직렬화 시 사용)
	void SetOriginName(const FString& InName) { OriginName = InName; }
	const FString& GetOriginName() const { return OriginName; }

	// 인스턴스 이름 (런타임에서 구분용, DynamicMaterial 등)
	void SetInstanceName(const FString& InName) { InstanceName = InName; }
	const FString& GetInstanceName() const { return InstanceName; }

	// 인스턴스 이름이 있으면 인스턴스 이름, 없으면 원본 이름 반환
	const FString& GetName() const { return InstanceName.empty() ? OriginName : InstanceName; }

	void SetVertexShader(const std::shared_ptr<FVertexShader>& InVS) { VertexShader = InVS; }
	void SetPixelShader(const std::shared_ptr<FPixelShader>& InPS) { PixelShader = InPS; }
	void SetRasterizerOption(const FRasterizerStateOption InOption) { RasterizerOption = InOption; }
	void SetRasterizerState(const std::shared_ptr<FRasterizerState> InState) { RasterizerState = InState; }
	void SetDepthStencilOption(const FDepthStencilStateOption InOption) { DepthStencilOption = InOption; }
	void SetDepthStencilState(const std::shared_ptr<FDepthStencilState> InState) { DepthStencilState = InState; }
	void SetBlendOption(const FBlendStateOption InOption) { BlendOption = InOption; }
	void SetBlendState(const std::shared_ptr<FBlendState> InState) { BlendState = InState; }
	void SetMaterialTexture(const std::shared_ptr<FMaterialTexture> InTexture) { MaterialTexture = InTexture; }

	FVertexShader* GetVertexShader() const { return VertexShader.get(); }
	FPixelShader* GetPixelShader() const { return PixelShader.get(); }
	const FRasterizerStateOption& GetRasterizerOption() const { return RasterizerOption; }
	const FDepthStencilStateOption& GetDepthStencilOption() const { return DepthStencilOption; }
	const FBlendStateOption& GetBlendOption() const { return BlendOption; }
	std::shared_ptr<FRasterizerState> GetRasterizerState() const { return RasterizerState; }
	std::shared_ptr<FDepthStencilState> GetDepthStencilState() const { return DepthStencilState; }
	std::shared_ptr<FBlendState> GetBlendState() const { return BlendState; }
	std::shared_ptr<FMaterialTexture> GetMaterialTexture() const { return MaterialTexture; }

	// FDynamicMaterial에서 파라미터 설정 시 사용
	bool SetParameterData(const FString& ParamName, const void* Data, uint32 DataSize);

	// 상수 버퍼 슬롯 추가 (b2, b3, ... 순서대로)
	int32 CreateConstantBuffer(ID3D11Device* Device, uint32 InSize);

	// 슬롯 인덱스로 상수 버퍼 접근
	FMaterialConstantBuffer* GetConstantBuffer(int32 Index);

	// 파라미터 이름 등록 (MaterialManager에서 JSON 로드 시 호출)
	void RegisterParameter(const FString& ParamName, int32 BufferIndex, uint32 Offset, uint32 Size);

	// 독립적인 상수 버퍼를 가진 DynamicMaterial 복제본 생성
	std::unique_ptr<class FDynamicMaterial> CreateDynamicMaterial() const;

	// 셰이더 바인딩 + Dirty 상수 버퍼 업로드 + 바인딩
	void Bind(ID3D11DeviceContext* DeviceContext);

	void Release();

protected:

	// TODO: ShaderId가 실제 사용하는 쉐이더를 반영하도록 변경
	// NOTE: GetSortId에서 비트 연산 쓰는 경우 ShaderId가 32bit를 전부 쓰면 안 됨
	uint32 ShaderId = 0;
	static inline uint32 NextShaderId = 0;

	FString OriginName;
	FString InstanceName;
	std::shared_ptr<FVertexShader> VertexShader;
	std::shared_ptr<FPixelShader> PixelShader;
	// RasterizerState를 생성하기 위한 옵션, Serialize.
	FRasterizerStateOption RasterizerOption;
	FDepthStencilStateOption DepthStencilOption;
	FBlendStateOption BlendOption;
	// 머티리얼 로드시에 생성되는 RasterizerState 포인터. No-Serialize.
	std::shared_ptr<FRasterizerState> RasterizerState = nullptr;
	std::shared_ptr<FDepthStencilState> DepthStencilState = nullptr;
	std::shared_ptr<FBlendState> BlendState = nullptr;
	// Texture
	std::shared_ptr<FMaterialTexture> MaterialTexture = nullptr;

	TArray<FMaterialConstantBuffer> ConstantBuffers;
	TMap<FString, FMaterialParameterInfo> ParameterMap;

	static constexpr UINT MaterialCBStartSlot = 2; // b0=Frame, b1=Object, b2+=Material
};

// DynamicMaterial: 런타임에 파라미터 값을 변경할 수 있는 Material 인스턴스
// FMaterial::CreateDynamicMaterial()로 생성
class ENGINE_API FDynamicMaterial : public FMaterial
{
public:
	FDynamicMaterial() = default;

	// 이름 기반 파라미터 설정 (타입별 편의 함수)
	bool SetScalarParameter(const FString& ParamName, float Value);
	bool SetVectorParameter(const FString& ParamName, const FVector4& Value);
	bool SetVector3Parameter(const FString& ParamName, const FVector& Value);
};
