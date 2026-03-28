#include "ViewportSelectionController.h"

#include <algorithm>

#include "Editor/EditorContext.h"
#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Game/Actor.h"
#include "Core/Geometry/Primitives/Ray.h"
#include "Engine/Component/Mesh/QuadComponent.h"
#include "Renderer/Types/VertexTypes.h"

void FViewportSelectionController::ClickSelect(int32 MouseX, int32 MouseY, ESelectionMode Mode)
{
    SelectActor(PickActor(MouseX, MouseY), Mode);
}

void FViewportSelectionController::BeginSelection(int32 MouseX, int32 MouseY, ESelectionMode Mode)
{
    bIsDraggingSelection = true;
    SelectionStartX = MouseX;
    SelectionStartY = MouseY;
    SelectionCurrentX = MouseX;
    SelectionCurrentY = MouseY;
    CurSelectionMode = Mode;
}

void FViewportSelectionController::UpdateSelection(int32 MouseX, int32 MouseY)
{
    if (!bIsDraggingSelection)
    {
        return;
    }

    SelectionCurrentX = MouseX;
    SelectionCurrentY = MouseY;
}

void FViewportSelectionController::EndSelection(int32 MouseX, int32 MouseY)
{
    if (!bIsDraggingSelection)
    {
        return;
    }

    if (CurSelectionMode == ESelectionMode::Replace)
    {
        ClearSelection();
    }

    SelectionCurrentX = MouseX;
    SelectionCurrentY = MouseY;
    bIsDraggingSelection = false;

    //  Rect 계산

    const int32 MinX = std::min(SelectionStartX, SelectionCurrentX);
    const int32 MaxX = std::max(SelectionStartX, SelectionCurrentX);
    const int32 MinY = std::min(SelectionStartY, SelectionCurrentY);
    const int32 MaxY = std::max(SelectionStartY, SelectionCurrentY);

    if (std::abs(MaxX - MinX) < 5 && std::abs(MaxY - MinY) < 5)
    {
        bIsDraggingSelection = false;
        return;
    }

    for (AActor* Actor : *Actors)
    {
        if (Actor == nullptr || !Actor->IsPickable())
        {
            continue;
        }

        FVector2 ScreenPos{0.f, 0.f};

        if (!ProjectWorldToScreen(Actor->GetWorldMatrix().GetOrigin(), ScreenPos))
        {
            continue;
        }

        if (ScreenPos.X >= MinX && ScreenPos.X <= MaxX && ScreenPos.Y >= MinY && ScreenPos.Y <=
            MaxY)
        {
            AddSelection(Actor);
        }
    }
}

void FViewportSelectionController::SelectActor(AActor* Actor, ESelectionMode Mode)
{
    switch (Mode)
    {
    case ESelectionMode::Replace:
        if (Actor == nullptr)
        {
            ClearSelection();
        }
        else
        {
            SelectSingle(Actor);
        }
        break;
    case ESelectionMode::Add:
        if (Actor != nullptr)
        {
            AddSelection(Actor);
        }
        break;
    case ESelectionMode::Toggle:
        if (Actor != nullptr)
        {
            ToggleSelection(Actor);
        }
        break;
    default:
        break;
    }
}

void FViewportSelectionController::ClearSelection()
{
    ResetSelection();
    UpdatePrimarySelection();
}

void FViewportSelectionController::SyncSelectionFromContext()
{
    if (Context == nullptr)
    {
        return;
    }

    ResetSelection();

    for (AActor* SelectedActor : Context->SelectedActors)
    {
        if (SelectedActor != nullptr && !IsSelected(SelectedActor))
        {
            SelectedActors.push_back(SelectedActor);
            ApplySelectionState(SelectedActor, true);
        }
    }

    if (!SelectedActors.empty())
    {
        if (Context->SelectedObject != nullptr)
        {
            if (AActor* SelectedActorObject = Cast<AActor>(Context->SelectedObject))
            {
                if (IsSelected(SelectedActorObject))
                {
                    return;
                }
            }
            else if (auto* SelectedComponent =
                Cast<Engine::Component::USceneComponent>(Context->SelectedObject))
            {
                if (SelectedComponent->GetOwnerActor() != nullptr &&
                    IsSelected(SelectedComponent->GetOwnerActor()))
                {
                    return;
                }
            }
        }

        Context->SelectedObject = SelectedActors.back();
        return;
    }

    if (AActor* SelectedActor = ResolveActorFromContextSelection())
    {
        SelectedActors.push_back(SelectedActor);
        ApplySelectionState(SelectedActor, true);
        Context->SelectedActors = SelectedActors;
        Context->SelectedObject = SelectedActor;
        return;
    }

    Context->SelectedActors.clear();
    Context->SelectedObject = nullptr;
}

