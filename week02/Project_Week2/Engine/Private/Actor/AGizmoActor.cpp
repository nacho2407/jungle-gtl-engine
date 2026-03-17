#include "Actor/AGizmoActor.h"

#include <cfloat>
#include <cmath>

#include "Component/UStaticMeshComponent.h"
#include "Component/USceneComponent.h"
#include "Component/UCameraComponent.h"
#include "Resource/UStaticMesh.h"
#include "Math/FMatrix.h"
#include "Math/FVector4.h"

AGizmoActor::AGizmoActor()
{
    //auto ArrowMesh = BuiltInMeshFactory::CreateGizmoArrowMesh();
    //if (!ArrowMesh)
    //{
    //    return;
    //}

    //if (!XAxisComp)
    //{
    //    XAxisComp = new UStaticMeshComponent();
    //    AddComponent(XAxisComp);
    //}

    //if (!YAxisComp)
    //{
    //    YAxisComp = new UStaticMeshComponent();
    //    AddComponent(YAxisComp);
    //}

    //if (!ZAxisComp)
    //{
    //    ZAxisComp = new UStaticMeshComponent();
    //    AddComponent(ZAxisComp);
    //}

    //XAxisComp->SetStaticMesh(ArrowMesh);
    //YAxisComp->SetStaticMesh(ArrowMesh);
    //ZAxisComp->SetStaticMesh(ArrowMesh);

    //XAxisComp->SetRelativeLocation(FVector::ZeroVector);
    //YAxisComp->SetRelativeLocation(FVector::ZeroVector);
    //ZAxisComp->SetRelativeLocation(FVector::ZeroVector);

    //// Arrow mesh가 +X 방향 기준이라고 가정
    //XAxisComp->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
    //YAxisComp->SetRelativeRotation(FVector(0.0f, 0.0f, 1.5707963f));
    //ZAxisComp->SetRelativeRotation(FVector(0.0f, -1.5707963f, 0.0f));

    //XAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));
    //YAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));
    //ZAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));
    //XAxisComp = new UStaticMeshComponent();
    //YAxisComp = new UStaticMeshComponent();
    //ZAxisComp = new UStaticMeshComponent();

    //AddComponent(XAxisComp);
    //AddComponent(YAxisComp);
    //AddComponent(ZAxisComp);

    //XAxisComp->SetRenderColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    //YAxisComp->SetRenderColor(FVector4(0.0f, 1.0f, 0.0f, 1.0f));
    //ZAxisComp->SetRenderColor(FVector4(0.0f, 0.45f, 1.0f, 1.0f));

    //XAxisComp->SetDepthEnable(true);
    //YAxisComp->SetDepthEnable(true);
    //ZAxisComp->SetDepthEnable(true);
    //XAxisComp->SetDepthWrite(false);
    //YAxisComp->SetDepthWrite(false);
    //ZAxisComp->SetDepthWrite(false);

    //XAxisComp->SetCullMode(ERenderCullMode::None);
    //YAxisComp->SetCullMode(ERenderCullMode::None);
    //ZAxisComp->SetCullMode(ERenderCullMode::None);

    //XAxisComp->SetVisibility(false);
    //YAxisComp->SetVisibility(false);
    //ZAxisComp->SetVisibility(false);

    //XAxisShaftComp = new UStaticMeshComponent();
    //YAxisShaftComp = new UStaticMeshComponent();
    //ZAxisShaftComp = new UStaticMeshComponent();

    //AddComponent(XAxisShaftComp);
    //AddComponent(YAxisShaftComp);
    //AddComponent(ZAxisShaftComp);

    //XAxisShaftComp->SetRenderColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    //YAxisShaftComp->SetRenderColor(FVector4(0.0f, 1.0f, 0.0f, 1.0f));
    //ZAxisShaftComp->SetRenderColor(FVector4(0.0f, 0.45f, 1.0f, 1.0f));

    //XAxisShaftComp->SetVisibility(false);
    //YAxisShaftComp->SetVisibility(false);
    //ZAxisShaftComp->SetVisibility(false);

    //SetRootComponent(XAxisComp);
}

