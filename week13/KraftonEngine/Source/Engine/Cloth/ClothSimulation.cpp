#include "Cloth/ClothSimulation.h"

#include "Cloth/ClothBuildConfig.h"
#include "Cloth/NvClothContext.h"
#include "Core/Logging/Log.h"

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
}

void FClothSimulation::Tick(float DeltaTime)
{
	(void)DeltaTime;

	// commit 3 범위: 실제 solver 진행 없이 resource 연결 상태만 유지
	if (!IsSimulationAvailable())
	{
		return;
	}
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
	return false;
}