bool FViewportSelectionController::IsSelected(AActor* Actor) const
{
    if (Actor == nullptr)
    {
        return false;
    }

    const auto Iterator = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    return Iterator != SelectedActors.end();
}

//Geometry::FRay FViewportSelectionController::BuildPickRay(int32 MouseX, int32 MouseY) const
//{
//    if (ViewportCamera == nullptr || ViewportWidth <= 0 || ViewportHeight <= 0)
//    {
//        return Geometry::FRay{};
//    }
//
//    const float NDCX =
//        (2.0f * static_cast<float>(MouseX) / static_cast<float>(ViewportWidth) - 1.0f);
//    const float NDCY =
//        1.0f - (2.0f * static_cast<float>(MouseY) / static_cast<float>(ViewportHeight));
//
//    const FVector NearPointNDC(NDCX, NDCY, 0.0f);
//    const FVector FarPointNDC(NDCX, NDCY, 1.0f);
//
//    const FMatrix ViewProjection = ViewportCamera->GetViewProjectionMatrix();
//    const FMatrix InvViewProjection = ViewProjection.GetInverse();
//
//    const FVector NearWorld = InvViewProjection.TransformPosition(NearPointNDC);
//    const FVector FarWorld = InvViewProjection.TransformPosition(FarPointNDC);
//
//    return Geometry::FRay{NearWorld, (FarWorld - NearWorld).GetSafeNormal()};
//}