AGizmoActor::~AGizmoActor()
{
}

void AGizmoActor::Initialize(UStaticMesh* ArrowMesh, UStaticMesh* InScaleMesh, UStaticMesh* CubeMesh, UStaticMesh* TorusMesh)
{
    if (!ArrowMesh || !InScaleMesh || !CubeMesh || !TorusMesh)
    {
        return;
    }

    TranslateMesh = ArrowMesh;
    ScaleMesh = InScaleMesh;
    RotateMesh = TorusMesh;

    if (!PivotComp)
    {
        PivotComp = new USceneComponent();
        AddComponent(PivotComp);
        SetRootComponent(PivotComp);
    }

    if (!XAxisComp)
    {
        XAxisComp = new UStaticMeshComponent();
        AddComponent(XAxisComp);
        XAxisComp->SetupAttachment(PivotComp);
    }

    if (!YAxisComp)
    {
        YAxisComp = new UStaticMeshComponent();
        AddComponent(YAxisComp);
        YAxisComp->SetupAttachment(PivotComp);
    }

    if (!ZAxisComp)
    {
        ZAxisComp = new UStaticMeshComponent();
        AddComponent(ZAxisComp);
        ZAxisComp->SetupAttachment(PivotComp);
    }

    if (!XAxisShaftComp)
    {
        XAxisShaftComp = new UStaticMeshComponent();
        AddComponent(XAxisShaftComp);
        XAxisShaftComp->SetupAttachment(PivotComp);
    }

    if (!YAxisShaftComp)
    {
        YAxisShaftComp = new UStaticMeshComponent();
        AddComponent(YAxisShaftComp);
        YAxisShaftComp->SetupAttachment(PivotComp);
    }

    if (!ZAxisShaftComp)
    {
        ZAxisShaftComp = new UStaticMeshComponent();
        AddComponent(ZAxisShaftComp);
        ZAxisShaftComp->SetupAttachment(PivotComp);
    }

    //XAxisComp->SetStaticMesh(ArrowMesh);
    //YAxisComp->SetStaticMesh(ArrowMesh);
    //ZAxisComp->SetStaticMesh(ArrowMesh);

    //XAxisComp->SetRelativeLocation(FVector::ZeroVector);
    //YAxisComp->SetRelativeLocation(FVector::ZeroVector);
    //ZAxisComp->SetRelativeLocation(FVector::ZeroVector);

    //// Arrow mesh가 +X 방향 기준이라고 가정
    //XAxisComp->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
    //YAxisComp->SetRelativeRotation(FVector(0.0f, 0.0f, 1.5707963f));
    //ZAxisComp->SetRelativeRotation(FVector(0.0f, -1.5707963f, 0.0f));

    //XAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));
    //YAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));
    //ZAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));

    XAxisComp->SetRenderColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    YAxisComp->SetRenderColor(FVector4(0.0f, 1.0f, 0.0f, 1.0f));
    ZAxisComp->SetRenderColor(FVector4(0.0f, 0.45f, 1.0f, 1.0f));

    XAxisComp->SetDepthEnable(false);
    YAxisComp->SetDepthEnable(false);
    ZAxisComp->SetDepthEnable(false);
    XAxisComp->SetDepthWrite(false);
    YAxisComp->SetDepthWrite(false);
    ZAxisComp->SetDepthWrite(false);

    XAxisComp->SetCullMode(ERenderCullMode::None);
    YAxisComp->SetCullMode(ERenderCullMode::None);
    ZAxisComp->SetCullMode(ERenderCullMode::None);

    XAxisComp->SetVisibility(false);
    YAxisComp->SetVisibility(false);
    ZAxisComp->SetVisibility(false);

    XAxisShaftComp->SetRenderColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    YAxisShaftComp->SetRenderColor(FVector4(0.0f, 1.0f, 0.0f, 1.0f));
    ZAxisShaftComp->SetRenderColor(FVector4(0.0f, 0.45f, 1.0f, 1.0f));

    XAxisShaftComp->SetDepthEnable(false);
    YAxisShaftComp->SetDepthEnable(false);
    ZAxisShaftComp->SetDepthEnable(false);

    XAxisShaftComp->SetDepthWrite(false);
    YAxisShaftComp->SetDepthWrite(false);
    ZAxisShaftComp->SetDepthWrite(false);

    XAxisShaftComp->SetCullMode(ERenderCullMode::None);
    YAxisShaftComp->SetCullMode(ERenderCullMode::None);
    ZAxisShaftComp->SetCullMode(ERenderCullMode::None);

    XAxisShaftComp->SetVisibility(false);
    YAxisShaftComp->SetVisibility(false);
    ZAxisShaftComp->SetVisibility(false);

    //SetRootComponent(XAxisComp);
    SetRootComponent(PivotComp);
}

