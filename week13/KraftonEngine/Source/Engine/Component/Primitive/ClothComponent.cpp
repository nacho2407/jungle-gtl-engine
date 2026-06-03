#include "Component/Primitive/ClothComponent.h"

#include "Component/PrimitiveComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialManager.h"
#include "Object/GarbageCollection.h"
#include "Profiling/Stats/ClothStats.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Proxy/ClothSceneProxy.h"
#include "Core/Logging/Log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace
{
	constexpr int32 GMinClothParticlesPerAxis = 2;
	constexpr int32 GMaxClothParticlesPerAxis = 256;
	constexpr float GDefaultParticleSpacing = 10.0f;
	constexpr float GNormalTolerance = 1.0e-6f;
	constexpr float GTangentTolerance = 1.0e-6f;
	constexpr float GMinFixedTimeStep = 0.001f;
	constexpr float GMaxFixedTimeStep = 0.1f;
	constexpr int32 GMinClothSubsteps = 1;
	constexpr int32 GMaxClothSubsteps = 4;
	constexpr float GMinAccumulatedTime = 0.016f;
	constexpr float GMaxAccumulatedTime = 1.0f;
	constexpr float GDefaultGravityAcceleration = 980.0f;
	constexpr float GMaxGravityScale = 10.0f;
	constexpr float GMaxWindStrength = 10000.0f;
	constexpr float GMaxSelfCollisionDistance = 1000.0f;
	constexpr float GMaxCollisionLength = 10000.0f;
	constexpr float GMinBoxCollisionExtent = 0.001f;
	constexpr int32 GMinBodyCollisionPrimitiveCount = 0;
	constexpr int32 GMaxBodyCollisionPrimitiveCount = 128;

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
	 * @brief 두 world rotation 사이의 최단 회전 각도를 degree로 반환합니다
	 *
	 * @param PreviousRotation 이전 world rotation
	 *
	 * @param CurrentRotation 현재 world rotation
	 *
	 * @return 두 rotation 사이의 최단 각도
	 */
	float ComputeRotationDeltaDegrees(const FQuat& PreviousRotation, const FQuat& CurrentRotation)
	{
		const FQuat SafePreviousRotation = PreviousRotation.GetNormalized();
		const FQuat SafeCurrentRotation = CurrentRotation.GetNormalized();
		const float RotationDot = std::abs(
			SafePreviousRotation.X * SafeCurrentRotation.X
			+ SafePreviousRotation.Y * SafeCurrentRotation.Y
			+ SafePreviousRotation.Z * SafeCurrentRotation.Z
			+ SafePreviousRotation.W * SafeCurrentRotation.W);
		const float ClampedDot = ClampFloat(RotationDot, 0.0f, 1.0f);
		return 2.0f * std::acos(ClampedDot) * RAD_TO_DEG;
	}

	/**
	 * @brief 유한한 양수 spacing 값을 반환합니다
	 *
	 * @param Value 보정할 spacing 값
	 *
	 * @return 보정된 spacing 값
	 */
	float SanitizeSpacing(float Value)
	{
		if (!std::isfinite(Value) || Value <= 0.0f)
		{
			return GDefaultParticleSpacing;
		}

		return Value;
	}

	/**
	 * @brief property 이름과 표시 이름 중 하나라도 일치하는지 반환합니다
	 *
	 * @param PropertyName 변경된 property 이름
	 *
	 * @param InternalName c++ 멤버 이름
	 *
	 * @param DisplayName editor 표시 이름
	 *
	 * @return property 이름 일치 여부
	 */
	bool MatchesPropertyName(const char* PropertyName, const char* InternalName, const char* DisplayName = nullptr)
	{
		if (std::strcmp(PropertyName, InternalName) == 0)
		{
			return true;
		}

		return DisplayName && std::strcmp(PropertyName, DisplayName) == 0;
	}

	/**
	 * @brief cloth topology property인지 반환합니다
	 */
	bool IsClothTopologyProperty(const char* PropertyName)
	{
		return MatchesPropertyName(PropertyName, "NumParticlesX", "Num Particles X")
			|| MatchesPropertyName(PropertyName, "NumParticlesY", "Num Particles Y")
			|| MatchesPropertyName(PropertyName, "ParticleSpacing", "Particle Spacing");
	}

	/**
	 * @brief cloth material property인지 반환합니다
	 */
	bool IsClothMaterialProperty(const char* PropertyName)
	{
		return MatchesPropertyName(PropertyName, "MaterialSlot", "Material")
			|| MatchesPropertyName(PropertyName, "Material", "Material");
	}

	/**
	 * @brief cloth simulation lifecycle property인지 반환합니다
	 */
	bool IsClothSimulationLifecycleProperty(const char* PropertyName)
	{
		return MatchesPropertyName(PropertyName, "bEnableSimulation", "Enable Simulation");
	}

	/**
	 * @brief cloth timestep property인지 반환합니다
	 */
	bool IsClothTimestepProperty(const char* PropertyName)
	{
		return MatchesPropertyName(PropertyName, "FixedTimeStep", "Fixed Time Step")
			|| MatchesPropertyName(PropertyName, "MaxSubsteps", "Max Substeps")
			|| MatchesPropertyName(PropertyName, "MaxAccumulatedTime", "Max Accumulated Time");
	}

	/**
	 * @brief cloth pin selection property인지 반환합니다
	 */
	bool IsClothPinSelectionProperty(const char* PropertyName)
	{
		return MatchesPropertyName(PropertyName, "PinningMode", "Pinning Mode")
			|| MatchesPropertyName(PropertyName, "PinCenterActorLocal", "Pin Center Actor Local")
			|| MatchesPropertyName(PropertyName, "PinRadius", "Pin Radius")
			|| MatchesPropertyName(PropertyName, "PinBoxExtentActorLocal", "Pin Box Extent Actor Local")
			|| MatchesPropertyName(PropertyName, "PinRectMinActorLocalXZ", "Pin Rect Min Actor Local XZ")
			|| MatchesPropertyName(PropertyName, "PinRectMaxActorLocalXZ", "Pin Rect Max Actor Local XZ");
	}

	/**
	 * @brief cloth pin target property인지 반환합니다
	 */
	bool IsClothPinTargetProperty(const char* PropertyName)
	{
		return MatchesPropertyName(PropertyName, "PinOffsetActorLocal", "Pin Offset Actor Local");
	}

	/**
	 * @brief cloth force property인지 반환합니다
	 */
	bool IsClothForceProperty(const char* PropertyName)
	{
		return MatchesPropertyName(PropertyName, "GravityScale", "Gravity Scale")
			|| MatchesPropertyName(PropertyName, "Damping", "Damping")
			|| MatchesPropertyName(PropertyName, "Stiffness", "Stiffness")
			|| MatchesPropertyName(PropertyName, "bEnableWind", "Enable Wind")
			|| MatchesPropertyName(PropertyName, "bUseGlobalWind", "Use Global Wind")
			|| MatchesPropertyName(PropertyName, "GlobalWindResponse", "Global Wind Response")
			|| MatchesPropertyName(PropertyName, "LocalWindScale", "Local Wind Scale")
			|| MatchesPropertyName(PropertyName, "TurbulenceResponse", "Turbulence Response")
			|| MatchesPropertyName(PropertyName, "WindDirection", "Wind Direction")
			|| MatchesPropertyName(PropertyName, "WindStrength", "Wind Strength")
			|| MatchesPropertyName(PropertyName, "WindTurbulenceStrength", "Wind Turbulence Strength")
			|| MatchesPropertyName(PropertyName, "WindTurbulenceSpatialScale", "Wind Turbulence Spatial Scale")
			|| MatchesPropertyName(PropertyName, "WindTurbulenceTemporalScale", "Wind Turbulence Temporal Scale")
			|| MatchesPropertyName(PropertyName, "WindTurbulenceSeed", "Wind Turbulence Seed")
			|| MatchesPropertyName(PropertyName, "WindDragCoefficient", "Wind Drag Coefficient")
			|| MatchesPropertyName(PropertyName, "WindLiftCoefficient", "Wind Lift Coefficient")
			|| MatchesPropertyName(PropertyName, "WindFluidDensity", "Wind Fluid Density")
			|| MatchesPropertyName(PropertyName, "bEnableOwnerMotionInertia", "Enable Owner Motion Inertia")
			|| MatchesPropertyName(PropertyName, "OwnerLinearInertiaResponse", "Linear Inertia Response")
			|| MatchesPropertyName(PropertyName, "OwnerAngularInertiaResponse", "Angular Inertia Response")
			|| MatchesPropertyName(PropertyName, "OwnerCentrifugalInertiaResponse", "Centrifugal Inertia Response")
			|| MatchesPropertyName(PropertyName, "OwnerMotionTeleportDistance", "Owner Motion Teleport Distance")
			|| MatchesPropertyName(PropertyName, "OwnerMotionTeleportAngleDegrees", "Owner Motion Teleport Angle")
			|| MatchesPropertyName(PropertyName, "bEnableSelfCollision", "Enable Self Collision")
			|| MatchesPropertyName(PropertyName, "SelfCollisionDistance", "Self Collision Distance")
			|| MatchesPropertyName(PropertyName, "SelfCollisionStiffness", "Self Collision Stiffness")
			|| MatchesPropertyName(PropertyName, "SelfCollisionCullScale", "Self Collision Cull Scale");
	}

	/**
	 * @brief cloth collision property인지 반환합니다
	 */
	bool IsClothCollisionProperty(const char* PropertyName)
	{
		return MatchesPropertyName(PropertyName, "bEnableSphereCollision", "Enable Sphere Collision")
			|| MatchesPropertyName(PropertyName, "SphereCenterActorLocal", "Sphere Center Actor Local")
			|| MatchesPropertyName(PropertyName, "SphereRadius", "Sphere Radius")
			|| MatchesPropertyName(PropertyName, "bEnablePlaneCollision", "Enable Plane Collision")
			|| MatchesPropertyName(PropertyName, "PlanePointActorLocal", "Plane Point Actor Local")
			|| MatchesPropertyName(PropertyName, "PlaneNormalActorLocal", "Plane Normal Actor Local")
			|| MatchesPropertyName(PropertyName, "bEnableCapsuleCollision", "Enable Capsule Collision")
			|| MatchesPropertyName(PropertyName, "CapsuleCenterActorLocal", "Capsule Center Actor Local")
			|| MatchesPropertyName(PropertyName, "CapsuleAxisActorLocal", "Capsule Axis Actor Local")
			|| MatchesPropertyName(PropertyName, "CapsuleRadius", "Capsule Radius")
			|| MatchesPropertyName(PropertyName, "CapsuleHalfHeight", "Capsule Half Height")
			|| MatchesPropertyName(PropertyName, "bEnableBoxCollision", "Enable Box Collision")
			|| MatchesPropertyName(PropertyName, "BoxCenterActorLocal", "Box Center Actor Local")
			|| MatchesPropertyName(PropertyName, "BoxExtentActorLocal", "Box Extent Actor Local")
			|| MatchesPropertyName(PropertyName, "BoxRotationActorLocal", "Box Rotation Actor Local")
			|| MatchesPropertyName(PropertyName, "bEnableBodyCollision", "Enable Body Collision")
			|| MatchesPropertyName(PropertyName, "bAutoFindOwnerBodyCollision", "Auto Find Owner Body Collision")
			|| MatchesPropertyName(PropertyName, "bAutoFindOwnerSkeletalMeshCollision", "Auto Find Owner Skeletal Mesh")
			|| MatchesPropertyName(PropertyName, "CollisionSourceComponentName", "Collision Source Component Name")
			|| MatchesPropertyName(PropertyName, "MaxBodyCollisionPrimitives", "Max Body Collision Primitives");
	}

	/**
	 * @brief cloth editor preview property인지 반환합니다
	 */
	bool IsClothEditorPreviewProperty(const char* PropertyName)
	{
		return MatchesPropertyName(PropertyName, "bSimulateInEditor", "Simulate In Editor");
	}

	/**
	 * @brief 지정된 particle index를 중복 없이 추가합니다
	 *
	 * @param OutIndices hard pin index 배열
	 *
	 * @param ParticleIndex 추가할 particle index
	 */
	void AddUniquePinnedIndex(TArray<uint32>& OutIndices, uint32 ParticleIndex)
	{
		if (std::find(OutIndices.begin(), OutIndices.end(), ParticleIndex) != OutIndices.end())
		{
			return;
		}

		OutIndices.push_back(ParticleIndex);
	}

	/**
	 * @brief all-pinned 입력으로 simulation resource 생성이 생략된 실패인지 반환합니다
	 *
	 * @param FailureDetail simulation resource 생성 실패 사유
	 *
	 * @return all-pinned guard 실패 여부
	 */
	bool IsAllPinnedBuildFailure(const FString& FailureDetail)
	{
		return FailureDetail.find("zero inverse mass") != FString::npos;
	}
}

