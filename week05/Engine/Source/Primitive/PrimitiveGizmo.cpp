#include "PrimitiveGizmo.h"

#include "UnrealEditorStyledGizmo.h"

#include <optional>

#include "Renderer/MeshData.h"

static TMap<FString, std::shared_ptr<FDynamicMesh>> GizmoCache;

namespace
{
	FRotationGizmoDesc MakeRotationDesc()
	{
		FRotationGizmoDesc Desc{};
		Desc.fullAxisRings = false;
		Desc.includeInnerDisk = false;
		Desc.includeScreenRing = true;
		Desc.includeArcball = false;
		return Desc;
	}

	std::shared_ptr<FDynamicMesh> CreateMeshDataFromMesh(const FGizmoMesh& GizmoMesh, const std::optional<FVector4>& OverrideColor = std::nullopt)
	{
		if (GizmoMesh.vertices.empty() || GizmoMesh.indices.empty())
		{
			return nullptr;
		}

		auto Data = std::make_shared<FDynamicMesh>();
		Data->Vertices.reserve(GizmoMesh.vertices.size());
		for (const auto& FGizmoVertex : GizmoMesh.vertices)
		{
			FVector4 FGizmoColor = OverrideColor.value_or(FVector4(FGizmoVertex.color.r, FGizmoVertex.color.g, FGizmoVertex.color.b, FGizmoVertex.color.a));

			FVertex V;
			V.Position = FGizmoVertex.position;
			V.Normal = FGizmoVertex.normal;
			V.Color = FGizmoColor;
			V.UV = FVector2(0.0f, 0.0f);

			Data->Vertices.push_back(V);
		}

		Data->Indices.reserve(GizmoMesh.indices.size());
		for (auto Index : GizmoMesh.indices)
		{
			Data->Indices.push_back(Index);
		}

		Data->Topology = EMeshTopology::EMT_TriangleList;
		Data->bIsDirty;

		return Data;
	}

	const FGizmoMesh& SelectTranslationAxisMesh(const FTranslationGizmo& Gizmo, EAxis Axis)
	{
		switch (Axis)
		{
		case EAxis::X:
			return Gizmo.axisX;
		case EAxis::Y:
			return Gizmo.axisY;
		case EAxis::Z:
		default:
			return Gizmo.axisZ;
		}
	}

	const FGizmoMesh& SelectTranslationPlaneMesh(const FTranslationGizmo& Gizmo, FPrimitiveGizmo::ETranslationPlane Plane)
	{
		switch (Plane)
		{
		case FPrimitiveGizmo::ETranslationPlane::XY:
			return Gizmo.planeXY;
		case FPrimitiveGizmo::ETranslationPlane::XZ:
			return Gizmo.planeXZ;
		case FPrimitiveGizmo::ETranslationPlane::YZ:
		default:
			return Gizmo.planeYZ;
		}
	}

	const FGizmoMesh& SelectTranslationScreenMesh(const FTranslationGizmo& Gizmo)
	{
		return Gizmo.screenSphere;
	}

	const FGizmoMesh& SelectScaleAxisMesh(const FScaleGizmo& Gizmo, EAxis Axis)
	{
		switch (Axis)
		{
		case EAxis::X:
			return Gizmo.axisX;
		case EAxis::Y:
			return Gizmo.axisY;
		case EAxis::Z:
		default:
			return Gizmo.axisZ;
		}
	}

	const FGizmoMesh& SelectScalePlaneMesh(const FScaleGizmo& Gizmo, FPrimitiveGizmo::EScalePlane Plane)
	{
		switch (Plane)
		{
		case FPrimitiveGizmo::EScalePlane::XY:
			return Gizmo.planeXY;
		case FPrimitiveGizmo::EScalePlane::XZ:
			return Gizmo.planeXZ;
		case FPrimitiveGizmo::EScalePlane::YZ:
		default:
			return Gizmo.planeYZ;
		}
	}

	const FGizmoMesh& SelectScaleCenterMesh(const FScaleGizmo& Gizmo)
	{
		return Gizmo.centerCube;
	}

	const FGizmoMesh& SelectRotationAxisMesh(const FRotationGizmo& Gizmo, EAxis Axis)
	{
		switch (Axis)
		{
		case EAxis::X:
			return Gizmo.ringX;
		case EAxis::Y:
			return Gizmo.ringY;
		case EAxis::Z:
		default:
			return Gizmo.ringZ;
		}
	}

	const FGizmoMesh& SelectRotationScreenMesh(const FRotationGizmo& Gizmo)
	{
		return Gizmo.screenRing;
	}
}

const FString FPrimitiveGizmo::Key = "Gizmo";
const FString FPrimitiveGizmo::FilePath = "Assets/Meshed/Gizmo.mesh";

FPrimitiveGizmo::FPrimitiveGizmo(EGizmoType Type)
	: GizmoType(Type)
{
	FString CacheKey = GetKey(GizmoType);

	auto It = GizmoCache.find(CacheKey);
	if (It != GizmoCache.end())
	{
		RenderMesh = It->second;
	}
	else
	{
		Generate(GizmoType);
	}
}

void FPrimitiveGizmo::ClearCache()
{
	GizmoCache.clear();
}

