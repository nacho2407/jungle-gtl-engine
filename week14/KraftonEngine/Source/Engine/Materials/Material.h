#pragma once

#include "Object/Reflection/ObjectFactory.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/RenderStateTypes.h"
#include "Render/Types/MaterialTextureSlot.h"
#include "Render/Types/RenderConstants.h"
#include "Materials/MaterialCore.h"
#include <memory>

#include "Source/Engine/Materials/Material.generated.h"


class UTexture2D;
class FArchive;
class FShader;
class FMaterialManager;
class UMaterialInstanceDynamic;


UENUM()
enum class EMaterialShadowMode : uint8
{
	Opaque,
	Masked,
	None,
};

UCLASS()
class UMaterialInterface : public UObject
{
public:
	GENERATED_BODY()

	virtual bool SetScalarParameter(const FString& ParamName, float Value) { return false; }
	virtual bool SetVector3Parameter(const FString& ParamName, const FVector& Value) { return false; }
	virtual bool SetVector4Parameter(const FString& ParamName, const FVector4& Value) { return false; }
	virtual bool SetTextureParameter(const FString& ParamName, UTexture2D* Texture) { return false; }
	virtual bool SetMatrixParameter(const FString& ParamName, const FMatrix& Value) { return false; }

	virtual bool GetScalarParameter(const FString& ParamName, float& OutValue) const { return false; }
	virtual bool GetVector3Parameter(const FString& ParamName, FVector& OutValue) const { return false; }
	virtual bool GetVector4Parameter(const FString& ParamName, FVector4& OutValue) const { return false; }
	virtual bool GetTextureParameter(const FString& ParamName, UTexture2D*& OutTexture) const { return false; }
	virtual bool GetMatrixParameter(const FString& ParamName, FMatrix& Value) const { return false; }

	virtual FShader* GetShader() const { return nullptr; }
	virtual ERenderPass GetRenderPass() const { return ERenderPass::Opaque; }
	virtual EBlendState GetBlendState() const { return EBlendState::Opaque; }
	virtual EDepthStencilState GetDepthStencilState() const { return EDepthStencilState::Default; }
	virtual ERasterizerState GetRasterizerState() const { return ERasterizerState::SolidBackCull; }
	virtual FConstantBuffer* GetGPUBufferBySlot(uint32 InSlot) const { return nullptr; }
	virtual void FlushDirtyBuffers(ID3D11Device* Device, ID3D11DeviceContext* Ctx) {}
	virtual ID3D11ShaderResourceView* GetSRV(EMaterialTextureSlot Slot) const { return nullptr; }
	virtual const FString& GetAssetPathFileName() const
	{
		static const FString EmptyString;
		return EmptyString;
	}
	// UMaterialInterface
	virtual EMaterialShadowMode GetShadowMode() const
	{
		return EMaterialShadowMode::Opaque;
	}

	virtual bool CastsShadow() const
	{
		return GetShadowMode() != EMaterialShadowMode::None;
	}
};


//파라미터 값 + 텍스처 (런타임 데이터)
//JSON으로 직렬화되는 데이터
UCLASS()
class UMaterial : public UMaterialInterface
{
	friend class FMaterialManager;
	friend class UMaterialInstanceDynamic;

protected:

	FString PathFileName;// 어떤 Material인지 판별하는 고유 이름
	uint32 MaterialInstanceID; // 고유 ID
	FMaterialTemplate* Template; // 공유

	// 렌더링 상태 정보 (인스턴스별)
	UPROPERTY(Edit, Save, Category="Material", DisplayName="Render Pass", Enum=ERenderPass)
	ERenderPass RenderPass = ERenderPass::Opaque;

	UPROPERTY(Edit, Save, Category="Material", DisplayName="Blend State", Enum=EBlendState)
	EBlendState BlendState = EBlendState::Opaque;

	UPROPERTY(Edit, Save, Category="Material", DisplayName="Depth-Stencil State", Enum=EDepthStencilState)
	EDepthStencilState DepthStencilState = EDepthStencilState::Default;

