#pragma once

class UStaticMesh;

namespace BuiltInMeshFactory
{
    UStaticMesh* CreateSphereMesh();
    UStaticMesh* CreateCubeMesh();
    UStaticMesh* CreateTriangleMesh();
    UStaticMesh* CreateAxesMesh();
}