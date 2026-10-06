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
constexpr uint32 GMaxNvClothConvexPlanes = 32;

/**
 * @brief 유효한 고유 hard pin particle 수를 계산합니다
 *
 * @param PinnedIndices hard pin으로 선택된 particle index 배열
 *
 * @param ParticleCount simulation particle 수
 *
 * @return 유효한 고유 hard pin particle 수
 */
uint32 CountUniqueValidPinnedParticles(const TArray<uint32>& PinnedIndices, uint32 ParticleCount)
{
	if (ParticleCount == 0 || PinnedIndices.empty())
	{
		return 0;
	}

	TArray<uint8> PinMask;
	PinMask.resize(ParticleCount, 0);

	uint32 UniquePinnedCount = 0;
	for (uint32 ParticleIndex : PinnedIndices)
	{
		if (ParticleIndex >= ParticleCount)
		{
			continue;
		}

		if (PinMask[ParticleIndex] != 0)
		{
			continue;
		}

		// 중복 pin index를 제외한 실제 hard pin 개수
		PinMask[ParticleIndex] = 1;
		++UniquePinnedCount;
	}

	return UniquePinnedCount;
}

/**
 * @brief 움직일 수 있는 particle inverse mass 존재 여부를 반환합니다
 *
 * @param InvMasses particle inverse mass 배열
 *
 * @return dynamic particle inverse mass 존재 여부
 */
bool HasDynamicParticleInvMass(const TArray<float>& InvMasses)
{
	if (InvMasses.empty())
	{
		return true;
	}

	for (float InvMass : InvMasses)
	{
		if (std::isfinite(InvMass) && InvMass > GVectorTolerance)
		{
			return true;
		}
	}

	return false;
}

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
 * @brief engine quaternion을 physx quaternion으로 변환합니다
 *
 * @param Quat 변환할 engine quaternion
 *
 * @return 변환된 physx quaternion
 */
