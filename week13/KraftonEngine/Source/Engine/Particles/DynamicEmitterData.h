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
	int32 SubUVInterpMethod = 0;
	int32 SubUVDataOffset = 0;
	int32 SubImages_Horizontal = 0;
	int32 SubImages_Vertical = 0;
	bool bScaleUV = false;
	int32 MeshRotationOffset  = 0;
	int32 MeshMotionBlurOffset = 0;
	uint8 MeshAlignment = 0;
	bool bMeshRotationActive = false;
	FVector LockedAxis = FVector::XAxisVector;
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
	float RelativeTime = 0.0f;
	FVector OldPosition = FVector::ZeroVector;
	float ParticleId = 0.0f;
	FVector2 Size = FVector2(0.0f, 0.0f);
	float Rotation = 0.0f;
	float SubImageIndex = 0.0f;
	FLinearColor Color = FLinearColor::White();
	float Tex_U = 0.0f;
	float Tex_V = 0.0f;
	float Tex_U2 = 0.0f;
	float Tex_V2 = 0.0f;
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
	int32 LastFramePreRendered = -1;

	FDynamicBeam2EmitterData() { Source.eEmitterType = EDynamicEmitterType::Beam; }
	~FDynamicBeam2EmitterData() override;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleBeamTrailVertex); }
	const TArray<FParticleBeamTrailVertex>& GetBuiltVertices() const;
	const TArray<uint32>& GetBuiltIndices() const;

	void BuildMeshData();

	int32 FillIndexData();
	int32 FillVertexData_NoNoise();
	int32 FillData_Noise();
	int32 FillData_InterpolatedNoise();
	void DoBufferFill();
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

struct FDynamicTrailsEmitterData : FDynamicSpriteEmitterDataBase
{
	FDynamicTrailsEmitterReplayData* SourcePointer = nullptr;
	int32 LastFramePreRendered = -1;
	uint32 bClipSourceSegement : 1;
	uint32 bRenderGeometry : 1;
	uint32 bRenderParticles : 1;
	uint32 bRenderTangents : 1;
	uint32 bRenderTessellation : 1;
	uint32 bTextureTileDistance : 1;
	float DistanceTessellationStepSize = 12.5f;
	float TangentTessellationScalar = 25.0f;
	float TextureTileDistance = 0.0f;

	FDynamicTrailsEmitterData()
		: bClipSourceSegement(false)
		, bRenderGeometry(true)
		, bRenderParticles(false)
		, bRenderTangents(false)
		, bRenderTessellation(false)
		, bTextureTileDistance(false)
	{
	}

	const FDynamicEmitterReplayDataBase& GetSource() const override { return *SourcePointer; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleBeamTrailVertex); }
	int32 FillIndexData();
	virtual int32 FillVertexData();
	void DoBufferFill();
};

struct FDynamicRibbonEmitterData : FDynamicTrailsEmitterData
{
	FDynamicRibbonEmitterReplayData Source;
	uint32 RenderAxisOption : 2;

	FDynamicRibbonEmitterData()
		: RenderAxisOption(0)
	{
		Source.eEmitterType = EDynamicEmitterType::Ribbon;
		SourcePointer = &Source;
	}

	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleBeamTrailVertex); }
	~FDynamicRibbonEmitterData() override;
	const TArray<FParticleBeamTrailVertex>& GetBuiltVertices() const;
	const TArray<uint32>& GetBuiltIndices() const;

	void BuildMeshData();
	int32 FillVertexData() override;
	int32 FillInterpolatedVertexData();
};
