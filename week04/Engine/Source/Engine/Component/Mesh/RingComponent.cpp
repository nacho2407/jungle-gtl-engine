#include "RingComponent.h"

#include "Resources/Mesh/Ring.h"

namespace Engine::Component
{
    bool URingComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        OutTriangles.clear();

        if (ring_topology != EMeshPrimitiveTopology::TriangleList)
        {
            return false;
        }

        for (uint32_t i = 0; i + 2 < ring_index_count; i += 3)
        {
            const uint16_t I0 = ring_indices[i + 0];
            const uint16_t I1 = ring_indices[i + 1];
            const uint16_t I2 = ring_indices[i + 2];

            if (I0 >= ring_vertex_count || I1 >= ring_vertex_count || I2 >= ring_vertex_count)
            {
                continue;
            }

            Geometry::FTriangle Triangle;
            Triangle.V0 = FVector{ring_vertices[I0].x, ring_vertices[I0].y, ring_vertices[I0].z};
            Triangle.V1 = FVector{ring_vertices[I1].x, ring_vertices[I1].y, ring_vertices[I1].z};
            Triangle.V2 = FVector{ring_vertices[I2].x, ring_vertices[I2].y, ring_vertices[I2].z};

            OutTriangles.push_back(Triangle);
        }

        return OutTriangles.size() > 0;
    }

    Geometry::FAABB URingComponent::GetLocalAABB() const
    {
        FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (uint32_t i = 0; i < ring_vertex_count; ++i)
        {
            const FVector P(ring_vertices[i].x, ring_vertices[i].y, ring_vertices[i].z);

            Min.X = std::min(Min.X, P.X);
            Min.Y = std::min(Min.Y, P.Y);
            Min.Z = std::min(Min.Z, P.Z);

            Max.X = std::max(Max.X, P.X);
            Max.Y = std::max(Max.Y, P.Y);
            Max.Z = std::max(Max.Z, P.Z);
        }

        return Geometry::FAABB(Min, Max);
    }

    REGISTER_CLASS(Engine::Component, URingComponent)
} // namespace Engine::Component