void AGizmoActor::SetTargetActor(AActor* InTarget)
{
    TargetActor = InTarget;

    if (!TargetActor)
    {
        SetGizmoVisible(false);
        return;
    }

    UpdateTransformFromTarget();
    SetGizmoVisible(true);
}

AActor* AGizmoActor::GetTargetActor() const
{
    return TargetActor;
}

void AGizmoActor::SetGizmoVisible(bool bVisible)
{
    if (XAxisComp) XAxisComp->SetVisibility(bVisible);
    if (YAxisComp) YAxisComp->SetVisibility(bVisible);
    if (ZAxisComp) ZAxisComp->SetVisibility(bVisible);

    if (XAxisShaftComp) XAxisShaftComp->SetVisibility(bVisible && CurrentMode == EGizmoMode::Scale);
    if (YAxisShaftComp) YAxisShaftComp->SetVisibility(bVisible && CurrentMode == EGizmoMode::Scale);
    if (ZAxisShaftComp) ZAxisShaftComp->SetVisibility(bVisible && CurrentMode == EGizmoMode::Scale);
}

bool AGizmoActor::IsGizmoVisible() const
{
    return XAxisComp && XAxisComp->IsVisible();
}

void AGizmoActor::UpdateTransformFromTarget()
{
    if (!TargetActor || !PivotComp)
    {
        return;
    }

    USceneComponent* Root = TargetActor->GetRootComponent();
    if (!Root)
    {
        return;
    }

    //const FVector Pos = Root->GetRelativeLocation();

    //if (XAxisComp) XAxisComp->SetRelativeLocation(Pos);
    //if (YAxisComp) YAxisComp->SetRelativeLocation(Pos);
    //if (ZAxisComp) ZAxisComp->SetRelativeLocation(Pos);
    PivotComp->SetRelativeLocation(Root->GetRelativeLocation());
    PivotComp->SetRelativeRotation(Root->GetRelativeRotation());
    PivotComp->SetRelativeScale(FVector::OneVector);

    //if (CurrentMode == EGizmoMode::Scale)
    //{
    //    if (XAxisComp) XAxisComp->SetRelativeLocation(FVector(AxisLength, 0.0f, 0.0f));
    //    if (YAxisComp) YAxisComp->SetRelativeLocation(FVector(0.0f, AxisLength, 0.0f));
    //    if (ZAxisComp) ZAxisComp->SetRelativeLocation(FVector(0.0f, 0.0f, AxisLength));

    //    if (XAxisShaftComp) XAxisShaftComp->SetRelativeLocation(FVector(AxisLength * 0.5f, 0.0f, 0.0f));
    //    if (YAxisShaftComp) YAxisShaftComp->SetRelativeLocation(FVector(0.0f, AxisLength * 0.5f, 0.0f));
    //    if (ZAxisShaftComp) ZAxisShaftComp->SetRelativeLocation(FVector(0.0f, 0.0f, AxisLength * 0.5f));

    //    if (XAxisShaftComp) XAxisShaftComp->SetVisibility(IsGizmoVisible());
    //    if (YAxisShaftComp) YAxisShaftComp->SetVisibility(IsGizmoVisible());
    //    if (ZAxisShaftComp) ZAxisShaftComp->SetVisibility(IsGizmoVisible());
    //}
    //else
    //{
    //    if (XAxisComp) XAxisComp->SetRelativeLocation(FVector::ZeroVector);
    //    if (YAxisComp) YAxisComp->SetRelativeLocation(FVector::ZeroVector);
    //    if (ZAxisComp) ZAxisComp->SetRelativeLocation(FVector::ZeroVector);

    //    if (XAxisShaftComp) XAxisShaftComp->SetVisibility(false);
    //    if (YAxisShaftComp) YAxisShaftComp->SetVisibility(false);
    //    if (ZAxisShaftComp) ZAxisShaftComp->SetVisibility(false);
    //}
    if (XAxisComp) XAxisComp->SetRelativeLocation(FVector::ZeroVector);
    if (YAxisComp) YAxisComp->SetRelativeLocation(FVector::ZeroVector);
    if (ZAxisComp) ZAxisComp->SetRelativeLocation(FVector::ZeroVector);
}