UClothComponent::UClothComponent()
{
	PrimaryComponentTick.SetTickGroup(TG_PostPhysics);
	PrimaryComponentTick.SetEndTickGroup(TG_PostPhysics);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEnabled = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

FPrimitiveSceneProxy* UClothComponent::CreateSceneProxy()
{
	ApplyEditorPreviewPolicy();

	// editor viewport에서 tick 전에 proxy가 만들어지는 초기 표시 경로 보장
	RebuildClothIfNeeded();
	return new FClothSceneProxy(this);
}

void UClothComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	LogBackendStatusOnce();
	RebuildClothIfNeeded();

	const FClothConfig Config = MakeClothConfig();
	TickSimulationIfNeeded(DeltaTime, TickType, Config);
}

void UClothComponent::PostEditProperty(const char* PropertyName)
{
	UMeshComponent::PostEditProperty(PropertyName);

	if (!PropertyName)
	{
		return;
	}

	if (IsClothTopologyProperty(PropertyName))
	{
		// 에디터 입력값 보정과 rebuild 병합
		const FClothConfig Config = MakeClothConfig();
		NumParticlesX = Config.NumParticlesX;
		NumParticlesY = Config.NumParticlesY;
		ParticleSpacing = Config.ParticleSpacing;
		MarkClothRebuildDirty();
		return;
	}

	if (IsClothMaterialProperty(PropertyName))
	{
		LoadMaterialFromSlot();
		MarkProxyDirty(EDirtyFlag::Material);
		return;
	}

	if (IsClothSimulationLifecycleProperty(PropertyName))
	{
		// simulation enable 변경은 resource 생명주기 자체를 바꾸므로 rebuild로 처리
		MarkClothSimulationRebuildDirty();
		return;
	}

	if (IsClothTimestepProperty(PropertyName))
	{
		// fixed timestep 값은 fabric rebuild 없이 다음 simulation tick에서 live update
		const FClothConfig Config = MakeClothConfig();
		FixedTimeStep = Config.Timestep.FixedTimeStep;
		MaxSubsteps = Config.Timestep.MaxSubsteps;
		MaxAccumulatedTime = Config.Timestep.MaxAccumulatedTime;
		MarkClothForceDirty();
		return;
	}

	if (IsClothPinSelectionProperty(PropertyName))
	{
		MarkClothPinningDirty();
		return;
	}

	if (IsClothPinTargetProperty(PropertyName))
	{
		MarkClothPinTargetDirty();
		return;
	}

	if (IsClothForceProperty(PropertyName))
	{
		MarkClothForceDirty();
		return;
	}

	if (IsClothCollisionProperty(PropertyName))
	{
		MarkClothCollisionDirty();
		return;
	}

	if (IsClothEditorPreviewProperty(PropertyName))
	{
		MarkClothEditorPreviewDirty();
	}
}

void UClothComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	Simulation.Shutdown();
	SimulationReadbackPositions.clear();
	CachedCollisionPrimitives.clear();
	CachedIndependentCollisionPrimitiveCount = 0;
	CachedBodyCollisionPrimitiveCount = 0;
	ResetOwnerMotionCache();
	LoadMaterialFromSlot();
	MarkClothRebuildDirty();
}

void UClothComponent::RouteComponentDestroyed()
{
	Simulation.Shutdown();
	SimulationReadbackPositions.clear();
	CachedCollisionPrimitives.clear();
	CachedIndependentCollisionPrimitiveCount = 0;
	CachedBodyCollisionPrimitiveCount = 0;
	ResetOwnerMotionCache();
	UMeshComponent::RouteComponentDestroyed();
}

void UClothComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
	UMeshComponent::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(Material, "UClothComponent.Material");
}

FMeshDataView UClothComponent::GetMeshDataView() const
{
	if (!RenderData.IsValid())
	{
		return {};
	}

	FMeshDataView View;
	View.VertexData = RenderData.Vertices.data();
	View.VertexCount = static_cast<uint32>(RenderData.Vertices.size());
	View.Stride = sizeof(FVertexPNCTT);
	View.IndexData = RenderData.Indices.data();
	View.IndexCount = static_cast<uint32>(RenderData.Indices.size());
	return View;
}

void UClothComponent::UpdateWorldAABB() const
{
	if (!bHasValidLocalBounds)
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FVector WorldCenter = WorldMatrix.TransformPositionWithW(CachedLocalCenter);

	const float Ex = std::abs(WorldMatrix.M[0][0]) * CachedLocalExtent.X
		+ std::abs(WorldMatrix.M[1][0]) * CachedLocalExtent.Y
		+ std::abs(WorldMatrix.M[2][0]) * CachedLocalExtent.Z;
	const float Ey = std::abs(WorldMatrix.M[0][1]) * CachedLocalExtent.X
		+ std::abs(WorldMatrix.M[1][1]) * CachedLocalExtent.Y
		+ std::abs(WorldMatrix.M[2][1]) * CachedLocalExtent.Z;
	const float Ez = std::abs(WorldMatrix.M[0][2]) * CachedLocalExtent.X
		+ std::abs(WorldMatrix.M[1][2]) * CachedLocalExtent.Y
		+ std::abs(WorldMatrix.M[2][2]) * CachedLocalExtent.Z;

	WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
	WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void UClothComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
	if (ElementIndex != 0)
	{
		return;
	}

	Material = InMaterial;
	MaterialSlot = Material ? Material->GetAssetPathFileName() : "None";

	// 단일 material slot만 바뀌므로 geometry rebuild 없이 material dirty만 전파
	MarkProxyDirty(EDirtyFlag::Material);
}

UMaterial* UClothComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex != 0)
	{
		return nullptr;
	}

	return Material.Get();
}