physx::PxQuat ToPxQuat(const FQuat& Quat)
{
	FQuat NormalizedQuat = Quat;
	NormalizedQuat.Normalize();
	return physx::PxQuat(
		NormalizedQuat.X,
		NormalizedQuat.Y,
		NormalizedQuat.Z,
		NormalizedQuat.W);
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

/**
 * @brief engine sphere 값을 NvCloth sphere 값으로 변환합니다
 *
 * @param Center component local 기준 sphere 중심
 *
 * @param Radius sphere 반지름
 *
 * @return NvCloth sphere 값
 */
physx::PxVec4 ToPxSphere(const FVector& Center, float Radius)
{
	return physx::PxVec4(Center.X, Center.Y, Center.Z, Radius);
}

/**
 * @brief engine plane 값을 NvCloth plane 값으로 변환합니다
 *
 * @param Normal component local 기준 단위 plane normal
 *
 * @param Distance normal dot point 형식의 plane 거리
 *
 * @return NvCloth plane 값
 */
physx::PxVec4 ToPxPlane(const FVector& Normal, float Distance)
{
	// NvCloth plane은 ax + by + cz + d = 0 형식이므로 d는 -normal dot point
	return physx::PxVec4(Normal.X, Normal.Y, Normal.Z, -Distance);
}

/**
 * @brief std vector 기반 배열을 NvCloth range로 변환합니다
 *
 * @param Values range로 전달할 값 배열
 *
 * @return NvCloth const range
 */
template <typename ValueType>
nv::cloth::Range<const ValueType> MakeNvConstRange(const TArray<ValueType>& Values)
{
	if (Values.empty())
	{
		return nv::cloth::Range<const ValueType>();
	}

	return nv::cloth::Range<const ValueType>(Values.data(), Values.data() + Values.size());
}

/**
 * @brief plane 배열에 convex mask bit를 함께 추가합니다
 *
 * @param Normal component local 기준 단위 plane normal
 *
 * @param Distance normal dot point 형식의 plane 거리
 *
 * @param OutPlanes NvCloth plane 배열
 *
 * @param OutMask convex mask 값
 *
 * @return plane 추가 성공 여부
 */
bool AppendCollisionPlane(
	const FVector& Normal,
	float Distance,
	TArray<physx::PxVec4>& OutPlanes,
	uint32& OutMask)
{
	if (OutPlanes.size() >= GMaxNvClothConvexPlanes)
	{
		return false;
	}

	const uint32 PlaneIndex = static_cast<uint32>(OutPlanes.size());
	OutPlanes.push_back(ToPxPlane(Normal, Distance));
	OutMask |= (1u << PlaneIndex);
	return true;
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

	const uint32 BuildParticleCount = static_cast<uint32>(BuildDesc.InitialPositionsComponentLocal.size());
	const uint32 BuildIndexCount = static_cast<uint32>(BuildDesc.Indices.size());
	const uint32 BuildPinnedCount = CountUniqueValidPinnedParticles(BuildDesc.PinnedIndices, BuildParticleCount);
	if (!HasDynamicParticleInvMass(BuildDesc.InvMasses))
	{
		// NvCloth fabric cooker는 모든 particle inverse mass가 0인 입력에서 빈 내부 배열을 참조할 수 있음
		ParticleCount = BuildParticleCount;
		IndexCount = BuildIndexCount;
		PinnedCount = BuildPinnedCount > 0 ? BuildPinnedCount : BuildParticleCount;
		LastFailureDetail = "NvCloth fabric cooking skipped because all particles have zero inverse mass";
		UE_LOG("[ClothSimulation] Resource skipped: particles=%u indices=%u pinned=%u reason=all particles have zero inverse mass",
			ParticleCount,
			IndexCount,
			PinnedCount);
		return false;
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

	// cloth instance를 solver에 등록해 이후 fixed timestep simulation에서 처리
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

bool FClothSimulation::UpdateCollisionPrimitives(const TArray<FClothCollisionPrimitive>& CollisionPrimitives)
{
	CollisionPrimitiveCount = 0;
	LastFailureDetail.clear();

	if (!IsSimulationAvailable())
	{
		LastFailureDetail = "cloth simulation is unavailable for collision update";
		return false;
	}

#if WITH_NV_CLOTH
	if (!Impl || !Impl->Cloth)
	{
		LastFailureDetail = "NvCloth cloth instance is null";
		return false;
	}

	TArray<physx::PxVec4> Spheres;
	TArray<uint32_t> Capsules;
	TArray<physx::PxVec4> Planes;
	TArray<uint32_t> ConvexMasks;
	uint32 AppliedPrimitiveCount = 0;

	for (const FClothCollisionPrimitive& Primitive : CollisionPrimitives)
	{
		switch (Primitive.Type)
		{
		case EClothCollisionPrimitiveType::Sphere:
		{
			const float Radius = (std::max)(0.0f, Primitive.Radius);
			if (Radius <= GVectorTolerance)
			{
				break;
			}

			// NvCloth sphere는 center xyz와 radius w를 한 값으로 전달
			Spheres.push_back(ToPxSphere(Primitive.Center, Radius));
			++AppliedPrimitiveCount;
			break;
		}

		case EClothCollisionPrimitiveType::Capsule:
		{
			const float Radius = (std::max)(0.0f, Primitive.Radius);
			if (Radius <= GVectorTolerance)
			{
				break;
			}

			const FVector Segment = Primitive.CapsuleEnd - Primitive.CapsuleStart;
			if (Segment.Length() <= GVectorTolerance)
			{
				// 축 길이가 없는 capsule은 sphere collision으로 안전하게 축소
				Spheres.push_back(ToPxSphere(Primitive.Center, Radius));
				++AppliedPrimitiveCount;
				break;
			}

			const uint32 FirstSphereIndex = static_cast<uint32>(Spheres.size());
			Spheres.push_back(ToPxSphere(Primitive.CapsuleStart, Radius));
			Spheres.push_back(ToPxSphere(Primitive.CapsuleEnd, Radius));
			Capsules.push_back(FirstSphereIndex);
			Capsules.push_back(FirstSphereIndex + 1);
			++AppliedPrimitiveCount;
			break;
		}

		case EClothCollisionPrimitiveType::Plane:
		{
			const FVector PlaneNormal = Primitive.PlaneNormal.GetSafeNormal(GVectorTolerance, FVector::UpVector);
			const float PlaneDistance = PlaneNormal.Dot(Primitive.PlanePoint);
			uint32 PlaneMask = 0;
			if (!AppendCollisionPlane(PlaneNormal, PlaneDistance, Planes, PlaneMask))
			{
				ClearCollisionPrimitives();
				LastFailureDetail = "NvCloth supports at most 32 collision planes per convex mask set";
				return false;
			}

			// 단일 plane도 convex mask를 통해 활성화해야 NvCloth collision에 반영됨
			ConvexMasks.push_back(PlaneMask);
			++AppliedPrimitiveCount;
			break;
		}

		case EClothCollisionPrimitiveType::Box:
		{
			const FVector AxisX = Primitive.BoxAxisX.GetSafeNormal(GVectorTolerance, FVector::XAxisVector);
			const FVector AxisY = Primitive.BoxAxisY.GetSafeNormal(GVectorTolerance, FVector::YAxisVector);
			const FVector AxisZ = Primitive.BoxAxisZ.GetSafeNormal(GVectorTolerance, FVector::ZAxisVector);
			const FVector Extent = Primitive.BoxExtent.GetAbs();
			if (Extent.X <= GVectorTolerance || Extent.Y <= GVectorTolerance || Extent.Z <= GVectorTolerance)
			{
				break;
			}

			const size_t PlaneStartCount = Planes.size();
			const bool bHasBoxPlaneBudget = PlaneStartCount + 6 <= GMaxNvClothConvexPlanes;
			uint32 BoxMask = 0;
			bool bAppendedBoxPlanes = false;
			if (bHasBoxPlaneBudget)
			{
				bAppendedBoxPlanes =
					AppendCollisionPlane(AxisX, AxisX.Dot(Primitive.Center + AxisX * Extent.X), Planes, BoxMask)
					&& AppendCollisionPlane(-AxisX, (-AxisX).Dot(Primitive.Center - AxisX * Extent.X), Planes, BoxMask)
					&& AppendCollisionPlane(AxisY, AxisY.Dot(Primitive.Center + AxisY * Extent.Y), Planes, BoxMask)
					&& AppendCollisionPlane(-AxisY, (-AxisY).Dot(Primitive.Center - AxisY * Extent.Y), Planes, BoxMask)
					&& AppendCollisionPlane(AxisZ, AxisZ.Dot(Primitive.Center + AxisZ * Extent.Z), Planes, BoxMask)
					&& AppendCollisionPlane(-AxisZ, (-AxisZ).Dot(Primitive.Center - AxisZ * Extent.Z), Planes, BoxMask);
			}

			if (bAppendedBoxPlanes)
			{
				// NvCloth에는 box 직접 primitive가 없어서 6개 plane을 하나의 convex로 묶음
				ConvexMasks.push_back(BoxMask);
			}
			else
			{
				// plane 예산을 넘는 box는 전체 collision update 실패 대신 보수적인 sphere로 축소
				Planes.resize(PlaneStartCount);
				const float FallbackRadius = Extent.Length();
				if (FallbackRadius <= GVectorTolerance)
				{
					break;
				}

				Spheres.push_back(ToPxSphere(Primitive.Center, FallbackRadius));
			}

			++AppliedPrimitiveCount;
			break;
		}
		}
	}

	// 기존 collision이 stale 상태로 남지 않도록 매 update마다 전체 range를 교체
	ClearCollisionPrimitives();
	Impl->Cloth->setSpheres(MakeNvConstRange(Spheres), 0, 0);
	Impl->Cloth->setCapsules(MakeNvConstRange(Capsules), 0, 0);
	Impl->Cloth->setPlanes(MakeNvConstRange(Planes), 0, 0);
	Impl->Cloth->setConvexes(MakeNvConstRange(ConvexMasks), 0, 0);

	CollisionPrimitiveCount = AppliedPrimitiveCount;
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
	CollisionPrimitiveCount = 0;
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

	if (AccumulatedTime + GVectorTolerance < FixedStep)
	{
		// fixed step이 없는 frame은 NvCloth local-space frame을 소비하지 않고 누적 이동량 보존
		return false;
	}

	// 실제 solver step 직전에만 최신 world transform을 NvCloth local-space motion으로 반영
	ApplyLocalSpaceMotion(RuntimeConfig.LocalSpaceMotion);
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

	// NvCloth global gravity와 solver parameter 갱신
	Impl->Cloth->setGravity(ToPxVec3(RuntimeConfig.GravityAccelerationWorld));
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
		Impl->Cloth->setDragCoefficient(ClampFloat(RuntimeConfig.Wind.DragCoefficient, 0.0f, 10.0f));
		Impl->Cloth->setLiftCoefficient(ClampFloat(RuntimeConfig.Wind.LiftCoefficient, 0.0f, 10.0f));
		Impl->Cloth->setFluidDensity(ClampFloat(RuntimeConfig.Wind.FluidDensity, 0.0f, 10.0f));
	}
	else
	{
		Impl->Cloth->setWindVelocity(physx::PxVec3(0.0f, 0.0f, 0.0f));
		Impl->Cloth->setDragCoefficient(0.0f);
		Impl->Cloth->setLiftCoefficient(0.0f);
		Impl->Cloth->setFluidDensity(0.0f);
	}

	if (RuntimeConfig.SelfCollision.bEnabled && RuntimeConfig.SelfCollision.Distance > 0.0f)
	{
		Impl->Cloth->setSelfCollisionDistance((std::max)(0.0f, RuntimeConfig.SelfCollision.Distance));
		Impl->Cloth->setSelfCollisionStiffness(ClampFloat(RuntimeConfig.SelfCollision.Stiffness, 0.0f, 1.0f));
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

void FClothSimulation::ApplyLocalSpaceMotion(const FClothLocalSpaceMotionConfig& MotionConfig)
{
#if WITH_NV_CLOTH
	if (!Impl || !Impl->Cloth)
	{
		return;
	}

	const physx::PxVec3 CurrentTranslation = ToPxVec3(MotionConfig.CurrentWorldTransform.Location);
	const physx::PxQuat CurrentRotation = ToPxQuat(MotionConfig.CurrentWorldTransform.Rotation);

	auto SetZeroInertia = [this]()
	{
		Impl->Cloth->setLinearInertia(physx::PxVec3(0.0f, 0.0f, 0.0f));
		Impl->Cloth->setAngularInertia(physx::PxVec3(0.0f, 0.0f, 0.0f));
		Impl->Cloth->setCentrifugalInertia(physx::PxVec3(0.0f, 0.0f, 0.0f));
	};

	if (!MotionConfig.bEnabled)
	{
		// 비활성 상태에서도 frame transform은 최신값으로 유지해 재활성화 순간의 큰 delta 방지
		SetZeroInertia();
		Impl->Cloth->setTranslation(CurrentTranslation);
		Impl->Cloth->setRotation(CurrentRotation);
		Impl->Cloth->clearInertia();
		return;
	}

	if (!MotionConfig.bHasPreviousTransform || MotionConfig.bTeleport)
	{
		// 최초 frame 또는 teleport는 흔들림 없이 local-space frame만 이동
		SetZeroInertia();
		Impl->Cloth->teleportToLocation(CurrentTranslation, CurrentRotation);
		Impl->Cloth->clearInertia();
		return;
	}

	// 1.0 초과 값은 실제 물리보다 강한 게임용 local-space 관성 반응
	constexpr float MaxOwnerMotionInertiaResponse = 3.0f;
	const float LinearInertia = ClampFloat(MotionConfig.LinearInertia, 0.0f, MaxOwnerMotionInertiaResponse);
	const float AngularInertia = ClampFloat(MotionConfig.AngularInertia, 0.0f, MaxOwnerMotionInertiaResponse);
	const float CentrifugalInertia = ClampFloat(MotionConfig.CentrifugalInertia, 0.0f, MaxOwnerMotionInertiaResponse);

	// NvCloth가 이전 local-space frame과 현재 frame 차이로 관성 force를 계산
	Impl->Cloth->setLinearInertia(physx::PxVec3(LinearInertia, LinearInertia, LinearInertia));
	Impl->Cloth->setAngularInertia(physx::PxVec3(AngularInertia, AngularInertia, AngularInertia));
	Impl->Cloth->setCentrifugalInertia(physx::PxVec3(CentrifugalInertia, CentrifugalInertia, CentrifugalInertia));
	Impl->Cloth->setTranslation(CurrentTranslation);
	Impl->Cloth->setRotation(CurrentRotation);
#else
	(void)MotionConfig;
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

bool FClothSimulation::ClearCollisionPrimitives()
{
	CollisionPrimitiveCount = 0;

#if WITH_NV_CLOTH
	if (!Impl || !Impl->Cloth)
	{
		LastFailureDetail = "NvCloth cloth instance is null";
		return false;
	}

	// capsule/convex가 sphere/plane index를 참조하므로 참조 primitive를 지우기 전에 먼저 제거
	Impl->Cloth->setCapsules(nv::cloth::Range<const uint32_t>(), 0, Impl->Cloth->getNumCapsules());
	Impl->Cloth->setConvexes(nv::cloth::Range<const uint32_t>(), 0, Impl->Cloth->getNumConvexes());
	Impl->Cloth->setPlanes(nv::cloth::Range<const physx::PxVec4>(), 0, Impl->Cloth->getNumPlanes());
	Impl->Cloth->setSpheres(nv::cloth::Range<const physx::PxVec4>(), 0, Impl->Cloth->getNumSpheres());
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

void FClothSimulation::ResetAccumulator()
{
	AccumulatedTime = 0.0f;
	LastStepCount = 0;
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
	CollisionPrimitiveCount = 0;
	AccumulatedTime = 0.0f;
	SimulationTime = 0.0f;
	LastStepCount = 0;
	return false;
}
