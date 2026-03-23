#include "ViewportSelectionController.h"

#include <algorithm>

#include "Engine/Component/PrimitiveComponent.h"
#include "Engine/Game/Actor.h"
#include "Core/Geometry/Primitives/Ray.h"

void FViewportSelectionController::ClickSelect(int32 MouseX, int32 MouseY, ESelectionMode Mode)
{
    AActor* PickedActor = PickActor(MouseX, MouseY);

    switch (Mode)
    {
    case ESelectionMode::Replace:
        if (PickedActor == nullptr)
        {
            ClearSelection();
        }
        else
        {
            SelectSingle(PickedActor);
        }
        break;
    case ESelectionMode::Add:
        if (PickedActor != nullptr)
        {
            AddSelection(PickedActor);
        }
        break;
    case ESelectionMode::Toggle:
        if (PickedActor != nullptr)
        {
            ToggleSelection(PickedActor);
        }
        break;
    default:
        break;
    }
}

void FViewportSelectionController::BeginSelection(int32 MouseX, int32 MouseY, ESelectionMode Mode)
{
    bIsDraggingSelection = true;
    SelectionStartX = MouseX;
    SelectionStartY = MouseY;
    CurSelectionMode = Mode;
}

void FViewportSelectionController::UpdateSelection(int32 MouseX, int32 MouseY)
{
    if (!bIsDraggingSelection)
    {
        return;
    }
    //  TODO : Drag 선택 영역 갱신
}

void FViewportSelectionController::EndSelection(int32 MouseX, int32 MouseY)
{
    if (!bIsDraggingSelection)
    {
        return;
    }

    bIsDraggingSelection = false;

    //  TODO : 박스 선택 결과 반영
}

void FViewportSelectionController::ClearSelection() { SelectedActors.clear(); }

bool FViewportSelectionController::IsSelected(AActor* Actor) const
{
    if (Actor == nullptr)
    {
        return false;
    }

    auto Iterator = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);

    if (Iterator != SelectedActors.end())
    {
        return true;
    }

    return false;
}

// Geometry::FRay FViewportSelectionController::BuildPickRay(int32 MouseX, int32 MouseY) const
//{
//     if (ViewportCamera == nullptr || ViewportWidth <= 0 || ViewportHeight <= 0)
//     {
//         return Geometry::FRay{};
//     }
//
//     const float NDCX = (2.0f * static_cast<float>(MouseX) / static_cast<float>(ViewportWidth)
//     - 1.0f); const float NDCY = 1.0f - (2.0f * static_cast<float>(MouseY) /
//     static_cast<float>(ViewportHeight));
//
//     const FVector NearPointNDC(NDCX, NDCY, 0.0f);
//     const FVector FarPointNDC(NDCX, NDCY, 1.0f);
//
//     const FMatrix ViewProjection = ViewportCamera->GetViewProjectionMatrix();
//     const FMatrix InvViewProjection = ViewProjection.GetInverse();
//
//     const FVector NearWorld = InvViewProjection.TransformPosition(NearPointNDC);
//     const FVector FarWorld = InvViewProjection.TransformPosition(FarPointNDC);
//
//     const FVector Direction = (FarWorld - NearWorld).GetSafeNormal();
//
//     return Geometry::FRay{NearWorld, Direction};
// }