void UClothComponent::MarkClothRebuildDirty()
{
	bTopologyRebuildDirty = true;
	MarkClothSimulationRebuildDirty();
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UClothComponent::MarkClothSimulationRebuildDirty()
{
	bSimulationRebuildDirty = true;
	bPinningDirty = true;
	bPinTargetDirty = true;
	bForceConfigDirty = true;
	bCollisionDirty = true;
}

void UClothComponent::MarkClothPinningDirty()
{
	bPinningDirty = true;
	MarkClothPinTargetDirty();
}

void UClothComponent::MarkClothPinTargetDirty()
{
	bPinTargetDirty = true;
}

void UClothComponent::MarkClothForceDirty()
{
	bForceConfigDirty = true;
}

void UClothComponent::MarkClothCollisionDirty()
{
	bCollisionDirty = true;
}

void UClothComponent::MarkClothEditorPreviewDirty()
{
	bEditorPreviewDirty = true;
	Simulation.ResetAccumulator();
	ResetOwnerMotionCache();
	ApplyEditorPreviewPolicy();
}

void UClothComponent::ResetOwnerMotionCache()
{
	PreviousClothWorldTransform = FTransform();
	bHasPreviousClothWorldTransform = false;
	CachedOwnerMotionDeltaWorld = FVector::ZeroVector;
	bOwnerMotionInertiaAppliedLastTick = false;
}

void UClothComponent::StoreOwnerMotionCache(const FClothSimulationRuntimeConfig& RuntimeConfig)
{
	PreviousClothWorldTransform = RuntimeConfig.LocalSpaceMotion.CurrentWorldTransform;
	bHasPreviousClothWorldTransform = true;

	// 실제 fixed step이 소비된 tick만 inertia 적용으로 기록
	bOwnerMotionInertiaAppliedLastTick =
		RuntimeConfig.LocalSpaceMotion.bEnabled
		&& RuntimeConfig.LocalSpaceMotion.bHasPreviousTransform
		&& !RuntimeConfig.LocalSpaceMotion.bTeleport
		&& Simulation.GetLastStepCount() > 0
		&& (RuntimeConfig.LocalSpaceMotion.LinearInertia > GNormalTolerance
			|| RuntimeConfig.LocalSpaceMotion.AngularInertia > GNormalTolerance
			|| RuntimeConfig.LocalSpaceMotion.CentrifugalInertia > GNormalTolerance);
}

void UClothComponent::RebuildClothIfNeeded(bool bNotifyProxyDirty)
{
	const FClothConfig Config = MakeClothConfig();

	if (bTopologyRebuildDirty)
	{
		BuildGrid(Config, bNotifyProxyDirty);
	}

	RebuildSimulationIfNeeded(Config);
	ApplySimulationPinningIfNeeded(Config);
}

FClothConfig UClothComponent::MakeClothConfig() const
{
	FClothConfig Config;
	Config.NumParticlesX = ClampInt(NumParticlesX, GMinClothParticlesPerAxis, GMaxClothParticlesPerAxis);
	Config.NumParticlesY = ClampInt(NumParticlesY, GMinClothParticlesPerAxis, GMaxClothParticlesPerAxis);
	Config.ParticleSpacing = SanitizeSpacing(ParticleSpacing);
	Config.Timestep.FixedTimeStep = ClampFloat(FixedTimeStep, GMinFixedTimeStep, GMaxFixedTimeStep);
	Config.Timestep.MaxSubsteps = ClampInt(MaxSubsteps, GMinClothSubsteps, GMaxClothSubsteps);
	Config.Timestep.MaxAccumulatedTime = ClampFloat(MaxAccumulatedTime, GMinAccumulatedTime, GMaxAccumulatedTime);
	return Config;
}

FClothSimulationRuntimeConfig UClothComponent::MakeSimulationRuntimeConfig(const FClothConfig& Config) const
{
	FClothSimulationRuntimeConfig RuntimeConfig;
	RuntimeConfig.Timestep = Config.Timestep;

	const float SafeGravityScale = ClampFloat(GravityScale, 0.0f, GMaxGravityScale);
	RuntimeConfig.GravityAccelerationComponentLocal =
		TransformWorldDirectionToComponentLocal(FVector::DownVector) * (GDefaultGravityAcceleration * SafeGravityScale);

	RuntimeConfig.Damping = ClampFloat(Damping, 0.0f, 1.0f);
	RuntimeConfig.Stiffness = ClampFloat(Stiffness, 0.0f, 1.0f);

	// bEnableWind는 이 component가 wind를 받을지 결정하는 master switch
	FVector FinalWindVelocityWorld = FVector::ZeroVector;
	float FinalTurbulenceStrength = 0.0f;
	float FinalTurbulenceSpatialScale = ClampFloat(WindTurbulenceSpatialScale, 0.001f, 10000.0f);
	float FinalTurbulenceTemporalScale = ClampFloat(WindTurbulenceTemporalScale, 0.0f, 100.0f);
	int32 FinalTurbulenceSeed = WindTurbulenceSeed;
	bool bGlobalWindApplied = false;

	if (bEnableWind)
	{
		const float SafeGlobalResponse = ClampFloat(GlobalWindResponse, 0.0f, 10.0f);
		const float SafeLocalScale = ClampFloat(LocalWindScale, 0.0f, 10.0f);
		const float SafeTurbulenceResponse = ClampFloat(TurbulenceResponse, 0.0f, 10.0f);

		if (bUseGlobalWind)
		{
			if (const UWorld* World = GetWorld())
			{
				const FWorldClothWindSettings& GlobalWind = World->GetWorldSettings().ClothWind;
				if (GlobalWind.bEnabled)
				{
					// world settings wind는 world 기준 velocity로 먼저 합성
					const FVector GlobalDirectionWorld = GlobalWind.Direction.GetSafeNormal(GNormalTolerance, FVector::ForwardVector);
					const float GlobalStrength = ClampFloat(GlobalWind.Strength, 0.0f, GMaxWindStrength) * SafeGlobalResponse;
					const float GlobalTurbulenceStrength =
						ClampFloat(GlobalWind.TurbulenceStrength, 0.0f, GMaxWindStrength)
						* SafeGlobalResponse
						* SafeTurbulenceResponse;

					FinalWindVelocityWorld += GlobalDirectionWorld * GlobalStrength;
					FinalTurbulenceStrength += GlobalTurbulenceStrength;
					FinalTurbulenceSpatialScale = ClampFloat(GlobalWind.TurbulenceSpatialScale, 0.001f, 10000.0f);
					FinalTurbulenceTemporalScale = ClampFloat(GlobalWind.TurbulenceTemporalScale, 0.0f, 100.0f);
					FinalTurbulenceSeed = GlobalWind.TurbulenceSeed;
					bGlobalWindApplied = GlobalStrength > GNormalTolerance || GlobalTurbulenceStrength > GNormalTolerance;
				}
			}
		}

		// 기존 component local wind property는 world 기준 local wind source로 유지
		const FVector LocalWindDirectionWorld = WindDirection.GetSafeNormal(GNormalTolerance, FVector::ForwardVector);
		const float LocalWindStrength = ClampFloat(WindStrength, 0.0f, GMaxWindStrength) * SafeLocalScale;
		const float LocalTurbulenceStrength =
			ClampFloat(WindTurbulenceStrength, 0.0f, GMaxWindStrength)
			* SafeLocalScale
			* SafeTurbulenceResponse;

		FinalWindVelocityWorld += LocalWindDirectionWorld * LocalWindStrength;
		FinalTurbulenceStrength += LocalTurbulenceStrength;
	}

	const float FinalWindSpeed = FinalWindVelocityWorld.Length();
	RuntimeConfig.Wind.bEnabled = bEnableWind && (FinalWindSpeed > GNormalTolerance || FinalTurbulenceStrength > GNormalTolerance);
	RuntimeConfig.Wind.Direction =
		TransformWorldDirectionToComponentLocal(FinalWindVelocityWorld.GetSafeNormal(GNormalTolerance, FVector::ForwardVector));
	RuntimeConfig.Wind.Strength = FinalWindSpeed;
	RuntimeConfig.Wind.TurbulenceStrength = ClampFloat(FinalTurbulenceStrength, 0.0f, GMaxWindStrength);
	RuntimeConfig.Wind.TurbulenceSpatialScale = FinalTurbulenceSpatialScale;
	RuntimeConfig.Wind.TurbulenceTemporalScale = FinalTurbulenceTemporalScale;
	RuntimeConfig.Wind.TurbulenceSeed = FinalTurbulenceSeed;
	RuntimeConfig.Wind.DragCoefficient = ClampFloat(WindDragCoefficient, 0.0f, 10.0f);
	RuntimeConfig.Wind.LiftCoefficient = ClampFloat(WindLiftCoefficient, 0.0f, 10.0f);
	RuntimeConfig.Wind.FluidDensity = ClampFloat(WindFluidDensity, 0.0f, 10.0f);

	CachedFinalWindVelocityWorld = RuntimeConfig.Wind.bEnabled ? FinalWindVelocityWorld : FVector::ZeroVector;
	bGlobalWindAppliedLastTick = RuntimeConfig.Wind.bEnabled && bGlobalWindApplied;

	RuntimeConfig.SelfCollision.bEnabled = bEnableSelfCollision;
	RuntimeConfig.SelfCollision.Distance = ClampFloat(SelfCollisionDistance, 0.0f, GMaxSelfCollisionDistance);
	RuntimeConfig.SelfCollision.Stiffness = ClampFloat(SelfCollisionStiffness, 0.0f, 1.0f);
	RuntimeConfig.SelfCollision.CullScale = ClampFloat(SelfCollisionCullScale, 0.0f, 10.0f);

	const FTransform CurrentClothWorldTransform = FTransform::FromMatrixWithScale(GetWorldMatrix());
	const float SafeTeleportDistance = ClampFloat(OwnerMotionTeleportDistance, 0.0f, 100000.0f);
	const float SafeTeleportAngleDegrees = ClampFloat(OwnerMotionTeleportAngleDegrees, 0.0f, 180.0f);

	RuntimeConfig.LocalSpaceMotion.bEnabled = bEnableOwnerMotionInertia;
	RuntimeConfig.LocalSpaceMotion.bHasPreviousTransform = bHasPreviousClothWorldTransform;
	RuntimeConfig.LocalSpaceMotion.PreviousWorldTransform = PreviousClothWorldTransform;
	RuntimeConfig.LocalSpaceMotion.CurrentWorldTransform = CurrentClothWorldTransform;
	// 1.0 초과 값은 실제 물리보다 강한 게임용 owner motion 반응
	constexpr float MaxOwnerMotionInertiaResponse = 3.0f;
	RuntimeConfig.LocalSpaceMotion.LinearInertia = ClampFloat(OwnerLinearInertiaResponse, 0.0f, MaxOwnerMotionInertiaResponse);
	RuntimeConfig.LocalSpaceMotion.AngularInertia = ClampFloat(OwnerAngularInertiaResponse, 0.0f, MaxOwnerMotionInertiaResponse);
	RuntimeConfig.LocalSpaceMotion.CentrifugalInertia = ClampFloat(OwnerCentrifugalInertiaResponse, 0.0f, MaxOwnerMotionInertiaResponse);
	RuntimeConfig.LocalSpaceMotion.TeleportDistance = SafeTeleportDistance;
	RuntimeConfig.LocalSpaceMotion.TeleportAngleDegrees = SafeTeleportAngleDegrees;

	CachedOwnerMotionDeltaWorld = FVector::ZeroVector;
	bOwnerMotionInertiaAppliedLastTick = false;
	if (bHasPreviousClothWorldTransform)
	{
		// component world transform 변화량 기준의 owner motion 판정
		const FVector DeltaLocationWorld = CurrentClothWorldTransform.Location - PreviousClothWorldTransform.Location;
		const float DeltaDistance = DeltaLocationWorld.Length();
		const float DeltaAngleDegrees = ComputeRotationDeltaDegrees(
			PreviousClothWorldTransform.Rotation,
			CurrentClothWorldTransform.Rotation);

		RuntimeConfig.LocalSpaceMotion.bTeleport =
			DeltaDistance > SafeTeleportDistance
			|| DeltaAngleDegrees > SafeTeleportAngleDegrees;

		if (bEnableOwnerMotionInertia && !RuntimeConfig.LocalSpaceMotion.bTeleport)
		{
			CachedOwnerMotionDeltaWorld = DeltaLocationWorld;
		}
	}

	return RuntimeConfig;
}

bool UClothComponent::ShouldTickSimulation(ELevelTick TickType) const
{
	if (!bEnableSimulation || !RenderData.IsValid() || !Simulation.IsSimulationAvailable())
	{
		return false;
	}

	if (TickType == LEVELTICK_ViewportsOnly)
	{
		return bSimulateInEditor;
	}

	return TickType == LEVELTICK_All;
}

void UClothComponent::ApplyEditorPreviewPolicy()
{
	if (!bSimulateInEditor)
	{
		return;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		// editor preview를 켤 때만 owner tick을 활성화하고, 끌 때는 다른 component 정책을 보존
		OwnerActor->bTickInEditor = true;
	}
}

void UClothComponent::BuildGrid(const FClothConfig& Config, bool bNotifyProxyDirty)
{
	// 현재 property에도 보정값 반영
	NumParticlesX = Config.NumParticlesX;
	NumParticlesY = Config.NumParticlesY;
	ParticleSpacing = Config.ParticleSpacing;

	RenderData.Vertices.clear();
	RenderData.Indices.clear();
	RenderData.Sections.clear();
	RestPositionsComponentLocal.clear();

	const uint32 NumX = static_cast<uint32>(Config.NumParticlesX);
	const uint32 NumY = static_cast<uint32>(Config.NumParticlesY);
	const uint32 VertexCount = NumX * NumY;
	const uint32 QuadCount = (NumX - 1) * (NumY - 1);

	RenderData.Vertices.resize(VertexCount);
	RestPositionsComponentLocal.resize(VertexCount);
	RenderData.Indices.reserve(static_cast<size_t>(QuadCount) * 6);

	const float Width = static_cast<float>(NumX - 1) * Config.ParticleSpacing;
	const float Height = static_cast<float>(NumY - 1) * Config.ParticleSpacing;
	const float MinX = -Width * 0.5f;
	const float MaxZ = Height * 0.5f;

	for (uint32 Row = 0; Row < NumY; ++Row)
	{
		for (uint32 Col = 0; Col < NumX; ++Col)
		{
			const uint32 VertexIndex = Row * NumX + Col;
			const float U = NumX > 1 ? static_cast<float>(Col) / static_cast<float>(NumX - 1) : 0.0f;
			const float V = NumY > 1 ? 1.0f - static_cast<float>(Row) / static_cast<float>(NumY - 1) : 0.0f;

			FVertexPNCTT& Vertex = RenderData.Vertices[VertexIndex];
			const FVector RestPosition(MinX + static_cast<float>(Col) * Config.ParticleSpacing, 0.0f, MaxZ - static_cast<float>(Row) * Config.ParticleSpacing);
			RestPositionsComponentLocal[VertexIndex] = RestPosition;
			Vertex.Position = RestPosition;
			Vertex.Normal = FVector::YAxisVector;
			Vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
			Vertex.UV = FVector2(U, V);
			Vertex.Tangent = FVector4(FVector::XAxisVector, 1.0f);
		}
	}

	for (uint32 Row = 0; Row + 1 < NumY; ++Row)
	{
		for (uint32 Col = 0; Col + 1 < NumX; ++Col)
		{
			const uint32 V00 = Row * NumX + Col;
			const uint32 V10 = Row * NumX + Col + 1;
			const uint32 V01 = (Row + 1) * NumX + Col;
			const uint32 V11 = (Row + 1) * NumX + Col + 1;

			// Row 0이 높은 Z인 X-Z 평면에서 +Y가 front face가 되도록 winding 고정
			RenderData.Indices.push_back(V00);
			RenderData.Indices.push_back(V10);
			RenderData.Indices.push_back(V01);

			RenderData.Indices.push_back(V10);
			RenderData.Indices.push_back(V11);
			RenderData.Indices.push_back(V01);
		}
	}

	RecalculateNormalsAndTangents();
	UpdateRenderSections();
	UpdateLocalBoundsFromRenderData(Config);
	IncrementRenderRevision();

	bTopologyRebuildDirty = false;
	MarkClothSimulationRebuildDirty();

	// local shape 변경 전파
	MarkWorldBoundsDirty();
	if (bNotifyProxyDirty)
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

FClothSimulationBuildDesc UClothComponent::BuildSimulationDesc(const FClothConfig& Config) const
{
	FClothSimulationBuildDesc BuildDesc;
	BuildDesc.Config = Config;
	BuildDesc.Indices = RenderData.Indices;

	const bool bHasValidRestPositions = RestPositionsComponentLocal.size() == RenderData.Vertices.size();
	const size_t PositionCount = bHasValidRestPositions ? RestPositionsComponentLocal.size() : RenderData.Vertices.size();
	BuildDesc.InitialPositionsComponentLocal.reserve(PositionCount);
	BuildDesc.InvMasses.resize(PositionCount, 1.0f);

	if (bHasValidRestPositions)
	{
		for (const FVector& RestPosition : RestPositionsComponentLocal)
		{
			// simulation resource는 deformed readback이 아닌 procedural rest grid 기준으로 생성
			BuildDesc.InitialPositionsComponentLocal.push_back(RestPosition);
		}
	}
	else
	{
		for (const FVertexPNCTT& Vertex : RenderData.Vertices)
		{
			// rest position cache가 아직 준비되지 않은 초기 경로의 보수적 fallback
			BuildDesc.InitialPositionsComponentLocal.push_back(Vertex.Position);
		}
	}

	BuildPinnedParticles(Config, BuildDesc.PinnedIndices, BuildDesc.PinTargetPositionsComponentLocal);
	for (uint32 PinnedIndex : BuildDesc.PinnedIndices)
	{
		if (PinnedIndex < BuildDesc.InvMasses.size())
		{
			// hard pin은 inverse mass 0으로 solver에 전달
			BuildDesc.InvMasses[PinnedIndex] = 0.0f;
		}
	}

	return BuildDesc;
}

void UClothComponent::BuildPinnedParticles(
	const FClothConfig& Config,
	TArray<uint32>& OutPinnedIndices,
	TArray<FVector>& OutPinTargetPositionsComponentLocal) const
{
	OutPinnedIndices.clear();
	OutPinTargetPositionsComponentLocal.clear();

	if (RenderData.Vertices.empty())
	{
		return;
	}

	const uint32 NumX = static_cast<uint32>(Config.NumParticlesX);
	const uint32 NumY = static_cast<uint32>(Config.NumParticlesY);
	const uint32 VertexCount = static_cast<uint32>(RenderData.Vertices.size());
	const bool bHasValidRestPositions = RestPositionsComponentLocal.size() == RenderData.Vertices.size();
	const FVector PinOffsetComponentLocal = TransformActorLocalVectorToComponentLocal(PinOffsetActorLocal);

	auto GetBasePosition = [&](uint32 ParticleIndex) -> const FVector&
	{
		// pin selection과 target은 simulation으로 변형된 render 위치가 아니라 rest grid 위치 기준
		return bHasValidRestPositions
			? RestPositionsComponentLocal[ParticleIndex]
			: RenderData.Vertices[ParticleIndex].Position;
	};

	auto AddPinnedParticle = [&](uint32 ParticleIndex)
	{
		if (ParticleIndex >= VertexCount)
		{
			return;
		}

		const size_t PreviousCount = OutPinnedIndices.size();
		AddUniquePinnedIndex(OutPinnedIndices, ParticleIndex);
		if (OutPinnedIndices.size() == PreviousCount)
		{
			return;
		}

		// target은 rest grid 위치에 actor local offset을 더한 component local 위치
		OutPinTargetPositionsComponentLocal.push_back(GetBasePosition(ParticleIndex) + PinOffsetComponentLocal);
	};

	switch (PinningMode)
	{
	case EClothPinSelectionType::None:
	case EClothPinSelectionType::ExplicitVertices:
		// explicit vertex 목록은 아직 editor property로 제공하지 않으므로 빈 selection 유지
		break;

	case EClothPinSelectionType::TopEdge:
		for (uint32 Col = 0; Col < NumX; ++Col)
		{
			AddPinnedParticle(Col);
		}
		break;

	case EClothPinSelectionType::BottomEdge:
		for (uint32 Col = 0; Col < NumX; ++Col)
		{
			AddPinnedParticle((NumY - 1) * NumX + Col);
		}
		break;

	case EClothPinSelectionType::LeftEdge:
		for (uint32 Row = 0; Row < NumY; ++Row)
		{
			AddPinnedParticle(Row * NumX);
		}
		break;

	case EClothPinSelectionType::RightEdge:
		for (uint32 Row = 0; Row < NumY; ++Row)
		{
			AddPinnedParticle(Row * NumX + (NumX - 1));
		}
		break;

	case EClothPinSelectionType::ActorLocalSphere:
	{
		const FVector CenterComponentLocal = TransformActorLocalPointToComponentLocal(PinCenterActorLocal);
		const float RadiusActorLocal = ClampFloat(PinRadius, 0.0f, GMaxCollisionLength);
		const float RadiusComponentLocal = TransformActorLocalLengthToComponentLocal(RadiusActorLocal);
		const float RadiusSquared = RadiusComponentLocal * RadiusComponentLocal;

		for (uint32 ParticleIndex = 0; ParticleIndex < VertexCount; ++ParticleIndex)
		{
			const FVector Delta = GetBasePosition(ParticleIndex) - CenterComponentLocal;
			if (Delta.Dot(Delta) <= RadiusSquared)
			{
				AddPinnedParticle(ParticleIndex);
			}
		}
		break;
	}

	case EClothPinSelectionType::ActorLocalBox:
	{
		const FVector CenterComponentLocal = TransformActorLocalPointToComponentLocal(PinCenterActorLocal);
		const FVector SafeExtent = PinBoxExtentActorLocal.GetAbs();
		const FVector ExtentXVector = TransformActorLocalVectorToComponentLocal(FVector(SafeExtent.X, 0.0f, 0.0f));
		const FVector ExtentYVector = TransformActorLocalVectorToComponentLocal(FVector(0.0f, SafeExtent.Y, 0.0f));
		const FVector ExtentZVector = TransformActorLocalVectorToComponentLocal(FVector(0.0f, 0.0f, SafeExtent.Z));
		const float ExtentX = ExtentXVector.Length();
		const float ExtentY = ExtentYVector.Length();
		const float ExtentZ = ExtentZVector.Length();
		const FVector AxisX = ExtentXVector.GetSafeNormal(GNormalTolerance, FVector::XAxisVector);
		const FVector AxisY = ExtentYVector.GetSafeNormal(GNormalTolerance, FVector::YAxisVector);
		const FVector AxisZ = ExtentZVector.GetSafeNormal(GNormalTolerance, FVector::ZAxisVector);

		for (uint32 ParticleIndex = 0; ParticleIndex < VertexCount; ++ParticleIndex)
		{
			const FVector Delta = GetBasePosition(ParticleIndex) - CenterComponentLocal;
			if (std::abs(Delta.Dot(AxisX)) <= ExtentX
				&& std::abs(Delta.Dot(AxisY)) <= ExtentY
				&& std::abs(Delta.Dot(AxisZ)) <= ExtentZ)
			{
				AddPinnedParticle(ParticleIndex);
			}
		}
		break;
	}

	case EClothPinSelectionType::ActorLocalRectXZ:
	{
		const float MinX = (std::min)(PinRectMinActorLocalXZ.X, PinRectMaxActorLocalXZ.X);
		const float MaxX = (std::max)(PinRectMinActorLocalXZ.X, PinRectMaxActorLocalXZ.X);
		const float MinZ = (std::min)(PinRectMinActorLocalXZ.Z, PinRectMaxActorLocalXZ.Z);
		const float MaxZ = (std::max)(PinRectMinActorLocalXZ.Z, PinRectMaxActorLocalXZ.Z);
		const FVector RectCenterActorLocal((MinX + MaxX) * 0.5f, 0.0f, (MinZ + MaxZ) * 0.5f);
		const FVector CenterComponentLocal = TransformActorLocalPointToComponentLocal(RectCenterActorLocal);
		const FVector ExtentXVector = TransformActorLocalVectorToComponentLocal(FVector((MaxX - MinX) * 0.5f, 0.0f, 0.0f));
		const FVector ExtentZVector = TransformActorLocalVectorToComponentLocal(FVector(0.0f, 0.0f, (MaxZ - MinZ) * 0.5f));
		const float ExtentX = ExtentXVector.Length();
		const float ExtentZ = ExtentZVector.Length();
		const FVector AxisX = ExtentXVector.GetSafeNormal(GNormalTolerance, FVector::XAxisVector);
		const FVector AxisZ = ExtentZVector.GetSafeNormal(GNormalTolerance, FVector::ZAxisVector);

		for (uint32 ParticleIndex = 0; ParticleIndex < VertexCount; ++ParticleIndex)
		{
			const FVector Delta = GetBasePosition(ParticleIndex) - CenterComponentLocal;
			if (std::abs(Delta.Dot(AxisX)) <= ExtentX
				&& std::abs(Delta.Dot(AxisZ)) <= ExtentZ)
			{
				AddPinnedParticle(ParticleIndex);
			}
		}
		break;
	}
	}
}

void UClothComponent::RebuildSimulationIfNeeded(const FClothConfig& Config)
{
	if (!bSimulationRebuildDirty)
	{
		return;
	}

	if (!bEnableSimulation)
	{
		// simulation off 상태에서는 static grid render만 유지
		Simulation.Shutdown();
		SimulationReadbackPositions.clear();
		CachedPinnedIndices.clear();
		CachedPinTargetPositionsComponentLocal.clear();
		CachedCollisionPrimitives.clear();
		CachedIndependentCollisionPrimitiveCount = 0;
		CachedBodyCollisionPrimitiveCount = 0;
		ResetOwnerMotionCache();
		bSimulationRebuildDirty = false;
		bLastSimulationBuildSkippedByAllPinned = false;
		bSimulationTickWarningLogged = false;
		bCollisionUpdateWarningLogged = false;
		return;
	}

	if (!RenderData.IsValid())
	{
		Simulation.Shutdown();
		SimulationReadbackPositions.clear();
		CachedPinnedIndices.clear();
		CachedPinTargetPositionsComponentLocal.clear();
		CachedCollisionPrimitives.clear();
		CachedIndependentCollisionPrimitiveCount = 0;
		CachedBodyCollisionPrimitiveCount = 0;
		ResetOwnerMotionCache();
		bSimulationRebuildDirty = false;
		bLastSimulationBuildSkippedByAllPinned = false;
		bSimulationTickWarningLogged = false;
		bCollisionUpdateWarningLogged = false;
		return;
	}

	if (!GEngine)
	{
		// engine context가 아직 없으면 다음 tick에서 다시 시도
		return;
	}

	const FClothSimulationBuildDesc BuildDesc = BuildSimulationDesc(Config);
	CachedPinnedIndices = BuildDesc.PinnedIndices;
	CachedPinTargetPositionsComponentLocal = BuildDesc.PinTargetPositionsComponentLocal;
	bLastSimulationBuildSkippedByAllPinned = false;
	ResetOwnerMotionCache();
	if (!Simulation.Rebuild(&GEngine->GetClothContext(), BuildDesc))
	{
		bLastSimulationBuildSkippedByAllPinned = IsAllPinnedBuildFailure(Simulation.GetLastFailureDetail());
		UE_LOG("[ClothComponent] Simulation resource unavailable: %s", Simulation.GetLastFailureDetail().c_str());
	}
	else
	{
		bSimulationTickWarningLogged = false;
		bCollisionUpdateWarningLogged = false;
	}

	bSimulationRebuildDirty = false;
	bPinningDirty = false;
	bPinTargetDirty = false;
	bForceConfigDirty = false;
	bCollisionDirty = false;
}

void UClothComponent::ApplySimulationPinningIfNeeded(const FClothConfig& Config)
{
	if (!bPinningDirty && !bPinTargetDirty)
	{
		return;
	}

	if (bSimulationRebuildDirty)
	{
		// simulation rebuild가 남아 있으면 rebuild 입력에서 pinning을 함께 처리
		return;
	}

	if (!bEnableSimulation || !RenderData.IsValid())
	{
		CachedPinnedIndices.clear();
		CachedPinTargetPositionsComponentLocal.clear();
		bPinningDirty = false;
		bPinTargetDirty = false;
		return;
	}

	BuildPinnedParticles(Config, CachedPinnedIndices, CachedPinTargetPositionsComponentLocal);
	if (Simulation.IsSimulationAvailable())
	{
		if (!Simulation.ApplyPinning(CachedPinnedIndices, CachedPinTargetPositionsComponentLocal))
		{
			UE_LOG("[ClothComponent] Pinning update skipped: %s", Simulation.GetLastFailureDetail().c_str());
		}
	}
	else if (bPinningDirty
		&& bLastSimulationBuildSkippedByAllPinned
		&& GEngine
		&& GEngine->GetClothContext().IsAvailable()
		&& CachedPinnedIndices.size() < RenderData.Vertices.size())
	{
		// all-pinned guard 이후 pin selection이 동적인 particle을 남기면 simulation resource를 즉시 복구
		bSimulationRebuildDirty = true;
		RebuildSimulationIfNeeded(Config);
		return;
	}

	bPinningDirty = false;
	bPinTargetDirty = false;
}

void UClothComponent::TickSimulationIfNeeded(float DeltaTime, ELevelTick TickType, const FClothConfig& Config)
{
	if (!ShouldTickSimulation(TickType))
	{
		if (TickType == LEVELTICK_ViewportsOnly)
		{
			bEditorPreviewDirty = false;
		}
		ResetOwnerMotionCache();
		return;
	}

	UpdateSimulationCollisionPrimitives();

	const FClothSimulationRuntimeConfig RuntimeConfig = MakeSimulationRuntimeConfig(Config);
	SimulationReadbackPositions.clear();
	const auto SimulationStartTime = std::chrono::high_resolution_clock::now();
	const bool bTickResult = Simulation.Tick(DeltaTime, RuntimeConfig, SimulationReadbackPositions);
	const auto SimulationEndTime = std::chrono::high_resolution_clock::now();
	const double SimulationElapsedMs = std::chrono::duration<double, std::milli>(SimulationEndTime - SimulationStartTime).count();
	StoreOwnerMotionCache(RuntimeConfig);

	CLOTH_STATS_RECORD_COMPONENT(
		Simulation.GetParticleCount(),
		Simulation.GetIndexCount(),
		Simulation.GetPinnedCount(),
		Simulation.GetCollisionPrimitiveCount(),
		Simulation.GetLastStepCount(),
		SimulationElapsedMs);

	if (!bTickResult)
	{
		if (Simulation.IsSimulationAvailable()
			&& !Simulation.GetLastFailureDetail().empty()
			&& !bSimulationTickWarningLogged)
		{
			bSimulationTickWarningLogged = true;
			UE_LOG("[ClothComponent] Simulation tick skipped: %s", Simulation.GetLastFailureDetail().c_str());
		}
		return;
	}

	if (ApplySimulationPositionsToRenderData(Config, SimulationReadbackPositions))
	{
		bForceConfigDirty = false;
		bEditorPreviewDirty = false;
		bSimulationTickWarningLogged = false;
	}
}

void UClothComponent::BuildCollisionPrimitivesComponentLocal(TArray<FClothCollisionPrimitive>& OutPrimitives) const
{
	OutPrimitives.clear();

	if (bEnableSphereCollision)
	{
		const float SafeRadiusActorLocal = ClampFloat(SphereRadius, 0.0f, GMaxCollisionLength);
		const float RadiusComponentLocal = TransformActorLocalLengthToComponentLocal(SafeRadiusActorLocal);
		if (RadiusComponentLocal > GNormalTolerance)
		{
			FClothCollisionPrimitive Primitive;
			Primitive.Type = EClothCollisionPrimitiveType::Sphere;
			Primitive.Source = EClothCollisionPrimitiveSource::Independent;
			Primitive.Center = TransformActorLocalPointToComponentLocal(SphereCenterActorLocal);
			Primitive.Radius = RadiusComponentLocal;
			OutPrimitives.push_back(Primitive);
		}
	}

	if (bEnablePlaneCollision)
	{
		FVector PlanePointComponentLocal = FVector::ZeroVector;
		FVector PlaneNormalComponentLocal = FVector::UpVector;
		TransformActorLocalPlaneToComponentLocal(
			PlanePointActorLocal,
			PlaneNormalActorLocal,
			PlanePointComponentLocal,
			PlaneNormalComponentLocal);

		FClothCollisionPrimitive Primitive;
		Primitive.Type = EClothCollisionPrimitiveType::Plane;
		Primitive.Source = EClothCollisionPrimitiveSource::Independent;
		Primitive.PlanePoint = PlanePointComponentLocal;
		Primitive.PlaneNormal = PlaneNormalComponentLocal.GetSafeNormal(GNormalTolerance, FVector::UpVector);
		Primitive.PlaneDistance = Primitive.PlaneNormal.Dot(Primitive.PlanePoint);
		OutPrimitives.push_back(Primitive);
	}

	if (bEnableCapsuleCollision)
	{
		const float SafeRadiusActorLocal = ClampFloat(CapsuleRadius, 0.0f, GMaxCollisionLength);
		const float RadiusComponentLocal = TransformActorLocalLengthToComponentLocal(SafeRadiusActorLocal);
		if (RadiusComponentLocal > GNormalTolerance)
		{
			FVector CapsuleStartComponentLocal = FVector::ZeroVector;
			FVector CapsuleEndComponentLocal = FVector::ZeroVector;
			const float SafeHalfHeightActorLocal = ClampFloat(CapsuleHalfHeight, 0.0f, GMaxCollisionLength);
			TransformActorLocalCapsuleToComponentLocal(
				CapsuleCenterActorLocal,
				CapsuleAxisActorLocal,
				SafeHalfHeightActorLocal,
				CapsuleStartComponentLocal,
				CapsuleEndComponentLocal);

			const FVector Segment = CapsuleEndComponentLocal - CapsuleStartComponentLocal;
			if (Segment.Length() <= GNormalTolerance)
			{
				// half height가 0이면 capsule 대신 같은 중심의 sphere로 안전하게 전달
				FClothCollisionPrimitive Primitive;
				Primitive.Type = EClothCollisionPrimitiveType::Sphere;
				Primitive.Source = EClothCollisionPrimitiveSource::Independent;
				Primitive.Center = TransformActorLocalPointToComponentLocal(CapsuleCenterActorLocal);
				Primitive.Radius = RadiusComponentLocal;
				OutPrimitives.push_back(Primitive);
			}
			else
			{
				FClothCollisionPrimitive Primitive;
				Primitive.Type = EClothCollisionPrimitiveType::Capsule;
				Primitive.Source = EClothCollisionPrimitiveSource::Independent;
				Primitive.CapsuleStart = CapsuleStartComponentLocal;
				Primitive.CapsuleEnd = CapsuleEndComponentLocal;
				Primitive.Center = (CapsuleStartComponentLocal + CapsuleEndComponentLocal) * 0.5f;
				Primitive.Axis = Segment.GetSafeNormal(GNormalTolerance, FVector::UpVector);
				Primitive.HalfHeight = Segment.Length() * 0.5f;
				Primitive.Radius = RadiusComponentLocal;
				OutPrimitives.push_back(Primitive);
			}
		}
	}

	if (bEnableBoxCollision)
	{
		const FVector SafeExtentActorLocal(
			ClampFloat(std::abs(BoxExtentActorLocal.X), GMinBoxCollisionExtent, GMaxCollisionLength),
			ClampFloat(std::abs(BoxExtentActorLocal.Y), GMinBoxCollisionExtent, GMaxCollisionLength),
			ClampFloat(std::abs(BoxExtentActorLocal.Z), GMinBoxCollisionExtent, GMaxCollisionLength));

		const FVector ActorLocalAxisX = BoxRotationActorLocal.GetForwardVector();
		const FVector ActorLocalAxisY = BoxRotationActorLocal.GetRightVector();
		const FVector ActorLocalAxisZ = BoxRotationActorLocal.GetUpVector();
		const FVector ExtentXVector = TransformActorLocalVectorToComponentLocal(ActorLocalAxisX * SafeExtentActorLocal.X);
		const FVector ExtentYVector = TransformActorLocalVectorToComponentLocal(ActorLocalAxisY * SafeExtentActorLocal.Y);
		const FVector ExtentZVector = TransformActorLocalVectorToComponentLocal(ActorLocalAxisZ * SafeExtentActorLocal.Z);
		const float ExtentX = (std::max)(ExtentXVector.Length(), GMinBoxCollisionExtent);
		const float ExtentY = (std::max)(ExtentYVector.Length(), GMinBoxCollisionExtent);
		const float ExtentZ = (std::max)(ExtentZVector.Length(), GMinBoxCollisionExtent);

		FClothCollisionPrimitive Primitive;
		Primitive.Type = EClothCollisionPrimitiveType::Box;
		Primitive.Source = EClothCollisionPrimitiveSource::Independent;
		Primitive.Center = TransformActorLocalPointToComponentLocal(BoxCenterActorLocal);
		Primitive.BoxExtent = FVector(ExtentX, ExtentY, ExtentZ);
		Primitive.BoxAxisX = ExtentXVector.GetSafeNormal(GNormalTolerance, FVector::XAxisVector);
		Primitive.BoxAxisY = ExtentYVector.GetSafeNormal(GNormalTolerance, FVector::YAxisVector);
		Primitive.BoxAxisZ = ExtentZVector.GetSafeNormal(GNormalTolerance, FVector::ZAxisVector);
		OutPrimitives.push_back(Primitive);
	}
}

UPrimitiveComponent* UClothComponent::ResolveBodyCollisionSource() const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	const bool bHasRequestedSourceName =
		CollisionSourceComponentName.IsValid()
		&& CollisionSourceComponentName != FName::None;
	const TArray<UActorComponent*> OwnerComponents = OwnerActor->GetComponents();
	UPrimitiveComponent* FirstPrimitiveBodySource = nullptr;
	for (UActorComponent* Component : OwnerComponents)
	{
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
		if (!IsValid(PrimitiveComponent) || PrimitiveComponent == this)
		{
			continue;
		}

		if (bHasRequestedSourceName)
		{
			// 이름이 지정된 경우에는 component 종류와 무관하게 명시 선택된 primitive를 source로 사용
			if (PrimitiveComponent->GetFName() == CollisionSourceComponentName)
			{
				return PrimitiveComponent;
			}

			continue;
		}

		if (!bAutoFindOwnerBodyCollision)
		{
			continue;
		}

		if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(PrimitiveComponent))
		{
			// 기존 계획과 호환되도록 skeletal mesh가 있으면 첫 번째 skeletal mesh를 우선 source로 사용
			return SkeletalMeshComponent;
		}

		if (!FirstPrimitiveBodySource && PrimitiveComponent->GetBodyInstance().IsValidBodyInstance())
		{
			// skeletal mesh가 없을 때 차량 chassis 같은 일반 primitive body를 fallback source로 사용
			FirstPrimitiveBodySource = PrimitiveComponent;
		}
	}

	return FirstPrimitiveBodySource;
}