	UPROPERTY(Edit, Save, Category="Material", DisplayName="Rasterizer State", Enum=ERasterizerState)
	ERasterizerState RasterizerState = ERasterizerState::SolidBackCull;

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> ConstantBufferMap; // 인스턴스 고유
	TMap<FString, UTexture2D*> TextureParameters;  //텍스처는 슬롯 이름으로 관리

	FShader* TransientShader = nullptr; // CreateTransient에서 직접 지정된 셰이더 (Template 없는 경우)

	// Per-shader CB 오버라이드 — transient Material에서 프록시가 관리하는 외부 CB
	FConstantBufferBinding PerShaderOverride;

	// SRV 캐시 — SetTextureParameter 시 갱신, BuildCommandForProxy에서 map lookup 회피
	ID3D11ShaderResourceView* CachedSRVs[(int)EMaterialTextureSlot::Max] = {};

	UPROPERTY(Edit, Save, Category="Material", DisplayName="Shadow Mode", Enum=EMaterialShadowMode)
	EMaterialShadowMode ShadowMode = EMaterialShadowMode::Opaque;

	bool SetParameter(const FString& Name, const void* Data, uint32 Size);
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> CloneConstantBuffers(ID3D11Device* Device) const;
	void CopyTextureParametersFrom(const UMaterial& Other);

public:
	GENERATED_BODY()
	~UMaterial() override;

	void Create(const FString& InPathFileName, FMaterialTemplate* InTemplate,
		ERenderPass InRenderPass,
		EBlendState InBlend,
		EDepthStencilState InDepth,
		ERasterizerState InRaster,
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers);

	const uint8* GetRawPtr(const FString& BufferName, uint32 Offset) const;

	const TMap<FString, FMaterialParameterInfo*>& GetParameterInfo() const;
	FMaterialTemplate* GetTemplate() const { return Template; }

	bool SetScalarParameter(const FString& ParamName, float Value) override;
	bool SetVector3Parameter(const FString& ParamName, const FVector& Value) override;
	bool SetVector4Parameter(const FString& ParamName, const FVector4& Value) override;
	bool SetTextureParameter(const FString& ParamName, UTexture2D* Texture) override;
	bool SetMatrixParameter(const FString& ParamName, const FMatrix& Value) override;

	bool GetScalarParameter(const FString& ParamName, float& OutValue) const override;
	bool GetVector3Parameter(const FString& ParamName, FVector& OutValue) const override;
	bool GetVector4Parameter(const FString& ParamName, FVector4& OutValue) const override;
	bool GetTextureParameter(const FString& ParamName, UTexture2D*& OutTexture) const override;
	bool GetMatrixParameter(const FString& ParamName, FMatrix& Value) const override;

	TMap<FString, UTexture2D*>* GetTexture() { return &TextureParameters; }

	EMaterialShadowMode GetShadowMode()const override  { return ShadowMode; }
	void SetShadowMode(EMaterialShadowMode InMode) { ShadowMode = InMode; }

	bool CastsShadow() const override { return ShadowMode != EMaterialShadowMode::None; }

	void Bind(ID3D11DeviceContext* Context);

	FShader* GetShader() const override  { return Template ? Template->GetShader() : TransientShader; }
	ERenderPass GetRenderPass() const override { return RenderPass; }
	EBlendState GetBlendState() const override { return BlendState; }
	EDepthStencilState GetDepthStencilState() const override { return DepthStencilState; }
	ERasterizerState GetRasterizerState() const override { return RasterizerState; }

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

	const FString& GetAssetPathFileName() const override { return PathFileName; }
	void SetAssetPathFileName(const FString& InPath) { PathFileName = InPath; }
	void Serialize(FArchive& Ar);//>>>>>Manager가 위임

	FConstantBuffer* GetGPUBufferBySlot(uint32 InSlot) const override
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
	void FlushDirtyBuffers(ID3D11Device* Device, ID3D11DeviceContext* Ctx) override
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
	ID3D11ShaderResourceView* GetSRV(EMaterialTextureSlot Slot) const override { return CachedSRVs[(int)Slot]; }

	// SRV 캐시 재구축 — Material 생성/텍스처 로드 후 호출
	void RebuildCachedSRVs();

