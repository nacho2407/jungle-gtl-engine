#pragma once
#include <dxgiformat.h>
#include <memory>

#include "Core/CoreMinimal.h"
#include "Asset.h"

struct FSourceRecord;
struct FTextureBuildSettings;
struct FTextureResource;
struct ID3D11ShaderResourceView;

/*
 * UTexture2DAsset 클래스 책임
 * - 원본 source 메타데이터 저장
 * - 텍스처 크기/포맷 저장
 * - 빌드 설정 일부 저장
 * - 최종 FTextureResource 참조 보관
 * - 렌더러가 SRV를 꺼내갈 수 있는 최소 getter 제공
 * 이 클래스는 "자산 껍데기 + 런타임 리소스 핸들" 입니다.
 */

class ENGINE_API UTexture2DAsset : public UAsset
{
public:
    DECLARE_RTTI(UTexture2DAsset, UAsset)

    void Initialize(const FSourceRecord& Source,
        const FTextureBuildSettings& Settings,
        std::shared_ptr<FTextureResource> InResource,
        uint32 InWidth,
        uint32 InHeight);

    uint32 GetWidth() const { return Width; }
    uint32 GetHeight() const { return Height; }
    DXGI_FORMAT GetFormat() const { return Format; }
    bool IsSRGB() const { return bSRGB; }

    FTextureResource* GetResource() const { return Resource.get(); }
    ID3D11ShaderResourceView* GetSRV() const;

private:
    uint32 Width = 0;
    uint32 Height = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    bool bSRGB = true;

    std::shared_ptr<FTextureResource> Resource;
};

