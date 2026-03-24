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
};

struct FRenderPlacement
{
    ERenderPlacementMode Mode = ERenderPlacementMode::World;

    FMatrix World = FMatrix::Identity;
    FVector WorldOffset = FVector::ZeroVector;

    bool IsWorldSpace() const { return true; }
    bool IsBillboard() const { return Mode == ERenderPlacementMode::WorldBillboard; }
};

enum class ETextLayoutMode : uint8
{
    Natural = 0, // 글자 크기는 TextScale이 결정
    FitToBox = 1 // transform scale을 박스 크기로 해석
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

    FRenderPlacement Placement;
    FRenderItemState State;
};

struct FTextRenderItem
{
    const FFontResource* FontResource = nullptr;
    FString              Text;
    FColor               Color = FColor::White();

    FRenderPlacement Placement;

    float TextScale = 1.0f;
    float LetterSpacing = 0.0f;
    float LineSpacing = 0.0f;

    ETextLayoutMode LayoutMode = ETextLayoutMode::Natural;

    FRenderItemState State;
};