void AGizmoActor::UpdateColors(EGizmoAxis HighlightAxis)
{
    if (!XAxisComp || !YAxisComp || !ZAxisComp)
    {
        return;
    }

    const FVector4 XBase(1.0f, 0.0f, 0.0f, 1.0f);
    const FVector4 YBase(0.0f, 1.0f, 0.0f, 1.0f);
    const FVector4 ZBase(0.0f, 0.45f, 1.0f, 1.0f);

    if (XAxisShaftComp)
    {
        XAxisShaftComp->SetRenderColor(
            HighlightAxis == EGizmoAxis::X ? LightenColor(XBase, 0.45f) : XBase);
    }

    if (YAxisShaftComp)
    {
        YAxisShaftComp->SetRenderColor(
            HighlightAxis == EGizmoAxis::Y ? LightenColor(YBase, 0.45f) : YBase);
    }

    if (ZAxisShaftComp)
    {
        ZAxisShaftComp->SetRenderColor(
            HighlightAxis == EGizmoAxis::Z ? LightenColor(ZBase, 0.45f) : ZBase);
    }
}

EGizmoAxis AGizmoActor::PickAxis(
    int MouseX,
    int MouseY,
    const UCameraComponent* Camera,
    int ViewWidth,
    int ViewHeight) const
{
    if (!TargetActor || !Camera || !IsGizmoVisible())
    {
        return EGizmoAxis::None;
    }

    switch (CurrentMode)
    {
    case EGizmoMode::Translate:
        return PickAxisTranslate(MouseX, MouseY, Camera, ViewWidth, ViewHeight);
    case EGizmoMode::Rotate:
        return PickAxisRotate(MouseX, MouseY, Camera, ViewWidth, ViewHeight);
    case EGizmoMode::Scale:
        return PickAxisScale(MouseX, MouseY, Camera, ViewWidth, ViewHeight);
    default:
        return EGizmoAxis::None;
    }
}

UStaticMeshComponent* AGizmoActor::GetAxisComponent(EGizmoAxis Axis) const
{
    switch (Axis)
    {
    case EGizmoAxis::X: return XAxisComp;
    case EGizmoAxis::Y: return YAxisComp;
    case EGizmoAxis::Z: return ZAxisComp;
    default: return nullptr;
    }
}

