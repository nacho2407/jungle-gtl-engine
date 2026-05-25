#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Vector.h"
#include "Particles/ParticleHelper.h"

class UMaterial;

enum EDynamicEmitterType
{
    DET_Unknown = 0,
    DET_Sprite,
    DET_Mesh
};

struct FDynamicEmitterReplayDataBase
{
    EDynamicEmitterType EmitterType = DET_Unknown;

    int32 ActiveParticleCount = 0;
    int32 ParticleStride = 0;
    int32 SortMode = 0;

    FParticleDataContainer DataContainer;

    FVector Scale = FVector::OneVector;

    virtual ~FDynamicEmitterReplayDataBase() = default;

    virtual void Reset()
    {
        EmitterType = DET_Unknown;
        ActiveParticleCount = 0;
        ParticleStride = 0;
        SortMode = 0;
        Scale = FVector::OneVector;
        DataContainer.Free();
    }
};

struct FDynamicSpriteEmitterReplayData : public FDynamicEmitterReplayDataBase
{
    int32 SubUVDataOffset = 0;
    int32 DynamicParameterDataOffset = 0;
    int32 LightDataOffset = 0;
    int32 OrbitModuleOffset = 0;
    int32 CameraPayloadOffset = 0;

    bool bUseLocalSpace = false;
    bool bLockAxis = false;

    FVector PivotOffset = FVector::ZeroVector;

    UMaterial* Material = nullptr;

    FDynamicSpriteEmitterReplayData()
    {
        EmitterType = DET_Sprite;
    }
};

struct FDynamicMeshEmitterReplayData : public FDynamicEmitterReplayDataBase
{
    int32 MeshRotationOffset = 0;
    int32 MeshMotionBlurOffset = 0;
    int32 DynamicParameterDataOffset = 0;
    int32 LightDataOffset = 0;
    int32 OrbitModuleOffset = 0;
    int32 CameraPayloadOffset = 0;

    bool bUseLocalSpace = false;

    UMaterial* Material = nullptr;

    FDynamicMeshEmitterReplayData()
    {
        EmitterType = DET_Mesh;
    }
};

struct FDynamicEmitterDataBase
{
    bool bValid = false;
    int32 EmitterIndex = INDEX_NONE;

    virtual ~FDynamicEmitterDataBase() = default;

    virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
};

struct FDynamicSpriteEmitterData : public FDynamicEmitterDataBase
{
    FDynamicSpriteEmitterReplayData Source;

    const FDynamicEmitterReplayDataBase& GetSource() const override
    {
        return Source;
    }
};

struct FDynamicMeshEmitterData : public FDynamicEmitterDataBase
{
    FDynamicMeshEmitterReplayData Source;

    const FDynamicEmitterReplayDataBase& GetSource() const override
    {
        return Source;
    }
};