void UClothComponent::BuildBodyCollisionPrimitivesWorld(
	const UPrimitiveComponent* SourceComponent,
	TArray<FClothCollisionPrimitive>& OutPrimitives) const
{
	OutPrimitives.clear();
	if (!IsValid(SourceComponent))
	{
		return;
	}

	if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(SourceComponent))
	{
		// skeletal mesh는 ragdoll body 배열 전체를 cloth collision snapshot으로 변환
		SkeletalMeshComponent->GetClothCollisionPrimitives(OutPrimitives);
		return;
	}

	// 일반 primitive component는 자기 BodyInstance 하나를 cloth collision snapshot으로 변환
	SourceComponent->GetBodyInstance().AppendClothCollisionPrimitives(OutPrimitives);
}

uint32 UClothComponent::AppendBodyCollisionPrimitivesComponentLocal(
	const TArray<FClothCollisionPrimitive>& BodyWorldPrimitives,
	TArray<FClothCollisionPrimitive>& OutPrimitives) const
{
	const int32 SafeMaxBodyPrimitiveCount = ClampInt(
		MaxBodyCollisionPrimitives,
		GMinBodyCollisionPrimitiveCount,
		GMaxBodyCollisionPrimitiveCount);
	if (SafeMaxBodyPrimitiveCount <= 0 || BodyWorldPrimitives.empty())
	{
		return 0;
	}

	uint32 AppendedPrimitiveCount = 0;
	for (const FClothCollisionPrimitive& WorldPrimitive : BodyWorldPrimitives)
	{
		if (AppendedPrimitiveCount >= static_cast<uint32>(SafeMaxBodyPrimitiveCount))
		{
			break;
		}

		FClothCollisionPrimitive ComponentLocalPrimitive;
		ConvertWorldCollisionPrimitiveToComponentLocal(WorldPrimitive, ComponentLocalPrimitive);
		ComponentLocalPrimitive.Source = EClothCollisionPrimitiveSource::Body;
		OutPrimitives.push_back(ComponentLocalPrimitive);
		++AppendedPrimitiveCount;
	}

	return AppendedPrimitiveCount;
}

