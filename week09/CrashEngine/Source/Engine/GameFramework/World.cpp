// 게임 프레임워크 영역의 세부 동작을 구현합니다.
#include "World.h"

#include "Core/Logging/LogMacros.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Shape/ShapeComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Component/CameraComponent.h"
#include "Render/Visibility/LOD/LODContext.h"
#include <algorithm>
#include "Profiling/Stats.h"
#include "Engine/Physics/CollisionManager.h"

IMPLEMENT_CLASS(UWorld, UObject)

UWorld::UWorld() = default;
UWorld::~UWorld()
{
    if (PersistentLevel && !PersistentLevel->GetActors().empty())
    {
        EndPlay();
    }
}

AActor* UWorld::SpawnActorByClassName(const FString& ClassName)
{
    if (!PersistentLevel)
    {
        return nullptr;
    }

    UObject* Obj = FObjectFactory::Get().Create(ClassName, PersistentLevel);
    AActor* Actor = Cast<AActor>(Obj);
    if (!Actor)
    {
        if (Obj)
        {
            UObjectManager::Get().DestroyObject(Obj);
        }
        return nullptr;
    }

    AddActor(Actor);
    return Actor;
}

UObject* UWorld::Duplicate(UObject* NewOuter) const
{
    // UE의 CreatePIEWorldByDuplication 대응 (간소화 버전).
    // 새 UWorld를 만들고, 소스의 Actor들을 하나씩 복제해 NewWorld를 Outer로 삼아 등록한다.
    // AActor::Duplicate 내부에서 Dup->GetTypedOuter<UWorld>() 경유 AddActor가 호출되므로
    // 여기서는 World 단위 상태만 챙기면 된다.


    UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
    if (!NewWorld)
    {
        return nullptr;
    }
    NewWorld->SetOuter(NewOuter);
    NewWorld->InitWorld(); // Partition/VisibleSet 초기화 — 이거 없으면 복제 액터가 렌더링되지 않음
    NewWorld->SetWorldType(GetWorldType());

    for (AActor* Src : GetActors())
    {
        if (!Src)
            continue;
        AActor* Dup = Cast<AActor>(Src->Duplicate(NewWorld));
        if (Dup)
        {
            Dup->SetFName(Src->GetFName());
        }
    }

    NewWorld->PostDuplicate();
    return NewWorld;
}

void UWorld::DestroyActor(AActor* Actor)
{
    // remove and clean up
    if (!Actor)
        return;

    if (bIsEndingPlay)
    {
        return;
    }

	//충돌체 해재
	for (UPrimitiveComponent* Comp : Actor->GetPrimitiveComponents())
    {
        if (Comp->IsA<UShapeComponent>())
        {
            CollisionManager->UnregisterComponent(static_cast<UShapeComponent*>(Comp));
        }
    }

    Actor->EndPlay();
    // Remove from actor list
    PersistentLevel->RemoveActor(Actor);

    MarkEditorPickingAndScenePrimitiveBVHsDirty();
    Partition.RemoveActor(Actor);

    // Mark for garbage collection
    UObjectManager::Get().DestroyObject(Actor);
}

void UWorld::QueueDestroyActor(AActor* Actor)
{
    if (!Actor || Actor->GetWorld() != this || bIsEndingPlay)
    {
        return;
    }

    const uint32 ActorUUID = Actor->GetUUID();
    if (std::find(PendingDestroyActorUUIDs.begin(), PendingDestroyActorUUIDs.end(), ActorUUID) != PendingDestroyActorUUIDs.end())
    {
        return;
    }

    PendingDestroyActorUUIDs.push_back(ActorUUID);
}

void UWorld::AddActor(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    PersistentLevel->AddActor(Actor);

    // 액터 생성자 시점에는 Outer가 아직 Level/World에 연결되지 않았을 수 있다.
    // 그 상태에서 만들어진 primitive component는 CreateRenderState()가 early-out 되므로
    // 월드 등록 직후 누락된 렌더 스테이트를 한 번 더 보정한다.
    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
        {
            if (!Primitive->GetSceneProxy())
            {
                Primitive->CreateRenderState();
            }
        }
    }

    InsertActorToOctree(Actor);
    MarkEditorPickingAndScenePrimitiveBVHsDirty();

	//충돌체 등록
	//for (UPrimitiveComponent* Comp : Actor->GetPrimitiveComponents())
 //   {
 //       if (Comp->IsA<UShapeComponent>())
 //       {
 //           CollisionManager->RegisterComponent(static_cast<UShapeComponent*>(Comp));
 //       }
 //   }

    // PIE 중 Duplicate(Ctrl+D)나 SpawnActor로 들어온 액터에도 BeginPlay를 보장.
    if (bHasBegunPlay && !Actor->HasActorBegunPlay())
    {
        Actor->BeginPlay();
    }
}

