#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"
#include "Render/Types/VertexTypes.h"
#include "Physics/NvClothInclude.h"
#include "Physics/ClothCollisionTypes.h"

#include "Source/Engine/Physics/ClothInstance.generated.h"

UENUM()
enum class EClothPinMode : uint8
{
	None = 0,
	TopRow = 1,
	TopCorners = 2,
	LeftEdge = 3,
};

USTRUCT()
struct FClothDesc
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category = "Cloth|Grid", DisplayName = "Initial Offset", Type = Vec3, Speed = 0.1f)
	FVector InitialOffset = FVector(0.0f, 0.0f, 5.0f);

	UPROPERTY(Edit, Save, Category = "Cloth|Grid", DisplayName = "Width", Min = 2, Max = 200, Speed = 1.0f)
	int32 Width = 20;

	UPROPERTY(Edit, Save, Category = "Cloth|Grid", DisplayName = "Height", Min = 2, Max = 200, Speed = 1.0f)
	int32 Height = 20;

	UPROPERTY(Edit, Save, Category = "Cloth|Grid", DisplayName = "Spacing", Min = 0.01f, Max = 100.0f, Speed = 0.01f)
	float Spacing = 0.5f;

	UPROPERTY(Edit, Save, Category = "Cloth|Simulation", DisplayName = "Gravity", Type = Vec3, Speed = 0.1f)
	FVector Gravity = FVector(0.0f, 0.0f, -9.8f);

	UPROPERTY(Edit, Save, Category = "Cloth|Simulation", DisplayName = "Solver Frequency", Min = 1.0f, Max = 1000.0f, Speed = 1.0f)
	float SolverFrequency = 120.0f;

	UPROPERTY(Edit, Save, Category = "Cloth|Simulation", DisplayName = "Stiffness Frequency", Min = 1.0f, Max = 1000.0f, Speed = 1.0f)
	float StiffnessFrequency = 120.0f;

	UPROPERTY(Edit, Save, Category = "Cloth|Simulation", DisplayName = "Substep Count", Min = 1, Max = 8, Speed = 1.0f)
	int32 SubstepCount = 2;

	UPROPERTY(Edit, Save, Category = "Cloth|Simulation", DisplayName = "Enable CCD")
	bool bEnableCCD = true;

	UPROPERTY(Edit, Save, Category = "Cloth|Simulation", DisplayName = "Damping", Type = Vec3, Speed = 0.001f)
	FVector Damping = FVector(0.02f, 0.02f, 0.02f);

	UPROPERTY(Edit, Save, Category = "Cloth|Simulation", DisplayName = "Linear Drag", Type = Vec3, Speed = 0.001f)
	FVector LinearDrag = FVector(0.005f, 0.005f, 0.005f);

	UPROPERTY(Edit, Save, Category = "Cloth|Simulation", DisplayName = "Angular Drag", Type = Vec3, Speed = 0.001f)
	FVector AngularDrag = FVector(0.005f, 0.005f, 0.005f);

	UPROPERTY(Edit, Save, Category = "Cloth|Simulation", DisplayName = "Teleport Distance Threshold", Min = 0.0f, Max = 1000.0f, Speed = 0.1f)
	float TeleportDistanceThreshold = 3.0f;

	UPROPERTY(Edit, Save, Category = "Cloth|Constraint", DisplayName = "Structural Stiffness", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float StructuralStiffness = 1.0f;

	UPROPERTY(Edit, Save, Category = "Cloth|Constraint", DisplayName = "Shear Stiffness", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float ShearStiffness = 1.0f;

	UPROPERTY(Edit, Save, Category = "Cloth|Constraint", DisplayName = "Bending Stiffness", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float BendingStiffness = 0.35f;

	UPROPERTY(Edit, Save, Category = "Cloth|Constraint", DisplayName = "Use Tether")
	bool bUseTether = true;

	UPROPERTY(Edit, Save, Category = "Cloth|Constraint", DisplayName = "Use Shear")
	bool bUseShear = true;

	UPROPERTY(Edit, Save, Category = "Cloth|Constraint", DisplayName = "Use Bending")
	bool bUseBending = true;

	UPROPERTY(Edit, Save, Category = "Cloth|Constraint", DisplayName = "Pin Mode", Enum = EClothPinMode)
	EClothPinMode PinMode = EClothPinMode::None;

	UPROPERTY(Edit, Save, Category = "Cloth|Collision", DisplayName = "Enable World Collision")
	bool bEnableWorldCollision = true;

	UPROPERTY(Edit, Save, Category = "Cloth|Collision", DisplayName = "Collision Bounds Padding", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float CollisionBoundsPadding = 2.0f;

	UPROPERTY(Edit, Save, Category = "Cloth|Collision", DisplayName = "Collision Mass Scale", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float CollisionMassScale = 0.0f;

	UPROPERTY(Edit, Save, Category = "Cloth|Collision", DisplayName = "Friction", Min = 0.0f, Max = 10.0f, Speed = 0.01f)
	float Friction = 0.5f;

	UPROPERTY(Edit, Save, Category = "Cloth|Render", DisplayName = "Render Normal Offset", Min = 0.0f, Max = 1.0f, Speed = 0.001f)
	float RenderNormalOffset = 0.01f;
};

class FClothInstance
{
public:
	bool InitializeGrid(const FClothDesc& Desc);
	bool InitializeMesh(const FClothDesc& Desc, const FMeshDataView& MeshView);

	void Release();
	void Simulate(float DeltaTime, int32 SubstepCount, float RenderNormalOffset);

	void ApplySimulationSettings(const FClothDesc& Desc);
	
	void UpdateCollision(const FClothCollisionData& CollisionData);
	void UpdateRenderData(float RenderNormalOffset);

	static void SimulateSolver(float DeltaTime, int32 SubstepCount);

	void SetSimulationSpaceTransform(const FVector& WorldLocation, const FQuat& WorldRotation, bool bTeleport);

	const TArray<FVertexPNCTT>& GetRenderVertices() const { return RenderVertices; }
	const TArray<uint32>& GetRenderIndices() const { return Triangles; }
	uint64 GetRenderRevision() const { return RenderRevision; }
	bool IsInitialized() const { return Cloth != nullptr; }
	uint32 GetParticleCount() const { return static_cast<uint32>(Particles.size()); }
	uint32 GetTriangleCount() const { return static_cast<uint32>(Triangles.size() / 3); }

private:
	void UpdateRenderVerticesFromParticles(float RenderNormalOffset);

	nv::cloth::Fabric* Fabric = nullptr;
	nv::cloth::Cloth* Cloth = nullptr;

	TArray<physx::PxVec4> Particles;
	TArray<uint32> PhaseIndices;
	TArray<uint32> Sets;
	TArray<float> RestValues;
	TArray<float> StiffnessValues;
	TArray<uint32> Indices;
	TArray<uint32> Anchors;
	TArray<float> TetherLengths;
	TArray<uint32> Triangles;

	TArray<FVertexPNCTT> RenderVertices;
	uint64 RenderRevision = 0;
	int32 GridWidth = 0;
	int32 GridHeight = 0;

	TArray<FVector2> SourceUVs;
	TArray<FVector4> SourceTangents;
	bool bUseSourceMeshAttributes = false;
};
