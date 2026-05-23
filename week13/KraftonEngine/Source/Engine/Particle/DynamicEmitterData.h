#pragma once
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Types/VertexTypes.h"

// ================================================================
// [컴포넌트 팀 구현 필요]
// UParticleSystemComponent 틱에서 파티클 배열을 이 구조체에 채워넣을 것.
// 프록시의 FillStagingBuffer가 이 필드들을 읽어 GPU 인스턴스 버퍼를 만듦.
// ================================================================
struct FParticleDataContainer
{
	const uint8*  ParticleData    = nullptr;  // FBaseParticle 파생 구조체의 raw 배열
	                                          // 접근: ParticleData + Index * ParticleStride
	const uint32* ParticleIndices = nullptr;  // 정렬된 인덱스 배열 (SortMode != None 시 사용)
	                                          // nullptr이면 0,1,2,... 순서로 접근
	int32         AllocationCount = 0;        // 할당된 슬롯 수 (>= ActiveParticleCount)
};
class UParticleModuleRequired;

class UMaterial;
class FMeshBuffer;

enum class EDynamicEmitterType {Sprite, Mesh, Beam, Ribbon};
enum class EParticleSortMode { None, ViewDepth, ViewProjectedDepth };
enum class EParticleBlendMode { AlphaBlend, Additive, Translucent };

struct FDynamicEmitterReplayDataBase
{
	EDynamicEmitterType  eEmitterType;
	int32 ActiveParticleCount = 0;
	int32 ParticleStride = 0;
	FParticleDataContainer DataContainer;
	FVector Scale = FVector(1, 1, 1);
	EParticleSortMode SortMode = EParticleSortMode::None;
};

struct FDynamicSpriteEmitterReplayDataBase : FDynamicEmitterReplayDataBase
{
	UMaterial* Material = nullptr;
	UParticleModuleRequired* RequiredModule = nullptr;
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
	void SortSpriteParticles(EParticleSortMode SortMode,
		bool bSortReversed,
		const FVector& CameraPos,
		FParticleDataContainer& DataContainer,
		int32 ParticleStride);
};

struct FDynamicSpriteEmitterData : FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase Source;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FParticleSpriteInstance); }
};

struct FDynamicMeshEmitterData : FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase Source;
	FMeshBuffer* MeshBuffer = nullptr;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	int32 GetDynamicVertexStride() const override { return sizeof(FMeshParticleInstanceVertex); }
};

