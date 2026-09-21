#pragma once

class UStaticMesh;

struct FBuiltInMeshes
{
	UStaticMesh* CubeMesh = nullptr;
	UStaticMesh* SphereMesh = nullptr;
	UStaticMesh* TorusMesh = nullptr;
	UStaticMesh* AxesMesh = nullptr;
	UStaticMesh* GridMesh = nullptr; // 그리드 안 쓰면 nullptr 그대로 두셔도 됩니다.
	UStaticMesh* GizmoArrowMesh = nullptr;
	UStaticMesh* GizmoShaftMesh = nullptr;
	UStaticMesh* ClickCircleMesh = nullptr;
};