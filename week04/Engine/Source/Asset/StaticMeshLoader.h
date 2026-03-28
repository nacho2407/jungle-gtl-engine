#pragma once
#include "Core/CoreMinimal.h"
#include "AssetLoader.h"
#include "AssetManager.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"


class FD3D11RHI;
struct FVertex; // 엔진에 정의된 버텍스 구조체 (Position, UV, Normal 등 포함)

class ENGINE_API FStaticMeshLoader : public IAssetLoader
{
  public:
    explicit FStaticMeshLoader(FD3D11RHI* InRHI, UAssetManager* InAssetManager);

    bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
    EAssetType GetAssetType() const override;
    uint64     MakeBuildSignature(const FAssetLoadParams& Params) const override;
    UAsset*    LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;

  private:
    bool ParseObjText(const FSourceRecord& Source, FStaticMeshResource& OutMesh,
                      TArray<FVertex>& OutVertices) const;
    bool CreateBuffers(const TArray<FVertex>& InVertices, FStaticMeshResource& OutMesh) const;

    // 문자열 유틸리티
    FString WidePathToUtf8(const FWString& Path) const;

  private:
    FD3D11RHI* RHI = nullptr;
    UAssetManager* AssetManager = nullptr;


    //Asset* LoadFromBinary(const std::string& BinPath)
    //{
    //    // 바이너리 파일을 열고 FNormalVertex 배열과 Index 배열을
    //    // 메모리에 통째로 읽어 들인 후 Asset 생성
    //}

    void SaveToBinary(const std::string& BinPath, FStaticMeshResource* CookedData)
    {
        // CookedData의 Vertices 배열과 Indices 배열의 크기 및 메모리 블록을
        // .bin 파일에 바이너리 쓰기(fwrite 또는 std::ofstream::write)로 저장
    }
};