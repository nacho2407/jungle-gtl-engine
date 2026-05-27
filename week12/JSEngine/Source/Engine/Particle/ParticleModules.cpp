#include "Particle/ParticleModules.h"

#include "Asset/CurveColorAsset.h"
#include "Asset/CurveFloatAsset.h"
#include "Asset/CurveVectorAsset.h"
#include "Component/PrimitiveComponent.h"
#include "Core/ResourceManager.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleEmitterInstanceOwner.h"
#include "Particle/ParticleHelper.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <variant>

namespace
{
	struct FParticleDistributionPayload
	{
		float RandomAlpha = 0.0f;
	};

	bool IsLiveObject(const UObject* Object)
	{
		return Object != nullptr && UObjectManager::Get().ContainsObject(Object);
	}

	bool IsSpriteTypeDataModule(const UParticleModuleTypeDataBase* TypeData)
	{
		return TypeData != nullptr && TypeData->GetClass() == UParticleModuleTypeDataBase::StaticClass();
	}

	int32 AlignParticleBytes(int32 Value)
	{
		return ParticleHelper::AlignParticleSize(Value);
	}

	/**
	 * @brief LOD level의 명시적 SpawnModule 포인터를 반환합니다.
	 */
	UParticleModuleSpawn* ResolveSpawnModule(const UParticleLODLevel* LODLevel)
	{
		if (LODLevel == nullptr)
		{
			return nullptr;
		}

		// Version 2 에셋 계약의 명시적 SpawnModule만 사용
		return LODLevel->SpawnModule;
	}

	bool AreModuleClassesCompatible(const UParticleModule* LayoutModule, const UParticleModule* LODModule)
	{
		if (LayoutModule == nullptr || LODModule == nullptr)
		{
			// null slot 대칭성
			return LayoutModule == LODModule;
		}

		return LayoutModule->GetClass() == LODModule->GetClass();
	}

	void LogLODWarning(bool bLogWarnings, const char* Message)
	{
		if (bLogWarnings)
		{
			UE_LOG_WARNING("[Particle] %s", Message);
		}
	}

	void AddParticlePayloadOffset(
		FParticleLODLevelRuntimeCache& Cache,
		UParticleModule* Module,
		UParticleModuleTypeDataBase* TypeData,
		int32& InOutParticleBytes)
	{
		if (Module == nullptr)
		{
			return;
		}

		const int32 RequiredBytes = Module->RequiredBytes(TypeData);
		if (RequiredBytes <= 0)
		{
			return;
		}

		InOutParticleBytes = AlignParticleBytes(InOutParticleBytes);
		Cache.ModulePayloadOffsets[Module] = InOutParticleBytes;
		InOutParticleBytes += RequiredBytes;
	}

	void AddTypeDataParticlePayloadOffset(
		FParticleLODLevelRuntimeCache& Cache,
		UParticleModuleTypeDataBase* TypeData,
		int32& InOutParticleBytes)
	{
		if (TypeData == nullptr)
		{
			return;
		}

		const int32 RequiredBytes = TypeData->GetRequiredPayloadSize();
		if (RequiredBytes <= 0)
		{
			return;
		}

		// TypeData payload 시작 offset
		InOutParticleBytes = AlignParticleBytes(InOutParticleBytes);
		Cache.ModulePayloadOffsets[TypeData] = InOutParticleBytes;
		InOutParticleBytes += RequiredBytes;
	}

	void AddInstancePayloadOffset(
		FParticleLODLevelRuntimeCache& Cache,
		UParticleModule* Module,
		UParticleModuleTypeDataBase* TypeData,
		int32& InOutInstancePayloadSize)
	{
		if (Module == nullptr)
		{
			return;
		}

		const int32 RequiredBytes = Module->RequiredBytesPerInstance(TypeData);
		if (RequiredBytes <= 0)
		{
			return;
		}

		InOutInstancePayloadSize = AlignParticleBytes(InOutInstancePayloadSize);
		Cache.ModuleInstanceOffsets[Module] = InOutInstancePayloadSize;
		InOutInstancePayloadSize += RequiredBytes;
	}

	void CopyStablePayloadOffsets(
		FParticleLODLevelRuntimeCache& Cache,
		UParticleModule* LODModule,
		UParticleModule* LayoutModule,
		const FParticleLODLevelRuntimeCache& StableLayoutCache)
	{
		if (LODModule == nullptr || LayoutModule == nullptr)
		{
			return;
		}

		// particle payload offset 공유
		const int32 ParticlePayloadOffset = StableLayoutCache.GetParticlePayloadOffset(LayoutModule);
		if (ParticlePayloadOffset >= 0)
		{
			Cache.ModulePayloadOffsets[LODModule] = ParticlePayloadOffset;
		}

		// instance payload offset 공유
		const int32 InstancePayloadOffset = StableLayoutCache.GetInstancePayloadOffset(LayoutModule);
		if (InstancePayloadOffset >= 0)
		{
			Cache.ModuleInstanceOffsets[LODModule] = InstancePayloadOffset;
		}
	}

	void AddEnabledModuleExecution(FParticleLODLevelRuntimeCache& Cache, UParticleModule* Module)
	{
		if (Module == nullptr || !Module->bEnabled)
		{
			return;
		}

		if (UParticleModuleCollision* CollisionModule = Cast<UParticleModuleCollision>(Module))
		{
			// Collision은 적분 뒤 OldLocation에서 Location까지의 이동 구간을 검사
			Cache.CollisionModules.push_back(CollisionModule);
			return;
		}

		if (UParticleModuleEventGenerator* EventGeneratorModule = Cast<UParticleModuleEventGenerator>(Module))
		{
			if (Cache.EventGeneratorModule == nullptr)
			{
				Cache.EventGeneratorModule = EventGeneratorModule;
				EventGeneratorModule->ValidateConfiguredEvents();
			}
			else
			{
				UE_LOG_WARNING("[Particle] Multiple enabled Event Generator modules found. Using the first module.");
			}
			return;
		}

		if (UParticleModuleEventReceiverSpawn* EventReceiverModule = Cast<UParticleModuleEventReceiverSpawn>(Module))
		{
			Cache.EventReceiverSpawnModules.push_back(EventReceiverModule);
			return;
		}

		if (Module->IsSpawnModule())
		{
			Cache.SpawnModules.push_back(Module);
		}

		if (Module->IsUpdateModule())
		{
			Cache.UpdateModules.push_back(Module);
		}
	}

	FParticleLODLevelRuntimeCache BuildStableLOD0RuntimeCache(const UParticleLODLevel* LODLevel)
	{
		FParticleLODLevelRuntimeCache Cache;
		Cache.PayloadOffset = AlignParticleBytes(static_cast<int32>(sizeof(FBaseParticle)));

		int32 ParticleBytes = Cache.PayloadOffset;
		int32 InstancePayloadSize = 0;

		if (LODLevel == nullptr)
		{
			Cache.ParticleStride = AlignParticleBytes(ParticleBytes);
			Cache.InstancePayloadSize = AlignParticleBytes(InstancePayloadSize);
			return Cache;
		}

		Cache.RequiredModule = LODLevel->RequiredModule;
		Cache.SpawnModule = ResolveSpawnModule(LODLevel);
		Cache.TypeDataModule = LODLevel->TypeDataModule;

		UParticleModuleTypeDataBase* TypeData = Cache.TypeDataModule;
		AddTypeDataParticlePayloadOffset(Cache, TypeData, ParticleBytes);
		AddInstancePayloadOffset(Cache, TypeData, TypeData, InstancePayloadSize);

		// LOD 0 특수 module 고정 layout
		if (Cache.RequiredModule != nullptr)
		{
			Cache.RequiredModule->bEnabled = true;
		}
		AddParticlePayloadOffset(Cache, Cache.RequiredModule, TypeData, ParticleBytes);
		AddInstancePayloadOffset(Cache, Cache.RequiredModule, TypeData, InstancePayloadSize);
		AddParticlePayloadOffset(Cache, Cache.SpawnModule, TypeData, ParticleBytes);
		AddInstancePayloadOffset(Cache, Cache.SpawnModule, TypeData, InstancePayloadSize);

		for (UParticleModule* Module : LODLevel->Modules)
		{
			if (Module == nullptr)
			{
				continue;
			}

			if (Cast<UParticleModuleSubUV>(Module) != nullptr && !IsSpriteTypeDataModule(TypeData))
			{
				Module->bEnabled = false;
			}

			if (Module == Cache.RequiredModule || Module == Cache.SpawnModule || Module == Cache.TypeDataModule)
			{
				continue;
			}

			// LOD 0 module slot 고정 layout
			AddParticlePayloadOffset(Cache, Module, TypeData, ParticleBytes);
			AddInstancePayloadOffset(Cache, Module, TypeData, InstancePayloadSize);
		}

		Cache.ParticleStride = AlignParticleBytes(ParticleBytes);
		Cache.InstancePayloadSize = AlignParticleBytes(InstancePayloadSize);
		return Cache;
	}

	FParticleLODLevelRuntimeCache BuildLODLevelRuntimeCacheFromStableLayout(
		const UParticleLODLevel* LODLevel,
		const UParticleLODLevel* LayoutLODLevel,
		const FParticleLODLevelRuntimeCache& StableLayoutCache)
	{
		FParticleLODLevelRuntimeCache Cache;
		Cache.ParticleStride = StableLayoutCache.ParticleStride;
		Cache.PayloadOffset = StableLayoutCache.PayloadOffset;
		Cache.InstancePayloadSize = StableLayoutCache.InstancePayloadSize;

		if (LODLevel == nullptr || LayoutLODLevel == nullptr)
		{
			return Cache;
		}

		Cache.RequiredModule = LODLevel->RequiredModule;
		Cache.SpawnModule = ResolveSpawnModule(LODLevel);
		Cache.TypeDataModule = LODLevel->TypeDataModule;

		// 특수 module stable offset
		CopyStablePayloadOffsets(Cache, Cache.RequiredModule, StableLayoutCache.RequiredModule, StableLayoutCache);
		CopyStablePayloadOffsets(Cache, Cache.SpawnModule, StableLayoutCache.SpawnModule, StableLayoutCache);
		CopyStablePayloadOffsets(Cache, Cache.TypeDataModule, StableLayoutCache.TypeDataModule, StableLayoutCache);

		const int32 SharedModuleCount = std::min(
			static_cast<int32>(LODLevel->Modules.size()),
			static_cast<int32>(LayoutLODLevel->Modules.size()));
		for (int32 ModuleIndex = 0; ModuleIndex < SharedModuleCount; ++ModuleIndex)
		{
			UParticleModule* Module = LODLevel->Modules[static_cast<size_t>(ModuleIndex)];
			UParticleModule* LayoutModule = LayoutLODLevel->Modules[static_cast<size_t>(ModuleIndex)];
			CopyStablePayloadOffsets(Cache, Module, LayoutModule, StableLayoutCache);

			// 특수 module 실행 목록 제외
			if (Module == nullptr ||
				Module == Cache.RequiredModule ||
				Module == Cache.SpawnModule ||
				Module == Cache.TypeDataModule)
			{
				continue;
			}

			// 현재 LOD 실행 목록
			AddEnabledModuleExecution(Cache, Module);
		}

		return Cache;
	}

