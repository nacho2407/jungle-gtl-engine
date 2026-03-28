#pragma once
#include "PrimitiveBase.h"

struct RotationDesc;

class ENGINE_API CPrimitiveGizmo : public CPrimitiveBase
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

	explicit CPrimitiveGizmo(EGizmoType Type = EGizmoType::Scale);

	void Generate(EGizmoType Type);
	static std::shared_ptr<FMeshData> CreateTranslationAxisMesh(EAxis Axis);
	static std::shared_ptr<FMeshData> CreateTranslationAxisMesh(EAxis Axis, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateTranslationPlaneMesh(ETranslationPlane Plane);
	static std::shared_ptr<FMeshData> CreateTranslationPlaneMesh(ETranslationPlane Plane, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateTranslationScreenMesh();
	static std::shared_ptr<FMeshData> CreateTranslationScreenMesh(const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateRotationAxisMesh(EAxis Axis);
	static std::shared_ptr<FMeshData> CreateRotationAxisMesh(EAxis Axis, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateRotationAxisMesh(EAxis Axis, const RotationDesc& Desc);
	static std::shared_ptr<FMeshData> CreateRotationAxisMesh(EAxis Axis, const RotationDesc& Desc, const FVector4& OverrideColor);
	static std::shared_ptr<FMeshData> CreateRotationScreenMesh(const RotationDesc& Desc);
	static std::shared_ptr<FMeshData> CreateRotationScreenMesh(const RotationDesc& Desc, const FVector4& OverrideColor);
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

