#include "UnrealEditorStyledGizmo.h"

#include <algorithm>
#include <cmath>


namespace
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float kHalfPi = 0.5f * kPi;
constexpr float kTwoPi = 2.0f * kPi;

constexpr float kAxisLength = 35.0f;
constexpr float kAxisLengthScaleOffset = 1.0f;
constexpr float kCylinderRadius = .6f;
constexpr float kConeHeadOffset = 12.0f;
constexpr float kCubeHeadOffset = 0.0f;
constexpr float kConeScale = -13.0f;
constexpr int kAxisCircleSides = 24;
constexpr float kInnerAxisCircleRadius = 44.0f;
constexpr float kOuterAxisCircleRadius = 52.0f;
constexpr float kTranslateCornerStart = 7.0f;
constexpr float kTranslateCornerLength = 12.0f;
constexpr float kTranslateCornerThickness = 1.2f;
constexpr float kTranslateScreenSphereRadius = 4.0f;
constexpr float kScaleLineHalfThickness = 0.35f;
constexpr float kScaleCenterCubeHalf = 2.5f;
constexpr float kLargeInnerAlpha = 63.0f / 255.0f;
constexpr float kSmallInnerAlpha = 15.0f / 255.0f;
constexpr float kLargeOuterAlpha = 127.0f / 255.0f;
constexpr float kSmallOuterAlpha = 15.0f / 255.0f;
constexpr float kArcballAlpha = 6.0f / 255.0f;

struct Basis3
{
    FVector x;
    FVector y;
    FVector z;
};

FVector NormalizeSafe(const FVector& v, const FVector& fallback)
{
    const float lenSq = v.SizeSquared();
    if (lenSq <= 1.0e-12f)
    {
        return fallback;
    }

    return v.GetSafeNormal();
}

float DegreesToRadians(float degrees)
{
    return degrees * (kPi / 180.0f);
}

Color MakeColor(float r, float g, float b, float a = 1.0f)
{
    return Color{r, g, b, a};
}

Color ApplyAlpha(const Color& color, float alpha)
{
    return MakeColor(color.r, color.g, color.b, alpha);
}

Basis3 MakeBasisFromX(const FVector& axis)
{
    const FVector x = NormalizeSafe(axis, FVector(1.0f, 0.0f, 0.0f));
    const FVector up = (std::fabs(x.X) < 0.95f) ? FVector(1.0f, 0.0f, 0.0f) : FVector(0.0f, 1.0f, 0.0f);
    const FVector z = NormalizeSafe(FVector::CrossProduct(x, up), FVector(0.0f, 0.0f, 1.0f));
    const FVector y = NormalizeSafe(FVector::CrossProduct(z, x), FVector(0.0f, 1.0f, 0.0f));
    return Basis3{x, y, z};
}

Basis3 MakeBasisFromXAndNormal(const FVector& axis, const FVector& preferredNormal)
{
    const FVector x = NormalizeSafe(axis, FVector(1.0f, 0.0f, 0.0f));
    FVector z = NormalizeSafe(preferredNormal, FVector(0.0f, 0.0f, 1.0f));
    if (std::fabs(FVector::DotProduct(x, z)) > 0.999f)
    {
        z = (std::fabs(x.Z) < 0.95f) ? FVector(0.0f, 0.0f, 1.0f) : FVector(0.0f, 1.0f, 0.0f);
    }

    const FVector y = NormalizeSafe(FVector::CrossProduct(z, x), FVector(0.0f, 1.0f, 0.0f));
    z = NormalizeSafe(FVector::CrossProduct(x, y), z);
    return Basis3{x, y, z};
}

FVector TransformVector(const Basis3& basis, const FVector& local)
{
    return basis.x * local.X + basis.y * local.Y + basis.z * local.Z;
}

FVector TransformPoint(const Basis3& basis, const FVector& origin, const FVector& local)
{
    return origin + TransformVector(basis, local);
}

FVector TransformNormal(const Basis3& basis, const FVector& localNormal)
{
    return NormalizeSafe(TransformVector(basis, localNormal), FVector(0.0f, 0.0f, 1.0f));
}

FVector SwapXZ(const FVector& value)
{
    return FVector(value.Z, value.Y, value.X);
}