	// CachedSRV 슬롯 직접 설정 — UTexture2D 없이 raw SRV를 바인딩할 때 사용
	void SetCachedSRV(EMaterialTextureSlot Slot, ID3D11ShaderResourceView* SRV) { CachedSRVs[(int)Slot] = SRV; }

	// Device 해제 전 GPU 버퍼만 명시적으로 해제 (UObject 수명은 UObjectManager가 관리)
	void ReleaseGPUBuffers()
	{
		for (auto& Pair : ConstantBufferMap)
		{
			if (Pair.second) Pair.second->Release();
		}
	}

private:
	// Template/CB 없는 경량 머티리얼 생성 — SRV만 래핑할 때 사용
	// FMaterialManager::CreateTransientMaterial을 통해 생성/관리해야 한다.
	static UMaterial* CreateTransient(ERenderPass InPass, EBlendState InBlend,
		EDepthStencilState InDepth = EDepthStencilState::Default,
		ERasterizerState InRaster = ERasterizerState::SolidBackCull,
		FShader* InShader = nullptr);
};


UCLASS()
class UMaterialInstance : public UMaterialInterface
{
public:
	GENERATED_BODY()

	static UMaterialInstance* Create(UMaterial* InParent);
	static UMaterialInstance* Create(UMaterial* InParent,
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers);
	void Create(const FString& InPathFileName, UMaterial* InParent,
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers);

	UMaterial* GetParent() const { return Parent; }
	const FString& GetAssetPathFileName() const override { return PathFileName; }
	void Serialize(FArchive& Ar) override;

	bool SetScalarParameter(const FString& ParamName, float Value) override;
	bool SetVector3Parameter(const FString& ParamName, const FVector& Value) override;
	bool SetVector4Parameter(const FString& ParamName, const FVector4& Value) override;
	bool SetTextureParameter(const FString& ParamName, UTexture2D* Texture) override;
	bool SetMatrixParameter(const FString& ParamName, const FMatrix& Value) override;

	bool GetScalarParameter(const FString& ParamName, float& OutValue) const override;
	bool GetVector3Parameter(const FString& ParamName, FVector& OutValue) const override;
	bool GetVector4Parameter(const FString& ParamName, FVector4& OutValue) const override;
	bool GetTextureParameter(const FString& ParamName, UTexture2D*& OutTexture) const override;
	bool GetMatrixParameter(const FString& ParamName, FMatrix& Value) const override;

	FShader* GetShader() const override;
	ERenderPass GetRenderPass() const override;
	EBlendState GetBlendState() const override;
	EDepthStencilState GetDepthStencilState() const override;
	ERasterizerState GetRasterizerState() const override;
	FConstantBuffer* GetGPUBufferBySlot(uint32 InSlot) const override;
	void FlushDirtyBuffers(ID3D11Device* Device, ID3D11DeviceContext* Ctx) override;
	ID3D11ShaderResourceView* GetSRV(EMaterialTextureSlot Slot) const override;

	EMaterialShadowMode GetShadowMode()  const override { return Parent->GetShadowMode(); }
	bool CastsShadow() const override { return GetShadowMode() != EMaterialShadowMode::None; }

protected:
	bool SetParameter(const FString& Name, const void* Data, uint32 Size);
	void CopyParentConstantBuffers();

	FString PathFileName;
	UMaterial* Parent = nullptr;
	FString ParentPathFileName;

	TMap<FString, float> ScalarOverrides;
	TMap<FString, FVector> Vector3Overrides;
	TMap<FString, FVector4> Vector4Overrides;
	TMap<FString, FMatrix> MatrixOverrides;
	TMap<FString, UTexture2D*> TextureOverrides;

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> ConstantBufferMap;
	ID3D11ShaderResourceView* CachedOverrideSRVs[(int)EMaterialTextureSlot::Max] = {};	//빠른 조회를 위한 테이블
	bool bHasTextureOverride[(int)EMaterialTextureSlot::Max] = {};

	bool bConstantBufferDirty = true;
};


UCLASS()
class UMaterialInstanceDynamic : public UMaterialInstance
{
public:
	GENERATED_BODY()

	static UMaterialInstanceDynamic* Create(UMaterial* InParent);
	static UMaterialInstanceDynamic* Create(UMaterial* InParent,
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers);

};
