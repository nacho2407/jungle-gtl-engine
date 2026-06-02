#include "Cloth/ClothSimulation.h"

#include "Cloth/ClothBuildConfig.h"
#include "Cloth/NvClothContext.h"
#include "Core/Logging/Log.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#if WITH_NV_CLOTH
#include <NvCloth/Allocator.h>
#include <NvCloth/Cloth.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Factory.h>
#include <NvCloth/Range.h>
#include <NvCloth/Solver.h>
#include <NvClothExt/ClothFabricCooker.h>
#include <PxPhysicsAPI.h>
#endif

namespace
{
constexpr float GDefaultParticleInvMass = 1.0f;
constexpr float GVectorTolerance = 1.0e-6f;
constexpr float GMinFixedStep = 0.001f;
constexpr float GMaxFixedStep = 0.1f;
constexpr int32 GMinSubstepCount = 1;
constexpr int32 GMaxSubstepCount = 4;

#if WITH_NV_CLOTH
/**
 * @brief engine vector를 physx vector로 변환합니다
 *
 * @param Vector 변환할 engine vector
 *
 * @return 변환된 physx vector
 */
physx::PxVec3 ToPxVec3(const FVector& Vector)
{
	return physx::PxVec3(Vector.X, Vector.Y, Vector.Z);
}

/**
 * @brief NvCloth particle 위치를 engine vector로 변환합니다
 *
 * @param Particle 변환할 NvCloth particle
 *
 * @return 변환된 engine vector
 */
FVector ToFVectorPosition(const physx::PxVec4& Particle)
{
	return FVector(Particle.x, Particle.y, Particle.z);
}

/**
 * @brief engine vector와 inverse mass를 NvCloth particle로 변환합니다
 *
 * @param Vector 변환할 engine vector
 *
 * @param InvMass particle inverse mass
 *
 * @return 변환된 NvCloth particle
 */
physx::PxVec4 ToPxParticle(const FVector& Vector, float InvMass)
{
	return physx::PxVec4(Vector.X, Vector.Y, Vector.Z, InvMass);
}
#endif

/**
 * @brief 실수 값을 지정된 범위 안으로 보정합니다
 *
 * @param Value 보정할 실수 값
 *
 * @param MinValue 허용 최소값
 *
 * @param MaxValue 허용 최대값
 *
 * @return 보정된 실수 값
 */
float ClampFloat(float Value, float MinValue, float MaxValue)
{
	if (!std::isfinite(Value))
	{
		return MinValue;
	}

	return (std::max)(MinValue, (std::min)(Value, MaxValue));
}

/**
 * @brief 정수 값을 지정된 범위 안으로 보정합니다
 *
 * @param Value 보정할 정수 값
 *
 * @param MinValue 허용 최소값
 *
 * @param MaxValue 허용 최대값
 *
 * @return 보정된 정수 값
 */
int32 ClampInt(int32 Value, int32 MinValue, int32 MaxValue)
{
	return (std::max)(MinValue, (std::min)(Value, MaxValue));
}
}

struct FClothSimulation::FImpl
{
#if WITH_NV_CLOTH
	nv::cloth::Solver* Solver = nullptr;
	nv::cloth::Fabric* Fabric = nullptr;
	nv::cloth::Cloth* Cloth = nullptr;
#endif

	~FImpl()
	{
		ReleaseResources();
	}

	/**
	 * @brief 현재 보유한 NvCloth resource를 해제합니다
	 */
	void ReleaseResources()
	{
#if WITH_NV_CLOTH
		if (Solver && Cloth)
		{
			// solver가 더 이상 cloth instance를 참조하지 않도록 먼저 분리
			Solver->removeCloth(Cloth);
		}

		if (Cloth)
		{
			NV_CLOTH_DELETE(Cloth);
			Cloth = nullptr;
		}

		if (Fabric)
		{
			Fabric->decRefCount();
			Fabric = nullptr;
		}

		if (Solver)
		{
			NV_CLOTH_DELETE(Solver);
			Solver = nullptr;
		}
#endif
	}
};