	FParticleDistributionPayload* GetDistributionPayload(
		FParticleEmitterInstance* Owner,
		int32 Offset,
		FBaseParticle& Particle)
	{
		uint8* Payload = Owner != nullptr ? Owner->GetParticlePayloadByOffset(Particle, Offset) : nullptr;
		return Payload != nullptr ? reinterpret_cast<FParticleDistributionPayload*>(Payload) : nullptr;
	}

	void InitializeDistributionPayload(
		FParticleEmitterInstance* Owner,
		int32 Offset,
		FBaseParticle& Particle,
		bool bUseRandomAlpha)
	{
		FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
		if (Payload == nullptr)
		{
			return;
		}

		Payload->RandomAlpha = bUseRandomAlpha && Owner != nullptr ? Owner->RandomStream.GetFraction() : 0.0f;
	}

	FParticleDistributionContext MakeSpawnDistributionContext(
		FParticleEmitterInstance* Owner,
		float SpawnTime,
		const FBaseParticle& Particle,
		const FParticleDistributionPayload* Payload)
	{
		FParticleDistributionContext Context;
		Context.RandomStream = Owner != nullptr ? &Owner->RandomStream : nullptr;
		Context.RelativeTime = Particle.RelativeTime;
		Context.SpawnTime = SpawnTime;
		Context.CurveTime = SpawnTime;
		Context.EmitterTime = SpawnTime;
		Context.RandomAlpha = Payload != nullptr ? &Payload->RandomAlpha : nullptr;
		return Context;
	}

	FParticleDistributionContext MakeUpdateDistributionContext(
		FParticleEmitterInstance* Owner,
		const FBaseParticle& Particle,
		const FParticleDistributionPayload* Payload)
	{
		FParticleDistributionContext Context;
		Context.RandomStream = Owner != nullptr ? &Owner->RandomStream : nullptr;
		Context.RelativeTime = Particle.RelativeTime;
		Context.SpawnTime = 0.0f;
		Context.CurveTime = Particle.RelativeTime;
		Context.EmitterTime = Owner != nullptr ? Owner->EmitterTime : 0.0f;
		Context.RandomAlpha = Payload != nullptr ? &Payload->RandomAlpha : nullptr;
		return Context;
	}

	// TODO: Owned Particle Data 부모 클래스로 빼기
	uint8* GetAlignedSnapshotParticleData(FDynamicSpriteEmitterData& RenderData)
	{
		return RenderData.OwnedParticleData.empty()
			? nullptr
			: ParticleHelper::AlignParticlePointer(RenderData.OwnedParticleData.data());
	}

	uint8* GetAlignedSnapshotParticleData(FDynamicMeshEmitterData& RenderData)
	{
		return RenderData.OwnedParticleData.empty()
			? nullptr
			: ParticleHelper::AlignParticlePointer(RenderData.OwnedParticleData.data());
	}

	/**
	 * @brief Beam render snapshot의 aligned particle data 시작 주소를 반환합니다.
	 */
	uint8* GetAlignedSnapshotParticleData(FDynamicBeamEmitterData& RenderData)
	{
		return RenderData.OwnedParticleData.empty()
			? nullptr
			: ParticleHelper::AlignParticlePointer(RenderData.OwnedParticleData.data());
	}

	FVector GetParticleOldLocationForRender(const FParticleEmitterInstance& EmitterInstance, const FBaseParticle& Particle)
	{
		return EmitterInstance.UsesLocalSpace()
			? EmitterInstance.GetOwner().GetComponentToWorld().TransformPosition(Particle.OldLocation)
			: Particle.OldLocation;
	}

	/**
	 * @brief render snapshot으로 넘길 수 있는 live particle 포인터를 active index에서 조회합니다.
	 */
	const FBaseParticle* ResolveLiveParticleForRender(
		const FParticleEmitterInstance& EmitterInstance,
		int32 ActiveIndex,
		int32& OutPhysicalIndex)
	{
		OutPhysicalIndex = -1;

		// active index 범위
		if (ActiveIndex < 0 || ActiveIndex >= EmitterInstance.ActiveParticles)
		{
			return nullptr;
		}

		// particle storage 유효성
		if (EmitterInstance.ParticleData == nullptr ||
			EmitterInstance.ParticleIndices == nullptr ||
			EmitterInstance.ParticleStride <= 0 ||
			EmitterInstance.MaxActiveParticles <= 0)
		{
			return nullptr;
		}

		// physical index 범위
		const int32 PhysicalIndex = static_cast<int32>(EmitterInstance.ParticleIndices[ActiveIndex]);
		if (PhysicalIndex < 0 || PhysicalIndex >= EmitterInstance.MaxActiveParticles)
		{
			return nullptr;
		}

		// source particle memory 범위
		const size_t ParticleOffset = static_cast<size_t>(PhysicalIndex) * static_cast<size_t>(EmitterInstance.ParticleStride);
		if (ParticleOffset + sizeof(FBaseParticle) > static_cast<size_t>(EmitterInstance.DataContainer.ParticleDataNumBytes))
		{
			return nullptr;
		}

		const FBaseParticle* Particle = reinterpret_cast<const FBaseParticle*>(EmitterInstance.ParticleData + ParticleOffset);
		if (EmitterInstance.IsParticlePendingKill(*Particle))
		{
			return nullptr;
		}

		OutPhysicalIndex = PhysicalIndex;
		return Particle;
	}

	/**
	 * @brief render snapshot에 포함할 live particle 수를 계산합니다.
	 */
	int32 CountLiveParticlesForRender(const FParticleEmitterInstance& EmitterInstance)
	{
		int32 LiveParticleCount = 0;
		for (int32 ActiveIndex = 0; ActiveIndex < EmitterInstance.ActiveParticles; ++ActiveIndex)
		{
			int32 PhysicalIndex = -1;
			if (ResolveLiveParticleForRender(EmitterInstance, ActiveIndex, PhysicalIndex) != nullptr)
			{
				++LiveParticleCount;
			}
		}
		return LiveParticleCount;
	}

	/**
	 * @brief live particle count와 stride에서 render snapshot byte 크기를 계산합니다.
	 */
	bool CalculateRenderSnapshotByteSizes(
		int32 LiveParticleCount,
		int32 ParticleStride,
		size_t& OutParticleDataBytes,
		size_t& OutSnapshotLogicalBytes)
	{
		OutParticleDataBytes = 0;
		OutSnapshotLogicalBytes = 0;

		// 빈 snapshot 또는 잘못된 stride
		if (LiveParticleCount <= 0 || ParticleStride <= 0)
		{
			return false;
		}

		// snapshot buffer 크기
		OutParticleDataBytes = static_cast<size_t>(LiveParticleCount) * static_cast<size_t>(ParticleStride);
		const size_t ParticleIndexBytes = static_cast<size_t>(LiveParticleCount) * sizeof(uint16);
		OutSnapshotLogicalBytes = OutParticleDataBytes + ParticleIndexBytes;

		// DataContainer int32 계약 보호
		return OutParticleDataBytes <= static_cast<size_t>(std::numeric_limits<int32>::max()) &&
			   OutSnapshotLogicalBytes <= static_cast<size_t>(std::numeric_limits<int32>::max());
	}

	/**
	 * @brief live particle만 render snapshot buffer에 compact된 순서로 복사합니다.
	 */
	int32 CopyLiveParticlesForRenderSnapshot(
		const FParticleEmitterInstance& EmitterInstance,
		uint8* SnapshotParticleData,
		TArray<uint16>& SnapshotParticleIndices,
		bool bBakeWorldSpaceLocation)
	{
		if (SnapshotParticleData == nullptr || SnapshotParticleIndices.empty() || EmitterInstance.ParticleStride <= 0)
		{
			return 0;
		}

		int32 SnapshotIndex = 0;
		for (int32 ActiveIndex = 0; ActiveIndex < EmitterInstance.ActiveParticles; ++ActiveIndex)
		{
			if (SnapshotIndex >= static_cast<int32>(SnapshotParticleIndices.size()))
			{
				break;
			}

			// live source particle 조회
			int32 SourcePhysicalIndex = -1;
			const FBaseParticle* SourceParticle = ResolveLiveParticleForRender(EmitterInstance, ActiveIndex, SourcePhysicalIndex);
			if (SourceParticle == nullptr)
			{
				continue;
			}

			// source / destination stride 위치
			const uint8* SourceParticleData =
				EmitterInstance.ParticleData + static_cast<size_t>(SourcePhysicalIndex) * static_cast<size_t>(EmitterInstance.ParticleStride);
			uint8* DestinationParticleData =
				SnapshotParticleData + static_cast<size_t>(SnapshotIndex) * static_cast<size_t>(EmitterInstance.ParticleStride);

			// payload 포함 particle stride 전체 복사
			std::memcpy(DestinationParticleData, SourceParticleData, static_cast<size_t>(EmitterInstance.ParticleStride));

			// sprite snapshot world-space 위치 굽기
			if (bBakeWorldSpaceLocation)
			{
				FBaseParticle& SnapshotParticle = *reinterpret_cast<FBaseParticle*>(DestinationParticleData);
				SnapshotParticle.Location = EmitterInstance.GetParticleLocationForRender(*SourceParticle);
				SnapshotParticle.OldLocation = GetParticleOldLocationForRender(EmitterInstance, *SourceParticle);
			}

			// snapshot-local physical index
			SnapshotParticleIndices[static_cast<size_t>(SnapshotIndex)] = static_cast<uint16>(SnapshotIndex);
			++SnapshotIndex;
		}

		return SnapshotIndex;
	}
} // namespace

