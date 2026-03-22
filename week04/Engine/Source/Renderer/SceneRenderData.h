#pragma once

#include "Core/Containers/Array.h"
#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Color.h"
#include "Renderer/Types/BasicMeshType.h"
#include "Renderer/Types/ViewMode.h"

class FSceneView;

struct FPrimitiveRenderItem
{
    FMatrix        World;
    FColor       Color = FColor::White();
    EBasicMeshType MeshType;

    uint32 ObjectId = 0;

    bool bVisible = true;
    bool bPickable = true;
    bool bSelected = false;
    bool bHovered = false;
};

struct FSceneRenderData
{
    const FSceneView* SceneView = nullptr;
    EViewModeIndex    ViewMode = EViewModeIndex::Lit;

    bool bUseInstancing = true;

    TArray<FPrimitiveRenderItem> Primitives;
};