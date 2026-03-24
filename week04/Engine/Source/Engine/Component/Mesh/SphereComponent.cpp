#include "SphereComponent.h"

#include "Resources/Mesh/Sphere.h"

namespace Engine::Component
{
    bool USphereComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        OutTriangles.clear();

        if (sphere_topology != EMeshPrimitiveTopology::TriangleList)
        {
            return false;
        }

        for (uint32_t i = 0; i + 2 < sphere_index_count; i += 3)
        {
            const uint16_t I0 = sphere_indices[i + 0];
            const uint16_t I1 = sphere_indices[i + 1];
            const uint16_t I2 = sphere_indices[i + 2];

            if (I0 >= sphere_vertex_count || I1 >= sphere_vertex_count || I2 >= sphere_vertex_count)
            {
                continue;
            }

            Geometry::FTriangle Triangle;
            Triangle.V0 = FVector{sphere_vertices[I0].x, sphere_vertices[I0].y, sphere_vertices[I0].z};
            Triangle.V1 = FVector{sphere_vertices[I1].x, sphere_vertices[I1].y, sphere_vertices[I1].z};
            Triangle.V2 = FVector{sphere_vertices[I2].x, sphere_vertices[I2].y, sphere_vertices[I2].z};

            OutTriangles.push_back(Triangle);
        }

        return OutTriangles.size() > 0;
    }

    Geometry::FAABB USphereComponent::GetLocalAABB() const
    {
        FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (uint32_t i = 0; i < sphere_vertex_count; ++i)
        {
            const FVector P(sphere_vertices[i].x, sphere_vertices[i].y, sphere_vertices[i].z);

            Min.X = std::min(Min.X, P.X);
            Min.Y = std::min(Min.Y, P.Y);
            Min.Z = std::min(Min.Z, P.Z);

            Max.X = std::max(Max.X, P.X);
            Max.Y = std::max(Max.Y, P.Y);
            Max.Z = std::max(Max.Z, P.Z);
        }

        return Geometry::FAABB(Min, Max);
    }

    REGISTER_CLASS(Engine::Component, USphereComponent)
} // namespace Engine::Component