namespace
{
	float GetDistributionEvalTime(const FParticleDistributionContext& Context)
	{
		return Context.CurveTime;
	}

	float GetDistributionRandomAlpha(const FParticleDistributionContext& Context)
	{
		if (Context.RandomAlpha != nullptr)
		{
			return std::clamp(*Context.RandomAlpha, 0.0f, 1.0f);
		}

		return Context.RandomStream != nullptr ? Context.RandomStream->GetFraction() : 0.0f;
	}

	bool IsUniformXYZ(const FParticleVectorDistribution& Distribution)
	{
		return Distribution.VectorMode == EParticleVectorDistributionMode::UniformXYZ;
	}

	FVector MakeUniformVector(float Value)
	{
		return FVector(Value, Value, Value);
	}

	UCurveFloatAsset* ResolveFloatCurve(const TSoftObjectPtr<UCurveFloatAsset>& Curve)
	{
		const FString& Path = Curve.GetPath();
		return Path.empty() ? nullptr : FResourceManager::Get().LoadFloatCurve(Path);
	}

	UCurveVectorAsset* ResolveVectorCurve(const TSoftObjectPtr<UCurveVectorAsset>& Curve)
	{
		const FString& Path = Curve.GetPath();
		return Path.empty() ? nullptr : FResourceManager::Get().LoadVectorCurve(Path);
	}

	UCurveColorAsset* ResolveColorCurve(const TSoftObjectPtr<UCurveColorAsset>& Curve)
	{
		const FString& Path = Curve.GetPath();
		return Path.empty() ? nullptr : FResourceManager::Get().LoadColorCurve(Path);
	}

	float EvaluateFloatCurveOrFallback(const TSoftObjectPtr<UCurveFloatAsset>& Curve, float Time, float Fallback)
	{
		UCurveFloatAsset* CurveAsset = ResolveFloatCurve(Curve);
		return CurveAsset ? CurveAsset->Evaluate(Time) : Fallback;
	}

	FVector EvaluateVectorCurveOrFallback(const TSoftObjectPtr<UCurveVectorAsset>& Curve, float Time, const FVector& Fallback)
	{
		UCurveVectorAsset* CurveAsset = ResolveVectorCurve(Curve);
		return CurveAsset ? CurveAsset->Evaluate(Time) : Fallback;
	}

	FColor EvaluateColorCurveOrFallback(const TSoftObjectPtr<UCurveColorAsset>& Curve, float Time, const FColor& Fallback)
	{
		UCurveColorAsset* CurveAsset = ResolveColorCurve(Curve);
		return CurveAsset ? CurveAsset->Evaluate(Time) : Fallback;
	}

	UTexture* ResolveDiffuseTexture(const UMaterialInterface* Material)
	{
		if (Material == nullptr)
		{
			return nullptr;
		}

		FMaterialParamValue DiffuseMap;
		if (!Material->GetParam("DiffuseMap", DiffuseMap) ||
			DiffuseMap.Type != EMaterialParamType::Texture ||
			!std::holds_alternative<UTexture*>(DiffuseMap.Value))
		{
			return nullptr;
		}

		return std::get<UTexture*>(DiffuseMap.Value);
	}

	bool ShouldUseRelativeTimeForSubImageIndex(const FParticleFloatDistribution& Distribution)
	{
		return Distribution.Mode == EParticleDistributionMode::Constant &&
			Distribution.Constant == 0.0f &&
			Distribution.Min == 0.0f &&
			Distribution.Max == 0.0f &&
			Distribution.Curve.GetPath().empty() &&
			Distribution.MinCurve.GetPath().empty() &&
			Distribution.MaxCurve.GetPath().empty();
	}

	float EvaluateSubImageFrameIndex(
		const UParticleModuleSubUV& Module,
		const FParticleDistributionContext& Context,
		int32 TotalFrames)
	{
		if (ShouldUseRelativeTimeForSubImageIndex(Module.SubImageIndex))
		{
			return Context.RelativeTime * static_cast<float>(std::max(TotalFrames - 1, 0));
		}

		return EvaluateParticleFloat(Module.SubImageIndex, Context);
	}
}

float EvaluateParticleFloat(const FParticleFloatDistribution& Distribution, const FParticleDistributionContext& Context)
{
	const float Time = GetDistributionEvalTime(Context);
	switch (Distribution.Mode)
	{
	case EParticleDistributionMode::RandomRange:
		return Context.RandomStream
			? Context.RandomStream->GetRange(Distribution.Min, Distribution.Max)
			: Distribution.Min;
	case EParticleDistributionMode::Curve:
		return EvaluateFloatCurveOrFallback(Distribution.Curve, Time, Distribution.Constant);
	case EParticleDistributionMode::RandomRangeCurve:
	{
		const float MinValue = EvaluateFloatCurveOrFallback(Distribution.MinCurve, Time, Distribution.Min);
		const float MaxValue = EvaluateFloatCurveOrFallback(Distribution.MaxCurve, Time, Distribution.Max);
		const float Alpha = GetDistributionRandomAlpha(Context);
		return MinValue + (MaxValue - MinValue) * Alpha;
	}
	case EParticleDistributionMode::Constant:
	default:
		return Distribution.Constant;
	}
}

FVector EvaluateParticleVector(const FParticleVectorDistribution& Distribution, const FParticleDistributionContext& Context)
{
	const float Time = GetDistributionEvalTime(Context);
	switch (Distribution.Mode)
	{
	case EParticleDistributionMode::RandomRange:
		if (IsUniformXYZ(Distribution))
		{
			const float Value = Context.RandomStream
				? Context.RandomStream->GetRange(Distribution.Min.X, Distribution.Max.X)
				: Distribution.Min.X;
			return MakeUniformVector(Value);
		}
		else
		{
			return Context.RandomStream
				? FVector(
					Context.RandomStream->GetRange(Distribution.Min.X, Distribution.Max.X),
					Context.RandomStream->GetRange(Distribution.Min.Y, Distribution.Max.Y),
					Context.RandomStream->GetRange(Distribution.Min.Z, Distribution.Max.Z))
				: Distribution.Min;
		}
	case EParticleDistributionMode::Curve:
	{
		const FVector Value = EvaluateVectorCurveOrFallback(Distribution.Curve, Time, Distribution.Constant);
		return IsUniformXYZ(Distribution) ? MakeUniformVector(Value.X) : Value;
	}
	case EParticleDistributionMode::RandomRangeCurve:
	{
		const FVector MinValue = EvaluateVectorCurveOrFallback(Distribution.MinCurve, Time, Distribution.Min);
		const FVector MaxValue = EvaluateVectorCurveOrFallback(Distribution.MaxCurve, Time, Distribution.Max);
		const float Alpha = GetDistributionRandomAlpha(Context);
		if (IsUniformXYZ(Distribution))
		{
			return MakeUniformVector(MinValue.X + (MaxValue.X - MinValue.X) * Alpha);
		}
		return FVector::Lerp(MinValue, MaxValue, Alpha);
	}
	case EParticleDistributionMode::Constant:
	default:
		return IsUniformXYZ(Distribution) ? MakeUniformVector(Distribution.Constant.X) : Distribution.Constant;
	}
}

FColor EvaluateParticleColor(const FParticleColorDistribution& Distribution, const FParticleDistributionContext& Context)
{
	const float Time = GetDistributionEvalTime(Context);
	switch (Distribution.Mode)
	{
	case EParticleDistributionMode::RandomRange:
		return Context.RandomStream
			? FColor(
				Context.RandomStream->GetRange(Distribution.Min.R, Distribution.Max.R),
				Context.RandomStream->GetRange(Distribution.Min.G, Distribution.Max.G),
				Context.RandomStream->GetRange(Distribution.Min.B, Distribution.Max.B),
				Context.RandomStream->GetRange(Distribution.Min.A, Distribution.Max.A))
			: Distribution.Min;
	case EParticleDistributionMode::Curve:
		return EvaluateColorCurveOrFallback(Distribution.Curve, Time, Distribution.Constant);
	case EParticleDistributionMode::RandomRangeCurve:
	{
		const FColor MinValue = EvaluateColorCurveOrFallback(Distribution.MinCurve, Time, Distribution.Min);
		const FColor MaxValue = EvaluateColorCurveOrFallback(Distribution.MaxCurve, Time, Distribution.Max);
		const float Alpha = GetDistributionRandomAlpha(Context);
		return FColor::Lerp(MinValue, MaxValue, Alpha);
	}
	case EParticleDistributionMode::Constant:
	default:
		return Distribution.Constant;
	}
}

int32 UParticleModule::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return 0;
}

int32 UParticleModule::RequiredBytesPerInstance(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return 0;
}

bool UParticleModule::IsSpawnRateModule() const
{
	return false;
}

bool UParticleModule::IsSpawnModule() const
{
	return false;
}

bool UParticleModule::IsUpdateModule() const
{
	return false;
}

void UParticleModule::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	(void)Owner;
	(void)Offset;
	(void)Particle;
}

void UParticleModule::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	(void)Owner;
	(void)Offset;
	(void)SpawnTime;
	(void)Particle;
}

void UParticleModule::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	(void)Owner;
	(void)Offset;
	(void)DeltaTime;
}

bool UParticleModuleSpawn::IsSpawnRateModule() const
{
	return true;
}

UParticleModuleEventGenerator::UParticleModuleEventGenerator()
{
	Events.push_back(FParticleEventGenerateInfo{});
}

