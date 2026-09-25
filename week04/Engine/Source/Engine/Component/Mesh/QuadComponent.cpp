#include "Engine/Component/Mesh/QuadComponent.h"
#include "Resources/Mesh/Quad.h"

namespace Engine::Component
{
    bool UQuadComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        OutTriangles.clear();

        if (quad_topology != EMeshPrimitiveTopology::TriangleList)
        {
            return false;
        }

        for (uint32_t i = 0; i + 2 < quad_index_count; i += 3)
        {
            const uint16_t I0 = quad_indices[i + 0];
            const uint16_t I1 = quad_indices[i + 1];
            const uint16_t I2 = quad_indices[i + 2];

            if (I0 >= quad_vertex_count || I1 >= quad_vertex_count || I2 >= quad_vertex_count)
            {
                continue;
            }

            Geometry::FTriangle Triangle;
            Triangle.V0 = FVector{quad_vertices[I0].x, quad_vertices[I0].y, quad_vertices[I0].z};
            Triangle.V1 = FVector{quad_vertices[I1].x, quad_vertices[I1].y, quad_vertices[I1].z};
            Triangle.V2 = FVector{quad_vertices[I2].x, quad_vertices[I2].y, quad_vertices[I2].z};

            OutTriangles.push_back(Triangle);
        }

        return OutTriangles.size() > 0;
    }

    Geometry::FAABB UQuadComponent::GetLocalAABB() const
    {
        FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (uint32_t i = 0; i < quad_vertex_count; ++i)
        {
            const FVector P(quad_vertices[i].x, quad_vertices[i].y, quad_vertices[i].z);

            Min.X = std::min(Min.X, P.X);
            Min.Y = std::min(Min.Y, P.Y);
            Min.Z = std::min(Min.Z, P.Z);

            Max.X = std::max(Max.X, P.X);
            Max.Y = std::max(Max.Y, P.Y);
            Max.Z = std::max(Max.Z, P.Z);
        }

        return Geometry::FAABB(Min, Max);
    }

    REGISTER_CLASS(Engine::Component, UQuadComponent)
} // namespace Engine::Component