FClothSimulation::FClothSimulation()
	: Impl(std::make_unique<FImpl>())
{
}

FClothSimulation::~FClothSimulation()
{
	Shutdown();
}

bool FClothSimulation::Initialize(FNvClothContext* InContext, const FClothSimulationBuildDesc& BuildDesc)
{
	return Rebuild(InContext, BuildDesc);
}

bool FClothSimulation::Rebuild(FNvClothContext* InContext, const FClothSimulationBuildDesc& BuildDesc)
{
	Shutdown();

	Context = InContext;
	LastFailureDetail.clear();

	if (!Context)
	{
		return SetBuildFailure("NvCloth context is null");
	}

	if (!Context->GetBackendStatus().bAvailable)
	{
		return SetBuildFailure("NvCloth backend is unavailable: " + Context->GetBackendStatus().Detail);
	}

	if (!BuildDesc.IsValid())
	{
		return SetBuildFailure("cloth simulation build desc is invalid");
	}

#if WITH_NV_CLOTH
	nv::cloth::Factory* Factory = static_cast<nv::cloth::Factory*>(Context->GetFactoryHandle());
	if (!Factory)
	{
		return SetBuildFailure("NvCloth factory handle is null");
	}

	TArray<physx::PxVec3> CookPositions;
	TArray<physx::PxVec4> Particles;
	CookPositions.reserve(BuildDesc.InitialPositionsComponentLocal.size());
	Particles.reserve(BuildDesc.InitialPositionsComponentLocal.size());

	const bool bHasInvMasses = !BuildDesc.InvMasses.empty();
	for (size_t ParticleIndex = 0; ParticleIndex < BuildDesc.InitialPositionsComponentLocal.size(); ++ParticleIndex)
	{
		const FVector& Position = BuildDesc.InitialPositionsComponentLocal[ParticleIndex];
		const float InvMass = bHasInvMasses ? BuildDesc.InvMasses[ParticleIndex] : GDefaultParticleInvMass;

		// component local 기준 초기 위치와 inverse mass를 NvCloth particle 입력으로 변환
		CookPositions.push_back(ToPxVec3(Position));
		Particles.push_back(ToPxParticle(Position, InvMass));
	}

	nv::cloth::ClothMeshDesc MeshDesc;
	MeshDesc.points.count = static_cast<physx::PxU32>(CookPositions.size());
	MeshDesc.points.stride = sizeof(physx::PxVec3);
	MeshDesc.points.data = CookPositions.data();
	MeshDesc.triangles.count = static_cast<physx::PxU32>(BuildDesc.Indices.size() / 3);
	MeshDesc.triangles.stride = sizeof(uint32) * 3;
	MeshDesc.triangles.data = BuildDesc.Indices.data();

	if (bHasInvMasses)
	{
		MeshDesc.invMasses.count = static_cast<physx::PxU32>(BuildDesc.InvMasses.size());
		MeshDesc.invMasses.stride = sizeof(float);
		MeshDesc.invMasses.data = BuildDesc.InvMasses.data();
	}

	if (!MeshDesc.isValid())
	{
		return SetBuildFailure("NvCloth mesh desc is invalid");
	}

	// procedural grid는 component local에서 Z 아래 방향을 gravity 방향으로 사용
	const physx::PxVec3 GravityDirection(0.0f, 0.0f, -1.0f);
	Impl->Fabric = NvClothCookFabricFromMesh(Factory, MeshDesc, GravityDirection, nullptr, false);
	if (!Impl->Fabric)
	{
		Impl->ReleaseResources();
		return SetBuildFailure("NvCloth fabric cooking failed");
	}

	Impl->Cloth = Factory->createCloth(
		nv::cloth::Range<const physx::PxVec4>(Particles.data(), Particles.data() + Particles.size()),
		*Impl->Fabric);
	if (!Impl->Cloth)
	{
		Impl->ReleaseResources();
		return SetBuildFailure("NvCloth cloth creation failed");
	}

	Impl->Solver = Factory->createSolver();
	if (!Impl->Solver)
	{
		Impl->ReleaseResources();
		return SetBuildFailure("NvCloth solver creation failed");
	}

	// 이번 commit은 resource lifecycle 확인이 목적이므로 실제 simulation step은 다음 commit에서 연결
	Impl->Solver->addCloth(Impl->Cloth);

	ParticleCount = static_cast<uint32>(BuildDesc.InitialPositionsComponentLocal.size());
	IndexCount = static_cast<uint32>(BuildDesc.Indices.size());
	bInitialized = true;
	bValid = true;

	if (!ApplyPinning(BuildDesc.PinnedIndices, BuildDesc.PinTargetPositionsComponentLocal))
	{
		Impl->ReleaseResources();
		return SetBuildFailure("NvCloth pinning setup failed");
	}

	UE_LOG("[ClothSimulation] Resource initialized: particles=%u indices=%u pinned=%u",
		ParticleCount,
		IndexCount,
		PinnedCount);
	return true;
#else
	return SetBuildFailure("WITH_NV_CLOTH is disabled");
#endif
}

