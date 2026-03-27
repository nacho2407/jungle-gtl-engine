#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/RenderAsset/TextureResource.h"
#include <memory>

struct FMaterialResource
{
    // 표면 속성 (Ka, Kd, Ks 등)
    FVector AmbientColor = {0.2f, 0.2f, 0.2f};
    FVector DiffuseColor = {0.8f, 0.8f, 0.8f};
    FVector SpecularColor = {0.0f, 0.0f, 0.0f};
    float   SpecularHighlight = 10.0f; // Ns

    // 텍스처 리소스 참조 (map_Kd)
    // TextureLoader가 생성한 리소스의 참조 카운트를 공유하여 메모리 중복 방지
    std::shared_ptr<FTextureResource> DiffuseTexture;

    ID3D11ShaderResourceView* GetDiffuseSRV() const
    {
        return DiffuseTexture ? DiffuseTexture->GetSRV() : nullptr;
    }

    void Reset()
    {
        AmbientColor = {0.2f, 0.2f, 0.2f};
        DiffuseColor = {0.8f, 0.8f, 0.8f};
        SpecularColor = {0.0f, 0.0f, 0.0f};
        SpecularHighlight = 10.0f;
        DiffuseTexture.reset();
    }
};