void UClothComponent::ConvertWorldCollisionPrimitiveToComponentLocal(
	const FClothCollisionPrimitive& WorldPrimitive,
	FClothCollisionPrimitive& OutComponentLocalPrimitive) const
{
	OutComponentLocalPrimitive = WorldPrimitive;
	OutComponentLocalPrimitive.Source = EClothCollisionPrimitiveSource::Body;

	switch (WorldPrimitive.Type)
	{
	case EClothCollisionPrimitiveType::Sphere:
		OutComponentLocalPrimitive.Center = TransformWorldPointToComponentLocal(WorldPrimitive.Center);
		OutComponentLocalPrimitive.Radius = TransformWorldLengthToComponentLocal(WorldPrimitive.Radius);
		break;

	case EClothCollisionPrimitiveType::Capsule:
	{
		OutComponentLocalPrimitive.CapsuleStart = TransformWorldPointToComponentLocal(WorldPrimitive.CapsuleStart);
		OutComponentLocalPrimitive.CapsuleEnd = TransformWorldPointToComponentLocal(WorldPrimitive.CapsuleEnd);

		const FVector Segment = OutComponentLocalPrimitive.CapsuleEnd - OutComponentLocalPrimitive.CapsuleStart;
		OutComponentLocalPrimitive.Center =
			(OutComponentLocalPrimitive.CapsuleStart + OutComponentLocalPrimitive.CapsuleEnd) * 0.5f;
		OutComponentLocalPrimitive.Axis = Segment.GetSafeNormal(GNormalTolerance, FVector::UpVector);
		OutComponentLocalPrimitive.HalfHeight = Segment.Length() * 0.5f;
		OutComponentLocalPrimitive.Radius = TransformWorldLengthToComponentLocal(WorldPrimitive.Radius);
		break;
	}

	case EClothCollisionPrimitiveType::Box:
	{
		const FVector SafeWorldExtent = WorldPrimitive.BoxExtent.GetAbs();
		const FVector ExtentXVector = TransformWorldVectorToComponentLocal(
			WorldPrimitive.BoxAxisX.GetSafeNormal(GNormalTolerance, FVector::XAxisVector) * SafeWorldExtent.X);
		const FVector ExtentYVector = TransformWorldVectorToComponentLocal(
			WorldPrimitive.BoxAxisY.GetSafeNormal(GNormalTolerance, FVector::YAxisVector) * SafeWorldExtent.Y);
		const FVector ExtentZVector = TransformWorldVectorToComponentLocal(
			WorldPrimitive.BoxAxisZ.GetSafeNormal(GNormalTolerance, FVector::ZAxisVector) * SafeWorldExtent.Z);

		OutComponentLocalPrimitive.Center = TransformWorldPointToComponentLocal(WorldPrimitive.Center);
		OutComponentLocalPrimitive.BoxExtent = FVector(
			(std::max)(ExtentXVector.Length(), GMinBoxCollisionExtent),
			(std::max)(ExtentYVector.Length(), GMinBoxCollisionExtent),
			(std::max)(ExtentZVector.Length(), GMinBoxCollisionExtent));
		OutComponentLocalPrimitive.BoxAxisX = ExtentXVector.GetSafeNormal(GNormalTolerance, FVector::XAxisVector);
		OutComponentLocalPrimitive.BoxAxisY = ExtentYVector.GetSafeNormal(GNormalTolerance, FVector::YAxisVector);
		OutComponentLocalPrimitive.BoxAxisZ = ExtentZVector.GetSafeNormal(GNormalTolerance, FVector::ZAxisVector);
		break;
	}

	case EClothCollisionPrimitiveType::Plane:
		OutComponentLocalPrimitive.PlanePoint = TransformWorldPointToComponentLocal(WorldPrimitive.PlanePoint);
		OutComponentLocalPrimitive.PlaneNormal =
			TransformWorldDirectionToComponentLocal(WorldPrimitive.PlaneNormal);
		OutComponentLocalPrimitive.PlaneDistance =
			OutComponentLocalPrimitive.PlaneNormal.Dot(OutComponentLocalPrimitive.PlanePoint);
		break;
	}
}

