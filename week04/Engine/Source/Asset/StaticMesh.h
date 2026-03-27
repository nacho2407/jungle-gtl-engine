#pragma once

#include "Asset.h"

struct FStaticMesh;

class ENGINE_API UStaticMesh : public UAsset
{
  public:
    DECLARE_RTTI(UStaticMesh, UAsset)

    UStaticMesh();
    virtual ~UStaticMesh() override;

    // Mesh Data Getter/Setter
    void         SetStaticMeshAsset(FStaticMesh* InStaticMesh) { StaticMeshAsset = InStaticMesh; }
    FStaticMesh* GetStaticMeshAsset() const { return StaticMeshAsset; }

    const FString& GetAssetPathFileName() const { return GetAssetName(); }

  private:
    FStaticMesh* StaticMeshAsset = nullptr;
};