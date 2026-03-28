#include "Core/CoreMinimal.h"
#include "Asset/StaticMesh.h"

UStaticMesh::UStaticMesh() : RenderResource(nullptr) {}

UStaticMesh::~UStaticMesh() {}

void UStaticMesh::Initialize(std::shared_ptr<FStaticMeshResource> InResource)
{
    RenderResource = InResource;
}

void UStaticMesh::Serialize(class FArchive& Ar)
{
    UStreamableRenderAsset::Serialize(Ar);
    // StaticMesh 고유 데이터 직렬화 (나중에 구현)
}

REGISTER_CLASS(, UStaticMesh)
