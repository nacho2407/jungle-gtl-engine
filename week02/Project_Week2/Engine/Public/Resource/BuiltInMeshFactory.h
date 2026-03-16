#pragma once

class UStaticMesh;

namespace BuiltInMeshFactory
{
    UStaticMesh* CreateSphereMesh();
    UStaticMesh* CreateCubeMesh();
    UStaticMesh* CreateTriangleMesh();
    UStaticMesh* CreateAxesMesh();
    UStaticMesh* CreateTorusMesh(
        int MajorSegments = 48,
        int MinorSegments = 24,
        float MajorRadius = 1.2f,
        float MinorRadius = 0.35f);
    UStaticMesh* CreateGizmoArrowMesh();
    //UStaticMesh* CreateCircleMesh(int Segments = 48);
    UStaticMesh* CreateDiscMesh(int Segments = 48);
}