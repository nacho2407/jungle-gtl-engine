#include "Component/Primitive/ClothComponent.h"

#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "Materials/MaterialManager.h"
#include "Object/GarbageCollection.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Proxy/ClothSceneProxy.h"
#include "Core/Logging/Log.h"

#include <algorithm>
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
			|| MatchesPropertyName(PropertyName, "WindDirection", "Wind Direction")
			|| MatchesPropertyName(PropertyName, "WindStrength", "Wind Strength")
			|| MatchesPropertyName(PropertyName, "WindTurbulenceStrength", "Wind Turbulence Strength")
			|| MatchesPropertyName(PropertyName, "WindTurbulenceSpatialScale", "Wind Turbulence Spatial Scale")
			|| MatchesPropertyName(PropertyName, "WindTurbulenceTemporalScale", "Wind Turbulence Temporal Scale")
			|| MatchesPropertyName(PropertyName, "WindTurbulenceSeed", "Wind Turbulence Seed")
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
			|| MatchesPropertyName(PropertyName, "BoxRotationActorLocal", "Box Rotation Actor Local");
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
	LoadMaterialFromSlot();
	MarkClothRebuildDirty();
}

void UClothComponent::RouteComponentDestroyed()
{
	Simulation.Shutdown();
	SimulationReadbackPositions.clear();
	CachedCollisionPrimitives.clear();
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

	// wind direction property는 world 기준으로 해석한 뒤 component local simulation 공간으로 변환
	const FVector SafeWindDirectionWorld = WindDirection.GetSafeNormal(GNormalTolerance, FVector::ForwardVector);
	RuntimeConfig.Wind.bEnabled = bEnableWind;
	RuntimeConfig.Wind.Direction = TransformWorldDirectionToComponentLocal(SafeWindDirectionWorld);
	RuntimeConfig.Wind.Strength = ClampFloat(WindStrength, 0.0f, GMaxWindStrength);
	RuntimeConfig.Wind.TurbulenceStrength = ClampFloat(WindTurbulenceStrength, 0.0f, GMaxWindStrength);
	RuntimeConfig.Wind.TurbulenceSpatialScale = ClampFloat(WindTurbulenceSpatialScale, 0.001f, 10000.0f);
	RuntimeConfig.Wind.TurbulenceTemporalScale = ClampFloat(WindTurbulenceTemporalScale, 0.0f, 100.0f);
	RuntimeConfig.Wind.TurbulenceSeed = WindTurbulenceSeed;

	RuntimeConfig.SelfCollision.bEnabled = bEnableSelfCollision;
	RuntimeConfig.SelfCollision.Distance = ClampFloat(SelfCollisionDistance, 0.0f, GMaxSelfCollisionDistance);
	RuntimeConfig.SelfCollision.Stiffness = ClampFloat(SelfCollisionStiffness, 0.0f, 1.0f);
	RuntimeConfig.SelfCollision.CullScale = ClampFloat(SelfCollisionCullScale, 0.0f, 10.0f);

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

void UClothComponent::BuildGrid(const FClothConfig& Config, bool bNotifyProxyDirty)
{
	// 현재 property에도 보정값 반영
	NumParticlesX = Config.NumParticlesX;
	NumParticlesY = Config.NumParticlesY;
	ParticleSpacing = Config.ParticleSpacing;

	RenderData.Vertices.clear();
	RenderData.Indices.clear();
	RenderData.Sections.clear();

	const uint32 NumX = static_cast<uint32>(Config.NumParticlesX);
	const uint32 NumY = static_cast<uint32>(Config.NumParticlesY);
	const uint32 VertexCount = NumX * NumY;
	const uint32 QuadCount = (NumX - 1) * (NumY - 1);

	RenderData.Vertices.resize(VertexCount);
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
			Vertex.Position = FVector(MinX + static_cast<float>(Col) * Config.ParticleSpacing, 0.0f, MaxZ - static_cast<float>(Row) * Config.ParticleSpacing);
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

	BuildDesc.InitialPositionsComponentLocal.reserve(RenderData.Vertices.size());
	BuildDesc.InvMasses.resize(RenderData.Vertices.size(), 1.0f);

	for (const FVertexPNCTT& Vertex : RenderData.Vertices)
	{
		// render data와 simulation data는 모두 component local 기준 유지
		BuildDesc.InitialPositionsComponentLocal.push_back(Vertex.Position);
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
	const FVector PinOffsetComponentLocal = TransformActorLocalVectorToComponentLocal(PinOffsetActorLocal);

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

		// target은 현재 선택된 rest/render 위치에 actor local offset을 더한 component local 위치
		OutPinTargetPositionsComponentLocal.push_back(RenderData.Vertices[ParticleIndex].Position + PinOffsetComponentLocal);
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
		const float Radius = ClampFloat(PinRadius, 0.0f, 10000.0f);
		const float RadiusSquared = Radius * Radius;

		for (uint32 ParticleIndex = 0; ParticleIndex < VertexCount; ++ParticleIndex)
		{
			const FVector Delta = RenderData.Vertices[ParticleIndex].Position - CenterComponentLocal;
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
			const FVector Delta = RenderData.Vertices[ParticleIndex].Position - CenterComponentLocal;
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
			const FVector Delta = RenderData.Vertices[ParticleIndex].Position - CenterComponentLocal;
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
		bSimulationRebuildDirty = false;
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
		bSimulationRebuildDirty = false;
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
	if (!Simulation.Rebuild(&GEngine->GetClothContext(), BuildDesc))
	{
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
		return;
	}

	UpdateSimulationCollisionPrimitives();

	const FClothSimulationRuntimeConfig RuntimeConfig = MakeSimulationRuntimeConfig(Config);
	SimulationReadbackPositions.clear();
	if (!Simulation.Tick(DeltaTime, RuntimeConfig, SimulationReadbackPositions))
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
				Primitive.Center = TransformActorLocalPointToComponentLocal(CapsuleCenterActorLocal);
				Primitive.Radius = RadiusComponentLocal;
				OutPrimitives.push_back(Primitive);
			}
			else
			{
				FClothCollisionPrimitive Primitive;
				Primitive.Type = EClothCollisionPrimitiveType::Capsule;
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
		Primitive.Center = TransformActorLocalPointToComponentLocal(BoxCenterActorLocal);
		Primitive.BoxExtent = FVector(ExtentX, ExtentY, ExtentZ);
		Primitive.BoxAxisX = ExtentXVector.GetSafeNormal(GNormalTolerance, FVector::XAxisVector);
		Primitive.BoxAxisY = ExtentYVector.GetSafeNormal(GNormalTolerance, FVector::YAxisVector);
		Primitive.BoxAxisZ = ExtentZVector.GetSafeNormal(GNormalTolerance, FVector::ZAxisVector);
		OutPrimitives.push_back(Primitive);
	}
}

void UClothComponent::UpdateSimulationCollisionPrimitives()
{
	BuildCollisionPrimitivesComponentLocal(CachedCollisionPrimitives);

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

FVector UClothComponent::TransformWorldDirectionToComponentLocal(const FVector& WorldDirection) const
{
	const FVector ComponentLocalVector = TransformWorldVectorToComponentLocal(WorldDirection);
	return ComponentLocalVector.GetSafeNormal(GNormalTolerance, FVector::DownVector);
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