bool UParticleModuleEventGenerator::IsPrimaryEventEntry(int32 EventIndex) const
{
	if (EventIndex < 0 || EventIndex >= static_cast<int32>(Events.size()))
	{
		return false;
	}

	const FParticleEventGenerateInfo& Candidate = Events[static_cast<size_t>(EventIndex)];
	for (int32 EarlierIndex = 0; EarlierIndex < EventIndex; ++EarlierIndex)
	{
		const FParticleEventGenerateInfo& Earlier = Events[static_cast<size_t>(EarlierIndex)];
		if (Earlier.Type == Candidate.Type && Earlier.EventName == Candidate.EventName)
		{
			return false;
		}
	}

	return true;
}

void UParticleModuleEventGenerator::ValidateConfiguredEvents() const
{
	for (int32 EventIndex = 0; EventIndex < static_cast<int32>(Events.size()); ++EventIndex)
	{
		if (!IsPrimaryEventEntry(EventIndex))
		{
			UE_LOG_WARNING(
				"[Particle] Duplicate Event Generator entry found for '%s'. Using the first entry.",
				Events[static_cast<size_t>(EventIndex)].EventName.ToString().c_str());
		}
	}
}

void UParticleModuleEventGenerator::GenerateEvents(
	const FParticleEventPayload& Occurrence,
	IParticleEmitterInstanceOwner& Owner) const
{
	for (int32 EventIndex = 0; EventIndex < static_cast<int32>(Events.size()); ++EventIndex)
	{
		const FParticleEventGenerateInfo& Info = Events[static_cast<size_t>(EventIndex)];
		if (Info.Type != Occurrence.Type || !IsPrimaryEventEntry(EventIndex))
		{
			continue;
		}

		FParticleEventPayload Event = Occurrence;
		Event.EventName = Info.EventName;
		Owner.AddParticleEvent(Event);
	}
}

bool UParticleModuleEventReceiverSpawn::MatchesEvent(const FParticleEventPayload& Event) const
{
	return Event.Type == SourceEventType && Event.EventName == SourceEventName;
}

void UParticleModuleEventReceiverSpawn::ProcessEvent(
	FParticleEmitterInstance* Owner,
	const FParticleEventPayload& Event) const
{
	if (Owner == nullptr || SpawnCount <= 0 || !MatchesEvent(Event))
	{
		return;
	}

	Owner->SpawnParticlesFromEvent(
		Event,
		SpawnCount,
		bUseParticleSystemLocation,
		bInheritVelocity,
		InheritVelocityScale);
}

UParticleModuleLifetime::UParticleModuleLifetime()
{
	Lifetime.Constant = 1.0f;
	Lifetime.Min = 1.0f;
	Lifetime.Max = 1.0f;
}

bool UParticleModuleLifetime::IsSpawnModule() const
{
	return true;
}

int32 UParticleModuleLifetime::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleLifetime::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	InitializeDistributionPayload(Owner, Offset, Particle, Lifetime.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
	const FParticleDistributionContext Context = MakeSpawnDistributionContext(Owner, SpawnTime, Particle, Payload);
	Particle.Lifetime = std::max(EvaluateParticleFloat(Lifetime, Context), 0.0001f);
	Particle.OneOverMaxLifetime = 1.0f / Particle.Lifetime;
	Particle.RelativeTime = 0.0f;
}

bool UParticleModuleLocation::IsSpawnModule() const
{
	return true;
}

int32 UParticleModuleLocation::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleLocation::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	InitializeDistributionPayload(Owner, Offset, Particle, StartLocation.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
	const FParticleDistributionContext Context = MakeSpawnDistributionContext(Owner, SpawnTime, Particle, Payload);

	// StartLocation은 RequiredModule의 local / world 정책에 맞는 simulation space 값으로 저장
	Particle.Location = EvaluateParticleVector(StartLocation, Context);
	Particle.OldLocation = Particle.Location;
}

bool UParticleModuleVelocity::IsSpawnModule() const
{
	return true;
}

int32 UParticleModuleVelocity::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleVelocity::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	InitializeDistributionPayload(Owner, Offset, Particle, StartVelocity.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
	const FParticleDistributionContext Context = MakeSpawnDistributionContext(Owner, SpawnTime, Particle, Payload);
	Particle.Velocity = EvaluateParticleVector(StartVelocity, Context);
	Particle.BaseVelocity = Particle.Velocity;
}

bool UParticleModuleRotation::IsSpawnModule() const
{
	return true;
}

int32 UParticleModuleRotation::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleRotation::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	InitializeDistributionPayload(Owner, Offset, Particle, StartRotation.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleRotation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
	const FParticleDistributionContext Context = MakeSpawnDistributionContext(Owner, SpawnTime, Particle, Payload);
	Particle.Rotation = EvaluateParticleFloat(StartRotation, Context);
}

bool UParticleModuleMeshRotation::IsSpawnModule() const
{
	return true;
}

int32 UParticleModuleMeshRotation::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleMeshRotation::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	InitializeDistributionPayload(Owner, Offset, Particle, StartRotation.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleMeshRotation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
	const FParticleDistributionContext Context = MakeSpawnDistributionContext(Owner, SpawnTime, Particle, Payload);
	Particle.MeshRotation = EvaluateParticleVector(StartRotation, Context);
}

UParticleModuleColor::UParticleModuleColor()
{
	StartColor.Constant = FColor::White();
	StartColor.Min = FColor::White();
	StartColor.Max = FColor::White();
}

bool UParticleModuleColor::IsSpawnModule() const
{
	return true;
}

int32 UParticleModuleColor::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleColor::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	InitializeDistributionPayload(Owner, Offset, Particle, StartColor.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
	const FParticleDistributionContext Context = MakeSpawnDistributionContext(Owner, SpawnTime, Particle, Payload);
	Particle.Color = EvaluateParticleColor(StartColor, Context);
	Particle.BaseColor = Particle.Color;
}

UParticleModuleSize::UParticleModuleSize()
{
	StartSize.Constant = FVector::OneVector;
	StartSize.Min = FVector::OneVector;
	StartSize.Max = FVector::OneVector;
}

bool UParticleModuleSize::IsSpawnModule() const
{
	return true;
}

int32 UParticleModuleSize::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleSize::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	InitializeDistributionPayload(Owner, Offset, Particle, StartSize.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
	const FParticleDistributionContext Context = MakeSpawnDistributionContext(Owner, SpawnTime, Particle, Payload);
	Particle.Size = EvaluateParticleVector(StartSize, Context);
	Particle.BaseSize = Particle.Size;
}

UParticleModuleColorOverLife::UParticleModuleColorOverLife()
{
	// 기본 배율: spawn color를 그대로 유지하는 white multiplier
	ColorOverLife.Constant = FColor::White();
	ColorOverLife.Min = FColor::White();
	ColorOverLife.Max = FColor::White();
}

bool UParticleModuleColorOverLife::IsUpdateModule() const
{
	return true;
}

int32 UParticleModuleColorOverLife::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;

	// RandomRangeCurve의 particle별 random alpha 저장 payload
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleColorOverLife::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	// RandomRangeCurve 모드에서만 particle별 random alpha 고정
	InitializeDistributionPayload(Owner, Offset, Particle, ColorOverLife.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleColorOverLife::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	(void)DeltaTime;

	// update loop 진입 전 emitter instance 유효성 방어
	if (Owner == nullptr)
	{
		return;
	}

	// 공통 update loop 사용: pending kill particle 자동 건너뜀
	BEGIN_UPDATE_LOOP(Owner, Particle)
	{
		// spawn color를 기준값으로 유지하고, 수명 기반 색상 distribution은 multiplier로만 사용
		const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
		const FParticleDistributionContext Context = MakeUpdateDistributionContext(Owner, Particle, Payload);
		const FColor Factor = EvaluateParticleColor(ColorOverLife, Context);
		Particle.Color = Particle.BaseColor * Factor;
	}
	END_UPDATE_LOOP()
}

UParticleModuleSizeScaleOverLife::UParticleModuleSizeScaleOverLife()
{
	// 기본 배율: spawn size를 그대로 유지하는 one vector multiplier
	SizeScaleOverLife.Constant = FVector::OneVector;
	SizeScaleOverLife.Min = FVector::OneVector;
	SizeScaleOverLife.Max = FVector::OneVector;
}

bool UParticleModuleSizeScaleOverLife::IsUpdateModule() const
{
	return true;
}

int32 UParticleModuleSizeScaleOverLife::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;

	// RandomRangeCurve의 particle별 random alpha 저장 payload
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleSizeScaleOverLife::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	// RandomRangeCurve 모드에서만 particle별 random alpha 고정
	InitializeDistributionPayload(Owner, Offset, Particle, SizeScaleOverLife.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleSizeScaleOverLife::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	(void)DeltaTime;

	// update loop 진입 전 emitter instance 유효성 방어
	if (Owner == nullptr)
	{
		return;
	}

	// 공통 update loop 사용: pending kill particle 자동 건너뜀
	BEGIN_UPDATE_LOOP(Owner, Particle)
	{
		// 누적 곱 폭주 방지: 매 frame spawn size 기준으로 최종 크기 재계산
		const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
		const FParticleDistributionContext Context = MakeUpdateDistributionContext(Owner, Particle, Payload);
		const FVector Scale = EvaluateParticleVector(SizeScaleOverLife, Context);
		Particle.Size = Particle.BaseSize * Scale;
	}
	END_UPDATE_LOOP()
}

bool UParticleModuleVelocityOverLife::IsUpdateModule() const
{
	return true;
}

int32 UParticleModuleVelocityOverLife::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;

	// RandomRangeCurve의 particle별 random alpha 저장 payload
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleVelocityOverLife::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	// RandomRangeCurve 모드에서만 particle별 random alpha 고정
	InitializeDistributionPayload(Owner, Offset, Particle, VelocityOverLife.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleVelocityOverLife::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	(void)DeltaTime;

	// update loop 진입 전 emitter instance 유효성 방어
	if (Owner == nullptr)
	{
		return;
	}

	// 공통 update loop 사용: pending kill particle 자동 건너뜀
	BEGIN_UPDATE_LOOP(Owner, Particle)
	{
		// 수명 기반 velocity distribution 평가
		const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
		const FParticleDistributionContext Context = MakeUpdateDistributionContext(Owner, Particle, Payload);
		const FVector EvaluatedVelocity = EvaluateParticleVector(VelocityOverLife, Context);

		// absolute 모드: distribution 값을 최종 속도로 직접 사용
		if (bAbsolute)
		{
			Particle.Velocity = EvaluatedVelocity;
			continue;
		}

		// additive 모드: 초기 속도와 acceleration이 갱신한 기준 속도 위에 offset 추가
		Particle.Velocity = Particle.BaseVelocity + EvaluatedVelocity;
	}
	END_UPDATE_LOOP()
}

