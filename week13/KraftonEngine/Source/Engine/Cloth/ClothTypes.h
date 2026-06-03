#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Render/Types/VertexTypes.h"

/**
 * @brief Cloth simulation backend 종류
 */
enum class EClothBackendType
{
	Unavailable,
	CUDA,
	DX11,
	CPU,
	Disabled
};

/**
 * @brief Cloth backend 초기화 상태
 */
struct FClothBackendStatus
{
	EClothBackendType Backend = EClothBackendType::Unavailable;
	bool bAvailable = false;
	FString Detail;
};

/**
 * @brief 지정된 Cloth backend 종류의 표시 이름을 반환합니다
 *
 * @param Backend 표시 이름으로 변환할 Cloth backend 종류
 *
 * @return backend 표시 이름
 */
const char* GetClothBackendName(EClothBackendType Backend);

/**
 * @brief Cloth 고정점 선택 방식
 */
UENUM()
enum class EClothPinSelectionType : uint8
{
	None,
	ExplicitVertices,
	TopEdge,
	BottomEdge,
	LeftEdge,
	RightEdge,
	ActorLocalSphere,
	ActorLocalBox,
	ActorLocalRectXZ
};

/**
 * @brief Cloth 고정점 적용 방식
 */
enum class EClothPinConstraintType : uint8
{
	Hard,
	Soft
};

/**
 * @brief Cloth fixed timestep 설정
 */
struct FClothTimestepConfig
{
	float FixedTimeStep = 1.0f / 60.0f;
	int32 MaxSubsteps = 4;
	float MaxAccumulatedTime = 0.25f;
};

/**
 * @brief Cloth procedural grid와 simulation 입력 기본값
 */
struct FClothConfig
{
	int32 NumParticlesX = 20;
	int32 NumParticlesY = 20;
	float ParticleSpacing = 10.0f;
	float BoundsMargin = 1.0f;
	FClothTimestepConfig Timestep;
};

/**
 * @brief Cloth render section 정보
 */
struct FClothRenderSection
{
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
	uint32 MaterialIndex = 0;
};

/**
 * @brief Cloth component가 소유하는 CPU render data
 */
struct FClothRenderData
{
	TArray<FVertexPNCTT> Vertices;
	TArray<uint32> Indices;

	// 일단 뚫어두기는 하는데, 이번 과제에서는 section이 하나밖에 없을 예정
	TArray<FClothRenderSection> Sections;

	// 몇 번째 버전의 render data인지 나타냄
	uint64 Revision = 0;

	/**
	 * @brief render data 유효 여부를 반환합니다
	 *
	 * @return render data 유효 여부
	 */
	bool IsValid() const
	{
		return !Vertices.empty() && !Indices.empty() && !Sections.empty();
	}
};

/**
 * @brief Cloth simulation resource 생성 입력
 */
struct FClothSimulationBuildDesc
{
	TArray<FVector> InitialPositionsComponentLocal;
	TArray<uint32> Indices;
	TArray<float> InvMasses;
	TArray<uint32> PinnedIndices;
	TArray<FVector> PinTargetPositionsComponentLocal;
	FClothConfig Config;

	/**
	 * @brief simulation build 입력 유효 여부를 반환합니다
	 *
	 * @return simulation build 입력 유효 여부
	 */
	bool IsValid() const
	{
		return !InitialPositionsComponentLocal.empty()
			&& Indices.size() >= 3
			&& (Indices.size() % 3) == 0
			&& (InvMasses.empty() || InvMasses.size() == InitialPositionsComponentLocal.size())
			&& (PinTargetPositionsComponentLocal.empty()
				|| PinTargetPositionsComponentLocal.size() == PinnedIndices.size());
	}
};

/**
 * @brief Cloth vertex pinning 단위 데이터
 */
struct FClothPinData
{
	uint32 VertexIndex = 0;
	float Weight = 1.0f;
	FVector TargetOffset = FVector::ZeroVector;
	EClothPinConstraintType ConstraintType = EClothPinConstraintType::Hard;
};

/**
 * @brief 이름 있는 Cloth pin group 설명
 */
