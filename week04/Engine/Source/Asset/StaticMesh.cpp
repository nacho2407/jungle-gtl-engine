#include "Core/CoreMinimal.h"
#include "Asset/StaticMesh.h"

UStaticMesh::UStaticMesh() : StaticMeshAsset(nullptr) {}

UStaticMesh::~UStaticMesh() {}

void UStaticMesh::Serialize(class FArchive& Ar)
{
    UStreamableRenderAsset::Serialize(Ar);
    // StaticMesh 고유 데이터 직렬화 (나중에 구현)
}

REGISTER_CLASS(, UStaticMesh)