bool FClothSimulation::ApplyPinning(
	const TArray<uint32>& PinnedIndices,
	const TArray<FVector>& PinTargetPositionsComponentLocal)
{
	PinnedCount = 0;

	const bool bHasTargetPositions = !PinTargetPositionsComponentLocal.empty();
	if (bHasTargetPositions && PinTargetPositionsComponentLocal.size() != PinnedIndices.size())
	{
		LastFailureDetail = "cloth pin target count does not match pinned index count";
		return false;
	}

	if (!IsSimulationAvailable())
	{
		LastFailureDetail = "cloth simulation is unavailable for pinning";
		return false;
	}

#if WITH_NV_CLOTH
	if (!Impl || !Impl->Cloth)
	{
		LastFailureDetail = "NvCloth cloth instance is null";
		return false;
	}

	TArray<uint8> PinMask;
	TArray<FVector> TargetPositions;
	PinMask.resize(ParticleCount, 0);
	TargetPositions.resize(ParticleCount, FVector::ZeroVector);

	for (uint32 PinIndex = 0; PinIndex < PinnedIndices.size(); ++PinIndex)
	{
		const uint32 ParticleIndex = PinnedIndices[PinIndex];
		if (ParticleIndex >= ParticleCount)
		{
			LastFailureDetail = "cloth pinned particle index is out of range";
			return false;
		}

		if (PinMask[ParticleIndex] != 0)
		{
			continue;
		}

		// 같은 particle이 중복으로 들어와도 첫 target만 유지해 deterministic하게 처리
		PinMask[ParticleIndex] = 1;
		TargetPositions[ParticleIndex] = bHasTargetPositions
			? PinTargetPositionsComponentLocal[PinIndex]
			: FVector::ZeroVector;
		++PinnedCount;
	}

	{
		nv::cloth::MappedRange<physx::PxVec4> Particles = Impl->Cloth->getCurrentParticles();
		if (Particles.size() < ParticleCount)
		{
			LastFailureDetail = "current particle range is smaller than simulation particle count";
			return false;
		}

		for (uint32 ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex)
		{
			// unpinned particle은 다시 움직일 수 있도록 inverse mass를 복원
			Particles[ParticleIndex].w = GDefaultParticleInvMass;

			if (PinMask[ParticleIndex] != 0)
			{
				// hard pin은 inverse mass 0으로 표현하고, target이 있으면 위치도 함께 고정
				if (bHasTargetPositions)
				{
					const FVector& TargetPosition = TargetPositions[ParticleIndex];
					Particles[ParticleIndex] = ToPxParticle(TargetPosition, 0.0f);
				}
				else
				{
					Particles[ParticleIndex].w = 0.0f;
				}
			}
		}
	}

	{
		nv::cloth::MappedRange<physx::PxVec4> Particles = Impl->Cloth->getPreviousParticles();
		if (Particles.size() < ParticleCount)
		{
			LastFailureDetail = "previous particle range is smaller than simulation particle count";
			return false;
		}

		for (uint32 ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex)
		{
			// target 변경 직후 불필요한 속도가 생기지 않도록 previous particle도 같이 갱신
			Particles[ParticleIndex].w = GDefaultParticleInvMass;

			if (PinMask[ParticleIndex] != 0)
			{
				if (bHasTargetPositions)
				{
					const FVector& TargetPosition = TargetPositions[ParticleIndex];
					Particles[ParticleIndex] = ToPxParticle(TargetPosition, 0.0f);
				}
				else
				{
					Particles[ParticleIndex].w = 0.0f;
				}
			}
		}
	}

	LastFailureDetail.clear();
	return true;
#else
	LastFailureDetail = "WITH_NV_CLOTH is disabled";
	return false;
#endif
}

