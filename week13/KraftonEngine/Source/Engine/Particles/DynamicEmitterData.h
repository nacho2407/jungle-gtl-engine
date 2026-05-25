#pragma once
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Types/VertexTypes.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleModuleRequired.h"

class UMaterial;
class FMeshBuffer;
enum class EDynamicEmitterType {Sprite, Mesh, Beam, Ribbon};
enum class EParticleBlendMode { AlphaBlend, Additive, Translucent };

struct FParticleSortContext
{
	FVector CameraPosition;
	FVector CameraForward;
};

struct FDynamicEmitterReplayDataBase
{
	EDynamicEmitterType  eEmitterType;
	int32 ActiveParticleCount = 0;
	int32 MaxDrawCount = -1;
	int32 ParticleStride = 0;
	FParticleDataContainer DataContainer;
	FVector Scale = FVector(1, 1, 1);
	EParticleSortMode  SortMode  = PSORTMODE_None;
	EParticleBlendMode BlendMode = EParticleBlendMode::AlphaBlend;
};

struct FDynamicSpriteEmitterReplayDataBase : FDynamicEmitterReplayDataBase
{
	UMaterial* Material = nullptr;
	UParticleModuleRequired* RequiredModule = nullptr;

	int32 SubUVDataOffset            = 0;
	int32 DynamicParameterDataOffset = 0;
	int32 LightDataOffset            = 0;
	int32 OrbitModuleOffset          = 0;
	int32 CameraPayloadOffset        = 0;

	bool    bUseLocalSpace = false;
	bool    bLockAxis      = false;
	FVector PivotOffset    = FVector::ZeroVector;
};

struct FDynamicMeshEmitterReplayData : FDynamicSpriteEmitterReplayDataBase
{
	int32 MeshRotationOffset  = 0;
	int32 MeshMotionBlurOffset = 0;
	bool  bEnableMotionBlur   = false;
};

struct FDynamicEmitterDataBase
{
	int32 EmitterIndex = 0;
	virtual ~FDynamicEmitterDataBase() = default;
	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
	virtual int32 GetDynamicVertexStride() const = 0;
};

struct FDynamicSpriteEmitterDataBase : FDynamicEmitterDataBase
{
	void SortSpriteParticles(const FParticleSortContext& SortCtx);
};

struct FDynamicSpriteEmitterData : FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase Source;
	FDynamicSpriteEmitterData() { Source.eEmitterType = EDynamicEmitterType::Sprite; }
	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleSpriteInstance); }
};

struct FDynamicMeshEmitterData : FDynamicSpriteEmitterDataBase
{
	FDynamicMeshEmitterReplayData Source;
	FMeshBuffer* MeshBuffer = nullptr;
	FDynamicMeshEmitterData() { Source.eEmitterType = EDynamicEmitterType::Mesh; }
	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FMeshParticleInstanceVertex); }
};

struct FParticleBeamTrailVertex
{
	FVector Position = FVector::ZeroVector;
	FVector OldPosition = FVector::ZeroVector;
	FVector2 TexCoord = FVector2(0.0f, 0.0f);
	FLinearColor Color = FLinearColor::White();
};

struct FDynamicBeam2EmitterReplayData : FDynamicSpriteEmitterReplayDataBase
{
	int32 VertexCount = 0;
	int32 IndexCount = 0;
	int32 IndexStride = sizeof(uint32);
	TArray<int32> TrianglesPerSheet;
	int32 UpVectorStepSize = 0;

	int32 BeamDataOffset = -1;
	int32 InterpolatedPointsOffset = -1;
	int32 NoiseRateOffset = -1;
	int32 NoiseDeltaTimeOffset = -1;
	int32 TargetNoisePointsOffset = -1;
	int32 NextNoisePointsOffset = -1;
	int32 TaperValuesOffset = -1;
	int32 NoiseDistanceScaleOffset = -1;
	int32 TaperCount = 0;

	bool bLowFreqNoise_Enabled = false;
	bool bHighFreqNoise_Enabled = false;
	bool bSmoothNoise_Enabled = false;
	bool bUseSource = false;
	bool bUseTarget = false;
	bool bTargetNoise = false;
	int32 Sheets = 1;
	int32 Frequency = 1;
	int32 NoiseTessellation = 1;
	float NoiseRangeScale = 1.0f;
	float NoiseTangentStrength = 0.0f;
	FVector NoiseSpeed = FVector::ZeroVector;
	float NoiseLockTime = 0.0f;
	float NoiseLockRadius = 0.0f;
	float NoiseTension = 0.0f;

	int32 TextureTile = 0;
	float TextureTileDistance = 0.0f;
	uint8 TaperMethod = 0;
	int32 InterpolationPoints = 0;

	bool bRenderGeometry = true;
	bool bRenderDirectLine = false;
	bool bRenderLines = false;
	bool bRenderTessellation = false;
};

struct FDynamicBeam2EmitterData : FDynamicSpriteEmitterDataBase
{
	static const uint32 MaxBeams = 2 * 1024;
	static const uint32 MaxInterpolationPoints = 250;
	static const uint32 MaxNoiseFrequency = 250;

	FDynamicBeam2EmitterReplayData Source;
	TArray<FParticleBeamTrailVertex> Vertices;
	TArray<uint32> Indices;

	FDynamicBeam2EmitterData() { Source.eEmitterType = EDynamicEmitterType::Beam; }
	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleBeamTrailVertex); }

	void BuildMeshData();

	// Unreal FDynamicBeam2EmitterData responsibility slots.
	// RHI/async-buffer structures are not present in Jungle yet, so these fill
	// Jungle CPU staging arrays or remain stubs without changing simulation state.
	int32 FillIndexData_Stub();
	int32 FillVertexData_NoNoise_Stub();
	int32 FillData_Noise_Stub();
	int32 FillData_InterpolatedNoise_Stub();
	void DoBufferFill_Stub();
};

struct FDynamicTrailsEmitterReplayData : FDynamicSpriteEmitterReplayDataBase
{
	int32 PrimitiveCount = 0;
	int32 VertexCount = 0;
	int32 IndexCount = 0;
	int32 IndexStride = sizeof(uint32);
	int32 TrailDataOffset = -1;
	int32 MaxActiveParticleCount = 0;
	int32 TrailCount = 1;
	int32 Sheets = 1;
};

struct FDynamicRibbonEmitterReplayData : FDynamicTrailsEmitterReplayData
{
	int32 MaxTessellationBetweenParticles = 0;
};

struct FDynamicRibbonEmitterData : FDynamicSpriteEmitterDataBase
{
	FDynamicRibbonEmitterReplayData Source;
	TArray<FParticleBeamTrailVertex> Vertices;
	TArray<uint32> Indices;
	uint32 RenderAxisOption : 2;

	FDynamicRibbonEmitterData()
		: RenderAxisOption(0)
	{
		Source.eEmitterType = EDynamicEmitterType::Ribbon;
	}

	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleBeamTrailVertex); }

	void BuildMeshData();

	// Unreal FDynamicRibbonEmitterData responsibility slots.
	// These preserve the original renderer function boundaries while the
	// missing beam-trail vertex factory/RHI fill path remains stubbed.
	int32 FillIndexData_Stub();
	int32 FillVertexData_Stub();
	int32 FillInterpolatedVertexData_Stub();
	void DoBufferFill_Stub();
};