std::uint32_t ComputeCircleSides(int transformGizmoSize)
{
    if (transformGizmoSize > 0)
    {
        return static_cast<std::uint32_t>(kAxisCircleSides + (transformGizmoSize / 5));
    }
    return static_cast<std::uint32_t>(kAxisCircleSides);
}

bool IsAxisActive(AxisId activeAxis, AxisId axis)
{
    return activeAxis == axis;
}

void AppendTriangle(Mesh& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
    mesh.indices.push_back(c);
}

void AppendVertex(Mesh& mesh, const FVector& position, const FVector& normal, const Vec2& uv, const Color& color)
{
    mesh.vertices.push_back(Vertex{position, normal, uv, color});
}

void AppendLocalVertex(Mesh& mesh, const Basis3& basis, const FVector& origin, const FVector& localPosition, const FVector& localNormal, const Vec2& uv, const Color& color)
{
    AppendVertex(mesh, TransformPoint(basis, origin, localPosition), TransformNormal(basis, localNormal), uv, color);
}

void AppendQuad(Mesh& mesh, const FVector& v0, const FVector& v1, const FVector& v2, const FVector& v3, const FVector& normal, const Color& color)
{
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    AppendVertex(mesh, v0, normal, Vec2{0.0f, 0.0f}, color);
    AppendVertex(mesh, v1, normal, Vec2{1.0f, 0.0f}, color);
    AppendVertex(mesh, v2, normal, Vec2{1.0f, 1.0f}, color);
    AppendVertex(mesh, v3, normal, Vec2{0.0f, 1.0f}, color);
    AppendTriangle(mesh, base + 0, base + 1, base + 2);
    AppendTriangle(mesh, base + 0, base + 2, base + 3);
}

void AppendLocalQuad(Mesh& mesh, const Basis3& basis, const FVector& origin, const FVector& v0, const FVector& v1, const FVector& v2, const FVector& v3, const FVector& localNormal, const Color& color)
{
    const FVector normal = TransformNormal(basis, localNormal);
    AppendQuad(
        mesh,
        TransformPoint(basis, origin, v0),
        TransformPoint(basis, origin, v1),
        TransformPoint(basis, origin, v2),
        TransformPoint(basis, origin, v3),
        normal,
        color);
}

void AppendOrientedBox(Mesh& mesh, const FVector& center, const Basis3& basis, const FVector& halfExtents, const Color& color)
{
    const FVector px = basis.x * halfExtents.X;
    const FVector py = basis.y * halfExtents.Y;
    const FVector pz = basis.z * halfExtents.Z;

    const FVector c[8] = {
        center - px - py - pz,
        center + px - py - pz,
        center + px + py - pz,
        center - px + py - pz,
        center - px - py + pz,
        center + px - py + pz,
        center + px + py + pz,
        center - px + py + pz};

    AppendQuad(mesh, c[1], c[5], c[6], c[2], basis.x, color);
    AppendQuad(mesh, c[4], c[0], c[3], c[7], basis.x * -1.0f, color);
    AppendQuad(mesh, c[3], c[2], c[6], c[7], basis.y, color);
    AppendQuad(mesh, c[0], c[4], c[5], c[1], basis.y * -1.0f, color);
    AppendQuad(mesh, c[4], c[5], c[6], c[7], basis.z, color);
    AppendQuad(mesh, c[0], c[3], c[2], c[1], basis.z * -1.0f, color);
}

void AppendCylinder(Mesh& mesh, const FVector& start, const FVector& end, float radius, std::uint32_t sides, const Color& color)
{
    sides = std::max<std::uint32_t>(sides, 3);
    const FVector axis = NormalizeSafe(end - start, FVector(1.0f, 0.0f, 0.0f));
    const Basis3 basis = MakeBasisFromX(axis);

    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::uint32_t i = 0; i < sides; ++i)
    {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(sides);
        const FVector radial = basis.y * std::cos(angle) + basis.z * std::sin(angle);
        AppendVertex(mesh, start + radial * radius, radial, Vec2{0.0f, 0.0f}, color);
        AppendVertex(mesh, end + radial * radius, radial, Vec2{1.0f, 0.0f}, color);
    }

    for (std::uint32_t i = 0; i < sides; ++i)
    {
        const std::uint32_t next = (i + 1) % sides;
        const std::uint32_t v0 = base + i * 2 + 0;
        const std::uint32_t v1 = base + i * 2 + 1;
        const std::uint32_t v2 = base + next * 2 + 1;
        const std::uint32_t v3 = base + next * 2 + 0;
        AppendTriangle(mesh, v0, v2, v1);
        AppendTriangle(mesh, v0, v3, v2);
    }
}

