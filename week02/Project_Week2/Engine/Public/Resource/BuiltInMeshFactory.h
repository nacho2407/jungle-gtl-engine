#pragma once

class UStaticMesh;

namespace BuiltInMeshFactory
{
    UStaticMesh* CreateSphereMesh();
    UStaticMesh* CreateCubeMesh();
    UStaticMesh* CreateTriangleMesh();
    UStaticMesh* CreateAxesMesh();
    UStaticMesh* CreateGizmoArrowMesh();
    //UStaticMesh* CreateCircleMesh(int Segments = 48);
    UStaticMesh* CreateDiscMesh(int Segments = 48);
}