void FClothSimulation::Shutdown()
{
	if (Impl)
	{
		Impl->ReleaseResources();
	}

	Context = nullptr;
	bInitialized = false;
	bValid = false;
	ParticleCount = 0;
	IndexCount = 0;
	PinnedCount = 0;
	AccumulatedTime = 0.0f;
	SimulationTime = 0.0f;
	LastStepCount = 0;
}

bool FClothSimulation::Tick(
	float DeltaTime,
	const FClothSimulationRuntimeConfig& RuntimeConfig,
	TArray<FVector>& OutPositionsComponentLocal)
{
	OutPositionsComponentLocal.clear();
	LastStepCount = 0;
	LastFailureDetail.clear();

	if (!IsSimulationAvailable())
	{
		AccumulatedTime = 0.0f;
		return false;
	}

	if (!std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return false;
	}

	const float FixedStep = ClampFloat(RuntimeConfig.Timestep.FixedTimeStep, GMinFixedStep, GMaxFixedStep);
	const uint32 MaxSubsteps = static_cast<uint32>(ClampInt(RuntimeConfig.Timestep.MaxSubsteps, GMinSubstepCount, GMaxSubstepCount));
	const float MaxAccumulatedTime = (std::max)(FixedStep, RuntimeConfig.Timestep.MaxAccumulatedTime);
	AccumulatedTime = (std::min)(AccumulatedTime + DeltaTime, MaxAccumulatedTime);

	bool bSimulatedAnyStep = false;
	while (AccumulatedTime + GVectorTolerance >= FixedStep && LastStepCount < MaxSubsteps)
	{
		// live property 변경이 다음 solver step에 바로 들어가도록 매 step 전에 반영
		ApplyRuntimeConfig(RuntimeConfig);
		ApplyTurbulenceAcceleration(RuntimeConfig);

		if (!SimulateStep(FixedStep))
		{
			return false;
		}

		AccumulatedTime -= FixedStep;
		SimulationTime += FixedStep;
		++LastStepCount;
		bSimulatedAnyStep = true;
	}

	if (!bSimulatedAnyStep)
	{
		return false;
	}

	return ReadCurrentPositions(OutPositionsComponentLocal);
}