FVector CalcConeVert(float angle1, float angle2, float azimuthAngle)
{
    const float sinX_2 = std::sin(0.5f * angle1);
    const float sinY_2 = std::sin(0.5f * angle2);
    const float sinSqX_2 = sinX_2 * sinX_2;
    const float sinSqY_2 = sinY_2 * sinY_2;
    const float phi = std::atan2(std::sin(azimuthAngle) * sinSqY_2, std::cos(azimuthAngle) * sinSqX_2);
    const float sinPhi = std::sin(phi);
    const float cosPhi = std::cos(phi);
    const float sinSqPhi = sinPhi * sinPhi;
    const float cosSqPhi = cosPhi * cosPhi;
    const float rSq = sinSqX_2 * sinSqY_2 / (sinSqX_2 * sinSqPhi + sinSqY_2 * cosSqPhi);
    const float r = std::sqrt(rSq);
    const float sqr = std::sqrt(1.0f - rSq);
    const float alpha = r * cosPhi;
    const float beta = r * sinPhi;
    return FVector(1.0f - 2.0f * rSq, 2.0f * sqr * alpha, 2.0f * sqr * beta);
}

void AppendUnrealCone(Mesh& mesh, const FVector& tipPosition, const FVector& axisDirection, float scale, float angle, std::uint32_t sides, const Color& color)
{
    const Basis3 basis = MakeBasisFromX(axisDirection);
    for (std::uint32_t i = 0; i < sides; ++i)
    {
        const std::uint32_t next = (i + 1) % sides;
        const float a0 = kTwoPi * static_cast<float>(i) / static_cast<float>(sides);
        const float a1 = kTwoPi * static_cast<float>(next) / static_cast<float>(sides);
        const FVector tip = tipPosition;
        const FVector p0 = tipPosition + TransformVector(basis, CalcConeVert(angle, angle, a0) * scale);
        const FVector p1 = tipPosition + TransformVector(basis, CalcConeVert(angle, angle, a1) * scale);
        const FVector n = NormalizeSafe(FVector::CrossProduct(p0 - tip, p1 - tip), axisDirection);
        const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
        AppendVertex(mesh, tip, n, Vec2{0.0f, 0.5f}, color);
        AppendVertex(mesh, p0, n, Vec2{1.0f, 0.0f}, color);
        AppendVertex(mesh, p1, n, Vec2{1.0f, 1.0f}, color);
        AppendTriangle(mesh, base + 0, base + 1, base + 2);
    }
}

void AppendSphere(Mesh& mesh, const FVector& center, float radius, std::uint32_t slices, std::uint32_t stacks, const Color& color)
{
    slices = std::max<std::uint32_t>(slices, 3);
    stacks = std::max<std::uint32_t>(stacks, 2);
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());

    for (std::uint32_t stack = 0; stack <= stacks; ++stack)
    {
        const float v = static_cast<float>(stack) / static_cast<float>(stacks);
        const float phi = v * kPi;
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (std::uint32_t slice = 0; slice <= slices; ++slice)
        {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float theta = u * kTwoPi;
            const FVector n(sinPhi * std::cos(theta), sinPhi * std::sin(theta), cosPhi);
            AppendVertex(mesh, center + n * radius, n, Vec2{u, v}, color);
        }
    }

    const std::uint32_t stride = slices + 1;
    for (std::uint32_t stack = 0; stack < stacks; ++stack)
    {
        for (std::uint32_t slice = 0; slice < slices; ++slice)
        {
            const std::uint32_t a = base + stack * stride + slice;
            const std::uint32_t b = a + stride;
            const std::uint32_t c = b + 1;
            const std::uint32_t d = a + 1;
            AppendTriangle(mesh, a, b, c);
            AppendTriangle(mesh, a, c, d);
        }
    }
}

