#include "Resource/BuiltInMeshFactory.h"
#include "Resource/UStaticMesh.h"
#include "Geometry/Sphere.h"
#include "Geometry/Cube.h"

namespace
{
    //std::vector<FVertexSimple> CreateCubeVertices()
    //{
    //    const float s = 0.5f;

    //    return
    //    {
    //        FVertexSimple(-s, -s, -s, 1,0,0,1), FVertexSimple(-s, +s, -s, 1,0,0,1), FVertexSimple(+s, +s, -s, 1,0,0,1),
    //        FVertexSimple(-s, -s, -s, 1,0,0,1), FVertexSimple(+s, +s, -s, 1,0,0,1), FVertexSimple(+s, -s, -s, 1,0,0,1),
    //    };
    //}

    std::vector<FVertexSimple> CreateTriangleVertices()
    {
        return
        {
            FVertexSimple(0.0f, 0.5f, 0.0f, 1,0,0,1),
            FVertexSimple(0.5f, -0.5f, 0.0f, 0,1,0,1),
            FVertexSimple(-0.5f, -0.5f, 0.0f, 0,0,1,1),
        };
    }

    std::vector<FVertexSimple> CreateAxesVertices()
    {
        return
        {
            FVertexSimple(0,0,0, 1,0,0,1), FVertexSimple(3,0,0, 1,0,0,1),
            FVertexSimple(0,0,0, 0,1,0,1), FVertexSimple(0,3,0, 0,1,0,1),
            FVertexSimple(0,0,0, 0,0,1,1), FVertexSimple(0,0,3, 0,0,1,1),
        };
    }
}

namespace BuiltInMeshFactory
{
    UStaticMesh* CreateSphereMesh()
    {
        UStaticMesh* Mesh = new UStaticMesh();

        std::vector<FVertexSimple> Vertices(
            sphere_vertices,
            sphere_vertices + sphere_vertex_count);

        Mesh->SetVertices(Vertices);
        Mesh->SetTopology(EMeshTopology::TriangleList);
        return Mesh;
    }

    UStaticMesh* CreateCubeMesh()
    {
        UStaticMesh* Mesh = new UStaticMesh();

        std::vector<FVertexSimple> Vertices(
            cube_vertices,
            cube_vertices + cube_vertex_count);
        std::vector<uint32_t> Indices(
            cube_indices,
            cube_indices + cube_index_count);

        Mesh->SetVertices(Vertices);
        Mesh->SetIndices(Indices);
        Mesh->SetTopology(EMeshTopology::TriangleList);
        return Mesh;
    }

    UStaticMesh* CreateTriangleMesh()
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateTriangleVertices());
        Mesh->SetTopology(EMeshTopology::TriangleList);
        return Mesh;
    }

    UStaticMesh* CreateAxesMesh()
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateAxesVertices());
        Mesh->SetTopology(EMeshTopology::LineList);
        return Mesh;
    }
}