bool UParticleModuleAcceleration::IsUpdateModule() const
{
	return true;
}

int32 UParticleModuleAcceleration::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;

	// RandomRangeCurve의 particle별 random alpha 저장 payload
	return static_cast<int32>(sizeof(FParticleDistributionPayload));
}

void UParticleModuleAcceleration::InitializeParticle(FParticleEmitterInstance* Owner, int32 Offset, FBaseParticle& Particle)
{
	// RandomRangeCurve 모드에서만 particle별 random alpha 고정
	InitializeDistributionPayload(Owner, Offset, Particle, Acceleration.Mode == EParticleDistributionMode::RandomRangeCurve);
}

void UParticleModuleAcceleration::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	// update loop 진입 전 emitter instance 유효성 방어
	if (Owner == nullptr)
	{
		return;
	}

	// 공통 update loop 사용: pending kill particle 자동 건너뜀
	BEGIN_UPDATE_LOOP(Owner, Particle)
	{
		// frame별 acceleration distribution 평가
		const FParticleDistributionPayload* Payload = GetDistributionPayload(Owner, Offset, Particle);
		const FParticleDistributionContext Context = MakeUpdateDistributionContext(Owner, Particle, Payload);
		const FVector Accel = EvaluateParticleVector(Acceleration, Context);

		// 별도 acceleration field 없이 기준 속도 자체를 적분
		Particle.BaseVelocity += Accel * DeltaTime;

		// 기본 최종 속도 동기화: 뒤쪽 VelocityOverLife module이 있으면 이 값을 다시 보정
		Particle.Velocity = Particle.BaseVelocity;
	}
	END_UPDATE_LOOP()
}

bool UParticleModuleSizeScaleBySpeed::IsUpdateModule() const
{
	return true;
}

void UParticleModuleSizeScaleBySpeed::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	(void)Offset;
	(void)DeltaTime;

	// update loop 진입 전 emitter instance 유효성 방어
	if (Owner == nullptr)
	{
		return;
	}

	// 공통 update loop 사용: pending kill particle 자동 건너뜀
	BEGIN_UPDATE_LOOP(Owner, Particle)
	{
		// 현재 속도 크기 기반 축별 scale 계산
		const float Speed = Particle.Velocity.Size();
		const FVector RawScale = SpeedScale * Speed;

		// 최소 scale 1 보장 후 MaxScale로 축별 상한 적용
		const FVector ClampedScale(
			std::min(std::max(RawScale.X, 1.0f), MaxScale.X),
			std::min(std::max(RawScale.Y, 1.0f), MaxScale.Y),
			std::min(std::max(RawScale.Z, 1.0f), MaxScale.Z));

		// 앞선 size module이 만든 현재 크기에 속도 기반 scale 추가 적용
		Particle.Size = Particle.Size * ClampedScale;
	}
	END_UPDATE_LOOP()
}

bool UParticleModuleCollision::IsUpdateModule() const
{
	// 일반 update 이전 위치에서는 이번 frame 이동 구간을 검사할 수 없음
	return false;
}

int32 UParticleModuleCollision::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return static_cast<int32>(sizeof(FParticleCollisionPayload));
}

FParticleCollisionPayload* UParticleModuleCollision::GetCollisionPayload(
	FParticleEmitterInstance* Owner,
	int32 Offset,
	FBaseParticle& Particle) const
{
	if (Owner == nullptr || Offset < 0)
	{
		return nullptr;
	}

	uint8* Payload = Owner->GetParticlePayloadByOffset(Particle, Offset);
	return Payload != nullptr ? reinterpret_cast<FParticleCollisionPayload*>(Payload) : nullptr;
}

void UParticleModuleCollision::InitializeParticle(
	FParticleEmitterInstance* Owner,
	int32 Offset,
	FBaseParticle& Particle)
{
	FParticleCollisionPayload* Payload = GetCollisionPayload(Owner, Offset, Particle);
	if (Payload == nullptr)
	{
		return;
	}

	*Payload = FParticleCollisionPayload{};
	Payload->UsedMaxCollisions = std::max(MaxCollisions, 1);
	Payload->UsedDampingFactor = DampingFactor;
	Payload->UsedDelayAmount = std::max(DelayAmount, 0.0f);
}

float UParticleModuleCollision::ComputeCollisionRadius(const FBaseParticle& Particle) const
{
	if (!bUseParticleRadius)
	{
		return std::max(CollisionRadius, 0.0f);
	}

	const float MaxParticleSize = std::max(
		std::abs(Particle.Size.X),
		std::max(std::abs(Particle.Size.Y), std::abs(Particle.Size.Z)));
	return MaxParticleSize * std::max(ParticleRadiusScale, 0.0f);
}

void UParticleModuleCollision::ApplyCollisionCompleteOption(
	FParticleEmitterInstance* Owner,
	int32 ActiveIndex,
	FBaseParticle& Particle,
	FParticleCollisionPayload& Payload) const
{
	switch (CollisionCompletionOption)
	{
	case EParticleCollisionComplete::Kill:
		Owner->KillParticleByActiveIndex(ActiveIndex);
		break;
	case EParticleCollisionComplete::Freeze:
		SetParticleFlag(Particle, EParticleFlags::Freeze);
		SetParticleFlag(Particle, EParticleFlags::FreezeTranslation);
		SetParticleFlag(Particle, EParticleFlags::FreezeRotation);
		Particle.Velocity = FVector::ZeroVector;
		Particle.BaseVelocity = FVector::ZeroVector;
		Particle.RotationRate = 0.0f;
		break;
	case EParticleCollisionComplete::HaltCollisions:
		Payload.bIgnoreCollisions = true;
		SetParticleFlag(Particle, EParticleFlags::IgnoreCollisions);
		break;
	case EParticleCollisionComplete::FreezeTranslation:
		SetParticleFlag(Particle, EParticleFlags::FreezeTranslation);
		Particle.Velocity = FVector::ZeroVector;
		Particle.BaseVelocity = FVector::ZeroVector;
		break;
	case EParticleCollisionComplete::FreezeRotation:
		SetParticleFlag(Particle, EParticleFlags::FreezeRotation);
		Particle.RotationRate = 0.0f;
		break;
	case EParticleCollisionComplete::FreezeMovement:
		SetParticleFlag(Particle, EParticleFlags::FreezeMovement);
		SetParticleFlag(Particle, EParticleFlags::FreezeTranslation);
		SetParticleFlag(Particle, EParticleFlags::FreezeRotation);
		Particle.Velocity = FVector::ZeroVector;
		Particle.BaseVelocity = FVector::ZeroVector;
		Particle.RotationRate = 0.0f;
		break;
	}
}

void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	(void)DeltaTime;
	if (Owner == nullptr)
	{
		return;
	}

	for (int32 ActiveIndex = 0; ActiveIndex < Owner->GetActiveParticleCount(); ++ActiveIndex)
	{
		FBaseParticle& Particle = Owner->GetParticleByActiveIndex(ActiveIndex);
		FParticleCollisionPayload* Payload = GetCollisionPayload(Owner, Offset, Particle);
		if (Payload == nullptr ||
			Owner->IsParticlePendingKill(Particle) ||
			Payload->bIgnoreCollisions ||
			Payload->UsedCollisions >= Payload->UsedMaxCollisions ||
			HasParticleFlag(Particle, EParticleFlags::IgnoreCollisions) ||
			HasParticleFlag(Particle, EParticleFlags::Freeze) ||
			HasParticleFlag(Particle, EParticleFlags::FreezeMovement))
		{
			continue;
		}

		// RelativeTime은 수명 기준 0~1 값
		// DelayAmount는 초 단위라서 AgeSeconds와 비교
		if (Particle.AgeSeconds < Payload->UsedDelayAmount)
		{
			continue;
		}

		const FVector StartWS = Owner->TransformLocationToWorldSpace(Particle.OldLocation);
		const FVector EndWS = Owner->TransformLocationToWorldSpace(Particle.Location);
		const float RadiusWS = Owner->TransformRadiusToWorldSpace(ComputeCollisionRadius(Particle));
		const FCollisionShape CollisionShape = RadiusWS <= 1.e-6f
			? FCollisionShape::MakeLine()
			: FCollisionShape::MakeSphere(RadiusWS);
		AActor* SourceActor = bIgnoreSourceActor ? Owner->GetOwner().GetSourceActor() : nullptr;

		FHitResult Hit;
		if (!Owner->GetOwner().ParticleLineCheck(Hit, SourceActor, EndWS, StartWS, CollisionShape) ||
			!Hit.IsValid())
		{
			continue;
		}

		const FVector MoveWS = EndWS - StartWS;
		const FVector IncomingVelocityWS = Owner->TransformVelocityToWorldSpace(Particle.Velocity);
		const FVector HitCenterWS = StartWS + MoveWS * Hit.Time;
		const FVector NewCenterWS =
			HitCenterWS + Hit.Normal * std::max(CollisionPushOut, 0.0f);
		Particle.Location = Owner->TransformLocationToSimulationSpace(NewCenterWS);

		// 기존 외부 collision payload와 내부 occurrence의 공통 hit 정보
		FParticleEventCollideData Event;
		Event.EmitterIndex = Owner->GetEmitterIndex();
		Event.ParticleIndex = Owner->GetPhysicalIndexByActiveIndex(ActiveIndex);
		Event.SpawnId = Particle.SpawnId;
		Event.ParticleTime = Particle.RelativeTime;
		Event.CollisionTime = Hit.Time;
		Event.Location = Hit.Location;
		Event.Direction = MoveWS.GetSafeNormal();
		Event.Velocity = IncomingVelocityWS;
		Event.Normal = Hit.Normal;
		Event.FaceIndex = Hit.FaceIndex;
		Event.HitComponent = Hit.HitComponent;
		Event.HitActor = Hit.HitComponent != nullptr ? Hit.HitComponent->GetOwner() : nullptr;
		Owner->GetOwner().AddCollisionEvent(Event);
		Owner->ReportCollisionOccurrence(Event);

		++Payload->UsedCollisions;
		if (Payload->UsedCollisions >= Payload->UsedMaxCollisions)
		{
			ApplyCollisionCompleteOption(Owner, ActiveIndex, Particle, *Payload);
		}
		else
		{
			const FVector ReflectedVelocityWS =
				IncomingVelocityWS - Hit.Normal * (2.0f * FVector::DotProduct(IncomingVelocityWS, Hit.Normal));
			const FVector DampenedVelocityWS = ReflectedVelocityWS * Payload->UsedDampingFactor;
			Particle.Velocity = Owner->TransformVelocityToSimulationSpace(DampenedVelocityWS);
			Particle.BaseVelocity = Particle.Velocity;
		}
	}
}