void AppendArcBand(Mesh& mesh, const FVector& axis0, const FVector& axis1, float innerRadius, float outerRadius, float startAngle, float endAngle, const Color& color, std::uint32_t circleSides)
{
    const float range = endAngle - startAngle;
    const std::uint32_t quarterSides = std::max<std::uint32_t>(circleSides, 4);
    const std::uint32_t points = std::max<std::uint32_t>(2, static_cast<std::uint32_t>(std::floor(quarterSides * std::fabs(range) / kHalfPi)) + 1);
    const FVector normal = NormalizeSafe(FVector::CrossProduct(axis0, axis1), FVector(0.0f, 0.0f, 1.0f));

    const std::uint32_t outerBase = static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::uint32_t i = 0; i <= points; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(points);
        const float angle = startAngle + range * t;
        const FVector dir = NormalizeSafe(axis0 * std::cos(angle) + axis1 * std::sin(angle), axis0);
        AppendVertex(mesh, dir * outerRadius, normal, Vec2{t, 0.0f}, color);
    }

    const std::uint32_t innerBase = static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::uint32_t i = 0; i <= points; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(points);
        const float angle = startAngle + range * t;
        const FVector dir = NormalizeSafe(axis0 * std::cos(angle) + axis1 * std::sin(angle), axis0);
        AppendVertex(mesh, dir * innerRadius, normal, Vec2{t, 1.0f}, color);
    }

    for (std::uint32_t i = 0; i < points; ++i)
    {
        AppendTriangle(mesh, outerBase + i, outerBase + i + 1, innerBase + i);
        AppendTriangle(mesh, outerBase + i + 1, innerBase + i + 1, innerBase + i);
    }
}

void AppendCornerHelperStrip(Mesh& mesh, const Basis3& basis, const FVector& origin, const FVector& length, float thickness, const Color& color, bool swapXZ)
{
    const float tx = length.X * 0.5f;
    const float ty = length.Y * 0.5f;
    const float tz = length.Z * 0.5f;
    const float th = thickness;

    const auto mapLocal = [&](const FVector& v) -> FVector
    {
        return swapXZ ? SwapXZ(v) : v;
    };

    const auto addLocalVertex = [&](const FVector& localPosition, const FVector& localNormal, const Vec2& uv)
    {
        AppendLocalVertex(mesh, basis, origin, mapLocal(localPosition), mapLocal(localNormal), uv, color);
    };

    AppendLocalQuad(
        mesh,
        basis,
        origin,
        mapLocal(FVector(-tx, -ty, +tz)),
        mapLocal(FVector(-tx, +ty, +tz)),
        mapLocal(FVector(+tx, +ty, +tz)),
        mapLocal(FVector(+tx, -ty, +tz)),
        mapLocal(FVector(0.0f, 0.0f, 1.0f)),
        color);

    AppendLocalQuad(
        mesh,
        basis,
        origin,
        mapLocal(FVector(-tx, -ty, tz - th)),
        mapLocal(FVector(-tx, -ty, tz)),
        mapLocal(FVector(-tx, +ty, tz)),
        mapLocal(FVector(-tx, +ty, tz - th)),
        mapLocal(FVector(-1.0f, 0.0f, 0.0f)),
        color);

    {
        const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
        const FVector localNormal = mapLocal(FVector(0.0f, 1.0f, 0.0f));
        addLocalVertex(FVector(-tx, +ty, tz - th), localNormal, Vec2{0.0f, 0.0f});
        addLocalVertex(FVector(-tx, +ty, +tz), localNormal, Vec2{0.0f, 1.0f});
        addLocalVertex(FVector(+tx - th, +ty, +tz), localNormal, Vec2{1.0f, 1.0f});
        addLocalVertex(FVector(+tx, +ty, +tz), localNormal, Vec2{1.0f, 1.0f});
        addLocalVertex(FVector(+tx - th, +ty, tz - th), localNormal, Vec2{1.0f, 0.0f});
        AppendTriangle(mesh, base + 0, base + 1, base + 2);
        AppendTriangle(mesh, base + 0, base + 2, base + 4);
        AppendTriangle(mesh, base + 4, base + 2, base + 3);
    }

    {
        const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
        const FVector localNormal = mapLocal(FVector(0.0f, -1.0f, 0.0f));
        addLocalVertex(FVector(-tx, -ty, tz - th), localNormal, Vec2{0.0f, 0.0f});
        addLocalVertex(FVector(-tx, -ty, +tz), localNormal, Vec2{0.0f, 1.0f});
        addLocalVertex(FVector(+tx - th, -ty, +tz), localNormal, Vec2{1.0f, 1.0f});
        addLocalVertex(FVector(+tx, -ty, +tz), localNormal, Vec2{1.0f, 1.0f});
        addLocalVertex(FVector(+tx - th, -ty, tz - th), localNormal, Vec2{1.0f, 0.0f});
        AppendTriangle(mesh, base + 0, base + 2, base + 1);
        AppendTriangle(mesh, base + 0, base + 4, base + 2);
        AppendTriangle(mesh, base + 4, base + 3, base + 2);
    }

    AppendLocalQuad(
        mesh,
        basis,
        origin,
        mapLocal(FVector(-tx, -ty, tz - th)),
        mapLocal(FVector(-tx, +ty, tz - th)),
        mapLocal(FVector(+tx - th, +ty, tz - th)),
        mapLocal(FVector(+tx - th, -ty, tz - th)),
        mapLocal(FVector(0.0f, 0.0f, -1.0f)),
        color);
}