void UClothComponent::UpdateSimulationCollisionPrimitives()
{
	BuildCollisionPrimitivesComponentLocal(CachedCollisionPrimitives);
	CachedIndependentCollisionPrimitiveCount = static_cast<uint32>(CachedCollisionPrimitives.size());
	CachedBodyCollisionPrimitiveCount = 0;

	if (bEnableBodyCollision)
	{
		// body collision은 매 update마다 현재 source body snapshot을 가져와 병합
		if (UPrimitiveComponent* CollisionSourceComponent = ResolveBodyCollisionSource())
		{
			TArray<FClothCollisionPrimitive> BodyWorldPrimitives;
			BuildBodyCollisionPrimitivesWorld(CollisionSourceComponent, BodyWorldPrimitives);
			CachedBodyCollisionPrimitiveCount =
				AppendBodyCollisionPrimitivesComponentLocal(BodyWorldPrimitives, CachedCollisionPrimitives);
		}
	}

	if (!Simulation.UpdateCollisionPrimitives(CachedCollisionPrimitives))
	{
		if (Simulation.IsSimulationAvailable()
			&& !Simulation.GetLastFailureDetail().empty()
			&& !bCollisionUpdateWarningLogged)
		{
			bCollisionUpdateWarningLogged = true;
			UE_LOG("[ClothComponent] Collision update skipped: %s", Simulation.GetLastFailureDetail().c_str());
		}

		bCollisionDirty = true;
		return;
	}

	bCollisionDirty = false;
	bCollisionUpdateWarningLogged = false;
}

