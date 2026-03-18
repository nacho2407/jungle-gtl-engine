#include <cmath>

#include "CoreTypes.h"

#include "Resource/BuiltInMeshFactory.h"
#include "Resource/UStaticMesh.h"
#include "Geometry/Sphere.h"
#include "Geometry/Cube.h"
#include <cmath>

namespace
{
    const float Pi = 3.14159265358979323846f;

    TArray<FVertexSimple> CreateCubeVertices()
    {
        return
        {
            FVertexSimple(0.0f, 0.5f, 0.0f, 1,0,0,1),
            FVertexSimple(0.5f, -0.5f, 0.0f, 0,1,0,1),
            FVertexSimple(-0.5f, -0.5f, 0.0f, 0,0,1,1),
        };
    }

    TArray<FVertexSimple> CreateAxesVertices() {
        TArray<FVertexSimple> Vertices; 
        //const float Extent = 20.0f; 
        //Vertices.reserve(6); 
        //Vertices.emplace_back(-Extent, 0, 0, 1, 0, 0, 1); 
        //Vertices.emplace_back( Extent, 0, 0, 1, 0, 0, 1); 

        //Vertices.emplace_back(0, 0, 0, 0, 1, 0, 1); 
        //Vertices.emplace_back(0, Extent, 0, 0, 1, 0, 1); 
        //
        //Vertices.emplace_back(0, 0, -Extent, 0, 0, 1, 1); 
        //Vertices.emplace_back(0, 0, Extent, 0, 0, 1, 1); 

        // Y axis only
        Vertices.emplace_back(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
        Vertices.emplace_back(0.0f, 20.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);

        return Vertices;
    }

    TArray<FVertexSimple> CreateTriangleVertices()
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

    TArray<FVertexSimple> CreateGizmoArrowVertices()
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

    TArray<uint32> CreateGizmoArrowIndices()
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

    TArray<FVertexSimple> CreateGizmoScaleVertices()
    {
        const FVector4 W(1, 1, 1, 1);

        return
        {
            FVertexSimple(0.0f, -0.08f, -0.08f, W.X, W.Y, W.Z, W.W), // 0
            FVertexSimple(2.2f, -0.08f, -0.08f, W.X, W.Y, W.Z, W.W), // 1
            FVertexSimple(2.2f,  0.08f, -0.08f, W.X, W.Y, W.Z, W.W), // 2
            FVertexSimple(0.0f,  0.08f, -0.08f, W.X, W.Y, W.Z, W.W), // 3

            FVertexSimple(0.0f, -0.08f,  0.08f, W.X, W.Y, W.Z, W.W), // 4
            FVertexSimple(2.2f, -0.08f,  0.08f, W.X, W.Y, W.Z, W.W), // 5
            FVertexSimple(2.2f,  0.08f,  0.08f, W.X, W.Y, W.Z, W.W), // 6
            FVertexSimple(0.0f,  0.08f,  0.08f, W.X, W.Y, W.Z, W.W), // 7

            FVertexSimple(2.2f, -0.22f, -0.22f, W.X, W.Y, W.Z, W.W), // 8
            FVertexSimple(2.6f, -0.22f, -0.22f, W.X, W.Y, W.Z, W.W), // 9
            FVertexSimple(2.6f,  0.22f, -0.22f, W.X, W.Y, W.Z, W.W), // 10
            FVertexSimple(2.2f,  0.22f, -0.22f, W.X, W.Y, W.Z, W.W), // 11

            FVertexSimple(2.2f, -0.22f,  0.22f, W.X, W.Y, W.Z, W.W), // 12
            FVertexSimple(2.6f, -0.22f,  0.22f, W.X, W.Y, W.Z, W.W), // 13
            FVertexSimple(2.6f,  0.22f,  0.22f, W.X, W.Y, W.Z, W.W), // 14
            FVertexSimple(2.2f,  0.22f,  0.22f, W.X, W.Y, W.Z, W.W), // 15
        };
    }

    TArray<uint32> CreateGizmoScaleIndices()
    {
        return
        {
            // shaft
            0,2,1,  0,3,2,
            4,5,6,  4,6,7,

            0,1,5,  0,5,4,
            3,7,6,  3,6,2,

            0,4,7,  0,7,3,
            1,2,6,  1,6,5,

            // cube
            8,10,9,   8,11,10,
            12,13,14, 12,14,15,

            8,9,13,   8,13,12,
            11,15,14, 11,14,10,

            8,12,15,  8,15,11,
            9,10,14,  9,14,13
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

    //std::vector<FVertexSimple> CreateCircleVertices(int Segments)
    //{
    //    std::vector<FVertexSimple> V;
    //    V.reserve(Segments * 2);

    //    const float PI = 3.14159265358979323846f;

    //    for (int i = 0; i < Segments; ++i)
    //    {
    //        const float T0 = (2.0f * PI * i) / Segments;
    //        const float T1 = (2.0f * PI * (i + 1)) / Segments;

    //        const float X0 = std::cos(T0);
    //        const float Y0 = std::sin(T0);
    //        const float X1 = std::cos(T1);
    //        const float Y1 = std::sin(T1);

    //        // XY 평면의 unit circle, 카메라 정면을 향하게 회전시킬 예정
    //        V.emplace_back(X0, Y0, 0.0f, 1, 1, 1, 1);
    //        V.emplace_back(X1, Y1, 0.0f, 1, 1, 1, 1);
    //    }

    //    return V;
    //}

    TArray<FVertexSimple> CreateDiscVertices(int Segments)
    {
        TArray<FVertexSimple> V;
        V.reserve(Segments + 2);

        const float PI = 3.14159265358979323846f;

        // center
        V.emplace_back(0.0f, 0.0f, 0.0f, 1, 1, 1, 1);

        for (int i = 0; i <= Segments; ++i)
        {
            const float T = (2.0f * PI * i) / Segments;
            const float X = std::cos(T);
            const float Y = std::sin(T);
            V.emplace_back(X, Y, 0.0f, 1, 1, 1, 1);
        }

        return V;
    }

    TArray<uint32> CreateDiscIndices(int Segments)
    {
        TArray<uint32> I;
        I.reserve(Segments * 3);

        for (int i = 1; i <= Segments; ++i)
        {
            I.push_back(0);
            I.push_back(i);
            I.push_back(i + 1);
        }

        return I;
    }

    //TArray<FVertexSimple> CreateGridVertices(int HalfCount, float Spacing)
    //{
    //    TArray<FVertexSimple> Vertices;
    //    const float Extent = HalfCount * Spacing;

    //    for (int i = -HalfCount; i <= HalfCount; ++i)
    //    {
    //        if (i == 0)
    //        {
    //            continue; // 중심 X/Z 선은 axes가 대신 표현
    //        }

    //        const float P = i * Spacing;
    //        const float C = 0.25f;

    //        Vertices.emplace_back(-Extent, 0.0f, P, C, C, C, 1.0f);
    //        Vertices.emplace_back(Extent, 0.0f, P, C, C, C, 1.0f);

    //        Vertices.emplace_back(P, 0.0f, -Extent, C, C, C, 1.0f);
    //        Vertices.emplace_back(P, 0.0f, Extent, C, C, C, 1.0f);
    //    }

    //    return Vertices;
    //}
    TArray<FVertexSimple> CreateGridVertices(int HalfCount, float Spacing)
    {
        TArray<FVertexSimple> Vertices;
        const float Extent = HalfCount * Spacing;

        for (int i = -HalfCount; i <= HalfCount; ++i)
        {
            const float P = i * Spacing;

            if (i == 0)
            {
                // z = 0 선 (x축 방향 선)
                // 음의 X 쪽: 흰색
                Vertices.emplace_back(-Extent, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                Vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);

                // 양의 X 쪽: 빨강
                Vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
                Vertices.emplace_back(Extent, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);

                // x = 0 선 (z축 방향 선)
                // 음의 Z 쪽: 흰색
                Vertices.emplace_back(0.0f, 0.0f, -Extent, 1.0f, 1.0f, 1.0f, 1.0f);
                Vertices.emplace_back(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);

                // 양의 Z 쪽: 파랑
                Vertices.emplace_back(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
                Vertices.emplace_back(0.0f, 0.0f, Extent, 0.0f, 0.0f, 1.0f, 1.0f);

                continue;
            }

            // 나머지 일반 grid 선은 흰색
            Vertices.emplace_back(-Extent, 0.0f, P, 1.0f, 1.0f, 1.0f, 1.0f);
            Vertices.emplace_back(Extent, 0.0f, P, 1.0f, 1.0f, 1.0f, 1.0f);

            Vertices.emplace_back(P, 0.0f, -Extent, 1.0f, 1.0f, 1.0f, 1.0f);
            Vertices.emplace_back(P, 0.0f, Extent, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        return Vertices;
    }

    UStaticMesh* CreateGridMesh(int HalfCount, float Spacing)
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateGridVertices(HalfCount, Spacing));
        Mesh->SetTopology(EMeshTopology::LineList);
        return Mesh;
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

        TArray<FVertexSimple> Vertices(
            cube_vertices,
            cube_vertices + cube_vertex_count);

        Mesh->SetVertices(Vertices);
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

    UStaticMesh* CreateGizmoArrowMesh()
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateGizmoArrowVertices());
        Mesh->SetIndices(CreateGizmoArrowIndices());
        Mesh->SetTopology(EMeshTopology::TriangleList);
        return Mesh;
    }

    UStaticMesh* CreateGizmoScaleMesh()
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateGizmoScaleVertices());
        Mesh->SetIndices(CreateGizmoScaleIndices());
        Mesh->SetTopology(EMeshTopology::TriangleList);
        return Mesh;
	}

//    UStaticMesh* CreateCircleMesh(int Segments)
//    {
//        UStaticMesh* Mesh = new UStaticMesh();
//        Mesh->SetVertices(CreateCircleVertices(Segments));
//        Mesh->SetTopology(EMeshTopology::LineList);
//        return Mesh;
//    }

    UStaticMesh* CreateDiscMesh(int Segments)
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateDiscVertices(Segments));
        Mesh->SetIndices(CreateDiscIndices(Segments));
        Mesh->SetTopology(EMeshTopology::TriangleList);
        return Mesh;
    }
    
    UStaticMesh* CreateGridMesh(int HalfCount, float Spacing)
    {
        UStaticMesh* Mesh = new UStaticMesh();
        Mesh->SetVertices(CreateGridVertices(HalfCount, Spacing));
        Mesh->SetTopology(EMeshTopology::LineList);
        return Mesh;
    }

    UStaticMesh* CreateGizmoRotateRingMesh()
    {
        UStaticMesh* Mesh = new UStaticMesh();

        Mesh->SetVertices(CreateTorusVertices(
            96,     // MajorSegments
            20,     // MinorSegments
            2.0f,   // MajorRadius
            0.15f   // MinorRadius
        ));

        Mesh->SetTopology(EMeshTopology::TriangleList);
        return Mesh;
    }
}