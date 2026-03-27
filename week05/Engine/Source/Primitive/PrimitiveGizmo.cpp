#include "PrimitiveGizmo.h"

#include "UnrealEditorStyledGizmo.h"

#include <optional>

namespace
{
	RotationDesc MakeRotationDesc()
	{
		RotationDesc Desc{};
		Desc.fullAxisRings = false;
		Desc.includeInnerDisk = false;
		Desc.includeScreenRing = true;
		Desc.includeArcball = false;
		return Desc;
	}

	std::shared_ptr<FMeshData> CreateMeshDataFromMesh(const Mesh& GizmoMesh, const std::optional<FVector4>& OverrideColor = std::nullopt)
	{
		if (GizmoMesh.vertices.empty() || GizmoMesh.indices.empty())
		{
			return nullptr;
		}

		auto Data = std::make_shared<FMeshData>();
		Data->Vertices.reserve(GizmoMesh.vertices.size());
		for (const auto& Vertex : GizmoMesh.vertices)
		{
			FVector4 Color = OverrideColor.value_or(FVector4(Vertex.color.r, Vertex.color.g, Vertex.color.b, Vertex.color.a));
			Data->Vertices.push_back({ Vertex.position, Color, Vertex.normal });
		}

		Data->Indices.reserve(GizmoMesh.indices.size());
		for (auto Index : GizmoMesh.indices)
		{
			Data->Indices.push_back(Index);
		}

		Data->Topology = EMeshTopology::EMT_TriangleList;

		return Data;
	}

	const Mesh& SelectTranslationAxisMesh(const TranslationGizmo& Gizmo, EAxis Axis)
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

	const Mesh& SelectTranslationPlaneMesh(const TranslationGizmo& Gizmo, CPrimitiveGizmo::ETranslationPlane Plane)
	{
		switch (Plane)
		{
		case CPrimitiveGizmo::ETranslationPlane::XY:
			return Gizmo.planeXY;
		case CPrimitiveGizmo::ETranslationPlane::XZ:
			return Gizmo.planeXZ;
		case CPrimitiveGizmo::ETranslationPlane::YZ:
		default:
			return Gizmo.planeYZ;
		}
	}

	const Mesh& SelectTranslationScreenMesh(const TranslationGizmo& Gizmo)
	{
		return Gizmo.screenSphere;
	}

	const Mesh& SelectScaleAxisMesh(const ScaleGizmo& Gizmo, EAxis Axis)
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

	const Mesh& SelectScalePlaneMesh(const ScaleGizmo& Gizmo, CPrimitiveGizmo::EScalePlane Plane)
	{
		switch (Plane)
		{
		case CPrimitiveGizmo::EScalePlane::XY:
			return Gizmo.planeXY;
		case CPrimitiveGizmo::EScalePlane::XZ:
			return Gizmo.planeXZ;
		case CPrimitiveGizmo::EScalePlane::YZ:
		default:
			return Gizmo.planeYZ;
		}
	}

	const Mesh& SelectScaleCenterMesh(const ScaleGizmo& Gizmo)
	{
		return Gizmo.centerCube;
	}

	const Mesh& SelectRotationAxisMesh(const RotationGizmo& Gizmo, EAxis Axis)
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

	const Mesh& SelectRotationScreenMesh(const RotationGizmo& Gizmo)
	{
		return Gizmo.screenRing;
	}
}

const FString CPrimitiveGizmo::Key = "Gizmo";
const FString CPrimitiveGizmo::FilePath = "Assets/Meshed/Gizmo.mesh";

CPrimitiveGizmo::CPrimitiveGizmo(EGizmoType Type)
	: GizmoType(Type)
{
	auto Cached = GetCached(GetKey(GizmoType));
	if (Cached)
	{
		MeshData = Cached;
	}
	else
	{
		Generate(GizmoType);
	}
}

void CPrimitiveGizmo::Generate(EGizmoType Type)
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