bool AGizmoActor::ProjectWorldToScreen(
    const FVector& WorldPos,
    const UCameraComponent* Camera,
    int ViewWidth,
    int ViewHeight,
    float& OutX,
    float& OutY) const
{
    if (!Camera || ViewWidth <= 0 || ViewHeight <= 0)
    {
        return false;
    }

    const FMatrix View = Camera->GetViewMatrix();
    const FMatrix Projection = Camera->GetProjectionMatrix();

    FVector4 ClipPos = Projection * (View * FVector4(WorldPos, 1.0f));

    if (ClipPos.W <= 0.0001f)
    {
        return false;
    }

    const float InvW = 1.0f / ClipPos.W;
    const float NdcX = ClipPos.X * InvW;
    const float NdcY = ClipPos.Y * InvW;

    OutX = (NdcX * 0.5f + 0.5f) * static_cast<float>(ViewWidth);
    OutY = (1.0f - (NdcY * 0.5f + 0.5f)) * static_cast<float>(ViewHeight);

    return true;
}

float AGizmoActor::DistancePointToSegment2D(
    float Px, float Py,
    float Ax, float Ay,
    float Bx, float By) const
{
    const float ABx = Bx - Ax;
    const float ABy = By - Ay;
    const float APx = Px - Ax;
    const float APy = Py - Ay;

    const float ABLenSq = ABx * ABx + ABy * ABy;
    if (ABLenSq <= 0.000001f)
    {
        const float Dx = Px - Ax;
        const float Dy = Py - Ay;
        return std::sqrt(Dx * Dx + Dy * Dy);
    }

    float T = (APx * ABx + APy * ABy) / ABLenSq;
    if (T < 0.0f) T = 0.0f;
    if (T > 1.0f) T = 1.0f;

    const float ClosestX = Ax + ABx * T;
    const float ClosestY = Ay + ABy * T;

    const float Dx = Px - ClosestX;
    const float Dy = Py - ClosestY;
    return std::sqrt(Dx * Dx + Dy * Dy);
}

FVector4 AGizmoActor::LightenColor(const FVector4& Color, float T) const
{
    return FVector4(
        Color.X + (1.0f - Color.X) * T,
        Color.Y + (1.0f - Color.Y) * T,
        Color.Z + (1.0f - Color.Z) * T,
        Color.W);
}

void AGizmoActor::SetMode(EGizmoMode InMode)
{
    if (CurrentMode == InMode)
    {
        return;
    }

    CurrentMode = InMode;
    ApplyModeVisual();
    UpdateTransformFromTarget();
}