void AppendTranslatePlane(Mesh& mesh, const FVector& axis0, const FVector& axis1, const FVector& normal, const Color& axis0Color, const Color& axis1Color, float s)
{
    const Basis3 localToWorld{
        NormalizeSafe(axis0, FVector(1.0f, 0.0f, 0.0f)),
        NormalizeSafe(normal, FVector(0.0f, 0.0f, 1.0f)),
        NormalizeSafe(axis1, FVector(0.0f, 1.0f, 0.0f))};
    const FVector origin = axis0 * (kTranslateCornerStart * s) + axis1 * (kTranslateCornerStart * s);
    const FVector length(kTranslateCornerLength * s, kTranslateCornerThickness * s, kTranslateCornerLength * s);
    const float thickness = kTranslateCornerThickness * s;

    AppendCornerHelperStrip(mesh, localToWorld, origin, length, thickness, axis1Color, false);
    AppendCornerHelperStrip(mesh, localToWorld, origin, length, thickness, axis0Color, true);
}

void AppendSegmentBox(Mesh& mesh, const FVector& start, const FVector& end, float halfThickness, const FVector& planeNormal, const Color& color)
{
    const FVector segment = end - start;
    const float len = segment.Size();
    if (len <= 1.0e-6f)
    {
        return;
    }

    const Basis3 basis = MakeBasisFromXAndNormal(segment, planeNormal);
    const FVector center = (start + end) * 0.5f;
    AppendOrientedBox(mesh, center, basis, FVector(len * 0.5f, halfThickness, halfThickness), color);
}

void AppendScalePlane(Mesh& mesh, const FVector& axis0, const FVector& axis1, const FVector& normal, const Color& axis0Color, const Color& axis1Color, float s)
{
    const FVector p0 = axis0 * (24.0f * s);
    const FVector p1 = axis0 * (12.0f * s) + axis1 * (12.0f * s);
    const FVector p2 = axis1 * (24.0f * s);
    AppendSegmentBox(mesh, p0, p1, kScaleLineHalfThickness * s, normal, axis0Color);
    AppendSegmentBox(mesh, p1, p2, kScaleLineHalfThickness * s, normal, axis1Color);
}

} // namespace

void Mesh::Clear()
{
    vertices.clear();
    indices.clear();
}

bool Mesh::Empty() const
{
    return vertices.empty() || indices.empty();
}

Color AxisColorX()
{
    return MakeColor(0.594f, 0.0197f, 0.0f, 1.0f);
}

Color AxisColorY()
{
    return MakeColor(0.1349f, 0.3959f, 0.0f, 1.0f);
}

Color AxisColorZ()
{
    return MakeColor(0.0251f, 0.207f, 0.85f, 1.0f);
}

