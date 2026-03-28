#pragma once
#include "PrimitiveBase.h"

struct FRotationGizmoDesc;

class ENGINE_API FPrimitiveGizmo : public FPrimitiveBase
{
public:
	enum class EGizmoType : std::uint8_t
	{
		Translation,
		Rotation,
		Scale
	};

	enum class ETranslationPlane : std::uint8_t
	{
		XY,
		XZ,
		YZ
	};

	enum class EScalePlane : std::uint8_t
	{
		XY,
		XZ,
		YZ
	};

	static const FString Key;
	static const FString FilePath;

	explicit FPrimitiveGizmo(EGizmoType Type = EGizmoType::Scale);

	void Generate(EGizmoType Type);
	static std::shared_ptr<FMeshData> CreateTranslationAxisMesh(EAxis Axis);
	static std::shared_ptr<FMeshData> CreateTranslationAxisMesh(EAxis Axis, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateTranslationPlaneMesh(ETranslationPlane Plane);
	static std::shared_ptr<FMeshData> CreateTranslationPlaneMesh(ETranslationPlane Plane, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateTranslationScreenMesh();
	static std::shared_ptr<FMeshData> CreateTranslationScreenMesh(const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateRotationAxisMesh(EAxis Axis);
	static std::shared_ptr<FMeshData> CreateRotationAxisMesh(EAxis Axis, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateRotationAxisMesh(EAxis Axis, const FRotationGizmoDesc& Desc);
	static std::shared_ptr<FMeshData> CreateRotationAxisMesh(EAxis Axis, const FRotationGizmoDesc& Desc, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateRotationScreenMesh(const FRotationGizmoDesc& Desc);
	static std::shared_ptr<FMeshData> CreateRotationScreenMesh(const FRotationGizmoDesc& Desc, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateScaleAxisMesh(EAxis Axis);
	static std::shared_ptr<FMeshData> CreateScaleAxisMesh(EAxis Axis, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateScalePlaneMesh(EScalePlane Plane);
	static std::shared_ptr<FMeshData> CreateScalePlaneMesh(EScalePlane Plane, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateScaleCenterMesh();
	static std::shared_ptr<FMeshData> CreateScaleCenterMesh(const FVector4& OverrideColor);

	void GenerateTranslationGizmoMesh();
	void GenerateRotationGizmoMesh();
	void GenerateScaleGizmoMesh();

private:
	static FString GetKey(EGizmoType Type);

private:
	EGizmoType GizmoType = EGizmoType::Scale;
};

