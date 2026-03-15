#include <cmath>

#include "CoreTypes.h"

#include "Resource/BuiltInMeshFactory.h"
#include "Resource/UStaticMesh.h"
#include "Geometry/Sphere.h"

namespace
{
    const float Pi = 3.14159265358979323846f;

    TArray<FVertexSimple> CreateCubeVertices()
    {
        const float s = 0.5f;

        return
        {
            FVertexSimple(-s, -s, -s, 1,0,0,1), FVertexSimple(-s, +s, -s, 1,0,0,1), FVertexSimple(+s, +s, -s, 1,0,0,1),
            FVertexSimple(-s, -s, -s, 1,0,0,1), FVertexSimple(+s, +s, -s, 1,0,0,1), FVertexSimple(+s, -s, -s, 1,0,0,1),
        };
    }

    TArray<FVertexSimple> CreateTriangleVertices()
    {
        return
        {
            FVertexSimple(0.0f, 0.5f, 0.0f, 1,0,0,1),
            FVertexSimple(0.5f, -0.5f, 0.0f, 0,1,0,1),
            FVertexSimple(-0.5f, -0.5f, 0.0f, 0,0,1,1),
        };
    }

    TArray<FVertexSimple> CreateAxesVertices()
    {
        return
        {
            FVertexSimple(0,0,0, 1,0,0,1), FVertexSimple(3,0,0, 1,0,0,1),
            FVertexSimple(0,0,0, 0,1,0,1), FVertexSimple(0,3,0, 0,1,0,1),
            FVertexSimple(0,0,0, 0,0,1,1), FVertexSimple(0,0,3, 0,0,1,1),
        };
    }

    TArray<FVertexSimple> CreateTorusVertices(
        int MajorSegments,
        int MinorSegments,
        float MajorRadius,
        float MinorRadius)
    {
        TArray<FVertexSimple> Vertices;

        if (MajorSegments < 3 || MinorSegments < 3)
        {
            return Vertices;
        }

        auto MakePoint = [&](float U, float V) -> FVector
            {
                const float CosU = std::cos(U);
                const float SinU = std::sin(U);
                const float CosV = std::cos(V);
                const float SinV = std::sin(V);

                const float Ring = MajorRadius + MinorRadius * CosV;

                const float X = Ring * CosU;
                const float Y = MinorRadius * SinV;
                const float Z = Ring * SinU;

                return FVector(X, Y, Z);
            };

        for (int Major = 0; Major < MajorSegments; ++Major)
        {
            const int NextMajor = (Major + 1) % MajorSegments;

            const float U0 = (2.0f * Pi * static_cast<float>(Major)) / static_cast<float>(MajorSegments);
            const float U1 = (2.0f * Pi * static_cast<float>(NextMajor)) / static_cast<float>(MajorSegments);

            for (int Minor = 0; Minor < MinorSegments; ++Minor)
            {
                const int NextMinor = (Minor + 1) % MinorSegments;

                const float V0 = (2.0f * Pi * static_cast<float>(Minor)) / static_cast<float>(MinorSegments);
                const float V1 = (2.0f * Pi * static_cast<float>(NextMinor)) / static_cast<float>(MinorSegments);

                const FVector P00 = MakePoint(U0, V0);
                const FVector P01 = MakePoint(U0, V1);
                const FVector P10 = MakePoint(U1, V0);
                const FVector P11 = MakePoint(U1, V1);

                // 보기 편하게 약간 색 변화
                const float T0 = static_cast<float>(Major) / static_cast<float>(MajorSegments);
                const float T1 = static_cast<float>(Minor) / static_cast<float>(MinorSegments);

                const FVector4 C0(0.9f, 0.4f + 0.4f * T0, 0.2f + 0.5f * T1, 1.0f);
                const FVector4 C1(1.0f, 0.6f + 0.3f * T0, 0.3f + 0.4f * T1, 1.0f);

                // Quad -> triangle 2개
                Vertices.push_back(FVertexSimple(P00, C0));
                Vertices.push_back(FVertexSimple(P10, C0));
                Vertices.push_back(FVertexSimple(P11, C1));

                Vertices.push_back(FVertexSimple(P00, C0));
                Vertices.push_back(FVertexSimple(P11, C1));
                Vertices.push_back(FVertexSimple(P01, C1));
            }
        }

        return Vertices;
    }
}

namespace BuiltInMeshFactory
{
    UStaticMesh* CreateSphereMesh()
    {
        UStaticMesh* Mesh = new UStaticMesh();

        TArray<FVertexSimple> Vertices(
            sphere_vertices,
            sphere_vertices + sphere_vertex_count);

        Mesh->SetVertices(Vertices);
        Mesh->SetTopology(EMeshTopology::TriangleList);
        return Mesh;
    }

    UStaticMesh* CreateCubeMesh()
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateCubeVertices());
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

    // for test Hit Proxy
    UStaticMesh* CreateTorusMesh(
        int MajorSegments,
        int MinorSegments,
        float MajorRadius,
        float MinorRadius)
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateTorusVertices(
            MajorSegments,
            MinorSegments,
            MajorRadius,
            MinorRadius));
        Mesh->SetTopology(EMeshTopology::TriangleList);
        return Mesh;
    }
}