bool UClothComponent::ApplySimulationPositionsToRenderData(
	const FClothConfig& Config,
	const TArray<FVector>& PositionsComponentLocal)
{
	if (PositionsComponentLocal.size() != RenderData.Vertices.size())
	{
		if (!bSimulationTickWarningLogged)
		{
			bSimulationTickWarningLogged = true;
			UE_LOG("[ClothComponent] Simulation readback size mismatch: positions=%u vertices=%u",
				static_cast<uint32>(PositionsComponentLocal.size()),
				static_cast<uint32>(RenderData.Vertices.size()));
		}
		return false;
	}

	for (uint32 VertexIndex = 0; VertexIndex < RenderData.Vertices.size(); ++VertexIndex)
	{
		// simulation particle와 render vertex는 procedural grid에서 1:1 mapping 유지
		RenderData.Vertices[VertexIndex].Position = PositionsComponentLocal[VertexIndex];
	}

	RecalculateNormalsAndTangents();
	UpdateLocalBoundsFromRenderData(Config);
	IncrementRenderRevision();
	MarkWorldBoundsDirty();
	return true;
}

void UClothComponent::RecalculateNormalsAndTangents()
{
	TArray<FVector> AccumulatedNormals;
	TArray<FVector> AccumulatedTangents;
	AccumulatedNormals.resize(RenderData.Vertices.size(), FVector::ZeroVector);
	AccumulatedTangents.resize(RenderData.Vertices.size(), FVector::ZeroVector);

	for (uint32 Index = 0; Index + 2 < RenderData.Indices.size(); Index += 3)
	{
		const uint32 I0 = RenderData.Indices[Index];
		const uint32 I1 = RenderData.Indices[Index + 1];
		const uint32 I2 = RenderData.Indices[Index + 2];

		if (I0 >= RenderData.Vertices.size() || I1 >= RenderData.Vertices.size() || I2 >= RenderData.Vertices.size())
		{
			continue;
		}

		const FVertexPNCTT& V0 = RenderData.Vertices[I0];
		const FVertexPNCTT& V1 = RenderData.Vertices[I1];
		const FVertexPNCTT& V2 = RenderData.Vertices[I2];

		const FVector Edge1 = V1.Position - V0.Position;
		const FVector Edge2 = V2.Position - V0.Position;
		const FVector TriangleNormal = Edge1.Cross(Edge2);

		if (!TriangleNormal.IsNearlyZero(GNormalTolerance))
		{
			AccumulatedNormals[I0] += TriangleNormal;
			AccumulatedNormals[I1] += TriangleNormal;
			AccumulatedNormals[I2] += TriangleNormal;
		}

		const FVector2 DeltaUV1 = V1.UV - V0.UV;
		const FVector2 DeltaUV2 = V2.UV - V0.UV;
		const float Determinant = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;
		FVector TriangleTangent = FVector::XAxisVector;

		if (std::abs(Determinant) > GTangentTolerance)
		{
			const float InvDeterminant = 1.0f / Determinant;
			TriangleTangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDeterminant;
		}

		if (!TriangleTangent.IsNearlyZero(GTangentTolerance))
		{
			AccumulatedTangents[I0] += TriangleTangent;
			AccumulatedTangents[I1] += TriangleTangent;
			AccumulatedTangents[I2] += TriangleTangent;
		}
	}

	for (uint32 VertexIndex = 0; VertexIndex < RenderData.Vertices.size(); ++VertexIndex)
	{
		FVector Normal = AccumulatedNormals[VertexIndex];
		if (Normal.IsNearlyZero(GNormalTolerance))
		{
			Normal = FVector::YAxisVector;
		}
		else
		{
			Normal.Normalize();
		}

		FVector Tangent = AccumulatedTangents[VertexIndex];
		if (Tangent.IsNearlyZero(GTangentTolerance))
		{
			Tangent = FVector::XAxisVector;
		}
		else
		{
			// normal 방향 성분 제거 후 tangent 정규화
			Tangent = Tangent - Normal * Tangent.Dot(Normal);
			if (Tangent.IsNearlyZero(GTangentTolerance))
			{
				Tangent = FVector::XAxisVector;
			}
			else
			{
				Tangent.Normalize();
			}
		}

		RenderData.Vertices[VertexIndex].Normal = Normal;
		RenderData.Vertices[VertexIndex].Tangent = FVector4(Tangent, 1.0f);
	}
}

