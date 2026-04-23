#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

struct FPerObjectCBData
{
    FMatrix  Model;
    FMatrix  NormalMatrix;
    FVector4 Color;

    static FPerObjectCBData FromWorldMatrix(const FMatrix& WorldMatrix)
    {
        FPerObjectCBData CBData;
        CBData.Model        = WorldMatrix;
        CBData.NormalMatrix = WorldMatrix.GetInverse().GetTransposed();
        CBData.Color        = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        return CBData;
    }
};

struct FFrameCBData
{
    FMatrix View;
    FMatrix Projection;
    FMatrix InvViewProj;
    float   bIsWireframe;
    FVector WireframeColor;
    float   Time;
    FVector CameraWorldPos;
};

struct FSubUVRegionCBData
{
    float U      = 0.0f;
    float V      = 0.0f;
    float Width  = 1.0f;
    float Height = 1.0f;
};

struct FGizmoCBData
{
    FVector4 ColorTint;
    uint32   bIsInnerGizmo;
    uint32   bClicking;
    uint32   SelectedAxis;
    float    HoveredAxisOpacity;
    uint32   AxisMask;
    uint32   _pad[3];
};

struct FOutlinePostProcessCBData
{
    FVector4 OutlineColor     = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
    float    OutlineThickness = 1.0f;
    float    Padding[3]       = {};
};

struct FSceneDepthPCBData
{
    float   Exponent;
    float   NearClip;
    float   FarClip;
    float   Range;
    uint32  Mode;
    FVector _Padding;
};

struct FFogCBData
{
    FVector4 InscatteringColor;
    float    Density;
    float    HeightFalloff;
    float    FogBaseHeight;
    float    StartDistance;
    float    CutoffDistance;
    float    MaxOpacity;
    float    _pad[2];
};

struct FFXAACBData
{
    float EdgeThreshold;
    float EdgeThresholdMin;
    float _pad[2];
};