EGizmoMode AGizmoActor::GetMode() const
{
    return CurrentMode;
}
void AGizmoActor::ApplyModeVisual()
{
    if (!XAxisComp || !YAxisComp || !ZAxisComp)
    {
        return;
    }

    switch (CurrentMode)
    {
    case EGizmoMode::Translate:
    {
        XAxisComp->SetStaticMesh(TranslateMesh);
        YAxisComp->SetStaticMesh(TranslateMesh);
        ZAxisComp->SetStaticMesh(TranslateMesh);

        XAxisComp->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
        YAxisComp->SetRelativeRotation(FVector(0.0f, 0.0f, 1.5707963f));
        ZAxisComp->SetRelativeRotation(FVector(0.0f, -1.5707963f, 0.0f));

        XAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));
        YAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));
        ZAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));

        if (XAxisShaftComp) XAxisShaftComp->SetVisibility(IsGizmoVisible());
        if (YAxisShaftComp) YAxisShaftComp->SetVisibility(IsGizmoVisible());
        if (ZAxisShaftComp) ZAxisShaftComp->SetVisibility(IsGizmoVisible());
        break;
    }

    case EGizmoMode::Rotate:
    {
        XAxisComp->SetStaticMesh(RotateMesh);
        YAxisComp->SetStaticMesh(RotateMesh);
        ZAxisComp->SetStaticMesh(RotateMesh);

        // 현재 Torus는 XZ 평면(노멀 +Y) 기준으로 만들어져 있음
        XAxisComp->SetRelativeRotation(FVector(0.0f, 0.0f, 1.5707963f));
        YAxisComp->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
        ZAxisComp->SetRelativeRotation(FVector(1.5707963f, 0.0f, 0.0f));

        XAxisComp->SetRelativeScale(FVector(0.22f, 0.22f, 0.22f));
        YAxisComp->SetRelativeScale(FVector(0.22f, 0.22f, 0.22f));
        ZAxisComp->SetRelativeScale(FVector(0.22f, 0.22f, 0.22f));

        if (XAxisShaftComp) XAxisShaftComp->SetVisibility(false);
        if (YAxisShaftComp) YAxisShaftComp->SetVisibility(false);
        if (ZAxisShaftComp) ZAxisShaftComp->SetVisibility(false);
        break;
    }

    case EGizmoMode::Scale:
    {
        XAxisComp->SetStaticMesh(ScaleMesh);
        YAxisComp->SetStaticMesh(ScaleMesh);
        ZAxisComp->SetStaticMesh(ScaleMesh);

        XAxisComp->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
        YAxisComp->SetRelativeRotation(FVector(0.0f, 0.0f, 1.5707963f));
        ZAxisComp->SetRelativeRotation(FVector(0.0f, -1.5707963f, 0.0f));

        XAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));
        YAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));
        ZAxisComp->SetRelativeScale(FVector(GizmoScale, GizmoScale, GizmoScale));

        if (XAxisShaftComp) XAxisShaftComp->SetVisibility(false);
        if (YAxisShaftComp) YAxisShaftComp->SetVisibility(false);
        if (ZAxisShaftComp) ZAxisShaftComp->SetVisibility(false);
        break;
    }

    default:
        break;
    }
}

EGizmoAxis AGizmoActor::PickAxisTranslate(
    int MouseX,
    int MouseY,
    const UCameraComponent* Camera,
    int ViewWidth,
    int ViewHeight) const
{
    if (!PivotComp)
    {
        return EGizmoAxis::None;
    }

    const FMatrix PivotWorld = PivotComp->GetWorldTransformMatrix();

    auto TransformPoint = [&](const FVector& LocalPoint) -> FVector
        {
            const FVector4 W = PivotWorld * FVector4(LocalPoint, 1.0f);
            return FVector(W.X, W.Y, W.Z);
        };

    auto TransformDirection = [&](const FVector& LocalDir) -> FVector
        {
            const FVector4 W = PivotWorld * FVector4(LocalDir, 0.0f);
            FVector Dir(W.X, W.Y, W.Z);
            Dir.Normalize();
            return Dir;
        };

    const FVector Origin = TransformPoint(FVector::ZeroVector);

    const FVector XDir = TransformDirection(FVector(1.0f, 0.0f, 0.0f));
    const FVector YDir = TransformDirection(FVector(0.0f, 1.0f, 0.0f));
    const FVector ZDir = TransformDirection(FVector(0.0f, 0.0f, 1.0f));

    const FVector XEnd = Origin + XDir * AxisLength;
    const FVector YEnd = Origin + YDir * AxisLength;
    const FVector ZEnd = Origin + ZDir * AxisLength;

    float OX = 0.0f, OY = 0.0f;
    float XX = 0.0f, XY = 0.0f;
    float YX = 0.0f, YY = 0.0f;
    float ZX = 0.0f, ZY = 0.0f;

    if (!ProjectWorldToScreen(Origin, Camera, ViewWidth, ViewHeight, OX, OY))
    {
        return EGizmoAxis::None;
    }

    const bool bx = ProjectWorldToScreen(XEnd, Camera, ViewWidth, ViewHeight, XX, XY);
    const bool by = ProjectWorldToScreen(YEnd, Camera, ViewWidth, ViewHeight, YX, YY);
    const bool bz = ProjectWorldToScreen(ZEnd, Camera, ViewWidth, ViewHeight, ZX, ZY);

    float BestDist = FLT_MAX;
    EGizmoAxis BestAxis = EGizmoAxis::None;

    if (bx && XAxisComp && XAxisComp->IsVisible())
    {
        const float Dist = DistancePointToSegment2D((float)MouseX, (float)MouseY, OX, OY, XX, XY);
        if (Dist < PickThreshold && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::X;
        }
    }

    if (by && YAxisComp && YAxisComp->IsVisible())
    {
        const float Dist = DistancePointToSegment2D((float)MouseX, (float)MouseY, OX, OY, YX, YY);
        if (Dist < PickThreshold && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::Y;
        }
    }

    if (bz && ZAxisComp && ZAxisComp->IsVisible())
    {
        const float Dist = DistancePointToSegment2D((float)MouseX, (float)MouseY, OX, OY, ZX, ZY);
        if (Dist < PickThreshold && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::Z;
        }
    }
	return BestAxis;
}

