// 머티리얼 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Object/ObjectFactory.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "MaterialCore.h"

#include <memory>

class UTexture2D;
class FArchive;

// UMaterialInterface는 머티리얼 파라미터와 렌더 리소스를 다룹니다.
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

// UMaterial는 머티리얼 파라미터와 렌더 리소스를 다룹니다.
class UMaterial : public UMaterialInterface
{
private:
    FString PathFileName;
    uint32 MaterialInstanceID = 0;
    FMaterialTemplate* Template = nullptr;

    TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> ConstantBufferMap;
    TMap<FString, UTexture2D*> TextureParameters;
    TMap<FString, float> LooseScalarParameters;
    TMap<FString, FVector> LooseVector3Parameters;
    TMap<FString, FVector4> LooseVector4Parameters;
    TMap<FString, FMatrix> LooseMatrixParameters;

    bool SetParameter(const FString& Name, const void* Data, uint32 Size);

public:
    DECLARE_CLASS(UMaterial, UMaterialInterface)
    ~UMaterial() override;

    void Create(
        const FString& InPathFileName,
        FMaterialTemplate* InTemplate,
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
    UTexture2D* GetDiffuseTexture() const;
    UTexture2D* GetNormalTexture() const;
    UTexture2D* GetSpecularTexture() const;

    const FString& GetTexturePathFileName(const FString& TextureName) const;
    const FString& GetAssetPathFileName() const { return PathFileName; }
    void SetAssetPathFileName(const FString& InPath) { PathFileName = InPath; }
    void Serialize(FArchive& Ar);

    FConstantBuffer* GetGPUBufferBySlot(uint32 InSlot) const
    {
        for (const auto& Pair : ConstantBufferMap)
        {
            if (Pair.second->SlotIndex == InSlot)
            {
                return Pair.second->GetConstantBuffer();
            }
        }
        return nullptr;
    }
};

// UMaterialInstanceDynamic는 머티리얼 파라미터와 렌더 리소스를 다룹니다.
class UMaterialInstanceDynamic : public UMaterial
{
public:
    DECLARE_CLASS(UMaterialInstanceDynamic, UMaterial)
    static UMaterialInstanceDynamic* Create(UMaterial* InParent);

    void Serialize(FArchive& Ar) override;

private:
    UMaterial* Parent = nullptr;
    FString ParentPathFileName;
};
