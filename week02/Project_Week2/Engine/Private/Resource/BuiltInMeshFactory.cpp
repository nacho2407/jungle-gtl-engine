#include "Resource/BuiltInMeshFactory.h"
#include "Resource/UStaticMesh.h"
#include "Geometry/Sphere.h"
#include "Geometry/Cube.h"

namespace
{
    std::vector<FVertexSimple> CreateTriangleVertices()
    {
        return
        {
            FVertexSimple(0.0f, 0.5f, 0.0f, 1,0,0,1),
            FVertexSimple(0.5f, -0.5f, 0.0f, 0,1,0,1),
            FVertexSimple(-0.5f, -0.5f, 0.0f, 0,0,1,1),
        };
    }

    std::vector<FVertexSimple> CreateAxesVertices() // @@@ 이거 쓰고 있으면 Axes.h는 안 쓰고 있는 거 아닌가??
    {
        return
        {
            FVertexSimple(0,0,0, 1,0,0,1), FVertexSimple(5,0,0, 1,0,0,1),
            FVertexSimple(0,0,0, 0,1,0,1), FVertexSimple(0,5,0, 0,1,0,1),
            FVertexSimple(0,0,0, 0,0,1,1), FVertexSimple(0,0,5, 0,0,1,1),
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