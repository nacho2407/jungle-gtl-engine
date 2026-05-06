#pragma once

#include "Core/CoreMinimal.h"
#include "Geometry/OBB.h"
#include "Render/Common/ViewTypes.h"

#include <unordered_set>

class AActor;
class FMeshBufferManager;
class FRenderBus;
class UGizmoComponent;
class UPrimitiveComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USpotlightComponent;
struct FShowFlags;

class FEditorOverlayCollector
{
public:
    void CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode,
                          FRenderBus& RenderBus, FMeshBufferManager& MeshBufferManager,
                          bool bIncludeEditorOnlyPrimitives) const;
    void CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus,
                      FMeshBufferManager& MeshBufferManager, bool bIsActiveOperation) const;
    void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic) const;

private:
    bool CollectFromSelectedActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                  FRenderBus& RenderBus, FMeshBufferManager& MeshBufferManager,
                                  bool bIncludeEditorOnlyPrimitives) const;
    void CollectBVHInternalNodeAABBs(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags,
                                     FRenderBus& RenderBus, std::unordered_set<int32>& SeenNodeIndices) const;
    void CollectAABBCommand(const FAABB& Box, const FColor& Color, FRenderBus& RenderBus) const;
    void CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus) const;
    void CollectOBBCommand(const FOBB& Box, const FColor& Color, FRenderBus& RenderBus) const;
    void CollectOBBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus) const;

    void CollectDirectionalLightCommand(const UDirectionalLightComponent* Light, const FShowFlags& ShowFlags, FRenderBus& RenderBus) const;
    void CollectPointLightCommand(const UPointLightComponent* PointLight, const FShowFlags& ShowFlags, FRenderBus& RenderBus) const;
    void CollectSpotLightCommand(const USpotlightComponent* Spotlight, const FShowFlags& ShowFlags, FRenderBus& RenderBus) const;
};
