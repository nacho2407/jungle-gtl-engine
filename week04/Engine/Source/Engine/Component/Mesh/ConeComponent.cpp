#include "ConeComponent.h"

#include "Resources/Mesh/Cone.h"

namespace Engine::Component
{
    bool UConeComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        OutTriangles.clear();

        if (cone_topology != EMeshPrimitiveTopology::TriangleList)
        {
            return false;
        }

        for (uint32_t i = 0; i + 2 < cone_index_count; i += 3)
        {
            const uint16_t I0 = cone_indices[i + 0];
            const uint16_t I1 = cone_indices[i + 1];
            const uint16_t I2 = cone_indices[i + 2];

            if (I0 >= cone_vertex_count || I1 >= cone_vertex_count || I2 >= cone_vertex_count)
            {
                continue;
            }

            Geometry::FTriangle Triangle;
            Triangle.V0 = FVector{cone_vertices[I0].x, cone_vertices[I0].y, cone_vertices[I0].z};
            Triangle.V1 = FVector{cone_vertices[I1].x, cone_vertices[I1].y, cone_vertices[I1].z};
            Triangle.V2 = FVector{cone_vertices[I2].x, cone_vertices[I2].y, cone_vertices[I2].z};

            OutTriangles.push_back(Triangle);
        }

        return OutTriangles.size() > 0;
    }

    Geometry::FAABB UConeComponent::GetLocalAABB() const
    {
        FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (uint32_t i = 0; i < cone_vertex_count; ++i)
        {
            const FVector P(cone_vertices[i].x, cone_vertices[i].y, cone_vertices[i].z);

            Min.X = std::min(Min.X, P.X);
            Min.Y = std::min(Min.Y, P.Y);
            Min.Z = std::min(Min.Z, P.Z);

            Max.X = std::max(Max.X, P.X);
            Max.Y = std::max(Max.Y, P.Y);
            Max.Z = std::max(Max.Z, P.Z);
        }

        return Geometry::FAABB(Min, Max);
    }

    REGISTER_CLASS(Engine::Component, UConeComponent)
} // namespace Engine::Component