AActor* FViewportSelectionController::PickActor(int32 MouseX, int32 MouseY) const
{
    //  TODO : Ray Cast, AABB
    if (Actors == nullptr || ViewportCamera == nullptr)
    {
        return nullptr;
    }

    const Geometry::FRay PickRay =
        Geometry::FRay::BuildRay(MouseX, MouseY, ViewportCamera->GetViewProjectionMatrix(),
                                 static_cast<float>(ViewportCamera->GetWidth()),
                                 static_cast<float>(ViewportCamera->GetHeight()));

    AActor* ClosestActor = nullptr;
    float   ClosestT = FLT_MAX;

    for (AActor* Actor : *Actors)
    {
        if (Actor == nullptr || !Actor->IsPickable())
        {
            continue;
        }

        auto* RootComponent = Actor->GetRootComponent();
        if (RootComponent == nullptr)
        {
            continue;
        }

        //  TODO : 나중에 RTTI로 수정할 수 있나?
        auto* PrimitiveComponent =
            dynamic_cast<Engine::Component::UPrimitiveComponent*>(RootComponent);
        if (PrimitiveComponent == nullptr)
        {
            //  Casting Fail
            continue;
        }

        //  Broad Phase (AABB)
        const Geometry::FAABB& WorldAABB = PrimitiveComponent->GetWorldAABB();
        float                  AABBHitT = 0.0f;
        if (Geometry::IntersectRayAABB(PickRay, WorldAABB, AABBHitT))
        {
            if (AABBHitT >= 0.0f && AABBHitT <= ClosestT)
            {
                ClosestActor = Actor;
                ClosestT = AABBHitT;
            }
            continue;
        }

        //  Narrow Phase (Ray-Triangle)
        TArray<Geometry::FTriangle> LocalTriangles;
        if (!PrimitiveComponent->GetLocalTriangles(LocalTriangles))
        {
            //  Triangle Data가 없다면 AABB로 Fallback
            if (AABBHitT >= 0.0f && AABBHitT < ClosestT)
            {
                ClosestActor = Actor;
                ClosestT = AABBHitT;
            }
            continue;
        }

        const FMatrix WorldMatrix = PrimitiveComponent->GetRelativeMatrix();

        bool  bHitTriangle = false;
        float ClosestTriangleT = FLT_MAX;

        for (const Geometry::FTriangle& LocalTriangle : LocalTriangles)
        {
            Geometry::FTriangle WorldTriangle;
            WorldTriangle.V0 = WorldMatrix.TransformPosition(LocalTriangle.V0);
            WorldTriangle.V1 = WorldMatrix.TransformPosition(LocalTriangle.V1);
            WorldTriangle.V2 = WorldMatrix.TransformPosition(LocalTriangle.V2);

            float TriangleHitT = 0.0f;

            if (Geometry::IntersectRayTriangle(PickRay, WorldTriangle, TriangleHitT))
            {
                if (TriangleHitT >= 0.0f && TriangleHitT < ClosestTriangleT)
                {
                    ClosestTriangleT = TriangleHitT;
                    bHitTriangle = true;
                }
            }
        }

        if (bHitTriangle && ClosestTriangleT < ClosestT)
        {
            ClosestT = ClosestTriangleT;
            ClosestActor = Actor;
        }
    }

    return ClosestActor;
}

void FViewportSelectionController::SelectSingle(AActor* Actor)
{
    SelectedActors.clear();

    if (Actor != nullptr && Actor->IsPickable())
    {
        SelectedActors.push_back(Actor);
        MessageBox(nullptr, L"Actor->Name.GetString().c_str()", L"Selected Actor", MB_OK);
    }
}

void FViewportSelectionController::AddSelection(AActor* Actor)
{
    if (Actor != nullptr && Actor->IsPickable())
    {
        if (!IsSelected(Actor))
        {
            SelectedActors.push_back(Actor);
        }
    }
}

void FViewportSelectionController::RemoveSelection(AActor* Actor)
{
    if (Actor == nullptr)
    {
        return;
    }

    auto Iterator = std::remove(SelectedActors.begin(), SelectedActors.end(), Actor);
    SelectedActors.erase(Iterator, SelectedActors.end());
}

void FViewportSelectionController::ToggleSelection(AActor* Actor)
{
    if (Actor == nullptr || !Actor->IsPickable())
    {
        return;
    }

    if (IsSelected(Actor))
    {
        RemoveSelection(Actor);
    }
    else
    {
        AddSelection(Actor);
    }
}