/**
 * @brief SubUV module의 particle payload를 조회합니다.
 */
static FSubUVParticlePayload* GetSubUVPayload(FParticleEmitterInstance* Owner, FBaseParticle& Particle, int32 Offset)
{
	// 유효하지 않은 emitter instance 또는 payload offset 방어
	if (Owner == nullptr || Offset < 0)
	{
		return nullptr;
	}

	// particle stride 범위 검사는 emitter instance의 payload 조회 함수에 위임
	uint8* Raw = Owner->GetParticlePayloadByOffset(Particle, Offset);
	return reinterpret_cast<FSubUVParticlePayload*>(Raw);
}

void UParticleModuleSubUV::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle)
{
	FSubUVParticlePayload* Payload = GetSubUVPayload(Owner, Particle, Offset);
	if (!Payload)
	{
		return;
	}

	const FParticleDistributionContext Context = MakeSpawnDistributionContext(Owner, SpawnTime, Particle, nullptr);

	if (InterpMethod == EParticleSubUVInterpMethod::Random) // Random: Spawn 시 프레임 Random 결정
	{
		const int32 TotalFrames = std::max(Columns * Rows, 1);
		Payload->ImageIndex = Owner->RandomStream.GetRange(0.0f, static_cast<float>(TotalFrames - 1));
		Payload->RandomSeed = Particle.Seed;
	}
	else
	{
		const int32 TotalFrames = std::max(Columns * Rows, 1);
		Payload->ImageIndex = 0;
		Payload->RandomSeed = 0;
	}
}

void UParticleModuleSubUV::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	(void)DeltaTime;

	// SubUV frame 수와 emitter instance 유효성 방어
	const int32 TotalFrames = Columns * Rows;
	if (Owner == nullptr || TotalFrames <= 0)
	{
		return;
	}

	// 공통 update loop 사용: pending kill particle 자동 건너뜀
	BEGIN_UPDATE_LOOP(Owner, Particle)
	{
		// 특정 particle payload가 없더라도 나머지 particle update는 계속 진행
		FSubUVParticlePayload* Payload = GetSubUVPayload(Owner, Particle, Offset);
		if (!Payload)
		{
			continue;
		}

		// Linear 모드: 수명 또는 SubImageIndex distribution 기반 frame 갱신
		if (InterpMethod == EParticleSubUVInterpMethod::Linear)
		{
			const FParticleDistributionContext Context = MakeUpdateDistributionContext(Owner, Particle, nullptr);
			Payload->ImageIndex = EvaluateSubImageFrameIndex(*this, Context, TotalFrames);
		}

		// Random 모드: Spawn 시점에 정한 frame 유지, 공통 clamp만 적용
		Payload->ImageIndex = MathUtil::Clamp(Payload->ImageIndex, 0.0f, static_cast<float>(TotalFrames - 1));
	}
	END_UPDATE_LOOP()
}

UParticleLODLevel::~UParticleLODLevel()
{
	if (IsLiveObject(RequiredModule))
	{
		UObjectManager::Get().DestroyObject(RequiredModule);
	}
	RequiredModule = nullptr;

	if (IsLiveObject(SpawnModule))
	{
		UObjectManager::Get().DestroyObject(SpawnModule);
	}
	SpawnModule = nullptr;

	for (UParticleModule* Module : Modules)
	{
		if (Module != RequiredModule && Module != SpawnModule && Module != TypeDataModule && IsLiveObject(Module))
		{
			UObjectManager::Get().DestroyObject(Module);
		}
	}
	Modules.clear();

	if (IsLiveObject(TypeDataModule))
	{
		UObjectManager::Get().DestroyObject(TypeDataModule);
	}
	TypeDataModule = nullptr;
}

void UParticleLODLevel::PostDuplicate(UObject* Original)
{
	UObject::PostDuplicate(Original);

	UParticleLODLevel* SourceLOD = Cast<UParticleLODLevel>(Original);
	if (!SourceLOD)
	{
		return;
	}

	FDuplicateContext DuplicateContext;
	DuplicateContext.Add(SourceLOD, this);

	RequiredModule = SourceLOD->RequiredModule
		? Cast<UParticleModuleRequired>(SourceLOD->RequiredModule->Duplicate(&DuplicateContext))
		: nullptr;
	if (RequiredModule)
	{
		DuplicateContext.Add(SourceLOD->RequiredModule, RequiredModule);
	}

	SpawnModule = SourceLOD->SpawnModule
		? Cast<UParticleModuleSpawn>(SourceLOD->SpawnModule->Duplicate(&DuplicateContext))
		: nullptr;
	if (SpawnModule)
	{
		DuplicateContext.Add(SourceLOD->SpawnModule, SpawnModule);
	}

	TypeDataModule = SourceLOD->TypeDataModule
		? Cast<UParticleModuleTypeDataBase>(SourceLOD->TypeDataModule->Duplicate(&DuplicateContext))
		: nullptr;
	if (TypeDataModule)
	{
		DuplicateContext.Add(SourceLOD->TypeDataModule, TypeDataModule);
	}

	Modules.clear();
	for (UParticleModule* SourceModule : SourceLOD->Modules)
	{
		UParticleModule* DuplicatedModule = SourceModule
			? Cast<UParticleModule>(SourceModule->Duplicate(&DuplicateContext))
			: nullptr;
		if (DuplicatedModule)
		{
			Modules.push_back(DuplicatedModule);
		}
	}
}

UParticleEmitter::~UParticleEmitter()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (IsLiveObject(LODLevel))
		{
			UObjectManager::Get().DestroyObject(LODLevel);
		}
	}
	LODLevels.clear();
}

void UParticleEmitter::PostDuplicate(UObject* Original)
{
	UObject::PostDuplicate(Original);

	UParticleEmitter* SourceEmitter = Cast<UParticleEmitter>(Original);
	if (!SourceEmitter)
	{
		return;
	}

	LODLevels.clear();
	for (UParticleLODLevel* SourceLOD : SourceEmitter->LODLevels)
	{
		UParticleLODLevel* DuplicatedLOD = SourceLOD
			? Cast<UParticleLODLevel>(SourceLOD->Duplicate())
			: nullptr;
		if (DuplicatedLOD)
		{
			LODLevels.push_back(DuplicatedLOD);
		}
	}
	CacheEmitterModuleInfo();
}

UParticleSystem::UParticleSystem()
{
	// LOD 0은 항상 0.0f로 설정되어야 함
	LODDistances.push_back(0.0f);
}

UParticleSystem::~UParticleSystem()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (IsLiveObject(Emitter))
		{
			UObjectManager::Get().DestroyObject(Emitter);
		}
	}
	Emitters.clear();
}

void UParticleSystem::PostDuplicate(UObject* Original)
{
	UObject::PostDuplicate(Original);

	UParticleSystem* SourceParticleSystem = Cast<UParticleSystem>(Original);
	if (!SourceParticleSystem)
	{
		return;
	}

	AssetPath = SourceParticleSystem->AssetPath;
	Emitters.clear();
	for (UParticleEmitter* SourceEmitter : SourceParticleSystem->Emitters)
	{
		UParticleEmitter* DuplicatedEmitter = SourceEmitter
			? Cast<UParticleEmitter>(SourceEmitter->Duplicate())
			: nullptr;
		if (DuplicatedEmitter)
		{
			Emitters.push_back(DuplicatedEmitter);
		}
	}
}

FParticleEmitterInstance* UParticleModuleTypeDataBase::CreateInstance(
	UParticleEmitter* InEmitterTemplate,
	IParticleEmitterInstanceOwner& InOwner)
{
	FParticleEmitterInstance* Instance = new FParticleEmitterInstance(InOwner);
	Instance->SpriteTemplate = InEmitterTemplate;
	return Instance;
}