void FClothSimulation::ApplyRuntimeConfig(const FClothSimulationRuntimeConfig& RuntimeConfig)
{
#if WITH_NV_CLOTH
	if (!Impl || !Impl->Cloth)
	{
		return;
	}

	const float Damping = ClampFloat(RuntimeConfig.Damping, 0.0f, 1.0f);
	const float Stiffness = ClampFloat(RuntimeConfig.Stiffness, 0.0f, 1.0f);
	const float FixedStep = ClampFloat(RuntimeConfig.Timestep.FixedTimeStep, GMinFixedStep, GMaxFixedStep);
	const float SolverFrequency = (std::max)(1.0f, 1.0f / FixedStep);

	// component local simulation 공간 기준 중력과 solver parameter 갱신
	Impl->Cloth->setGravity(ToPxVec3(RuntimeConfig.GravityAccelerationComponentLocal));
	Impl->Cloth->setDamping(physx::PxVec3(Damping, Damping, Damping));
	Impl->Cloth->setSolverFrequency(SolverFrequency);
	Impl->Cloth->setStiffnessFrequency(SolverFrequency);
	Impl->Cloth->setTetherConstraintStiffness(Stiffness);
	Impl->Cloth->setTetherConstraintScale(Stiffness > 0.0f ? 1.0f : 0.0f);

	if (Impl->Fabric)
	{
		const uint32 PhaseCount = Impl->Fabric->getNumPhases();
		TArray<nv::cloth::PhaseConfig> PhaseConfigs;
		PhaseConfigs.reserve(PhaseCount);

		for (uint32 PhaseIndex = 0; PhaseIndex < PhaseCount; ++PhaseIndex)
		{
			nv::cloth::PhaseConfig PhaseConfig(static_cast<uint16_t>(PhaseIndex));
			PhaseConfig.mStiffness = Stiffness;
			PhaseConfig.mStiffnessMultiplier = 1.0f;
			PhaseConfig.mCompressionLimit = 1.0f;
			PhaseConfig.mStretchLimit = 1.0f;
			PhaseConfigs.push_back(PhaseConfig);
		}

		if (!PhaseConfigs.empty())
		{
			Impl->Cloth->setPhaseConfig(
				nv::cloth::Range<const nv::cloth::PhaseConfig>(
					PhaseConfigs.data(),
					PhaseConfigs.data() + PhaseConfigs.size()));
		}
	}

	if (RuntimeConfig.Wind.bEnabled && RuntimeConfig.Wind.Strength > 0.0f)
	{
		const FVector WindDirection = RuntimeConfig.Wind.Direction.GetSafeNormal(GVectorTolerance, FVector::ForwardVector);
		const FVector WindVelocity = WindDirection * RuntimeConfig.Wind.Strength;
		Impl->Cloth->setWindVelocity(ToPxVec3(WindVelocity));
		Impl->Cloth->setDragCoefficient(0.5f);
		Impl->Cloth->setLiftCoefficient(0.05f);
		Impl->Cloth->setFluidDensity(1.0f);
	}
	else
	{
		Impl->Cloth->setWindVelocity(physx::PxVec3(0.0f, 0.0f, 0.0f));
		Impl->Cloth->setDragCoefficient(0.0f);
		Impl->Cloth->setLiftCoefficient(0.0f);
	}

	if (RuntimeConfig.SelfCollision.bEnabled && RuntimeConfig.SelfCollision.Distance > 0.0f)
	{
		Impl->Cloth->setSelfCollisionDistance((std::max)(0.0f, RuntimeConfig.SelfCollision.Distance));
		Impl->Cloth->setSelfCollisionStiffness(ClampFloat(RuntimeConfig.SelfCollision.Stiffness, 0.0f, 1.0f));
		// NvCloth 1.1 public header에는 self collision cull scale setter가 없어서 distance/stiffness만 반영
	}
	else
	{
		Impl->Cloth->setSelfCollisionDistance(0.0f);
		Impl->Cloth->setSelfCollisionStiffness(0.0f);
	}
#else
	(void)RuntimeConfig;
#endif
}