void UClothComponent::UpdateRenderSections()
{
	RenderData.Sections.clear();

	FClothRenderSection Section;
	Section.FirstIndex = 0;
	Section.IndexCount = static_cast<uint32>(RenderData.Indices.size());
	Section.MaterialIndex = 0;
	RenderData.Sections.push_back(Section);
}

void UClothComponent::UpdateLocalBoundsFromRenderData(const FClothConfig& Config)
{
	bHasValidLocalBounds = false;

	if (RenderData.Vertices.empty())
	{
		CachedLocalCenter = FVector::ZeroVector;
		CachedLocalExtent = FVector(0.5f, 0.5f, 0.5f);
		return;
	}

	FVector Min = RenderData.Vertices[0].Position;
	FVector Max = RenderData.Vertices[0].Position;

	for (const FVertexPNCTT& Vertex : RenderData.Vertices)
	{
		Min.X = (std::min)(Min.X, Vertex.Position.X);
		Min.Y = (std::min)(Min.Y, Vertex.Position.Y);
		Min.Z = (std::min)(Min.Z, Vertex.Position.Z);

		Max.X = (std::max)(Max.X, Vertex.Position.X);
		Max.Y = (std::max)(Max.Y, Vertex.Position.Y);
		Max.Z = (std::max)(Max.Z, Vertex.Position.Z);
	}

	const FVector Margin(Config.BoundsMargin, Config.BoundsMargin, Config.BoundsMargin);
	Min -= Margin;
	Max += Margin;

	CachedLocalCenter = (Min + Max) * 0.5f;
	CachedLocalExtent = (Max - Min) * 0.5f;
	LocalExtents = CachedLocalExtent;
	bHasValidLocalBounds = true;
}

void UClothComponent::IncrementRenderRevision()
{
	++RenderData.Revision;
	if (RenderData.Revision == 0)
	{
		++RenderData.Revision;
	}
}

void UClothComponent::LoadMaterialFromSlot()
{
	if (MaterialSlot.empty() || MaterialSlot == "None")
	{
		SetMaterial(0, nullptr);
		return;
	}

	UMaterial* LoadedMaterial = FMaterialManager::Get().GetOrCreateMaterial(MaterialSlot);
	if (LoadedMaterial)
	{
		SetMaterial(0, LoadedMaterial);
	}
}

FVector UClothComponent::TransformActorLocalPointToComponentLocal(const FVector& ActorLocalPoint) const
{
	// owner root가 있으면 actor local을 world로 먼저 올림
	FVector WorldPoint = ActorLocalPoint;
	if (AActor* OwnerActor = GetOwner())
	{
		if (USceneComponent* RootComponent = OwnerActor->GetRootComponent())
		{
			WorldPoint = RootComponent->GetWorldMatrix().TransformPosition(ActorLocalPoint);
		}
	}

	// world 위치를 현재 cloth component local로 변환
	return GetWorldInverseMatrix().TransformPosition(WorldPoint);
}

FVector UClothComponent::TransformActorLocalVectorToComponentLocal(const FVector& ActorLocalVector) const
{
	// vector는 위치와 달리 translation 영향을 받지 않도록 변환
	FVector WorldVector = ActorLocalVector;
	if (AActor* OwnerActor = GetOwner())
	{
		if (USceneComponent* RootComponent = OwnerActor->GetRootComponent())
		{
			WorldVector = RootComponent->GetWorldMatrix().TransformVector(ActorLocalVector);
		}
	}

	return GetWorldInverseMatrix().TransformVector(WorldVector);
}

FVector UClothComponent::TransformActorLocalDirectionToComponentLocal(const FVector& ActorLocalDirection) const
{
	// zero 방향 입력은 collision/pin 계산에서 안전한 기본 축으로 보정
	const FVector ComponentLocalVector = TransformActorLocalVectorToComponentLocal(ActorLocalDirection);
	return ComponentLocalVector.GetSafeNormal(GNormalTolerance, FVector::UpVector);
}

FVector UClothComponent::TransformWorldVectorToComponentLocal(const FVector& WorldVector) const
{
	// world 기준 vector를 component local simulation 공간으로 변환
	return GetWorldInverseMatrix().TransformVector(WorldVector);
}

FVector UClothComponent::TransformWorldPointToComponentLocal(const FVector& WorldPoint) const
{
	// world 기준 위치를 component local simulation 공간으로 변환
	return GetWorldInverseMatrix().TransformPosition(WorldPoint);
}

FVector UClothComponent::TransformWorldDirectionToComponentLocal(const FVector& WorldDirection) const
{
	const FVector ComponentLocalVector = TransformWorldVectorToComponentLocal(WorldDirection);
	return ComponentLocalVector.GetSafeNormal(GNormalTolerance, FVector::DownVector);
}

float UClothComponent::TransformWorldLengthToComponentLocal(float WorldLength) const
{
	const float SafeLength = ClampFloat(WorldLength, 0.0f, GMaxCollisionLength);
	if (SafeLength <= GNormalTolerance)
	{
		return 0.0f;
	}

	const float LengthX = TransformWorldVectorToComponentLocal(FVector(SafeLength, 0.0f, 0.0f)).Length();
	const float LengthY = TransformWorldVectorToComponentLocal(FVector(0.0f, SafeLength, 0.0f)).Length();
	const float LengthZ = TransformWorldVectorToComponentLocal(FVector(0.0f, 0.0f, SafeLength)).Length();

	// non-uniform scale에서는 sphere/capsule이 작아져 통과하지 않도록 가장 큰 축 길이를 사용
	return (std::max)(LengthX, (std::max)(LengthY, LengthZ));
}

void UClothComponent::TransformActorLocalPlaneToComponentLocal(
	const FVector& ActorLocalPoint,
	const FVector& ActorLocalNormal,
	FVector& OutComponentLocalPoint,
	FVector& OutComponentLocalNormal) const
{
	// plane은 한 점과 normal을 서로 다른 변환 규칙으로 처리
	OutComponentLocalPoint = TransformActorLocalPointToComponentLocal(ActorLocalPoint);
	OutComponentLocalNormal = TransformActorLocalDirectionToComponentLocal(ActorLocalNormal);
}

void UClothComponent::TransformActorLocalCapsuleToComponentLocal(
	const FVector& ActorLocalCenter,
	const FVector& ActorLocalAxis,
	float HalfHeight,
	FVector& OutComponentLocalStart,
	FVector& OutComponentLocalEnd) const
{
	// capsule axis가 비어 있으면 actor local up 기준으로 endpoint 생성
	const FVector SafeActorLocalAxis = ActorLocalAxis.GetSafeNormal(GNormalTolerance, FVector::UpVector);
	const float SafeHalfHeight = (std::max)(0.0f, HalfHeight);

	const FVector ActorLocalStart = ActorLocalCenter - SafeActorLocalAxis * SafeHalfHeight;
	const FVector ActorLocalEnd = ActorLocalCenter + SafeActorLocalAxis * SafeHalfHeight;

	OutComponentLocalStart = TransformActorLocalPointToComponentLocal(ActorLocalStart);
	OutComponentLocalEnd = TransformActorLocalPointToComponentLocal(ActorLocalEnd);
}

float UClothComponent::TransformActorLocalLengthToComponentLocal(float ActorLocalLength) const
{
	const float SafeLength = ClampFloat(ActorLocalLength, 0.0f, GMaxCollisionLength);
	if (SafeLength <= GNormalTolerance)
	{
		return 0.0f;
	}

	const float LengthX = TransformActorLocalVectorToComponentLocal(FVector(SafeLength, 0.0f, 0.0f)).Length();
	const float LengthY = TransformActorLocalVectorToComponentLocal(FVector(0.0f, SafeLength, 0.0f)).Length();
	const float LengthZ = TransformActorLocalVectorToComponentLocal(FVector(0.0f, 0.0f, SafeLength)).Length();

	// non-uniform scale에서는 sphere/capsule이 작아져 통과하지 않도록 가장 큰 축 길이를 사용
	return (std::max)(LengthX, (std::max)(LengthY, LengthZ));
}

void UClothComponent::LogBackendStatusOnce()
{
	if (bBackendStatusLogged)
	{
		return;
	}

	if (!GEngine)
	{
		return;
	}

	bBackendStatusLogged = true;

	const FClothBackendStatus& Status = GEngine->GetClothContext().GetBackendStatus();
	UE_LOG("[ClothComponent] Backend=%s Available=%s Detail=%s",
		GetClothBackendName(Status.Backend),
		Status.bAvailable ? "true" : "false",
		Status.Detail.c_str());
}
