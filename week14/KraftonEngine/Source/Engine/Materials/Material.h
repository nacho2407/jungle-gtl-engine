#pragma once

#include "Object/ObjectFactory.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/RenderStateTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/MaterialTextureSlot.h"
#include "Render/Types/RenderConstants.h"
#include <memory>

class UTexture2D;
class FArchive;
class FShader;

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

public:
	const TMap<FString, FMaterialParameterInfo*>& GetParameterInfo() const { return ParameterLayout; }
	void Create(FShader* InShader);

	FShader* GetShader() const { return Shader; }
	bool GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const;
};


// 실제 데이터가 올라가는 버퍼
struct FMaterialConstantBuffer
{
	uint8* CPUData;   // CPU 메모리의 실제 값
	FConstantBuffer GPUBuffer;
	uint32 Size = 0;
	UINT SlotIndex = 0;	//cbuffer 바인딩 슬롯 (b0, b1 등)
	bool bDirty = false;

	FMaterialConstantBuffer() = default;
	~FMaterialConstantBuffer();

	FMaterialConstantBuffer(const FMaterialConstantBuffer&) = delete;
	FMaterialConstantBuffer& operator=(const FMaterialConstantBuffer&) = delete;

	void Init(ID3D11Device* InDevice, uint32 InSize, uint32 InSlot);
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

	// 렌더링 상태 정보 (인스턴스별)
	ERenderPass RenderPass = ERenderPass::Opaque;
	EBlendState BlendState = EBlendState::Opaque;
	EDepthStencilState DepthStencilState = EDepthStencilState::Default;
	ERasterizerState RasterizerState = ERasterizerState::SolidBackCull;

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> ConstantBufferMap; // 인스턴스 고유
	TMap<FString, UTexture2D*> TextureParameters;  //텍스처는 슬롯 이름으로 관리

	FShader* TransientShader = nullptr; // CreateTransient에서 직접 지정된 셰이더 (Template 없는 경우)

	// Per-shader CB 오버라이드 — transient Material에서 프록시가 관리하는 외부 CB
	FConstantBufferBinding PerShaderOverride;

	// SRV 캐시 — SetTextureParameter 시 갱신, BuildCommandForProxy에서 map lookup 회피
	ID3D11ShaderResourceView* CachedSRVs[(int)EMaterialTextureSlot::Max] = {};

	bool SetParameter(const FString& Name, const void* Data, uint32 Size);

public:
	DECLARE_CLASS(UMaterial, UObject)
	~UMaterial() override;

	void Create(const FString& InPathFileName, FMaterialTemplate* InTemplate,
		ERenderPass InRenderPass,
		EBlendState InBlend,
		EDepthStencilState InDepth,
		ERasterizerState InRaster,
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers);

	const uint8* GetRawPtr(const FString& BufferName, uint32 Offset) const;

	const TMap<FString, FMaterialParameterInfo*> GetParameterInfo() const { return Template->GetParameterInfo(); }

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

	TMap<FString, UTexture2D*>* GetTexture() { return &TextureParameters; }

	void Bind(ID3D11DeviceContext* Context);

	FShader* GetShader() const { return Template ? Template->GetShader() : TransientShader; }
	ERenderPass GetRenderPass() const { return RenderPass; }
	EBlendState GetBlendState() const { return BlendState; }
	EDepthStencilState GetDepthStencilState() const { return DepthStencilState; }
	ERasterizerState GetRasterizerState() const { return RasterizerState; }

	// Per-shader CB 오버라이드 — transient Material에서 Gizmo/SubUV/Decal 등이 사용
	template<typename T>
	T& BindPerShaderCB(FConstantBuffer* Buffer, uint32 Slot)
	{
		return PerShaderOverride.Bind<T>(Buffer, Slot);
	}

	template<typename T>
	T& GetPerShaderAs() { return PerShaderOverride.As<T>(); }

	template<typename T>
	const T& GetPerShaderAs() const { return PerShaderOverride.As<T>(); }

	const FString& GetTexturePathFileName(const FString& TextureName)const;

	const FString& GetAssetPathFileName() const { return PathFileName; }
	void SetAssetPathFileName(const FString& InPath) { PathFileName = InPath; }
	void Serialize(FArchive& Ar);//>>>>>Manager가 위임

	FConstantBuffer* GetGPUBufferBySlot(uint32 InSlot) const
	{
		// Per-shader override (transient Material의 외부 CB)
		if (PerShaderOverride.Buffer && PerShaderOverride.Slot == InSlot)
			return PerShaderOverride.Buffer;

		for (const auto& Pair : ConstantBufferMap)
		{
			if (Pair.second->SlotIndex == InSlot)
				return Pair.second->GetConstantBuffer();
		}
		return nullptr;
	}

	// dirty CB를 GPU에 업로드 — BuildCommandForProxy 전에 호출
	void FlushDirtyBuffers(ID3D11Device* Device, ID3D11DeviceContext* Ctx)
	{
		for (auto& Pair : ConstantBufferMap)
		{
			if (Pair.second->bDirty)
				Pair.second->Upload(Ctx);
		}
		// Per-shader override CB (Gizmo/SubUV/Decal 등)
		if (PerShaderOverride.Buffer)
		{
			if (!PerShaderOverride.Buffer->GetBuffer())
				PerShaderOverride.Buffer->Create(Device, PerShaderOverride.Size);
			PerShaderOverride.Buffer->Update(Ctx, PerShaderOverride.Data, PerShaderOverride.Size);
		}
	}

	// 캐시된 SRV 배열 직접 접근 (map lookup 회피)
	const ID3D11ShaderResourceView* const* GetCachedSRVs() const { return CachedSRVs; }

	// SRV 캐시 재구축 — Material 생성/텍스처 로드 후 호출
	void RebuildCachedSRVs();

	// CachedSRV 슬롯 직접 설정 — UTexture2D 없이 raw SRV를 바인딩할 때 사용
	void SetCachedSRV(EMaterialTextureSlot Slot, ID3D11ShaderResourceView* SRV) { CachedSRVs[(int)Slot] = SRV; }

	// Template/CB 없는 경량 머티리얼 생성 — SRV만 래핑할 때 사용
	// InShader를 지정하면 GetShader()가 해당 셰이더를 반환 (DrawCommandBuilder per-section 셰이더 지원)
	static UMaterial* CreateTransient(ERenderPass InPass, EBlendState InBlend,
		EDepthStencilState InDepth = EDepthStencilState::Default,
		ERasterizerState InRaster = ERasterizerState::SolidBackCull,
		FShader* InShader = nullptr);
};
