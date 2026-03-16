#include "Resource/FBuiltInMeshRepository.h"
#include "Resource/BuiltInMeshFactory.h"

bool FBuiltInMeshRepository::Initialize()
{
	Meshes.SphereMesh = BuiltInMeshFactory::CreateSphereMesh();
	Meshes.CubeMesh = BuiltInMeshFactory::CreateCubeMesh();
	Meshes.TorusMesh = BuiltInMeshFactory::CreateTorusMesh(64, 32, 1.2f, 0.35f);
	Meshes.AxesMesh = BuiltInMeshFactory::CreateAxesMesh();
	Meshes.GizmoArrowMesh = BuiltInMeshFactory::CreateGizmoArrowMesh();
	Meshes.ClickCircleMesh = BuiltInMeshFactory::CreateDiscMesh(64);

	return true;
}

const FBuiltInMeshes& FBuiltInMeshRepository::Get() const
{
	return Meshes;
}