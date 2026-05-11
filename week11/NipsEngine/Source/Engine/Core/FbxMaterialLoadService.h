#pragma once

#include "Core/CoreMinimal.h"

class FResourceManager;
struct ID3D11Device;

/**
 * @brief FBX 내장 surface material을 UMaterial로 등록하고 slot alias를 거는 오케스트레이터.
 * MaterialLoadService의 MTL 등록 패턴 (line 82~142) 미러.
 */
class FFbxMaterialLoadService
{
public:
    explicit FFbxMaterialLoadService(FResourceManager& InResourceManager);

    bool Load(const FString& FbxFilePath, const FString& ShaderName, ID3D11Device* Device);

private:
    FResourceManager& ResourceManager;
};