FDynamicEmitterDataBase* UParticleModuleTypeDataBase::GetDynamicRenderData(FParticleEmitterInstance* InEmitterInstance)
{
	// note: TypeDataBase에서는 Sprite용 Render Data임에 유의. 다른 렌더러는 별도의 Render Data가 필요

	// EmitterInstance 유효성 체크
	if (InEmitterInstance == nullptr ||
		InEmitterInstance->ActiveParticles <= 0 ||
		InEmitterInstance->ParticleData == nullptr ||
		InEmitterInstance->ParticleIndices == nullptr ||
		InEmitterInstance->ParticleStride <= 0 ||
		InEmitterInstance->CurrentRuntimeCache == nullptr)
	{
		return nullptr;
	}

	// render 대상 live particle count
	const int32 ActiveParticleCount = CountLiveParticlesForRender(*InEmitterInstance);
	if (ActiveParticleCount <= 0)
	{
		return nullptr;
	}

	// live particle 기준 snapshot 크기
	const int32 ParticleStride = InEmitterInstance->ParticleStride;
	size_t ParticleDataBytes = 0;
	size_t SnapshotLogicalBytes = 0;
	if (!CalculateRenderSnapshotByteSizes(ActiveParticleCount, ParticleStride, ParticleDataBytes, SnapshotLogicalBytes))
	{
		return nullptr;
	}

	FDynamicSpriteEmitterData* RenderData = new FDynamicSpriteEmitterData();
	RenderData->OwnedParticleData.resize(ParticleDataBytes + ParticleHelper::ParticleAlignment);
	RenderData->OwnedParticleIndices.resize(static_cast<size_t>(ActiveParticleCount));

	uint8* SnapshotParticleData = GetAlignedSnapshotParticleData(*RenderData);
	if (SnapshotParticleData == nullptr)
	{
		delete RenderData;
		return nullptr;
	}

	const int32 SnapshotParticleCount = CopyLiveParticlesForRenderSnapshot(
		*InEmitterInstance,
		SnapshotParticleData,
		RenderData->OwnedParticleIndices,
		true);
	if (SnapshotParticleCount <= 0)
	{
		delete RenderData;
		return nullptr;
	}

	UParticleModuleRequired* RequiredModule = InEmitterInstance->CurrentRuntimeCache->RequiredModule;
	RenderData->ReplayData.EmitterType = EDynamicEmitterType::Sprite;
	RenderData->ReplayData.ActiveParticleCount = SnapshotParticleCount;
	RenderData->ReplayData.ParticleStride = ParticleStride;
	RenderData->ReplayData.SortMode = RequiredModule != nullptr
		? RequiredModule->SortMode
		: EParticleSortMode::ViewDepthBackToFront;
	RenderData->Material = RequiredModule != nullptr ? RequiredModule->Material : nullptr;

	// snapshot의 Location / OldLocation은 이미 world space이므로 renderer가 component transform을 다시 적용하면 안 됨!
	RenderData->ReplayData.CoordinateSpace = EParticleCoordinateSpace::World;
	RenderData->ComponentToWorld = FMatrix::Identity;
	RenderData->ReplayData.Scale = FVector::OneVector;
	RenderData->ReplayData.RequiredModule = RequiredModule;

	UParticleModuleSubUV* SubUVModule = FindSubUVModule(InEmitterInstance->CurrentLODLevel);
	if (SubUVModule != nullptr && SubUVModule->bEnabled)
	{
		const int32 SubUVPayloadOffset =
			InEmitterInstance->CurrentRuntimeCache->GetParticlePayloadOffset(SubUVModule);

		RenderData->ReplayData.SubUVPayloadOffset = SubUVPayloadOffset;
		RenderData->ReplayData.SubUVColumns = std::max(SubUVModule->Columns, 1);
		RenderData->ReplayData.SubUVRows = std::max(SubUVModule->Rows, 1);
		RenderData->ReplayData.SubUVTexture = ResolveDiffuseTexture(RenderData->Material);
	}

	// snapshot은 particle data와 index를 별도 버퍼로 소유. renderer는 연속 메모리를 가정하지 말고
	// 반드시 DataContainer의 ParticleData / ParticleIndices 포인터를 통해 접근해야 함
	RenderData->ReplayData.DataContainer.MemBlockSize = static_cast<int32>(SnapshotLogicalBytes);
	RenderData->ReplayData.DataContainer.ParticleDataNumBytes = static_cast<int32>(ParticleDataBytes);
	RenderData->ReplayData.DataContainer.ParticleIndicesNumShorts = SnapshotParticleCount;
	RenderData->ReplayData.DataContainer.ParticleData = SnapshotParticleData;
	RenderData->ReplayData.DataContainer.ParticleIndices = RenderData->OwnedParticleIndices.data();

	return RenderData;
}

UParticleModuleSubUV* UParticleModuleTypeDataBase::FindSubUVModule(const UParticleLODLevel* LODLevel)
{
	if (LODLevel == nullptr)
	{
		return nullptr;
	}

	if (!IsSpriteTypeDataModule(LODLevel->TypeDataModule))
	{
		return nullptr;
	}

	for (UParticleModule* Module : LODLevel->Modules)
	{
		UParticleModuleSubUV* SubUV = Cast<UParticleModuleSubUV>(Module);
		if (SubUV != nullptr && SubUV->bEnabled)
		{
			return SubUV;
		}
	}

	return nullptr;
}

int32 UParticleModuleTypeDataBase::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	(void)TypeData;
	return GetRequiredPayloadSize();
}

int32 UParticleModuleTypeDataBase::GetRequiredPayloadSize() const
{
	return 0;
}

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(
	UParticleEmitter* InEmitterTemplate,
	IParticleEmitterInstanceOwner& InOwner)
{
	FParticleMeshEmitterInstance* Instance = new FParticleMeshEmitterInstance(InOwner);
	Instance->SpriteTemplate = InEmitterTemplate;
	return Instance;
}

FDynamicEmitterDataBase* UParticleModuleTypeDataMesh::GetDynamicRenderData(FParticleEmitterInstance* InEmitterInstance)
{
	// EmitterIntsance 유효성 검사
	if (InEmitterInstance == nullptr ||
		InEmitterInstance->ActiveParticles <= 0 ||
		InEmitterInstance->ParticleData == nullptr ||
		InEmitterInstance->ParticleIndices == nullptr ||
		InEmitterInstance->ParticleStride <= 0 ||
		InEmitterInstance->CurrentRuntimeCache == nullptr)
	{
		return nullptr;
	}

	// render 대상 live particle count
	const int32 ActiveParticleCount = CountLiveParticlesForRender(*InEmitterInstance);
	if (ActiveParticleCount <= 0)
	{
		return nullptr;
	}

	// live particle 기준 snapshot 크기
	const int32 ParticleStride = InEmitterInstance->ParticleStride;
	size_t ParticleDataBytes = 0;
	size_t SnapshotLogicalBytes = 0;
	if (!CalculateRenderSnapshotByteSizes(ActiveParticleCount, ParticleStride, ParticleDataBytes, SnapshotLogicalBytes))
	{
		return nullptr;
	}

	// Mesh
	FDynamicMeshEmitterData* RenderData = new FDynamicMeshEmitterData();
	RenderData->Mesh = GetStaticMesh();

	// Particle Data, Indices
	RenderData->OwnedParticleData.resize(ParticleDataBytes + ParticleHelper::ParticleAlignment);
	RenderData->OwnedParticleIndices.resize(static_cast<size_t>(ActiveParticleCount));

	uint8* SnapshotParticleData = GetAlignedSnapshotParticleData(*RenderData);
	if (SnapshotParticleData == nullptr)
	{
		delete RenderData;
		return nullptr;
	}

	// Require Module
	const UParticleModuleRequired* RequiredModule = InEmitterInstance->CurrentRuntimeCache->RequiredModule;

	// Sort Mode
	RenderData->ReplayData.SortMode = RequiredModule != nullptr
		? RequiredModule->SortMode
		: EParticleSortMode::ViewDepthBackToFront;

	const int32 SnapshotParticleCount = CopyLiveParticlesForRenderSnapshot(
		*InEmitterInstance,
		SnapshotParticleData,
		RenderData->OwnedParticleIndices,
		false);
	if (SnapshotParticleCount <= 0)
	{
		delete RenderData;
		return nullptr;
	}
	// ReplayData
	RenderData->ReplayData.ActiveParticleCount = SnapshotParticleCount;
	RenderData->ReplayData.ParticleStride = ParticleStride;
	// Mesh emitters render with the static mesh section materials; RequiredModule.Material is intentionally ignored.
	RenderData->Material = nullptr;

	// Mesh particle snapshots stay in emitter local space; the renderer applies ComponentToWorld per instance.
	RenderData->ComponentToWorld = InEmitterInstance->GetOwner().GetComponentToWorld();
	RenderData->ReplayData.CoordinateSpace = EParticleCoordinateSpace::Local;
	RenderData->ReplayData.Scale = FVector::OneVector;

	// TODO: 중앙 renderer가 Mesh instance transform을 생성할 때 Mesh Particle의 회전 축과 정렬 정책을 반영한다.
	RenderData->ReplayData.DataContainer.MemBlockSize = static_cast<int32>(SnapshotLogicalBytes);
	RenderData->ReplayData.DataContainer.ParticleDataNumBytes = static_cast<int32>(ParticleDataBytes);
	RenderData->ReplayData.DataContainer.ParticleIndicesNumShorts = SnapshotParticleCount;
	RenderData->ReplayData.DataContainer.ParticleData = SnapshotParticleData;
	RenderData->ReplayData.DataContainer.ParticleIndices = RenderData->OwnedParticleIndices.data();

	return RenderData;
}

FParticleEmitterInstance* UParticleModuleTypeDataBeam::CreateInstance(
	UParticleEmitter* InEmitterTemplate,
	IParticleEmitterInstanceOwner& InOwner)
{
	// Beam TypeData 전용 emitter instance 생성
	FParticleBeamEmitterInstance* Instance = new FParticleBeamEmitterInstance(InOwner);

	// 기존 emitter instance 계약에 맞춘 template 보관
	Instance->SpriteTemplate = InEmitterTemplate;
	return Instance;
}

