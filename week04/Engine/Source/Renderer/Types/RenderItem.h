#pragma once

#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector.h"
#include "Renderer/Types/BasicMeshType.h"
#include "Renderer/RenderAsset/FontResource.h"
#include "Renderer/RenderAsset/TextureResource.h"
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

    bool bShowBounds = false;

    bool IsVisible() const { return IsFlagSet(Flags, ERenderItemFlags::Visible); }
    bool IsPickable() const { return IsFlagSet(Flags, ERenderItemFlags::Pickable); }
    bool IsSelected() const { return IsFlagSet(Flags, ERenderItemFlags::Selected); }
    bool IsHovered() const { return IsFlagSet(Flags, ERenderItemFlags::Hovered); }

    void SetVisible(bool bInVisible) { SetFlag(Flags, ERenderItemFlags::Visible, bInVisible); }
    void SetPickable(bool bInPickable) { SetFlag(Flags, ERenderItemFlags::Pickable, bInPickable); }
    void SetSelected(bool bInSelected) { SetFlag(Flags, ERenderItemFlags::Selected, bInSelected); }
    void SetHovered(bool bInHovered) { SetFlag(Flags, ERenderItemFlags::Hovered, bInHovered); }
};

enum class ERenderPlacementMode : uint8
{
    World = 0,
    WorldBillboard = 1,
    Screen = 2,
};

struct FRenderPlacement
{
    ERenderPlacementMode Mode = ERenderPlacementMode::World;

    FMatrix  World = FMatrix::Identity;
    FVector2 ScreenPosition = FVector2(0.0f, 0.0f);

    FVector WorldOffset = FVector::ZeroVector;

    bool IsWorldSpace() const
    {
        return Mode == ERenderPlacementMode::World || Mode == ERenderPlacementMode::WorldBillboard;
    }

    bool IsScreenSpace() const { return Mode == ERenderPlacementMode::Screen; }

    bool IsBillboard() const { return Mode == ERenderPlacementMode::WorldBillboard; }
};

struct FPrimitiveRenderItem
{
    FMatrix          World = FMatrix::Identity;
    FColor           Color = FColor::White();
    EBasicMeshType   MeshType = EBasicMeshType::None;
    FRenderItemState State;
};

struct FSpriteRenderItem
{
    const FTextureResource* TextureResource = nullptr;

    FColor   Color = FColor::White();
    FVector2 UVMin = FVector2(0.0f, 0.0f);
    FVector2 UVMax = FVector2(1.0f, 1.0f);

    int32  Priority = 0;
    uint64 SubmissionOrder = 0;

    FRenderPlacement Placement;
    FRenderItemState State;
};

struct FTextRenderItem
{
    const FFontResource* FontResource = nullptr;
    FString              Text;
    FColor               Color = FColor::White();

    int32  Priority = 0;
    uint64 SubmissionOrder = 0;

    FRenderPlacement Placement;

    float TextScale = 1.0f;
    float LetterSpacing = 0.0f;
    float LineSpacing = 0.0f;

    FRenderItemState State;
};