#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

/*
    셰이더와 CPU가 공유하는 상수 버퍼 레이아웃 정의입니다.
    슬롯 번호는 Resources/BindingSlots.h와 맞춰 사용합니다.
*/
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
