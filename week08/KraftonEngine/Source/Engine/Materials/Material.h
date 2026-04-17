#pragma once

#include "Object/ObjectFactory.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/RenderStateTypes.h"
#include "MaterialCore.h"
#include <memory>

class UTexture2D;
class FArchive;

class UMaterialInterface : public UObject
{
public:

	virtual bool SetScalarParameter(const FString& Name, float Value) = 0;
	virtual bool SetVector3Parameter(const FString& ParamName, const FVector& Value) = 0;
	virtual bool SetVector4Parameter(const FString& Name, const FVector4& Value) = 0;
	virtual bool SetTextureParameter(const FString& Name, UTexture2D* Texture) = 0;
	virtual bool SetMatrixParameter(const FString& ParamName, const FMatrix& Value) = 0;

	virtual bool GetScalarParameter(const FString& ParamName, float& OutValue) const = 0;
	virtual bool GetVector3Parameter(const FString& ParamName, FVector& OutValue) const = 0;
	virtual bool GetVector4Parameter(const FString& ParamName, FVector4& OutValue) const = 0;
	virtual bool GetTextureParameter(const FString& ParamName, UTexture2D*& OutTexture) const = 0;
	virtual bool GetMatrixParameter(const FString& ParamName, FMatrix& Value) const = 0;
};


//파라미터 값 + 텍스처 (런타임 데이터)
//JSON으로 직렬화되는 데이터
class UMaterial : public UMaterialInterface
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

	bool SetParameter(const FString& Name, const void* Data, uint32 Size);

public:
	DECLARE_CLASS(UMaterial, UMaterialInterface)
	~UMaterial() override;

	void Create(const FString& InPathFileName, FMaterialTemplate* InTemplate,
		ERenderPass InRenderPass,
		EBlendState InBlend,
		EDepthStencilState InDepth,
		ERasterizerState InRaster,
		TMap<FString, std::unique_ptr<FMaterialConstantBuffer>>&& InBuffers);

	const uint8* GetRawPtr(const FString& BufferName, uint32 Offset) const;
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

	FShader* GetShader() const { return Template ? Template->GetShader() : nullptr; }
	ERenderPass GetRenderPass() const { return RenderPass; }
	EBlendState GetBlendState() const { return BlendState; }
	EDepthStencilState GetDepthStencilState() const { return DepthStencilState; }
	ERasterizerState GetRasterizerState() const { return RasterizerState; }

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


//UMaterialInstanceDynamic: UMaterial 원본을 공유하며 파라미터만 오버라이드
class UMaterialInstanceDynamic : public UMaterial
{
public:
	DECLARE_CLASS(UMaterialInstanceDynamic, UMaterial)
	static UMaterialInstanceDynamic* Create(UMaterial* InParent);

	void Serialize(FArchive& Ar) override;
private:
	UMaterial* Parent = nullptr; // 공유 원본
	FString ParentPathFileName; // 직렬화용 원본 경로 (로드 시 Parent 연결에 사용)

};