float AGizmoActor::DistancePointToCircle2D(
    float Px, float Py,
    float Cx, float Cy,
    float Radius) const
{
    const float Dx = Px - Cx;
    const float Dy = Py - Cy;
    const float DistToCenter = std::sqrt(Dx * Dx + Dy * Dy);
    return std::fabs(DistToCenter - Radius);
}

EGizmoAxis AGizmoActor::PickAxisRotate(
    int MouseX,
    int MouseY,
    const UCameraComponent* Camera,
    int ViewWidth,
    int ViewHeight) const
{
    if (!PivotComp)
    {
        return EGizmoAxis::None;
    }

    const FMatrix PivotWorld = PivotComp->GetWorldTransformMatrix();

    auto TransformPoint = [&](const FVector& LocalPoint) -> FVector
        {
            const FVector4 W = PivotWorld * FVector4(LocalPoint, 1.0f);
            return FVector(W.X, W.Y, W.Z);
        };

    auto TransformDirection = [&](const FVector& LocalDir) -> FVector
        {
            const FVector4 W = PivotWorld * FVector4(LocalDir, 0.0f);
            FVector Dir(W.X, W.Y, W.Z);
            Dir.Normalize();
            return Dir;
        };

    const FVector Origin = TransformPoint(FVector::ZeroVector);

    auto PickRing = [&](const FVector& LocalBasisU, const FVector& LocalBasisV) -> float
        {
            constexpr int NumSegments = 64;
            float BestDist = FLT_MAX;
            bool bHasValidSegment = false;

            const FVector U = TransformDirection(LocalBasisU);
            const FVector V = TransformDirection(LocalBasisV);

            float PrevX = 0.0f, PrevY = 0.0f;
            bool bPrevValid = false;

            for (int i = 0; i <= NumSegments; ++i)
            {
                const float T = (float)i / (float)NumSegments;
                const float Angle = T * 6.28318530718f; // 2pi

                const FVector WorldPoint =
                    Origin +
                    U * (std::cos(Angle) * AxisLength) +
                    V * (std::sin(Angle) * AxisLength);

                float SX = 0.0f, SY = 0.0f;
                const bool bValid = ProjectWorldToScreen(WorldPoint, Camera, ViewWidth, ViewHeight, SX, SY);

                if (bPrevValid && bValid)
                {
                    const float Dist = DistancePointToSegment2D(
                        (float)MouseX, (float)MouseY,
                        PrevX, PrevY, SX, SY);

                    if (Dist < BestDist)
                    {
                        BestDist = Dist;
                    }

                    bHasValidSegment = true;
                }

                PrevX = SX;
                PrevY = SY;
                bPrevValid = bValid;
            }

            if (!bHasValidSegment)
            {
                return FLT_MAX;
            }

            return BestDist;
        };

    float BestDist = FLT_MAX;
    EGizmoAxis BestAxis = EGizmoAxis::None;

    const float RingPickThickness = 12.0f;

    // X ring = YZ plane
    if (XAxisComp && XAxisComp->IsVisible())
    {
        const float Dist = PickRing(FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
        if (Dist < RingPickThickness && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::X;
        }
    }

    // Y ring = XZ plane
    if (YAxisComp && YAxisComp->IsVisible())
    {
        const float Dist = PickRing(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
        if (Dist < RingPickThickness && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::Y;
        }
    }

    // Z ring = XY plane
    if (ZAxisComp && ZAxisComp->IsVisible())
    {
        const float Dist = PickRing(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f));
        if (Dist < RingPickThickness && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::Z;
        }
    }

    return BestAxis;
}

bool AGizmoActor::ProjectAxisEndToScreen(
    const FVector& Origin,
    const FVector& AxisDir,
    float Length,
    const UCameraComponent* Camera,
    int ViewWidth,
    int ViewHeight,
    float& OutOriginX,
    float& OutOriginY,
    float& OutEndX,
    float& OutEndY) const
{
    if (!ProjectWorldToScreen(Origin, Camera, ViewWidth, ViewHeight, OutOriginX, OutOriginY))
    {
        return false;
    }

    const FVector End = Origin + AxisDir * Length;
    return ProjectWorldToScreen(End, Camera, ViewWidth, ViewHeight, OutEndX, OutEndY);
}

EGizmoAxis AGizmoActor::PickAxisScale(
    int MouseX,
    int MouseY,
    const UCameraComponent* Camera,
    int ViewWidth,
    int ViewHeight) const
{
    if (!PivotComp)
    {
        return EGizmoAxis::None;
    }

    const FMatrix PivotWorld = PivotComp->GetWorldTransformMatrix();

    auto TransformPoint = [&](const FVector& LocalPoint) -> FVector
        {
            const FVector4 W = PivotWorld * FVector4(LocalPoint, 1.0f);
            return FVector(W.X, W.Y, W.Z);
        };
    const float ScaleCubeCenter = 3.0f * GizmoScale;

    const FVector XPos = TransformPoint(FVector(ScaleCubeCenter, 0.0f, 0.0f));
    const FVector YPos = TransformPoint(FVector(0.0f, ScaleCubeCenter, 0.0f));
    const FVector ZPos = TransformPoint(FVector(0.0f, 0.0f, ScaleCubeCenter));

    float XX = 0.0f, XY = 0.0f;
    float YX = 0.0f, YY = 0.0f;
    float ZX = 0.0f, ZY = 0.0f;

    const bool bX = ProjectWorldToScreen(XPos, Camera, ViewWidth, ViewHeight, XX, XY);
    const bool bY = ProjectWorldToScreen(YPos, Camera, ViewWidth, ViewHeight, YX, YY);
    const bool bZ = ProjectWorldToScreen(ZPos, Camera, ViewWidth, ViewHeight, ZX, ZY);

    float BestDist = FLT_MAX;
    EGizmoAxis BestAxis = EGizmoAxis::None;

    auto DistToPoint = [&](float Px, float Py) -> float
        {
            const float Dx = (float)MouseX - Px;
            const float Dy = (float)MouseY - Py;
            return std::sqrt(Dx * Dx + Dy * Dy);
        };

    const float CubePickRadius = 14.0f;

    if (bX && XAxisComp && XAxisComp->IsVisible())
    {
        const float Dist = DistToPoint(XX, XY);
        if (Dist < CubePickRadius && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::X;
        }
    }

    if (bY && YAxisComp && YAxisComp->IsVisible())
    {
        const float Dist = DistToPoint(YX, YY);
        if (Dist < CubePickRadius && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::Y;
        }
    }

    if (bZ && ZAxisComp && ZAxisComp->IsVisible())
    {
        const float Dist = DistToPoint(ZX, ZY);
        if (Dist < CubePickRadius && Dist < BestDist)
        {
            BestDist = Dist;
            BestAxis = EGizmoAxis::Z;
        }
    }

    return BestAxis;
}