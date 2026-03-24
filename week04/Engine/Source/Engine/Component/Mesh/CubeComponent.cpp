#include "CubeComponent.h"

#include "Resources/Mesh/Cube.h"

namespace Engine::Component
{
    bool UCubeComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        OutTriangles.clear();

        if (cube_topology != EMeshPrimitiveTopology::TriangleList)
        {
            return false;
        }

        for (uint32_t i = 0; i + 2 < cube_index_count; i += 3)
        {
            const uint16_t I0 = cube_indices[i + 0];
            const uint16_t I1 = cube_indices[i + 1];
            const uint16_t I2 = cube_indices[i + 2];
            
            if (I0 >= cube_vertex_count || I1 >= cube_vertex_count || I2 >= cube_vertex_count)
            {
                continue;
            }
            
            Geometry::FTriangle Triangle;
            Triangle.V0 = FVector{cube_vertices[I0].x , cube_vertices[I0].y, cube_vertices[I0].z};
            Triangle.V1 = FVector{cube_vertices[I1].x , cube_vertices[I1].y, cube_vertices[I1].z};
            Triangle.V2 = FVector{cube_vertices[I2].x , cube_vertices[I2].y, cube_vertices[I2].z};
            
            OutTriangles.push_back(Triangle);
        }
        
        return OutTriangles.size() > 0;
    }

    Geometry::FAABB UCubeComponent::GetLocalAABB() const
    {
        FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (uint32_t i = 0; i < cube_vertex_count; ++i)
        {
            const FVector P(cube_vertices[i].x, cube_vertices[i].y, cube_vertices[i].z);

            Min.X = std::min(Min.X, P.X);
            Min.Y = std::min(Min.Y, P.Y);
            Min.Z = std::min(Min.Z, P.Z);

            Max.X = std::max(Max.X, P.X);
            Max.Y = std::max(Max.Y, P.Y);
            Max.Z = std::max(Max.Z, P.Z);
        }

        return Geometry::FAABB(Min, Max);
    }

    REGISTER_CLASS(Engine::Component, UCubeComponent)
} // namespace Engine::Component