FString CPrimitiveGizmo::GetKey(EGizmoType Type)
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

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateTranslationAxisMesh(EAxis Axis)
{
	const TranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationAxisMesh(Gizmo, Axis));
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateTranslationAxisMesh(EAxis Axis, const FVector4& OverrideColor)
{
	const TranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationAxisMesh(Gizmo, Axis), OverrideColor);
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateTranslationPlaneMesh(ETranslationPlane Plane)
{
	const TranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationPlaneMesh(Gizmo, Plane));
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateTranslationPlaneMesh(ETranslationPlane Plane, const FVector4& OverrideColor)
{
	const TranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationPlaneMesh(Gizmo, Plane), OverrideColor);
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateTranslationScreenMesh()
{
	const TranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationScreenMesh(Gizmo));
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateTranslationScreenMesh(const FVector4& OverrideColor)
{
	const TranslationGizmo Gizmo = GenerateTranslationGizmo();
	return CreateMeshDataFromMesh(SelectTranslationScreenMesh(Gizmo), OverrideColor);
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateRotationAxisMesh(EAxis Axis)
{
	return CreateRotationAxisMesh(Axis, MakeRotationDesc());
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateRotationAxisMesh(EAxis Axis, const FVector4& OverrideColor)
{
	return CreateRotationAxisMesh(Axis, MakeRotationDesc(), OverrideColor);
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateRotationAxisMesh(EAxis Axis, const RotationDesc& Desc)
{
	const RotationGizmo Gizmo = GenerateRotationGizmo(Desc);
	return CreateMeshDataFromMesh(SelectRotationAxisMesh(Gizmo, Axis));
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateRotationAxisMesh(EAxis Axis, const RotationDesc& Desc, const FVector4& OverrideColor)
{
	const RotationGizmo Gizmo = GenerateRotationGizmo(Desc);
	return CreateMeshDataFromMesh(SelectRotationAxisMesh(Gizmo, Axis), OverrideColor);
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateRotationScreenMesh(const RotationDesc& Desc)
{
	const RotationGizmo Gizmo = GenerateRotationGizmo(Desc);
	return CreateMeshDataFromMesh(SelectRotationScreenMesh(Gizmo));
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateRotationScreenMesh(const RotationDesc& Desc, const FVector4& OverrideColor)
{
	const RotationGizmo Gizmo = GenerateRotationGizmo(Desc);
	return CreateMeshDataFromMesh(SelectRotationScreenMesh(Gizmo), OverrideColor);
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateScaleAxisMesh(EAxis Axis)
{
	const ScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScaleAxisMesh(Gizmo, Axis));
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateScaleAxisMesh(EAxis Axis, const FVector4& OverrideColor)
{
	const ScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScaleAxisMesh(Gizmo, Axis), OverrideColor);
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateScalePlaneMesh(EScalePlane Plane)
{
	const ScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScalePlaneMesh(Gizmo, Plane));
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateScalePlaneMesh(EScalePlane Plane, const FVector4& OverrideColor)
{
	const ScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScalePlaneMesh(Gizmo, Plane), OverrideColor);
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateScaleCenterMesh()
{
	const ScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScaleCenterMesh(Gizmo));
}

std::shared_ptr<FMeshData> CPrimitiveGizmo::CreateScaleCenterMesh(const FVector4& OverrideColor)
{
	const ScaleGizmo Gizmo = GenerateScaleGizmo();
	return CreateMeshDataFromMesh(SelectScaleCenterMesh(Gizmo), OverrideColor);
}

void CPrimitiveGizmo::GenerateTranslationGizmoMesh()
{
	TranslationDesc desc{};
	auto TranslationGizmo = GenerateTranslationGizmo(desc);
	Mesh Gizmo = Combine(TranslationGizmo);
	auto Data = CreateMeshDataFromMesh(Gizmo);
	MeshData = Data;
	RegisterMeshData(GetKey(EGizmoType::Translation), Data);
}

void CPrimitiveGizmo::GenerateRotationGizmoMesh()
{
	RotationDesc desc = MakeRotationDesc();
	auto RotationGizmo = GenerateRotationGizmo(desc);
	Mesh Gizmo = Combine(RotationGizmo);
	auto Data = CreateMeshDataFromMesh(Gizmo);
	MeshData = Data;
	RegisterMeshData(GetKey(EGizmoType::Rotation), Data);
}

void CPrimitiveGizmo::GenerateScaleGizmoMesh()
{
	ScaleDesc desc{};
	auto ScaleGizmo = GenerateScaleGizmo(desc);
	Mesh Gizmo = Combine(ScaleGizmo);
	auto Data = CreateMeshDataFromMesh(Gizmo);
	MeshData = Data;
	RegisterMeshData(GetKey(EGizmoType::Scale), Data);
}

