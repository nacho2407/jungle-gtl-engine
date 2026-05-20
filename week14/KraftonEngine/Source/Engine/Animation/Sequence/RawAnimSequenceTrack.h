#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Serialization/Archive.h"

struct FRawFloatCurveKey
{
    float TimeSeconds = 0.0f;
    float Value       = 0.0f;

    int32 Interpolation = 0;
    int32 TangentMode   = 0;

    float ArriveTangent       = 0.0f;
    float LeaveTangent        = 0.0f;
    float ArriveTangentWeight = 0.333333f;
    float LeaveTangentWeight  = 0.333333f;
    bool  bArriveTangentWeighted = false;
    bool  bLeaveTangentWeighted  = false;

    friend FArchive& operator<<(FArchive& Ar, FRawFloatCurveKey& Key)
    {
        Ar << Key.TimeSeconds;
        Ar << Key.Value;
        Ar << Key.Interpolation;
        Ar << Key.TangentMode;
        Ar << Key.ArriveTangent;
        Ar << Key.LeaveTangent;
        Ar << Key.ArriveTangentWeight;
        Ar << Key.LeaveTangentWeight;
        Ar << Key.bArriveTangentWeighted;
        Ar << Key.bLeaveTangentWeighted;
        return Ar;
    }
};

struct FRawFloatCurve
{
    TArray<FRawFloatCurveKey> Keys;

    bool HasAnyKeys() const
    {
        return !Keys.empty();
    }

    friend FArchive& operator<<(FArchive& Ar, FRawFloatCurve& Curve)
    {
        uint32 Count = static_cast<uint32>(Curve.Keys.size());
        Ar << Count;
        if (Ar.IsLoading()) Curve.Keys.resize(Count);
        for (FRawFloatCurveKey& Key : Curve.Keys) { Ar << Key; }
        return Ar;
    }
};

struct FMorphTargetCurve
{
    FString        MorphTargetName;
    FRawFloatCurve Curve;
    float          WeightScale = 1.0f;
    float          WeightBias  = 0.0f;
    bool           bEnabled    = true;

    friend FArchive& operator<<(FArchive& Ar, FMorphTargetCurve& MorphCurve)
    {
        Ar << MorphCurve.MorphTargetName;
        Ar << MorphCurve.Curve;
        Ar << MorphCurve.WeightScale;
        Ar << MorphCurve.WeightBias;
        Ar << MorphCurve.bEnabled;
        return Ar;
    }
};

struct FRawVectorCurve
{
    FRawFloatCurve X;
    FRawFloatCurve Y;
    FRawFloatCurve Z;

    bool HasAnyKeys() const
    {
        return X.HasAnyKeys() || Y.HasAnyKeys() || Z.HasAnyKeys();
    }

    friend FArchive& operator<<(FArchive& Ar, FRawVectorCurve& Curve)
    {
        Ar << Curve.X;
        Ar << Curve.Y;
        Ar << Curve.Z;
        return Ar;
    }
};

struct FSourceTransformCurveLayer
{
    int32   LayerIndex = 0;
    FString LayerName;

    float LayerWeight = 1.0f;
    bool  bMute       = false;
    bool  bSolo       = false;

    int32 BlendMode                = 0;
    int32 RotationAccumulationMode = 0;
    int32 ScaleAccumulationMode    = 0;

    FRawVectorCurve Translation;
    FRawVectorCurve Rotation;
    FRawVectorCurve Scale;

    bool HasAnyKeys() const
    {
        return Translation.HasAnyKeys() || Rotation.HasAnyKeys() || Scale.HasAnyKeys();
    }

    friend FArchive& operator<<(FArchive& Ar, FSourceTransformCurveLayer& Layer)
    {
        Ar << Layer.LayerIndex;
        Ar << Layer.LayerName;

        Ar << Layer.LayerWeight;
        Ar << Layer.bMute;
        Ar << Layer.bSolo;
        Ar << Layer.BlendMode;
        Ar << Layer.RotationAccumulationMode;
        Ar << Layer.ScaleAccumulationMode;
        
        Ar << Layer.Translation;
        Ar << Layer.Rotation;
        Ar << Layer.Scale;
        return Ar;
    }
};

// 한 본(bone)에 대한 키프레임 raw 데이터.
// PosKeys/RotKeys/ScaleKeys 는 시간축에 균등 간격으로 샘플링된 값이라고 가정한다.
// 키 개수가 1 이면 정적 transform, 0 이면 해당 채널은 ref pose 사용.
struct FRawAnimSequenceTrack
{
    TArray<FVector> PosKeys;
    TArray<FQuat>   RotKeys;
    TArray<FVector> ScaleKeys;

    TArray<FSourceTransformCurveLayer> SourceCurveLayers;

    friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrack& Track)
    {
        Ar << Track.PosKeys;
        Ar << Track.RotKeys;
        Ar << Track.ScaleKeys;
        Ar << Track.SourceCurveLayers;
        return Ar;
    }
};
