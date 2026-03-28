#pragma once

#include <cstdint>
#include <initializer_list>
#include <vector>
#include "Math/Vector.h"


struct FGizmoVec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct FGizmoColor
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct FGizmoVertex
{
    FVector position;
    FVector normal;
    FGizmoVec2 uv;
    FGizmoColor color;
};

struct FGizmoMesh
{
    std::vector<FGizmoVertex> vertices;
    std::vector<std::uint32_t> indices;

    void Clear();
    bool Empty() const;
};

enum class EGizmoAxisId : std::uint32_t
{
    None = 0,
    X,
    Y,
    Z,
    XYZ,
    Screen,
};

struct FTranslationGizmoDesc
{
    float uniformScale = 1.0f;
    int transformGizmoSize = 0;
    bool includeScreenHandle = true;
    bool leftUpForward = false;
};

struct FRotationGizmoDesc
{
    float uniformScale = 1.0f;
    int transformGizmoSize = 0;
    FVector cameraDirection = FVector(-1.0f, -1.0f, -1.0f);
    FVector viewUp = FVector(0.0f, 1.0f, 0.0f);
    FVector viewRight = FVector(1.0f, 0.0f, 0.0f);
    bool orthographic = false;
    bool fullAxisRings = false;
    bool includeInnerDisk = false;
    bool includeScreenRing = true;
    bool includeArcball = true;
    bool dragging = false;
    EGizmoAxisId activeAxis = EGizmoAxisId::None;
    float deltaRotationDegrees = 0.0f;
};

struct FScaleGizmoDesc
{
    float uniformScale = 1.0f;
    int transformGizmoSize = 0;
    bool includeCenterCube = true;
    bool leftUpForward = false;
};

struct FTranslationGizmo
{
    FGizmoMesh axisX;
    FGizmoMesh axisY;
    FGizmoMesh axisZ;
    FGizmoMesh planeXY;
    FGizmoMesh planeXZ;
    FGizmoMesh planeYZ;
    FGizmoMesh screenSphere;
};

struct FRotationGizmo
{
    FGizmoMesh ringX;
    FGizmoMesh ringY;
    FGizmoMesh ringZ;
    FGizmoMesh screenRing;
    FGizmoMesh arcball;
};

struct FScaleGizmo
{
    FGizmoMesh axisX;
    FGizmoMesh axisY;
    FGizmoMesh axisZ;
    FGizmoMesh planeXY;
    FGizmoMesh planeXZ;
    FGizmoMesh planeYZ;
    FGizmoMesh centerCube;
};

FTranslationGizmo GenerateTranslationGizmo(const FTranslationGizmoDesc& desc = {});
FRotationGizmo GenerateRotationGizmo(const FRotationGizmoDesc& desc = {});
FScaleGizmo GenerateScaleGizmo(const FScaleGizmoDesc& desc = {});

void AppendMesh(FGizmoMesh& destination, const FGizmoMesh& source);
FGizmoMesh MergeMeshes(std::initializer_list<const FGizmoMesh*> meshes);

FGizmoMesh Combine(const FTranslationGizmo& gizmo);
FGizmoMesh Combine(const FRotationGizmo& gizmo);
FGizmoMesh Combine(const FScaleGizmo& gizmo);

FGizmoColor AxisColorX();
FGizmoColor AxisColorY();
FGizmoColor AxisColorZ();
FGizmoColor ScreenAxisColor();
FGizmoColor ScreenSpaceColor();
FGizmoColor ArcballColor();
FGizmoColor HighlightColor();