void FClothSimulation::ApplyTurbulenceAcceleration(const FClothSimulationRuntimeConfig& RuntimeConfig)
{
#if WITH_NV_CLOTH
	if (!Impl || !Impl->Cloth)
	{
		return;
	}

	if (!RuntimeConfig.Wind.bEnabled || RuntimeConfig.Wind.TurbulenceStrength <= 0.0f)
	{
		Impl->Cloth->clearParticleAccelerations();
		return;
	}

	nv::cloth::Range<physx::PxVec4> Accelerations = Impl->Cloth->getParticleAccelerations();
	if (Accelerations.size() < ParticleCount)
	{
		// backend별 particle acceleration range가 제공되지 않으면 turbulence만 안전하게 비활성화
		Impl->Cloth->clearParticleAccelerations();
		return;
	}

	const nv::cloth::MappedRange<const physx::PxVec4> Particles = nv::cloth::readCurrentParticles(*Impl->Cloth);
	if (Particles.size() < ParticleCount)
	{
		Impl->Cloth->clearParticleAccelerations();
		return;
	}

	const float SpatialScale = (std::max)(1.0e-3f, RuntimeConfig.Wind.TurbulenceSpatialScale);
	const float TemporalScale = (std::max)(0.0f, RuntimeConfig.Wind.TurbulenceTemporalScale);
	const float Strength = (std::max)(0.0f, RuntimeConfig.Wind.TurbulenceStrength);
	const float Seed = static_cast<float>(RuntimeConfig.Wind.TurbulenceSeed) * 0.017f;
	const float TimePhase = SimulationTime * TemporalScale;

	for (uint32 ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex)
	{
		const FVector Position = ToFVectorPosition(Particles[ParticleIndex]);
		const float BasePhase = (Position.X * 0.071f + Position.Y * 0.113f + Position.Z * 0.173f) / SpatialScale + TimePhase + Seed;

		// 단순 deterministic noise 기반 particle별 turbulence acceleration
		const FVector Noise(
			static_cast<float>(std::sin(BasePhase)),
			static_cast<float>(std::sin(BasePhase * 1.37f + 2.11f)),
			static_cast<float>(std::sin(BasePhase * 1.91f + 4.23f)));
		const FVector Acceleration = Noise * Strength;
		Accelerations[ParticleIndex] = physx::PxVec4(Acceleration.X, Acceleration.Y, Acceleration.Z, 0.0f);
	}
#else
	(void)RuntimeConfig;
#endif
}

bool FClothSimulation::SimulateStep(float FixedStep)
{
#if WITH_NV_CLOTH
	if (!Impl || !Impl->Solver)
	{
		LastFailureDetail = "NvCloth solver is null";
		return false;
	}

	if (!Impl->Solver->beginSimulation(FixedStep))
	{
		return false;
	}

	const int ChunkCount = Impl->Solver->getSimulationChunkCount();
	for (int ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
	{
		Impl->Solver->simulateChunk(ChunkIndex);
	}

	Impl->Solver->endSimulation();
	if (Impl->Solver->hasError())
	{
		bValid = false;
		LastFailureDetail = "NvCloth solver reported an unrecoverable error";
		return false;
	}

	return true;
#else
	(void)FixedStep;
	LastFailureDetail = "WITH_NV_CLOTH is disabled";
	return false;
#endif
}

bool FClothSimulation::ReadCurrentPositions(TArray<FVector>& OutPositionsComponentLocal)
{
	OutPositionsComponentLocal.clear();

#if WITH_NV_CLOTH
	if (!Impl || !Impl->Cloth)
	{
		LastFailureDetail = "NvCloth cloth instance is null";
		return false;
	}

	const nv::cloth::MappedRange<const physx::PxVec4> Particles = nv::cloth::readCurrentParticles(*Impl->Cloth);
	if (Particles.size() < ParticleCount)
	{
		LastFailureDetail = "current particle range is smaller than simulation particle count";
		return false;
	}

	OutPositionsComponentLocal.reserve(ParticleCount);
	for (uint32 ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex)
	{
		OutPositionsComponentLocal.push_back(ToFVectorPosition(Particles[ParticleIndex]));
	}

	return true;
#else
	LastFailureDetail = "WITH_NV_CLOTH is disabled";
	return false;
#endif
}

bool FClothSimulation::IsSimulationAvailable() const
{
	return bInitialized && bValid && Context && Context->GetBackendStatus().bAvailable;
}

const FClothBackendStatus& FClothSimulation::GetBackendStatus() const
{
	static const FClothBackendStatus UnavailableStatus;

	if (!Context)
	{
		return UnavailableStatus;
	}

	return Context->GetBackendStatus();
}

bool FClothSimulation::SetBuildFailure(const FString& FailureDetail)
{
	LastFailureDetail = FailureDetail;
	bInitialized = false;
	bValid = false;
	ParticleCount = 0;
	IndexCount = 0;
	PinnedCount = 0;
	AccumulatedTime = 0.0f;
	SimulationTime = 0.0f;
	LastStepCount = 0;
	return false;
}
