#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/D3D11/D3D11Common.h"
#include "Renderer/RenderAsset/TextureResource.h"
#include <memory>

struct FMaterial
{
    FVector AmbientColor = {0.2f, 0.2f, 0.2f};  // Ka
    FVector DiffuseColor = {0.8f, 0.8f, 0.8f};  // Kd
    FVector SpecularColor = {0.0f, 0.0f, 0.0f}; // Ks
    FVector EmissiveColor = {0.0f, 0.0f, 0.0f}; // Ke
    FVector TransmissionFilter = {1.0f, 1.0f, 1.0f}; //Tf

    float   SpecularHighlight = 10.0f; // Ns
    float OpticalDensity = 1.0f; // Ni
    float Opacity = 1.0f;        // d (or 1.0 - Tr)
    int32 IlluminationModel = 2; // illum

    // --- 텍스처 경로 ---
    FString AmbientMapPath = "";           // map_Ka
    FString DiffuseMapPath = "";           // map_Kd
    FString SpecularHighlightMapPath = ""; // map_Ns
    FString NormalMapPath = "";            // map_bump 또는 bump

    // --- 런타임 텍스처 리소스 포인터 (Component에서 로드 시 채워짐) ---
    FTextureResource* AmbientTexture = nullptr;
    FTextureResource* DiffuseTexture = nullptr;
    FTextureResource* SpecularTexture = nullptr;
    FTextureResource* NormalTexture = nullptr;

    FVector2 UVScrollSpeed = {0.0f, 0.0f}; // UV 스크롤 속도 추가
};
