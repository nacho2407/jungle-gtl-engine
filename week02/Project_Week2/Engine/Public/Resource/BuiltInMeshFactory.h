#pragma once

class UStaticMesh;
class UObject;

namespace BuiltInMeshFactory
{
    UStaticMesh* CreateSphereMesh(UObject* Owner = nullptr);
    UStaticMesh* CreateCubeMesh(UObject* Owner = nullptr);
    UStaticMesh* CreateTriangleMesh(UObject* Owner = nullptr);
    UStaticMesh* CreateAxesMesh(UObject* Owner = nullptr);
    UStaticMesh* CreateTorusMesh(
        int MajorSegments = 48,
        int MinorSegments = 24,
        float MajorRadius = 1.2f,
        float MinorRadius = 0.5f,
        UObject* Owner = nullptr);
    UStaticMesh* CreateGizmoArrowMesh(UObject* Owner = nullptr);
    UStaticMesh* CreateGizmoScaleMesh(UObject* Owner = nullptr);
    //UStaticMesh* CreateCircleMesh(int Segments = 48);
    UStaticMesh* CreateDiscMesh(int Segments = 48, UObject* Owner = nullptr);
    UStaticMesh* CreateGridMesh(int HalfCount = 10, float Spacing = 1.0f, UObject* Owner = nullptr);
    UStaticMesh* CreateGizmoRotateRingMesh(UObject* Owner = nullptr);
}
