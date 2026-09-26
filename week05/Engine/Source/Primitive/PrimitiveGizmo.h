#pragma once
#include "Renderer/MeshData.h"

struct FDynamicMesh;
struct FRotationGizmoDesc;

class ENGINE_API FPrimitiveGizmo
{
public:
	enum class EGizmoType : std::uint8_t { Translation, Rotation, Scale };
	enum class ETranslationPlane : std::uint8_t { XY, XZ, YZ }; 
	enum class EScalePlane : std::uint8_t { XY, XZ, YZ };

	static const FString Key;
	static const FString FilePath;

	explicit FPrimitiveGizmo(EGizmoType Type = EGizmoType::Scale);

	FDynamicMesh* GetRenderMesh() const { return RenderMesh.get(); }
	static void ClearCache();

	void Generate(EGizmoType Type);
	static std::shared_ptr<FDynamicMesh> CreateTranslationAxisMesh(EAxis Axis);
	static std::shared_ptr<FDynamicMesh> CreateTranslationAxisMesh(EAxis Axis, const FVector4& OverrideColor);
	static std::shared_ptr<FDynamicMesh> CreateTranslationPlaneMesh(ETranslationPlane Plane);
	static std::shared_ptr<FDynamicMesh> CreateTranslationPlaneMesh(ETranslationPlane Plane, const FVector4& OverrideColor);
	static std::shared_ptr<FDynamicMesh> CreateTranslationScreenMesh();
	static std::shared_ptr<FDynamicMesh> CreateTranslationScreenMesh(const FVector4& OverrideColor);
	static std::shared_ptr<FDynamicMesh> CreateRotationAxisMesh(EAxis Axis);
	static std::shared_ptr<FDynamicMesh> CreateRotationAxisMesh(EAxis Axis, const FVector4& OverrideColor);
	static std::shared_ptr<FDynamicMesh> CreateRotationAxisMesh(EAxis Axis, const FRotationGizmoDesc& Desc);
	static std::shared_ptr<FDynamicMesh> CreateRotationAxisMesh(EAxis Axis, const FRotationGizmoDesc& Desc, const FVector4& OverrideColor);
	static std::shared_ptr<FDynamicMesh> CreateRotationScreenMesh(const FRotationGizmoDesc& Desc);
	static std::shared_ptr<FDynamicMesh> CreateRotationScreenMesh(const FRotationGizmoDesc& Desc, const FVector4& OverrideColor);
	static std::shared_ptr<FDynamicMesh> CreateScaleAxisMesh(EAxis Axis);
	static std::shared_ptr<FDynamicMesh> CreateScaleAxisMesh(EAxis Axis, const FVector4& OverrideColor);
	static std::shared_ptr<FDynamicMesh> CreateScalePlaneMesh(EScalePlane Plane);
	static std::shared_ptr<FDynamicMesh> CreateScalePlaneMesh(EScalePlane Plane, const FVector4& OverrideColor);
	static std::shared_ptr<FDynamicMesh> CreateScaleCenterMesh();
	static std::shared_ptr<FDynamicMesh> CreateScaleCenterMesh(const FVector4& OverrideColor);

	void GenerateTranslationGizmoMesh();
	void GenerateRotationGizmoMesh();
	void GenerateScaleGizmoMesh();

private:
	static FString GetKey(EGizmoType Type);

private:
	EGizmoType GizmoType = EGizmoType::Scale;
	std::shared_ptr<FDynamicMesh> RenderMesh;
};

