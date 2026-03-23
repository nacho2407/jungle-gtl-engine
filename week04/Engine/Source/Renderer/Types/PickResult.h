#pragma once

#include "Core/HAL/PlatformTypes.h"
#include "Renderer/EditorRenderData.h"
#include "Renderer/Types/AxisColors.h"
#include "Renderer/Types/PickId.h"

enum class EPickKind : uint8
{
    None = 0,
    SceneObject,
    Gizmo,
};

struct FPickResult
{
    EPickKind  Kind = EPickKind::None;
    uint32     ObjectId = PickId::None;
    EGizmoType GizmoType = EGizmoType::None;
    EAxis      Axis = EAxis::X;
};

namespace PickResult
{
    inline FPickResult FromPickId(uint32 InPickId)
    {
        FPickResult Result = {};
        Result.ObjectId = InPickId;

        if (InPickId == PickId::None)
        {
            Result.Kind = EPickKind::None;
            return Result;
        }

        if (!PickId::IsGizmoId(InPickId))
        {
            Result.Kind = EPickKind::SceneObject;
            return Result;
        }

        Result.Kind = EPickKind::Gizmo;
        PickId::DecodeGizmoPart(InPickId, Result.GizmoType, Result.Axis);
        return Result;
    }
} // namespace PickResult