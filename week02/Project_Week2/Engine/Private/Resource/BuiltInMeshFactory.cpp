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

    std::vector<FVertexSimple> CreateGizmoArrowVertices()
    {
        const FVector4 W(1, 1, 1, 1);

        return
        {
            // shaft box: x = [0.0, 2.2], y/z = ±0.08
            FVertexSimple(0.0f, -0.08f, -0.08f, W.X, W.Y, W.Z, W.W), // 0
            FVertexSimple(2.2f, -0.08f, -0.08f, W.X, W.Y, W.Z, W.W), // 1
            FVertexSimple(2.2f,  0.08f, -0.08f, W.X, W.Y, W.Z, W.W), // 2
            FVertexSimple(0.0f,  0.08f, -0.08f, W.X, W.Y, W.Z, W.W), // 3

            FVertexSimple(0.0f, -0.08f,  0.08f, W.X, W.Y, W.Z, W.W), // 4
            FVertexSimple(2.2f, -0.08f,  0.08f, W.X, W.Y, W.Z, W.W), // 5
            FVertexSimple(2.2f,  0.08f,  0.08f, W.X, W.Y, W.Z, W.W), // 6
            FVertexSimple(0.0f,  0.08f,  0.08f, W.X, W.Y, W.Z, W.W), // 7

            // arrow head base: x = 2.2, y/z = ±0.22
            FVertexSimple(2.2f, -0.22f, -0.22f, W.X, W.Y, W.Z, W.W), // 8
            FVertexSimple(2.2f,  0.22f, -0.22f, W.X, W.Y, W.Z, W.W), // 9
            FVertexSimple(2.2f,  0.22f,  0.22f, W.X, W.Y, W.Z, W.W), // 10
            FVertexSimple(2.2f, -0.22f,  0.22f, W.X, W.Y, W.Z, W.W), // 11

            // tip
            FVertexSimple(3.0f, 0.0f, 0.0f, W.X, W.Y, W.Z, W.W),     // 12
        };
    }

    std::vector<uint32_t> CreateGizmoArrowIndices()
    {
        return
        {
            // shaft box
            0,2,1,  0,3,2,   // back (-z)
            4,5,6,  4,6,7,   // front (+z)

            0,1,5,  0,5,4,   // bottom (-y)
            3,7,6,  3,6,2,   // top (+y)

            0,4,7,  0,7,3,   // left (x=0)
            1,2,6,  1,6,5,   // right (x=2.2)

            // head base quad
            8,10,9,
            8,11,10,

            // head sides
            8,9,12,
            9,10,12,
            10,11,12,
            11,8,12
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

    UStaticMesh* CreateGizmoArrowMesh()
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateGizmoArrowVertices());
        Mesh->SetIndices(CreateGizmoArrowIndices());
        Mesh->SetTopology(EMeshTopology::TriangleList);
        return Mesh;
    }
}