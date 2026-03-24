#include "TriangleComponent.h"

#include "Resources/Mesh/Triangle.h"

namespace Engine::Component
{
    bool UTriangleComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        OutTriangles.clear();

        if (triangle_topology != EMeshPrimitiveTopology::TriangleList)
        {
            return false;
        }

        for (uint32_t i = 0; i + 2 < triangle_index_count; i += 3)
        {
            const uint16_t I0 = triangle_indices[i + 0];
            const uint16_t I1 = triangle_indices[i + 1];
            const uint16_t I2 = triangle_indices[i + 2];

            if (I0 >= triangle_vertex_count || I1 >= triangle_vertex_count || I2 >= triangle_vertex_count)
            {
                continue;
            }

            Geometry::FTriangle Triangle;
            Triangle.V0 = FVector{triangle_vertices[I0].x, triangle_vertices[I0].y, triangle_vertices[I0].z};
            Triangle.V1 = FVector{triangle_vertices[I1].x, triangle_vertices[I1].y, triangle_vertices[I1].z};
            Triangle.V2 = FVector{triangle_vertices[I2].x, triangle_vertices[I2].y, triangle_vertices[I2].z};

            OutTriangles.push_back(Triangle);
        }

        return OutTriangles.size() > 0;
    }

    Geometry::FAABB UTriangleComponent::GetLocalAABB() const
    {
        FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (uint32_t i = 0; i < triangle_vertex_count; ++i)
        {
            const FVector P(triangle_vertices[i].x, triangle_vertices[i].y, triangle_vertices[i].z);

            Min.X = std::min(Min.X, P.X);
            Min.Y = std::min(Min.Y, P.Y);
            Min.Z = std::min(Min.Z, P.Z);

            Max.X = std::max(Max.X, P.X);
            Max.Y = std::max(Max.Y, P.Y);
            Max.Z = std::max(Max.Z, P.Z);
        }

        return Geometry::FAABB(Min, Max);
    }

    EBasicMeshType UTriangleComponent::GetBasicMeshType() const { return EBasicMeshType::Triangle; }

    REGISTER_CLASS(Engine::Component, UTriangleComponent)
} // namespace Engine::Component
