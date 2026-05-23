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

	// 파티클 데이터 스트라이드 내 페이로드 오프셋
	int32 SubUVDataOffset            = 0;
	int32 DynamicParameterDataOffset = 0;
	int32 LightDataOffset            = 0;
	int32 OrbitModuleOffset          = 0;
	int32 CameraPayloadOffset        = 0;

	// 스프라이트 렌더링 옵션
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
	// ParticleIndices를 카메라 거리 기준으로 정렬
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

