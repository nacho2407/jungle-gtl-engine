#pragma once

#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Renderer/Types/BasicMeshType.h"
#include "Renderer/RenderAsset/FontResource.h"
#include "Core/HAL/PlatformTypes.h"
#include "Core/Misc/BitMaskEnum.h"

enum class ERenderItemFlags : uint8
{
    None = 0,
    Visible = 1 << 0,
    Pickable = 1 << 1,
    Selected = 1 << 2,
    Hovered = 1 << 3,
};

template <> struct TEnableBitMaskOperators<ERenderItemFlags>
{
    static constexpr bool bEnabled = true;
};

struct FRenderItemState
{
    uint32           ObjectId = 0;
    ERenderItemFlags Flags = ERenderItemFlags::Visible | ERenderItemFlags::Pickable;

    bool IsVisible() const { return IsFlagSet(Flags, ERenderItemFlags::Visible); }
    bool IsPickable() const { return IsFlagSet(Flags, ERenderItemFlags::Pickable); }
    bool IsSelected() const { return IsFlagSet(Flags, ERenderItemFlags::Selected); }
    bool IsHovered() const { return IsFlagSet(Flags, ERenderItemFlags::Hovered); }

    void SetVisible(bool bInVisible) { SetFlag(Flags, ERenderItemFlags::Visible, bInVisible); }
    void SetPickable(bool bInPickable) { SetFlag(Flags, ERenderItemFlags::Pickable, bInPickable); }
    void SetSelected(bool bInSelected) { SetFlag(Flags, ERenderItemFlags::Selected, bInSelected); }
    void SetHovered(bool bInHovered) { SetFlag(Flags, ERenderItemFlags::Hovered, bInHovered); }
};

struct FPrimitiveRenderItem
{
    FMatrix          World;
    FColor           Color = FColor::White();
    EBasicMeshType   MeshType = EBasicMeshType::None;
    FRenderItemState State;
};

struct FTextRenderItem
{
    FMatrix              World;
    FColor               Color = FColor::White();
    const FFontResource* FontResource = nullptr;
    FString              Text;

    float TextScale = 1.0f;
    float LetterSpacing = 0.0f;
    float LineSpacing = 0.0f;

    bool    bBillboard = true;
    FVector BillboardOffset = FVector(0.0f, 0.0f, 0.0f);

    FRenderItemState State;
};