Color ScreenAxisColor()
{
    return MakeColor(0.76f, 0.72f, 0.14f, 1.0f);
}

Color ScreenSpaceColor()
{
    return MakeColor(196.0f / 255.0f, 196.0f / 255.0f, 196.0f / 255.0f, 1.0f);
}

Color ArcballColor()
{
    return MakeColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, kArcballAlpha);
}

Color HighlightColor()
{
    return MakeColor(1.0f, 1.0f, 0.0f, 1.0f);
}

void AppendMesh(Mesh& destination, const Mesh& source)
{
    if (source.vertices.empty())
    {
        return;
    }

    const std::uint32_t offset = static_cast<std::uint32_t>(destination.vertices.size());
    destination.vertices.insert(destination.vertices.end(), source.vertices.begin(), source.vertices.end());
    for (std::uint32_t index : source.indices)
    {
        destination.indices.push_back(offset + index);
    }
}

Mesh MergeMeshes(std::initializer_list<const Mesh*> meshes)
{
    Mesh merged;
    for (const Mesh* mesh : meshes)
    {
        if (mesh != nullptr)
        {
            AppendMesh(merged, *mesh);
        }
    }
    return merged;
}

TranslationGizmo GenerateTranslationGizmo(const TranslationDesc& desc)
{
    TranslationGizmo gizmo;
    const float s = desc.uniformScale;
    const float gizmoSize = static_cast<float>(desc.transformGizmoSize);
    const float axisLength = std::max(1.0f, kAxisLength + gizmoSize);
    const float coneTip = axisLength + kConeHeadOffset;
    const float coneAngle = DegreesToRadians(kPi * 5.0f);
    const FVector xDir(1.0f, 0.0f, 0.0f);
    const FVector yDir(0.0f, desc.leftUpForward ? -1.0f : 1.0f, 0.0f);
    const FVector zDir(0.0f, 0.0f, 1.0f);

    AppendCylinder(gizmo.axisX, FVector(0.0f, 0.0f, 0.0f), xDir * (axisLength * s), kCylinderRadius * s, 16, AxisColorX());
    AppendCylinder(gizmo.axisY, FVector(0.0f, 0.0f, 0.0f), yDir * (axisLength * s), kCylinderRadius * s, 16, AxisColorY());
    AppendCylinder(gizmo.axisZ, FVector(0.0f, 0.0f, 0.0f), zDir * (axisLength * s), kCylinderRadius * s, 16, AxisColorZ());

    AppendUnrealCone(gizmo.axisX, xDir * (coneTip * s), xDir, kConeScale * s, coneAngle, 32, AxisColorX());
    AppendUnrealCone(gizmo.axisY, yDir * (coneTip * s), yDir, kConeScale * s, coneAngle, 32, AxisColorY());
    AppendUnrealCone(gizmo.axisZ, zDir * (coneTip * s), zDir, kConeScale * s, coneAngle, 32, AxisColorZ());

    AppendTranslatePlane(gizmo.planeXY, xDir, yDir, zDir, AxisColorX(), AxisColorY(), s);
    AppendTranslatePlane(gizmo.planeXZ, xDir, zDir, yDir * -1.0f, AxisColorX(), AxisColorZ(), s);
    AppendTranslatePlane(gizmo.planeYZ, yDir, zDir, xDir, AxisColorY(), AxisColorZ(), s);

    if (desc.includeScreenHandle)
    {
        AppendSphere(gizmo.screenSphere, FVector(0.0f, 0.0f, 0.0f), kTranslateScreenSphereRadius * s, 10, 5, ScreenSpaceColor());
    }

    return gizmo;
}

