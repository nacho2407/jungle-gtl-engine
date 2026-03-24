#include "CylinderComponent.h"

#include "Resources/Mesh/Cylinder.h"

namespace Engine::Component
{
    bool UCylinderComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        OutTriangles.clear();

        if (cylinder_topology != EMeshPrimitiveTopology::TriangleList)
        {
            return false;
        }

        for (uint32_t i = 0; i + 2 < cylinder_index_count; i += 3)
        {
            const uint16_t I0 = cylinder_indices[i + 0];
            const uint16_t I1 = cylinder_indices[i + 1];
            const uint16_t I2 = cylinder_indices[i + 2];

            if (I0 >= cylinder_vertex_count || I1 >= cylinder_vertex_count ||
                I2 >= cylinder_vertex_count)
            {
                continue;
            }

            Geometry::FTriangle Triangle;
            Triangle.V0 = FVector{cylinder_vertices[I0].x, cylinder_vertices[I0].y,
                                  cylinder_vertices[I0].z};
            Triangle.V1 = FVector{cylinder_vertices[I1].x, cylinder_vertices[I1].y,
                                  cylinder_vertices[I1].z};
            Triangle.V2 = FVector{cylinder_vertices[I2].x, cylinder_vertices[I2].y,
                                  cylinder_vertices[I2].z};

            OutTriangles.push_back(Triangle);
        }

        return OutTriangles.size() > 0;
    }

    Geometry::FAABB UCylinderComponent::GetLocalAABB() const
    {
        FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (uint32_t i = 0; i < cylinder_vertex_count; ++i)
        {
            const FVector P(cylinder_vertices[i].x, cylinder_vertices[i].y, cylinder_vertices[i].z);

            Min.X = std::min(Min.X, P.X);
            Min.Y = std::min(Min.Y, P.Y);
            Min.Z = std::min(Min.Z, P.Z);

            Max.X = std::max(Max.X, P.X);
            Max.Y = std::max(Max.Y, P.Y);
            Max.Z = std::max(Max.Z, P.Z);
        }

        return Geometry::FAABB(Min, Max);
    }

    REGISTER_CLASS(Engine::Component, UCylinderComponent)
} // namespace Engine::Component
