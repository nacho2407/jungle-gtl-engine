#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"
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
 * @brief Cloth procedural grid와 simulation 입력 기본값
 */
struct FClothConfig
{
	int32 NumParticlesX = 20;
	int32 NumParticlesY = 20;
	float ParticleSpacing = 10.0f;
	float BoundsMargin = 5.0f;
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
 * @brief Cloth pin 선택 방식
 */
enum class EClothPinSelectionType
{
	ExplicitVertices,
	TopRow,
	BottomRow,
	LeftColumn,
	RightColumn
};

/**
 * @brief Cloth vertex pinning 단위 데이터
 */
struct FClothPinData
{
	uint32 VertexIndex = 0;
	float Weight = 1.0f;
};

/**
 * @brief 이름 있는 Cloth pin group 설명
 */
struct FClothPinGroupDesc
{
	FName Name = FName::None;
	EClothPinSelectionType SelectionType = EClothPinSelectionType::ExplicitVertices;
	TArray<FClothPinData> Pins;
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
	float Turbulence = 0.0f;
};

/**
 * @brief Cloth self collision 설정 초안
 */
struct FClothSelfCollisionConfig
{
	bool bEnabled = false;
	float Distance = 2.0f;
	float Stiffness = 1.0f;
};

/**
 * @brief Cloth collision primitive 종류
 */
enum class EClothCollisionPrimitiveType
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
	FVector BoxExtent = FVector::OneVector;
	FVector PlaneNormal = FVector::UpVector;
	float Radius = 1.0f;
	float HalfHeight = 0.0f;
	float PlaneDistance = 0.0f;
};