RotationGizmo GenerateRotationGizmo(const RotationDesc& desc)
{
    RotationGizmo gizmo;
    const float s = desc.uniformScale;
    const float gizmoSize = static_cast<float>(desc.transformGizmoSize);
    const float innerRadius = (kInnerAxisCircleRadius * s) + gizmoSize;
    const float outerRadius = (kOuterAxisCircleRadius * s) + gizmoSize;
    const std::uint32_t circleSides = ComputeCircleSides(desc.transformGizmoSize);
    const FVector dir = NormalizeSafe(desc.cameraDirection, FVector(-1.0f, -1.0f, -1.0f));

    auto appendAxisArc = [&](Mesh& mesh, AxisId axisId, const FVector& axis0, const FVector& axis1, const Color& baseColor)
    {
        if (desc.dragging)
        {
            if (!IsAxisActive(desc.activeAxis, axisId))
            {
                return;
            }

            const float deltaAngle = DegreesToRadians(std::fabs(desc.deltaRotationDegrees));
            if (deltaAngle <= 1.0e-6f)
            {
                AppendArcBand(mesh, axis0, axis1, innerRadius, outerRadius, 0.0f, kTwoPi, ApplyAlpha(baseColor, kLargeOuterAlpha), circleSides);
                if (desc.includeInnerDisk)
                {
                    AppendArcBand(mesh, axis0, axis1, 0.0f, innerRadius, 0.0f, kTwoPi, ApplyAlpha(baseColor, kSmallInnerAlpha), circleSides);
                }
                return;
            }

            const float startAngle = (desc.deltaRotationDegrees < 0.0f) ? -deltaAngle : 0.0f;
            AppendArcBand(mesh, axis0, axis1, innerRadius, outerRadius, startAngle, startAngle + deltaAngle, ApplyAlpha(baseColor, kLargeOuterAlpha), circleSides);
            AppendArcBand(mesh, axis0, axis1, innerRadius, outerRadius, startAngle + deltaAngle, startAngle + kTwoPi, ApplyAlpha(baseColor, kSmallOuterAlpha), circleSides);
            if (desc.includeInnerDisk)
            {
                AppendArcBand(mesh, axis0, axis1, 0.0f, innerRadius, startAngle, startAngle + kTwoPi, ApplyAlpha(baseColor, kSmallInnerAlpha), circleSides);
            }
            return;
        }

        if (desc.fullAxisRings)
        {
            AppendArcBand(mesh, axis0, axis1, innerRadius, outerRadius, 0.0f, kTwoPi, ApplyAlpha(baseColor, kLargeOuterAlpha), circleSides);
            if (desc.includeInnerDisk)
            {
                AppendArcBand(mesh, axis0, axis1, 0.0f, innerRadius, 0.0f, kTwoPi, ApplyAlpha(baseColor, kSmallInnerAlpha), circleSides);
            }
            return;
        }

        const bool mirror0 = FVector::DotProduct(axis0, dir) <= 0.0f;
        const bool mirror1 = FVector::DotProduct(axis1, dir) <= 0.0f;
        const FVector render0 = mirror0 ? axis0 : axis0 * -1.0f;
        const FVector render1 = mirror1 ? axis1 : axis1 * -1.0f;
        AppendArcBand(mesh, render0, render1, innerRadius, outerRadius, 0.0f, kHalfPi, ApplyAlpha(baseColor, kLargeOuterAlpha), circleSides);
        if (desc.includeInnerDisk)
        {
            AppendArcBand(mesh, render0, render1, 0.0f, innerRadius, 0.0f, kHalfPi, ApplyAlpha(baseColor, kSmallInnerAlpha), circleSides);
        }
    };

    appendAxisArc(gizmo.ringX, AxisId::X, FVector(0.0f, 0.0f, 1.0f), FVector(0.0f, 1.0f, 0.0f), AxisColorX());
    appendAxisArc(gizmo.ringY, AxisId::Y, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), AxisColorY());
    appendAxisArc(gizmo.ringZ, AxisId::Z, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), AxisColorZ());

    if (desc.includeScreenRing && (!desc.dragging || desc.activeAxis == AxisId::Screen))
    {
        const float screenOuter = (kOuterAxisCircleRadius * 1.25f * s) + gizmoSize;
        const float screenInner = ((kOuterAxisCircleRadius - 1.0f) * 1.25f * s) + gizmoSize;
        AppendArcBand(
            gizmo.screenRing,
            NormalizeSafe(desc.viewUp, FVector(0.0f, 1.0f, 0.0f)),
            NormalizeSafe(desc.viewRight, FVector(1.0f, 0.0f, 0.0f)),
            screenInner,
            screenOuter,
            0.0f,
            kTwoPi,
            ApplyAlpha(ScreenAxisColor(), kLargeOuterAlpha),
            circleSides);
    }

    if (desc.includeArcball && (!desc.dragging || desc.activeAxis == AxisId::XYZ))
    {
        AppendSphere(gizmo.arcball, FVector(0.0f, 0.0f, 0.0f), innerRadius, 32, 24, ArcballColor());
    }

    return gizmo;
}

