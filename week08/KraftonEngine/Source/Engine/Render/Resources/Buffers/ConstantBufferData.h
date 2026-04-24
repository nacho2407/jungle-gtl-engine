// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

// FPerObjectCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
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

// FFrameCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
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

// FSubUVRegionCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FSubUVRegionCBData
{
    float U      = 0.0f;
    float V      = 0.0f;
    float Width  = 1.0f;
    float Height = 1.0f;
};

// FGizmoCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
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

// FOutlinePostProcessCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FOutlinePostProcessCBData
{
    FVector4 OutlineColor     = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
    float    OutlineThickness = 1.0f;
    float    Padding[3]       = {};
};

// FSceneDepthPCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FSceneDepthPCBData
{
    float   Exponent;
    float   NearClip;
    float   FarClip;
    float   Range;
    uint32  Mode;
    FVector _Padding;
};

// FFogCBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
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

// FFXAACBData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FFXAACBData
{
    float EdgeThreshold;
    float EdgeThresholdMin;
    float _pad[2];
};