void FPrimitiveGizmo::Generate(EGizmoType Type)
{
	GizmoType = Type;

	if (Type == EGizmoType::Translation)
	{
		GenerateTranslationGizmoMesh();
	}

	if (Type == EGizmoType::Rotation)
	{
		GenerateRotationGizmoMesh();
	}

	if (Type == EGizmoType::Scale)
	{
		GenerateScaleGizmoMesh();
	}
}

FString FPrimitiveGizmo::GetKey(EGizmoType Type)
{
	switch (Type)
	{
	case EGizmoType::Translation:
		return Key + "_Translation";
	case EGizmoType::Rotation:
		return Key + "_Rotation";
	case EGizmoType::Scale:
	default:
		return Key + "_Scale";
	}
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateTranslationAxisMesh(EAxis Axis)
{
	const FTranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationAxisMesh(Gizmo, Axis));
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateTranslationAxisMesh(EAxis Axis, const FVector4& OverrideColor)
{
	const FTranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationAxisMesh(Gizmo, Axis), OverrideColor);
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateTranslationPlaneMesh(ETranslationPlane Plane)
{
	const FTranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationPlaneMesh(Gizmo, Plane));
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateTranslationPlaneMesh(ETranslationPlane Plane, const FVector4& OverrideColor)
{
	const FTranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationPlaneMesh(Gizmo, Plane), OverrideColor);
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateTranslationScreenMesh()
{
	const FTranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationScreenMesh(Gizmo));
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateTranslationScreenMesh(const FVector4& OverrideColor)
{
	const FTranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationScreenMesh(Gizmo), OverrideColor);
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateRotationAxisMesh(EAxis Axis)
{
	return CreateRotationAxisMesh(Axis, MakeRotationDesc());
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateRotationAxisMesh(EAxis Axis, const FVector4& OverrideColor)
{
	return CreateRotationAxisMesh(Axis, MakeRotationDesc(), OverrideColor);
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateRotationAxisMesh(EAxis Axis, const FRotationGizmoDesc& Desc)
{
	const FRotationGizmo Gizmo = GenerateRotationGizmo(Desc);
	return CreateMeshDataFromMesh(SelectRotationAxisMesh(Gizmo, Axis));
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateRotationAxisMesh(EAxis Axis, const FRotationGizmoDesc& Desc, const FVector4& OverrideColor)
{
	const FRotationGizmo Gizmo = GenerateRotationGizmo(Desc);
	return CreateMeshDataFromMesh(SelectRotationAxisMesh(Gizmo, Axis), OverrideColor);
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateRotationScreenMesh(const FRotationGizmoDesc& Desc)
{
	const FRotationGizmo Gizmo = GenerateRotationGizmo(Desc);
	return CreateMeshDataFromMesh(SelectRotationScreenMesh(Gizmo));
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateRotationScreenMesh(const FRotationGizmoDesc& Desc, const FVector4& OverrideColor)
{
	const FRotationGizmo Gizmo = GenerateRotationGizmo(Desc);
	return CreateMeshDataFromMesh(SelectRotationScreenMesh(Gizmo), OverrideColor);
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateScaleAxisMesh(EAxis Axis)
{
	const FScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScaleAxisMesh(Gizmo, Axis));
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateScaleAxisMesh(EAxis Axis, const FVector4& OverrideColor)
{
	const FScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScaleAxisMesh(Gizmo, Axis), OverrideColor);
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateScalePlaneMesh(EScalePlane Plane)
{
	const FScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScalePlaneMesh(Gizmo, Plane));
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateScalePlaneMesh(EScalePlane Plane, const FVector4& OverrideColor)
{
	const FScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScalePlaneMesh(Gizmo, Plane), OverrideColor);
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateScaleCenterMesh()
{
	const FScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScaleCenterMesh(Gizmo));
}

std::shared_ptr<FDynamicMesh> FPrimitiveGizmo::CreateScaleCenterMesh(const FVector4& OverrideColor)
{
	const FScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScaleCenterMesh(Gizmo), OverrideColor);
}

void FPrimitiveGizmo::GenerateTranslationGizmoMesh()
{
	FTranslationGizmoDesc desc{};
	auto FTranslationGizmo = GenerateTranslationGizmo(desc);
	FGizmoMesh Gizmo = Combine(FTranslationGizmo);
	auto Data = CreateMeshDataFromMesh(Gizmo);

	RenderMesh = Data;
	GizmoCache[GetKey(EGizmoType::Translation)] = Data;
}

void FPrimitiveGizmo::GenerateRotationGizmoMesh()
{
	FRotationGizmoDesc desc = MakeRotationDesc();
	auto FRotationGizmo = GenerateRotationGizmo(desc);
	FGizmoMesh Gizmo = Combine(FRotationGizmo);
	auto Data = CreateMeshDataFromMesh(Gizmo);

	RenderMesh = Data;
	GizmoCache[GetKey(EGizmoType::Rotation)] = Data;
}

void FPrimitiveGizmo::GenerateScaleGizmoMesh()
{
	FScaleGizmoDesc desc{};
	auto FScaleGizmo = GenerateScaleGizmo(desc);
	FGizmoMesh Gizmo = Combine(FScaleGizmo);
	auto Data = CreateMeshDataFromMesh(Gizmo);

	RenderMesh = Data;
	GizmoCache[GetKey(EGizmoType::Scale)] = Data;
}