void UWorld::QueueDamage(AActor* Target, float Damage, AActor* Instigator)
{
    if (!Target || Damage <= 0.0f)
    {
        return;
    }

    PendingDamages.push_back({
        Target->GetUUID(),
        Instigator ? Instigator->GetUUID() : 0,
        Damage
    });
}

void UWorld::ProcessPendingDamage()
{
    if (PendingDamages.empty())
    {
        return;
    }

    TArray<FPendingDamage> DamagesToProcess = PendingDamages;
    PendingDamages.clear();

    for (const FPendingDamage& PendingDamage : DamagesToProcess)
    {
        AActor* Target = Cast<AActor>(UObjectManager::Get().FindByUUID(PendingDamage.TargetUUID));
        if (!Target || Target->GetWorld() != this)
        {
            continue;
        }

        AActor* Instigator = Cast<AActor>(UObjectManager::Get().FindByUUID(PendingDamage.InstigatorUUID));
        if (Instigator && Instigator->GetWorld() != this)
        {
            Instigator = nullptr;
        }

        Target->TakeDamage(PendingDamage.Damage, Instigator);
    }
}

void UWorld::ProcessPendingActorDestroys()
{
    if (PendingDestroyActorUUIDs.empty())
    {
        return;
    }

    TArray<uint32> ActorsToDestroy = PendingDestroyActorUUIDs;
    PendingDestroyActorUUIDs.clear();

    for (uint32 ActorUUID : ActorsToDestroy)
    {
        AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(ActorUUID));
        if (!Actor || Actor->GetWorld() != this)
        {
            continue;
        }

        DestroyActor(Actor);
    }
}

void UWorld::MarkEditorPickingAndScenePrimitiveBVHsDirty()
{
    if (DeferredPickingBVHUpdateDepth > 0)
    {
        bDeferredPickingBVHDirty = true;
        return;
    }

    EditorPickingBVH.MarkDirty();
    ScenePrimitiveBVH.MarkDirty();
}

void UWorld::BuildEditorPickingBVHNow() const
{
    EditorPickingBVH.BuildNow(GetActors());
}

void UWorld::BuildScenePrimitiveBVHNow() const
{
    ScenePrimitiveBVH.BuildNow(GetActors());
}

void UWorld::BeginDeferredPickingBVHUpdate()
{
    ++DeferredPickingBVHUpdateDepth;
}

void UWorld::EndDeferredPickingBVHUpdate()
{
    if (DeferredPickingBVHUpdateDepth <= 0)
    {
        return;
    }

    --DeferredPickingBVHUpdateDepth;
    if (DeferredPickingBVHUpdateDepth == 0 && bDeferredPickingBVHDirty)
    {
        bDeferredPickingBVHDirty = false;
        BuildEditorPickingBVHNow();
        BuildScenePrimitiveBVHNow();
    }
}

void UWorld::WarmupPickingData() const
{
    for (AActor* Actor : GetActors())
    {
        if (!Actor || !Actor->IsVisible())
        {
            continue;
        }

        for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
        {
            if (!Primitive || !Primitive->ShouldRenderInCurrentWorld() || !Primitive->IsA<UStaticMeshComponent>())
            {
                continue;
            }

            UStaticMeshComponent* StaticMeshComponent = static_cast<UStaticMeshComponent*>(Primitive);
            if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
            {
                StaticMesh->EnsureMeshTrianglePickingBVHBuilt();
            }
        }
    }

    BuildEditorPickingBVHNow();
    BuildScenePrimitiveBVHNow();
}

bool UWorld::RaycastEditorPicking(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
    if (WorldType == EWorldType::PIE)
    {
        OutActor = nullptr;
        OutHitResult = FHitResult{};
        return false;
    }

    EditorPickingBVH.EnsureBuilt(GetActors());
    return EditorPickingBVH.Raycast(Ray, OutHitResult, OutActor);
}


void UWorld::InsertActorToOctree(AActor* Actor)
{
    Partition.InsertActor(Actor);
}

void UWorld::RemoveActorToOctree(AActor* Actor)
{
    Partition.RemoveActor(Actor);
}

void UWorld::UpdateActorInOctree(AActor* Actor)
{
    Partition.UpdateActor(Actor);
}