ScaleGizmo GenerateScaleGizmo(const ScaleDesc& desc)
{
    ScaleGizmo gizmo;
    const float s = desc.uniformScale;
    const float gizmoSize = static_cast<float>(desc.transformGizmoSize);
    const float axisLength = std::max(1.0f, kAxisLength + gizmoSize - (kAxisLengthScaleOffset * 2.0f));
    const float cubeCenter = kAxisLength + gizmoSize + kCubeHeadOffset - kAxisLengthScaleOffset;
    const FVector xDir(1.0f, 0.0f, 0.0f);
    const FVector yDir(0.0f, desc.leftUpForward ? -1.0f : 1.0f, 0.0f);
    const FVector zDir(0.0f, 0.0f, 1.0f);

    AppendCylinder(gizmo.axisX, FVector(0.0f, 0.0f, 0.0f), xDir * (axisLength * s), kCylinderRadius * s, 16, AxisColorX());
    AppendCylinder(gizmo.axisY, FVector(0.0f, 0.0f, 0.0f), yDir * (axisLength * s), kCylinderRadius * s, 16, AxisColorY());
    AppendCylinder(gizmo.axisZ, FVector(0.0f, 0.0f, 0.0f), zDir * (axisLength * s), kCylinderRadius * s, 16, AxisColorZ());

    AppendOrientedBox(gizmo.axisX, xDir * (cubeCenter * s), Basis3{xDir, yDir, zDir}, FVector(kScaleCenterCubeHalf * s, kScaleCenterCubeHalf * s, kScaleCenterCubeHalf * s), AxisColorX());
    AppendOrientedBox(gizmo.axisY, yDir * (cubeCenter * s), Basis3{yDir, xDir * -1.0f, zDir}, FVector(kScaleCenterCubeHalf * s, kScaleCenterCubeHalf * s, kScaleCenterCubeHalf * s), AxisColorY());
    AppendOrientedBox(gizmo.axisZ, zDir * (cubeCenter * s), Basis3{zDir, xDir, yDir}, FVector(kScaleCenterCubeHalf * s, kScaleCenterCubeHalf * s, kScaleCenterCubeHalf * s), AxisColorZ());

    AppendScalePlane(gizmo.planeXY, xDir, yDir, zDir, AxisColorX(), AxisColorY(), s);
    AppendScalePlane(gizmo.planeXZ, xDir, zDir, yDir * -1.0f, AxisColorX(), AxisColorZ(), s);
    AppendScalePlane(gizmo.planeYZ, yDir, zDir, xDir, AxisColorY(), AxisColorZ(), s);

    if (desc.includeCenterCube)
    {
        AppendOrientedBox(gizmo.centerCube, FVector(0.0f, 0.0f, 0.0f), Basis3{xDir, yDir, zDir}, FVector(kScaleCenterCubeHalf * s, kScaleCenterCubeHalf * s, kScaleCenterCubeHalf * s), ScreenSpaceColor());
    }

    return gizmo;
}

Mesh Combine(const TranslationGizmo& gizmo)
{
    return MergeMeshes({&gizmo.axisX, &gizmo.axisY, &gizmo.axisZ, &gizmo.planeXY, &gizmo.planeXZ, &gizmo.planeYZ, &gizmo.screenSphere});
}

Mesh Combine(const RotationGizmo& gizmo)
{
    return MergeMeshes({&gizmo.ringX, &gizmo.ringY, &gizmo.ringZ, &gizmo.screenRing, &gizmo.arcball});
}

Mesh Combine(const ScaleGizmo& gizmo)
{
    return MergeMeshes({&gizmo.axisX, &gizmo.axisY, &gizmo.axisZ, &gizmo.planeXY, &gizmo.planeXZ, &gizmo.planeYZ, &gizmo.centerCube});
}