AActor* FViewportSelectionController::PickActor(int32 MouseX, int32 MouseY) const
{
    if (Actors == nullptr || ViewportCamera == nullptr)
    {
        return nullptr;
    }

    const int32 LocalMouseX = MouseX - static_cast<int32>(ViewportCamera->GetOriginX());
    const int32 LocalMouseY = MouseY - static_cast<int32>(ViewportCamera->GetOriginY());
    const Geometry::FRay PickRay = Geometry::FRay::BuildRay(
        LocalMouseX, LocalMouseY,
        ViewportCamera->GetViewProjectionMatrix(), static_cast<float>(ViewportCamera->GetWidth()),
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

        auto* PrimitiveComponent = Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
        if (PrimitiveComponent == nullptr)
        {
            continue;
        }

        //  Quad Billboard는 처리 안합니다.
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

        TArray<Geometry::FTriangle> LocalTriangles;
        if (!PrimitiveComponent->GetLocalTriangles(LocalTriangles))
        {
            if (AABBHitT >= 0.0f && AABBHitT < ClosestT)
            {
                ClosestActor = Actor;
                ClosestT = AABBHitT;
            }
            continue;
        }

        FMatrix WorldMatrix = PrimitiveComponent->GetRelativeMatrix();
        
        bool  bHitTriangle = false;
        float ClosestTriangleT = FLT_MAX;

        if (auto* QuadComponent = dynamic_cast<Engine::Component::UQuadComponent*>(
            PrimitiveComponent))
        {
            if (QuadComponent != nullptr)
            {
                const FMatrix CameraMatrix = ViewportCamera->GetViewMatrix().GetInverse();
                FVector       RightAxis = CameraMatrix.GetRightVector();
                FVector       UpAxis = CameraMatrix.GetUpVector();

                RightAxis = RightAxis * WorldMatrix.GetScaleVector().X;
                UpAxis = UpAxis * WorldMatrix.GetScaleVector().Z;

                FVector Origin = WorldMatrix.GetOrigin();

                FVector BottomLeft = Origin - RightAxis - UpAxis;
                
                FVector V0 = BottomLeft;
                FVector V1 = BottomLeft + RightAxis * 2.0f;
                FVector V2 = BottomLeft + UpAxis * 2.0f + RightAxis * 2.0f;
                FVector V3 = BottomLeft + UpAxis * 2.0f;

                Geometry::FTriangle QuadTriangles[2] ={};
                
                QuadTriangles[0].V0 = V0;
                QuadTriangles[0].V1 = V1;
                QuadTriangles[0].V2 = V2;
                
                QuadTriangles[1].V0 = V0;
                QuadTriangles[1].V1 = V2;
                QuadTriangles[1].V2 = V3;
                
                float TriangleHitT = 0.0f;
                if (Geometry::IntersectRayTriangle(PickRay, QuadTriangles[0], TriangleHitT))
                {
                    if (TriangleHitT >= 0.0f && TriangleHitT < ClosestTriangleT)
                    {
                        ClosestTriangleT = TriangleHitT;
                        bHitTriangle = true;
                    }
                }
                if (Geometry::IntersectRayTriangle(PickRay, QuadTriangles[1], TriangleHitT))
                {
                    if (TriangleHitT >= 0.0f && TriangleHitT < ClosestTriangleT)
                    {
                        ClosestTriangleT = TriangleHitT;
                        bHitTriangle = true;
                    }
                }
            }
        }
        else
        {
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
    ResetSelection();

    if (Actor != nullptr && Actor->IsPickable())
    {
        SelectedActors.push_back(Actor);
        ApplySelectionState(Actor, true);
    }

    UpdatePrimarySelection();
}

void FViewportSelectionController::AddSelection(AActor* Actor)
{
    if (Actor != nullptr && Actor->IsPickable() && !IsSelected(Actor))
    {
        SelectedActors.push_back(Actor);
        ApplySelectionState(Actor, true);
    }

    UpdatePrimarySelection();
}

void FViewportSelectionController::RemoveSelection(AActor* Actor)
{
    if (Actor == nullptr)
    {
        return;
    }

    const auto Iterator = std::remove(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (Iterator != SelectedActors.end())
    {
        ApplySelectionState(Actor, false);
        SelectedActors.erase(Iterator, SelectedActors.end());
    }

    UpdatePrimarySelection();
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

void FViewportSelectionController::ResetSelection()
{
    for (AActor* SelectedActor : SelectedActors)
    {
        ApplySelectionState(SelectedActor, false);
    }

    SelectedActors.clear();
}

void FViewportSelectionController::ApplySelectionState(AActor* Actor, bool bSelected) const
{
    if (Actor == nullptr)
    {
        return;
    }

    if (Engine::Component::USceneComponent* RootComponent = Actor->GetRootComponent())
    {
        RootComponent->SetSelected(bSelected);
    }

    for (Engine::Component::USceneComponent* Component : Actor->GetOwnedComponents())
    {
        auto* PrimitiveComponent = Cast<Engine::Component::UPrimitiveComponent>(Component);
        if (PrimitiveComponent == nullptr)
        {
            continue;
        }

        PrimitiveComponent->SetShowBounds(bSelected);
    }
}

AActor* FViewportSelectionController::ResolveActorFromContextSelection() const
{
    if (Context == nullptr || Context->SelectedObject == nullptr)
    {
        return nullptr;
    }

    if (AActor* SelectedActor = Cast<AActor>(Context->SelectedObject))
    {
        return SelectedActor;
    }

    auto* SelectedComponent = Cast<Engine::Component::USceneComponent>(Context->SelectedObject);
    if (SelectedComponent == nullptr)
    {
        return nullptr;
    }

    if (SelectedComponent->GetOwnerActor() != nullptr)
    {
        return SelectedComponent->GetOwnerActor();
    }

    if (Actors == nullptr)
    {
        return nullptr;
    }

    for (AActor* Actor : *Actors)
    {
        if (Actor == nullptr)
        {
            continue;
        }

        if (Actor->GetRootComponent() == SelectedComponent)
        {
            return Actor;
        }

        for (Engine::Component::USceneComponent* Component : Actor->GetOwnedComponents())
        {
            if (Component == SelectedComponent)
            {
                return Actor;
            }
        }
    }

    return nullptr;
}

void FViewportSelectionController::UpdatePrimarySelection() const
{
    if (Context == nullptr)
    {
        return;
    }

    Context->SelectedActors = SelectedActors;
    Context->SelectedObject = SelectedActors.empty() ? nullptr : SelectedActors.back();
}

bool FViewportSelectionController::ProjectWorldToScreen(const FVector& WorldPos,
                                                        FVector2&      OutScreenPos) const
{
    if (ViewportCamera == nullptr || ViewportWidth <= 0 || ViewportHeight <= 0)
    {
        return false;
    }

    const FMatrix ViewProjection = ViewportCamera->GetViewProjectionMatrix();

    const FVector4 Pos = FVector4(WorldPos.X, WorldPos.Y, WorldPos.Z, 1.0f);

    const FVector4 Clip = ViewProjection.TransformVector4(Pos, ViewProjection);

    const float ClipX = Clip.X;
    const float ClipY = Clip.Y;
    const float ClipW = Clip.W;

    if (ClipW <= 0.0f)
    {
        return false;
    }

    const float NDCX = ClipX / ClipW;
    const float NDCY = ClipY / ClipW;

    OutScreenPos.X = (NDCX * 0.5f + 0.5f) * static_cast<float>(ViewportWidth);
    OutScreenPos.Y = (1.0f - (NDCY * 0.5f + 0.5f)) * static_cast<float>(ViewportHeight);

    return true;
}