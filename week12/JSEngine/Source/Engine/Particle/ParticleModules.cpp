#include "Particle/ParticleModules.h"

#include "Asset/CurveColorAsset.h"
#include "Asset/CurveFloatAsset.h"
#include "Asset/CurveVectorAsset.h"
#include "Core/ResourceManager.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleEmitterInstanceOwner.h"
#include "Particle/ParticleHelper.h"

#include <algorithm>
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

	UParticleModuleSpawn* ResolveSpawnModule(const UParticleLODLevel* LODLevel)
	{
		if (LODLevel == nullptr)
		{
			return nullptr;
		}

		// 명시적 SpawnModule 우선
		if (LODLevel->SpawnModule != nullptr)
		{
			return LODLevel->SpawnModule;
		}

		// legacy module 배열 fallback
		for (UParticleModule* Module : LODLevel->Modules)
		{
			if (Module != nullptr && Module->bEnabled && Module->IsSpawnRateModule())
			{
				return Cast<UParticleModuleSpawn>(Module);
			}
		}

		return nullptr;
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

bool UParticleModuleCollision::IsUpdateModule() const
{
	return true;
}

static FSubUVParticlePayload* GetSubUVPayload(FParticleEmitterInstance* Owner, FBaseParticle& Particle, int32 Offset)
{
	if (Offset < 0)
	{
		return nullptr;
	}

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

	const int32 TotalFrames = Columns * Rows;
	if (TotalFrames <= 0)
	{
		return;
	}

	const int32 ActiveCount = Owner->GetActiveParticleCount();
	for (int32 i = 0; i < ActiveCount; ++i)
	{
		FBaseParticle& Particle = Owner->GetParticleByActiveIndex(i);

		FSubUVParticlePayload* Payload = GetSubUVPayload(Owner, Particle, Offset);
		if (!Payload)
		{
			return;
		}
		// 기본값은 수명 기반 재생, 명시된 SubImageIndex 값은 실제 frame index로 사용.
		if (InterpMethod == EParticleSubUVInterpMethod::Linear)
		{
			const FParticleDistributionContext Context = MakeUpdateDistributionContext(Owner, Particle, nullptr);
			Payload->ImageIndex = EvaluateSubImageFrameIndex(*this, Context, TotalFrames);
		}
		// Random일 경우 고정된 프레임 유지

		Payload->ImageIndex = MathUtil::Clamp(Payload->ImageIndex, 0.0f, static_cast<float>(TotalFrames - 1));
	}
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