FDynamicEmitterDataBase* UParticleModuleTypeDataBeam::GetDynamicRenderData(FParticleEmitterInstance* InEmitterInstance)
{
	// EmitterInstance 기본 유효성
	if (InEmitterInstance == nullptr ||
		InEmitterInstance->ActiveParticles <= 0 ||
		InEmitterInstance->ParticleData == nullptr ||
		InEmitterInstance->ParticleIndices == nullptr ||
		InEmitterInstance->ParticleStride <= 0 ||
		InEmitterInstance->CurrentRuntimeCache == nullptr)
	{
		return nullptr;
	}

	// pending kill을 제외한 render 대상 particle 수
	const int32 ActiveParticleCount = CountLiveParticlesForRender(*InEmitterInstance);
	if (ActiveParticleCount <= 0)
	{
		return nullptr;
	}

	// live particle snapshot buffer 크기
	const int32 ParticleStride = InEmitterInstance->ParticleStride;
	size_t ParticleDataBytes = 0;
	size_t SnapshotLogicalBytes = 0;
	if (!CalculateRenderSnapshotByteSizes(ActiveParticleCount, ParticleStride, ParticleDataBytes, SnapshotLogicalBytes))
	{
		return nullptr;
	}

	// Beam render data와 snapshot 소유 buffer
	FDynamicBeamEmitterData* RenderData = new FDynamicBeamEmitterData();
	RenderData->OwnedParticleData.resize(ParticleDataBytes + ParticleHelper::ParticleAlignment);
	RenderData->OwnedParticleIndices.resize(static_cast<size_t>(ActiveParticleCount));

	// DataContainer가 참조할 aligned particle data 시작 주소
	uint8* SnapshotParticleData = GetAlignedSnapshotParticleData(*RenderData);
	if (SnapshotParticleData == nullptr)
	{
		delete RenderData;
		return nullptr;
	}

	// Beam source/target은 RequiredModule coordinate space를 그대로 따르므로 particle 위치를 world로 굽지 않음
	const int32 SnapshotParticleCount = CopyLiveParticlesForRenderSnapshot(
		*InEmitterInstance,
		SnapshotParticleData,
		RenderData->OwnedParticleIndices,
		false);
	if (SnapshotParticleCount <= 0)
	{
		delete RenderData;
		return nullptr;
	}

	// RequiredModule 기반 render 정책
	const UParticleModuleRequired* RequiredModule = InEmitterInstance->CurrentRuntimeCache->RequiredModule;
	RenderData->Material = RequiredModule != nullptr ? RequiredModule->Material : nullptr;
	RenderData->ComponentToWorld = InEmitterInstance->GetOwner().GetComponentToWorld();

	// Beam replay 공통 정보
	RenderData->ReplayData.EmitterType = EDynamicEmitterType::Beam;
	RenderData->ReplayData.ActiveParticleCount = SnapshotParticleCount;
	RenderData->ReplayData.ParticleStride = ParticleStride;
	RenderData->ReplayData.SortMode = RequiredModule != nullptr
		? RequiredModule->SortMode
		: EParticleSortMode::ViewDepthBackToFront;
	RenderData->ReplayData.CoordinateSpace = RequiredModule != nullptr
		? RequiredModule->CoordinateSpace
		: EParticleCoordinateSpace::Local;
	RenderData->ReplayData.Scale = FVector::OneVector;

	// Beam TypeData property snapshot
	RenderData->ReplayData.SourcePoint = SourcePoint;
	RenderData->ReplayData.TargetPoint = TargetPoint;
	RenderData->ReplayData.BeamWidth = std::max(BeamWidth, 0.1f);

	// snapshot은 particle data와 index를 별도 buffer로 소유
	RenderData->ReplayData.DataContainer.MemBlockSize = static_cast<int32>(SnapshotLogicalBytes);
	RenderData->ReplayData.DataContainer.ParticleDataNumBytes = static_cast<int32>(ParticleDataBytes);
	RenderData->ReplayData.DataContainer.ParticleIndicesNumShorts = SnapshotParticleCount;
	RenderData->ReplayData.DataContainer.ParticleData = SnapshotParticleData;
	RenderData->ReplayData.DataContainer.ParticleIndices = RenderData->OwnedParticleIndices.data();

	return RenderData;
}

void UParticleModuleTypeDataMesh::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	Mesh = InStaticMesh;
	MeshAssetPath.SetPath(Mesh != nullptr ? Mesh->GetAssetPathFileName() : FString());
}

void UParticleModuleTypeDataMesh::PostEditProperty(const char* PropertyName)
{
	UParticleModuleTypeDataBase::PostEditProperty(PropertyName);

	if (PropertyName != nullptr && std::strcmp(PropertyName, "MeshAssetPath") == 0)
	{
		const FString RequestedPath = MeshAssetPath.GetPath();
		SetStaticMesh(RequestedPath.empty() ? nullptr : FResourceManager::Get().LoadStaticMesh(RequestedPath));
	}
}

int32 FParticleLODLevelRuntimeCache::GetParticlePayloadOffset(UParticleModule* Module) const
{
	const auto It = ModulePayloadOffsets.find(Module);
	return It != ModulePayloadOffsets.end() ? It->second : -1;
}

int32 FParticleLODLevelRuntimeCache::GetInstancePayloadOffset(UParticleModule* Module) const
{
	const auto It = ModuleInstanceOffsets.find(Module);
	return It != ModuleInstanceOffsets.end() ? It->second : -1;
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	LODLevelRuntimeCaches.clear();
	LODLevelRuntimeCaches.reserve(LODLevels.size());

	if (LODLevels.empty())
	{
		return;
	}

	const UParticleLODLevel* LayoutLODLevel = LODLevels[0];
	const FParticleLODLevelRuntimeCache StableLayoutCache = BuildStableLOD0RuntimeCache(LayoutLODLevel);
	const bool bValidCascadeTopology = ValidateLODTopology(true);

	for (int32 LODIndex = 0; LODIndex < static_cast<int32>(LODLevels.size()); ++LODIndex)
	{
		const UParticleLODLevel* LODLevel = LODLevels[static_cast<size_t>(LODIndex)];
		const UParticleLODLevel* RuntimeLODLevel = (LODIndex == 0 || bValidCascadeTopology)
			? LODLevel
			: LayoutLODLevel;

		// invalid topology LOD 0 fallback
		LODLevelRuntimeCaches.push_back(
			BuildLODLevelRuntimeCacheFromStableLayout(RuntimeLODLevel, LayoutLODLevel, StableLayoutCache));
	}
}

bool UParticleEmitter::ValidateLODTopology(bool bLogWarnings) const
{
	if (LODLevels.empty() || !IsLiveObject(LODLevels[0]))
	{
		LogLODWarning(bLogWarnings, "LOD topology validation failed. LOD 0 is missing.");
		return false;
	}

	const UParticleLODLevel* LayoutLODLevel = LODLevels[0];
	const int32 LOD0MaxParticles = LayoutLODLevel->RequiredModule != nullptr
		? LayoutLODLevel->RequiredModule->MaxParticles
		: 1;

	for (int32 LODIndex = 1; LODIndex < static_cast<int32>(LODLevels.size()); ++LODIndex)
	{
		const UParticleLODLevel* LODLevel = LODLevels[static_cast<size_t>(LODIndex)];
		if (!IsLiveObject(LODLevel))
		{
			if (bLogWarnings)
			{
				UE_LOG_WARNING("[Particle] LOD topology validation failed. LOD %d is missing.", LODIndex);
			}
			return false;
		}

		if (!AreModuleClassesCompatible(LayoutLODLevel->RequiredModule, LODLevel->RequiredModule))
		{
			if (bLogWarnings)
			{
				UE_LOG_WARNING("[Particle] LOD topology validation failed. LOD %d RequiredModule class differs from LOD 0.", LODIndex);
			}
			return false;
		}

		if (!AreModuleClassesCompatible(LayoutLODLevel->SpawnModule, LODLevel->SpawnModule))
		{
			if (bLogWarnings)
			{
				UE_LOG_WARNING("[Particle] LOD topology validation failed. LOD %d SpawnModule class differs from LOD 0.", LODIndex);
			}
			return false;
		}

		if (!AreModuleClassesCompatible(LayoutLODLevel->TypeDataModule, LODLevel->TypeDataModule))
		{
			if (bLogWarnings)
			{
				UE_LOG_WARNING("[Particle] LOD topology validation failed. LOD %d TypeDataModule class differs from LOD 0.", LODIndex);
			}
			return false;
		}

		if (LODLevel->Modules.size() != LayoutLODLevel->Modules.size())
		{
			if (bLogWarnings)
			{
				UE_LOG_WARNING(
					"[Particle] LOD topology validation failed. LOD %d module slot count differs from LOD 0.",
					LODIndex);
			}
			return false;
		}

		for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(LayoutLODLevel->Modules.size()); ++ModuleIndex)
		{
			const UParticleModule* LayoutModule = LayoutLODLevel->Modules[static_cast<size_t>(ModuleIndex)];
			const UParticleModule* LODModule = LODLevel->Modules[static_cast<size_t>(ModuleIndex)];
			if (!AreModuleClassesCompatible(LayoutModule, LODModule))
			{
				if (bLogWarnings)
				{
					UE_LOG_WARNING(
						"[Particle] LOD topology validation failed. LOD %d module slot %d class differs from LOD 0.",
						LODIndex,
						ModuleIndex);
				}
				return false;
			}
		}

		if (LODLevel->RequiredModule != nullptr && LODLevel->RequiredModule->MaxParticles > LOD0MaxParticles && bLogWarnings)
		{
			UE_LOG_WARNING(
				"[Particle] LOD %d MaxParticles is greater than LOD 0. Runtime hard capacity remains LOD 0 MaxParticles.",
				LODIndex);
		}
	}

	return true;
}

TArray<int32> UParticleEmitter::CalculateTotalPayloadSize() const
{
	TArray<int32> Result;
	Result.reserve(LODLevels.size());

	if (LODLevels.empty())
	{
		return Result;
	}

	const UParticleLODLevel* LayoutLODLevel = LODLevels[0];
	const FParticleLODLevelRuntimeCache StableLayoutCache = BuildStableLOD0RuntimeCache(LayoutLODLevel);
	for (int32 LODIndex = 0; LODIndex < static_cast<int32>(LODLevels.size()); ++LODIndex)
	{
		(void)LODIndex;
		Result.push_back(StableLayoutCache.ParticleStride);
	}

	return Result;
}

FParticleLODLevelRuntimeCache* UParticleEmitter::GetLODLevelRuntimeCache(int32 LODIndex)
{
	if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODLevelRuntimeCaches.size()))
	{
		return nullptr;
	}

	return &LODLevelRuntimeCaches[LODIndex];
}

const FParticleLODLevelRuntimeCache* UParticleEmitter::GetLODLevelRuntimeCache(int32 LODIndex) const
{
	if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODLevelRuntimeCaches.size()))
	{
		return nullptr;
	}

	return &LODLevelRuntimeCaches[LODIndex];
}

FParticleLODLevelRuntimeCache* UParticleEmitter::GetLOD0RuntimeCache()
{
	return GetLODLevelRuntimeCache(0);
}

const FParticleLODLevelRuntimeCache* UParticleEmitter::GetLOD0RuntimeCache() const
{
	return GetLODLevelRuntimeCache(0);
}
