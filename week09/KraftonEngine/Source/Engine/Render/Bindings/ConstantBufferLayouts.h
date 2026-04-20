#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

class FConstantBuffer;

struct FPerObjectConstants
{
    FMatrix Model;
    FMatrix NormalMatrix;
    FVector4 Color;

    static FPerObjectConstants FromWorldMatrix(const FMatrix& WorldMatrix)
    {
        FPerObjectConstants Constants;
        Constants.Model = WorldMatrix;
        Constants.NormalMatrix = WorldMatrix.GetInverse().GetTransposed();
        Constants.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        return Constants;
    }
};

struct FFrameConstants
{
    FMatrix View;
    FMatrix Projection;
    FMatrix InvViewProj;
    float bIsWireframe;
    FVector WireframeColor;
    float Time;
    FVector CameraWorldPos;
};

struct FSubUVRegionConstants
{
    float U = 0.0f;
    float V = 0.0f;
    float Width = 1.0f;
    float Height = 1.0f;
};

struct FGizmoConstants
{
    FVector4 ColorTint;
    uint32 bIsInnerGizmo;
    uint32 bClicking;
    uint32 SelectedAxis;
    float HoveredAxisOpacity;
    uint32 AxisMask;
    uint32 _pad[3];
};

struct FOutlinePostProcessConstants
{
    FVector4 OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
    float OutlineThickness = 1.0f;
    float Padding[3] = {};
};

struct FSceneDepthPConstants
{
    float Exponent;
    float NearClip;
    float FarClip;
    uint32 Mode;
};

struct FFogConstants
{
    FVector4 InscatteringColor;
    float Density;
    float HeightFalloff;
    float FogBaseHeight;
    float StartDistance;
    float CutoffDistance;
    float MaxOpacity;
    float _pad[2];
};

struct FFXAAConstants
{
    float EdgeThreshold;
    float EdgeThresholdMin;
    float _pad[2];
};

struct FConstantBufferBinding
{
    FConstantBuffer* Buffer = nullptr;
    uint32 Size = 0;
    uint32 Slot = 0;

    static constexpr size_t kMaxDataSize = 128;
    alignas(16) uint8 Data[kMaxDataSize] = {};

    template<typename T>
    T& Bind(FConstantBuffer* InBuffer, uint32 InSlot)
    {
        static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
        Buffer = InBuffer;
        Size = sizeof(T);
        Slot = InSlot;
        return *reinterpret_cast<T*>(Data);
    }

    template<typename T>
    T& As()
    {
        static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
        return *reinterpret_cast<T*>(Data);
    }

    template<typename T>
    const T& As() const
    {
        static_assert(sizeof(T) <= kMaxDataSize, "CB data exceeds inline buffer size");
        return *reinterpret_cast<const T*>(Data);
    }
};
