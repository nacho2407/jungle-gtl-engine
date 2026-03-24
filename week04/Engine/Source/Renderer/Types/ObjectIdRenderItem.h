#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Core/Math/Matrix.h"
#include "Renderer/Types/BasicMeshType.h"

struct FObjectIdRenderItem
{
    FMatrix        World = FMatrix::Identity;
    EBasicMeshType MeshType = EBasicMeshType::Cube;
    uint32         ObjectId = 0;
};