void UWorld::UpdateCollisionInBVH(UPrimitiveComponent* Comp)
{
    if (CollisionManager)
    {
        CollisionManager->UpdateCollisionInBVH(Comp);
    }
}

FLODUpdateContext UWorld::PrepareLODContext()
{
    if (!ActiveCamera)
        return {};

    const FVector CameraPos = ActiveCamera->GetWorldLocation();
    const FVector CameraForward = ActiveCamera->GetForwardVector();

    const uint32 LODUpdateFrame = VisibleProxyBuildFrame++;
    const uint32 LODUpdateSlice = LODUpdateFrame & (LOD_UPDATE_SLICE_COUNT - 1);
    const bool bShouldStaggerLOD = Scene.GetPrimitiveProxyCount() >= LOD_STAGGER_MIN_VISIBLE;

    const bool bForceFullLODRefresh =
        !bShouldStaggerLOD || LastLODUpdateCamera != ActiveCamera || !bHasLastFullLODUpdateCameraPos || FVector::DistSquared(CameraPos, LastFullLODUpdateCameraPos) >= LOD_FULL_UPDATE_CAMERA_MOVE_SQ || CameraForward.Dot(LastFullLODUpdateCameraForward) < LOD_FULL_UPDATE_CAMERA_ROTATION_DOT;

    if (bForceFullLODRefresh)
    {
        LastLODUpdateCamera = ActiveCamera;
        LastFullLODUpdateCameraPos = CameraPos;
        LastFullLODUpdateCameraForward = CameraForward;
        bHasLastFullLODUpdateCameraPos = true;
    }

    FLODUpdateContext Ctx;
    Ctx.CameraPos = CameraPos;
    Ctx.LODUpdateFrame = LODUpdateFrame;
    Ctx.LODUpdateSlice = LODUpdateSlice;
    Ctx.bForceFullRefresh = bForceFullLODRefresh;
    Ctx.bValid = true;
    return Ctx;
}

void UWorld::InitWorld()
{
    Partition.Reset(FBoundingBox());
    PersistentLevel = UObjectManager::Get().CreateObject<ULevel>(this);
    PersistentLevel->SetWorld(this);

	CollisionManager = std::make_unique<FCollisionManager>();
}

void UWorld::BeginPlay()
{
    bHasBegunPlay = true;

    if (PersistentLevel)
    {
        PersistentLevel->BeginPlay();



		/*for (auto Actor : PersistentLevel->GetActors())
        {
            for (UPrimitiveComponent* Comp : Actor->GetPrimitiveComponents())
            {
                if (Comp->IsA<UShapeComponent>())
                {
                    CollisionManager->RegisterComponent(static_cast<UShapeComponent*>(Comp));
                }
            }
        }*/
    }
    

}

void UWorld::Tick(float DeltaTime, ELevelTick TickType)
{
    GameTimeManager.Tick(DeltaTime);
    WorldTimeContext.Deltatime = GameTimeManager.GetDeltatime();
    WorldTimeContext.UnScaledDeltatime = GameTimeManager.GetUnScaledDeltatime();

    const float GameDeltaTime = WorldTimeContext.Deltatime;

    {
        SCOPE_STAT_CAT("FlushPrimitive", "1_WorldTick");
        Partition.FlushPrimitive();
    }

    Scene.GetDebugPrimitiveQueue().ClearOneFramePrimitives();
    Scene.GetDebugPrimitiveQueue().Tick(GameDeltaTime);

    TickManager.Tick(this, GameDeltaTime, TickType);
    ProcessPendingDamage();
    ProcessPendingActorDestroys();

	CollisionManager->TickCollision(GameDeltaTime, &Scene);
}

void UWorld::EndPlay()
{
    bHasBegunPlay = false;
    TickManager.Reset();
    PendingDamages.clear();
    PendingDestroyActorUUIDs.clear();

    if (!PersistentLevel)
    {
        return;
    }

    bIsEndingPlay = true;

    // Actor destruction calls PrimitiveComponent::DestroyRenderState().
    // Clear the partition first so components no longer hold octree node
    // pointers while the octree can be reshaped or destroyed during shutdown.
    Partition.Reset(FBoundingBox());

    PersistentLevel->EndPlay();
    bIsEndingPlay = false;

    Partition.Reset(FBoundingBox());

    PersistentLevel->Clear();
    MarkEditorPickingAndScenePrimitiveBVHsDirty();

    // PersistentLevel은 CreateObject로 생성되었으므로 DestroyObject로 해제해야 alloc count가 맞음
    UObjectManager::Get().DestroyObject(PersistentLevel);
    PersistentLevel = nullptr;
}