struct FClothPinGroupDesc
{
	FName Name = FName::None;
	EClothPinSelectionType SelectionType = EClothPinSelectionType::TopEdge;
	TArray<FClothPinData> Pins;
	FVector SelectionCenterActorLocal = FVector::ZeroVector;
	FVector SelectionBoxExtentActorLocal = FVector(50.0f, 50.0f, 50.0f);
	FVector SelectionRectMinActorLocalXZ = FVector(-50.0f, 0.0f, -50.0f);
	FVector SelectionRectMaxActorLocalXZ = FVector(50.0f, 0.0f, 50.0f);
	FVector TargetOffsetActorLocal = FVector::ZeroVector;
	float SelectionRadius = 50.0f;
	float Weight = 1.0f;
	bool bHardPin = true;
};

/**
 * @brief Cloth pin group anchor 설명
 */
struct FClothAnchorDesc
{
	FName PinGroupName = FName::None;
	FName TargetComponentName = FName::None;
	FVector LocalOffset = FVector::ZeroVector;
};

/**
 * @brief Cloth wind 설정 초안
 */
struct FClothWindConfig
{
	bool bEnabled = false;
	FVector Direction = FVector::ForwardVector;
	float Strength = 0.0f;
	float TurbulenceStrength = 0.0f;
	float TurbulenceSpatialScale = 100.0f;
	float TurbulenceTemporalScale = 1.0f;
	int32 TurbulenceSeed = 1337;

	// NvCloth wind drag 반응 계수
	float DragCoefficient = 0.5f;

	// NvCloth wind lift 반응 계수
	float LiftCoefficient = 0.05f;

	// NvCloth wind 유체 밀도
	float FluidDensity = 1.0f;
};

/**
 * @brief Cloth self collision 설정 초안
 */
struct FClothSelfCollisionConfig
{
	bool bEnabled = false;
	float Distance = 2.0f;
	float Stiffness = 1.0f;

	// NvCloth에서 얼마나 적극적으로 충돌 후보를 솎아낼지
	float CullScale = 1.0f;
};

/**
 * @brief component world transform 기반 NvCloth local-space motion 설정
 */
struct FClothLocalSpaceMotionConfig
{
	bool bEnabled = false;
	bool bHasPreviousTransform = false;
	bool bTeleport = false;
	FTransform PreviousWorldTransform;
	FTransform CurrentWorldTransform;
	float LinearInertia = 0.35f;
	float AngularInertia = 0.15f;
	float CentrifugalInertia = 0.15f;
	float TeleportDistance = 300.0f;
	float TeleportAngleDegrees = 45.0f;
};

/**
 * @brief Cloth runtime simulation 설정 묶음
 */
struct FClothSimulationRuntimeConfig
{
	FClothTimestepConfig Timestep;
	FVector GravityAccelerationComponentLocal = FVector(0.0f, 0.0f, -980.0f);
	float Damping = 0.1f;
	float Stiffness = 1.0f;
	FClothWindConfig Wind;
	FClothSelfCollisionConfig SelfCollision;
	FClothLocalSpaceMotionConfig LocalSpaceMotion;
};

/**
 * @brief Cloth collision primitive 종류
 */
enum class EClothCollisionPrimitiveType : uint8
{
	Sphere,
	Capsule,
	Box,
	Plane
};

/**
 * @brief Cloth collision bridge용 primitive 초안
 */
struct FClothCollisionPrimitive
{
	EClothCollisionPrimitiveType Type = EClothCollisionPrimitiveType::Sphere;
	FVector Center = FVector::ZeroVector;
	FVector Axis = FVector::UpVector;
	FVector CapsuleStart = FVector::ZeroVector;
	FVector CapsuleEnd = FVector::UpVector;
	FVector BoxExtent = FVector::OneVector;
	FVector BoxAxisX = FVector::XAxisVector;
	FVector BoxAxisY = FVector::YAxisVector;
	FVector BoxAxisZ = FVector::ZAxisVector;
	FVector PlanePoint = FVector::ZeroVector;
	FVector PlaneNormal = FVector::UpVector;
	float Radius = 1.0f;
	float HalfHeight = 0.0f;
	float PlaneDistance = 0.0f;
};
