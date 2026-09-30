// 게임 프레임워크 영역의 세부 동작을 구현합니다.
#include "FakeLightActor.h"
#include "AActor.h"
#include "Component/CylindricalBillboardComponent.h"
#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Materials/MaterialManager.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Runtime/Engine.h"

IMPLEMENT_CLASS(AFakeLightActor, AActor)

AFakeLightActor::AFakeLightActor()
{
    bNeedsTick = true;
    bTickInEditor = true;
}

void AFakeLightActor::InitDefaultComponents()
{
    // lamp mesh
    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UStaticMesh* MeshData = FObjManager::LoadObjStaticMesh(LampMeshDir, Device);
    StaticMeshComponent = AddComponent<UStaticMeshComponent>();
    StaticMeshComponent->SetStaticMesh(MeshData);
    SetRootComponent(StaticMeshComponent);

    // light beam Billboard
    BillboardComponent = AddComponent<UCylindricalBillboardComponent>();
    BillboardComponent->SetBillboardAxis(FVector(0.0f, 0.0f, 1.0f));

    auto LampshadeMaterial = FMaterialManager::Get().GetOrCreateMaterial(LampshadeMaterialPath);
    BillboardComponent->SetMaterial(LampshadeMaterial);

    BillboardComponent->SetRelativeLocation(FVector(0, 0, -0.7f));
    BillboardComponent->SetRelativeScale(FVector(3.0f, 3.0f, 3.0f));
    BillboardComponent->AttachToComponent(StaticMeshComponent);

    // 바닥 밝은 영역
    // NOTE: 빌보드 효과를 최대한 활용하기 위해 '전등'의 pivot은 아래, 즉, 바닥 밝은 영역과 일치해야 함
    DecalComponent = AddComponent<UDecalComponent>();
    auto Material = FMaterialManager::Get().GetOrCreateMaterial(DecalMaterialPath);
    DecalComponent->SetMaterial(0, Material);
    DecalComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    DecalComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
    DecalComponent->SetRelativeScale(FVector(3.0f, 3.0f, 3.0f));
    DecalComponent->AttachToComponent(StaticMeshComponent);
}
