// 렌더 영역의 세부 동작을 구현합니다.
#include "MeshBufferManager.h"
#include "Math/MathUtils.h"

void FMeshBufferManager::Initialize(ID3D11Device* InDevice)
{
    if (bIsInitialized)
        return;

    CreatePrimitiveMeshData();

    // CPU 메시 데이터 → GPU 버퍼 업로드
    for (auto& [Type, Data] : MeshDataMap)
    {
        MeshBufferMap[Type].Create(InDevice, Data);
    }
    for (auto& [Type, Data] : PNCTMeshDataMap)
    {
        MeshBufferMap[Type].Create(InDevice, Data);
    }

    bIsInitialized = true;
}

void FMeshBufferManager::Release()
{
    for (auto& [Type, Buffer] : MeshBufferMap)
    {
        Buffer.Release();
    }
    MeshBufferMap.clear();
    MeshDataMap.clear();
    bIsInitialized = false;
}

FMeshBuffer& FMeshBufferManager::GetMeshBuffer(EPrimitiveMeshShape InShape)
{
    auto it = MeshBufferMap.find(InShape);
    if (it != MeshBufferMap.end())
    {
        return it->second;
    }
    return MeshBufferMap.at(EPrimitiveMeshShape::Cube);
}

const FMeshData& FMeshBufferManager::GetMeshData(EPrimitiveMeshShape InShape) const
{
    auto it = MeshDataMap.find(InShape);
    if (it != MeshDataMap.end())
    {
        return it->second;
    }
    return MeshDataMap.at(EPrimitiveMeshShape::Cube);
}

void FMeshBufferManager::CreatePrimitiveMeshData()
{
    CreateCube();
    CreatePlane();
    CreateSphere();
    CreateTranslationGizmo();
    CreateRotationGizmo();
    CreateScaleGizmo();
    CreateQuad();
    CreateTexturedQuad();
}

void FMeshBufferManager::CreateCube()
{
    FMeshData&       Data     = MeshDataMap[EPrimitiveMeshShape::Cube];
    TArray<FVertex>& vertices = Data.Vertices;
    TArray<uint32>&  indices  = Data.Indices;

    auto MakeColor = [](float x, float y, float z) -> FVector4
    {
        float r = powf(x + 0.5f, 0.8f);
        float g = powf(y + 0.5f, 0.8f);
        float b = powf(z + 0.5f, 0.8f);
        return FVector4(r, g, b, 1.0f);
    };

    auto V = [&](float x, float y, float z)
    {
        return FVertex{ FVector(x, y, z), MakeColor(x, y, z) };
    };

    vertices = {
        // FRONT
        V(0.5f, -0.5f, -0.5f),
        V(0.5f, -0.5f, 0.5f),
        V(0.5f, 0.5f, 0.5f),
        V(0.5f, 0.5f, -0.5f),
        // BACK
        V(-0.5f, -0.5f, -0.5f),
        V(-0.5f, 0.5f, -0.5f),
        V(-0.5f, 0.5f, 0.5f),
        V(-0.5f, -0.5f, 0.5f),
        // LEFT
        V(-0.5f, -0.5f, -0.5f),
        V(-0.5f, -0.5f, 0.5f),
        V(0.5f, -0.5f, 0.5f),
        V(0.5f, -0.5f, -0.5f),
        // RIGHT
        V(-0.5f, 0.5f, -0.5f),
        V(0.5f, 0.5f, -0.5f),
        V(0.5f, 0.5f, 0.5f),
        V(-0.5f, 0.5f, 0.5f),
        // TOP
        V(-0.5f, -0.5f, 0.5f),
        V(-0.5f, 0.5f, 0.5f),
        V(0.5f, 0.5f, 0.5f),
        V(0.5f, -0.5f, 0.5f),
        // BOTTOM
        V(-0.5f, -0.5f, -0.5f),
        V(0.5f, -0.5f, -0.5f),
        V(0.5f, 0.5f, -0.5f),
        V(-0.5f, 0.5f, -0.5f),
    };

    indices = {
        0, 2, 1, 0, 3, 2,
        4, 6, 5, 4, 7, 6,
        8, 10, 9, 8, 11, 10,
        12, 14, 13, 12, 15, 14,
        16, 18, 17, 16, 19, 18,
        20, 22, 21, 20, 23, 22
    };
}

void FMeshBufferManager::CreateSphere(int Slices, int Stacks)
{
    FMeshData&       Data     = MeshDataMap[EPrimitiveMeshShape::Sphere];
    TArray<FVertex>& vertices = Data.Vertices;
    TArray<uint32>&  indices  = Data.Indices;

    const float Radius = 0.5f;

    for (int i = 0; i <= Stacks; ++i)
    {
        float pi = 3.141592f * (float)i / Stacks;
        for (int j = 0; j <= Slices; ++j)
        {
            float theta = 2.0f * 3.141592f * (float)j / Slices;

            float x = Radius * sin(pi) * cos(theta);
            float y = Radius * sin(pi) * sin(theta);
            float z = Radius * cos(pi);

            FVector4 color(x * 0.5f + 0.5f, y * 0.5f + 0.5f, z * 0.5f + 0.5f, 1.0f);
            vertices.push_back({ { x, y, z }, color });
        }
    }

    for (int i = 0; i < Stacks; ++i)
    {
        for (int j = 0; j < Slices; ++j)
        {
            uint32 first  = i * (Slices + 1) + j;
            uint32 second = first + Slices + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
}

void FMeshBufferManager::CreateRotationGizmo()
{
    FMeshData&       Data     = MeshDataMap[EPrimitiveMeshShape::RotGizmo];
    TArray<FVertex>& vertices = Data.Vertices;
    TArray<uint32>&  indices  = Data.Indices;

    const float Radius       = 1.0f;
    const float Thickness    = 0.03f;
    const int   Segments     = 64;
    const int   TubeSegments = 8;

    FVector4 Colors[3] = {
        FVector4(1.0f, 0.0f, 0.0f, 1.0f),
        FVector4(0.0f, 1.0f, 0.0f, 1.0f),
        FVector4(0.0f, 0.0f, 1.0f, 1.0f)
    };

    for (int axis = 0; axis < 3; ++axis)
    {
        uint32 StartVertexIdx = (uint32)vertices.size();

        for (int i = 0; i <= Segments; ++i)
        {
            float longitude = (float)i / Segments * 2.0f * M_PI;
            float sinLong   = sin(longitude);
            float cosLong   = cos(longitude);

            for (int j = 0; j < TubeSegments; ++j)
            {
                float latitude = (float)j / TubeSegments * 2.0f * M_PI;
                float sinLat   = sin(latitude);
                float cosLat   = cos(latitude);

                float x = (Radius + Thickness * cosLat) * cosLong;
                float y = (Radius + Thickness * cosLat) * sinLong;
                float z = Thickness * sinLat;

                FVector pos;
                if (axis == 0)
                    pos = FVector(z, x, y);
                else if (axis == 1)
                    pos = FVector(x, z, y);
                else
                    pos = FVector(x, y, z);

                vertices.push_back({ pos, Colors[axis], axis });
            }
        }

        for (int i = 0; i < Segments; ++i)
        {
            for (int j = 0; j < TubeSegments; ++j)
            {
                uint32 nextI = i + 1;
                uint32 nextJ = (j + 1) % TubeSegments;

                uint32 i0 = StartVertexIdx + (i * TubeSegments + j);
                uint32 i1 = StartVertexIdx + (nextI * TubeSegments + j);
                uint32 i2 = StartVertexIdx + (nextI * TubeSegments + nextJ);
                uint32 i3 = StartVertexIdx + (i * TubeSegments + nextJ);

                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }
    }
}

void FMeshBufferManager::CreateScaleGizmo()
{
    FMeshData&       Data     = MeshDataMap[EPrimitiveMeshShape::ScaleGizmo];
    TArray<FVertex>& vertices = Data.Vertices;
    TArray<uint32>&  indices  = Data.Indices;

    const float LineLength    = 1.0f;
    const float BoxSize       = 0.05f;
    const float StemThickness = 0.03f;

    FVector4 colors[3] = {
        FVector4(1.0f, 0.0f, 0.0f, 1.0f),
        FVector4(0.0f, 1.0f, 0.0f, 1.0f),
        FVector4(0.0f, 0.0f, 1.0f, 1.0f)
    };

    FVector dirs[3] = { FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1) };

    auto AddBox = [&](const FVector& Center, const FVector& Extent, const FVector4& Color, int SubID)
    {
        uint32  StartIdx = (uint32)vertices.size();
        FVector p[8]     = {
            Center + FVector(-Extent.X, -Extent.Y, -Extent.Z), Center + FVector(Extent.X, -Extent.Y, -Extent.Z),
            Center + FVector(Extent.X, Extent.Y, -Extent.Z), Center + FVector(-Extent.X, Extent.Y, -Extent.Z),
            Center + FVector(-Extent.X, -Extent.Y, Extent.Z), Center + FVector(Extent.X, -Extent.Y, Extent.Z),
            Center + FVector(Extent.X, Extent.Y, Extent.Z), Center + FVector(-Extent.X, Extent.Y, Extent.Z)
        };

        for (int j = 0; j < 8; ++j)
            vertices.push_back({ p[j], Color, SubID });

        uint32 BoxIndices[] = {
            0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
            0, 1, 5, 0, 5, 4, 2, 3, 7, 2, 7, 6,
            0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5
        };
        for (uint32 Idx : BoxIndices)
            indices.push_back(StartIdx + Idx);
    };

    for (int i = 0; i < 3; ++i)
    {
        FVector StemExtent = (i == 0) ? FVector(LineLength * 0.5f, StemThickness, StemThickness) : (i == 1) ? FVector(StemThickness, LineLength * 0.5f, StemThickness)
                                                                                                            : FVector(StemThickness, StemThickness, LineLength * 0.5f);

        AddBox(dirs[i] * (LineLength * 0.5f), StemExtent, colors[i], i);

        float boxSizeHalf = BoxSize;
        AddBox(dirs[i] * LineLength, FVector(boxSizeHalf, boxSizeHalf, boxSizeHalf), colors[i], i);
    }
}

void FMeshBufferManager::CreateQuad()
{
    FMeshData&       Data     = MeshDataMap[EPrimitiveMeshShape::Quad];
    TArray<FVertex>& vertices = Data.Vertices;
    TArray<uint32>&  indices  = Data.Indices;

    FVector4 DefaultColor(1.0f, 1.0f, 1.0f, 1.0f);

    vertices.push_back({ FVector(0.0f, -0.5f, 0.5f), DefaultColor, 0 });
    vertices.push_back({ FVector(0.0f, 0.5f, 0.5f), DefaultColor, 0 });
    vertices.push_back({ FVector(0.0f, 0.5f, -0.5f), DefaultColor, 0 });
    vertices.push_back({ FVector(0.0f, -0.5f, -0.5f), DefaultColor, 0 });

    indices.assign({ 0, 1, 2, 0, 2, 3 });
}

void FMeshBufferManager::CreateTranslationGizmo()
{
    FMeshData&       Data     = MeshDataMap[EPrimitiveMeshShape::TransGizmo];
    TArray<FVertex>& vertices = Data.Vertices;
    TArray<uint32>&  indices  = Data.Indices;

    const int32 segments    = 16;
    const float radius      = 0.06f;
    const float headRadius  = 0.12f;
    const float stemLength  = 0.8f;
    const float totalLength = 1.0f;

    FVector4 colors[3] = {
        FVector4(1.0f, 0.0f, 0.0f, 1.0f),
        FVector4(0.0f, 1.0f, 0.0f, 1.0f),
        FVector4(0.0f, 0.0f, 1.0f, 1.0f)
    };

    for (int32 axis = 0; axis < 3; ++axis)
    {
        int32 axisStartVertex = (int32)vertices.size();

        auto GetRotatedPos = [&](float x, float y, float z) -> FVector
        {
            FVector P(x, y, z);
            if (axis == 0)
                return FVector(P.Z, P.X, P.Y);
            if (axis == 1)
                return FVector(P.X, P.Z, P.Y);
            return P;
        };

        for (int32 i = 0; i <= segments; ++i)
        {
            float angle = (2.0f * M_PI * i) / segments;
            float c     = cos(angle);
            float s     = sin(angle);

            vertices.push_back({ GetRotatedPos(c * radius, s * radius, 0.0f), colors[axis], axis });
            vertices.push_back({ GetRotatedPos(c * radius, s * radius, stemLength), colors[axis], axis });
            vertices.push_back({ GetRotatedPos(c * headRadius, s * headRadius, stemLength), colors[axis], axis });
        }

        FVector TipPos = (axis == 0) ? FVector(totalLength, 0, 0) : (axis == 1) ? FVector(0, totalLength, 0)
                                                                                : FVector(0, 0, totalLength);

        vertices.push_back({ TipPos, colors[axis], axis });
        int32 tipIndex = (int32)vertices.size() - 1;

        FVector baseCenterPos = (axis == 0) ? FVector(stemLength, 0, 0) : (axis == 1) ? FVector(0, stemLength, 0)
                                                                                      : FVector(0, 0, stemLength);

        vertices.push_back({ baseCenterPos, colors[axis], axis });
        int32 baseCenterIndex = (int32)vertices.size() - 1;

        for (int32 i = 0; i < segments; ++i)
        {
            int32 curr = axisStartVertex + (i * 3);
            int32 next = axisStartVertex + ((i + 1) * 3);

            indices.push_back(curr);
            indices.push_back(curr + 1);
            indices.push_back(next + 1);
            indices.push_back(curr);
            indices.push_back(next + 1);
            indices.push_back(next);
            indices.push_back(curr + 1);
            indices.push_back(next + 2);
            indices.push_back(curr + 2);
            indices.push_back(curr + 1);
            indices.push_back(next + 1);
            indices.push_back(next + 2);
            indices.push_back(curr + 2);
            indices.push_back(next + 2);
            indices.push_back(tipIndex);
            indices.push_back(baseCenterIndex);
            indices.push_back(next + 2);
            indices.push_back(curr + 2);
        }
    }
}

void FMeshBufferManager::CreateTexturedQuad()
{
    TMeshData<FVertexPNCT>& Data     = PNCTMeshDataMap[EPrimitiveMeshShape::TexturedQuad];
    TArray<FVertexPNCT>&    vertices = Data.Vertices;
    TArray<uint32>&         indices  = Data.Indices;

    FVector  Normal(1.0f, 0.0f, 0.0f);
    FVector4 Color(1.0f, 1.0f, 1.0f, 1.0f);

    // Same positions as Quad: camera-facing billboard plane
    vertices.push_back({ FVector(0.0f, -0.5f, 0.5f), Normal, Color, FVector2(0.0f, 0.0f) });  // top-left
    vertices.push_back({ FVector(0.0f, 0.5f, 0.5f), Normal, Color, FVector2(1.0f, 0.0f) });   // top-right
    vertices.push_back({ FVector(0.0f, 0.5f, -0.5f), Normal, Color, FVector2(1.0f, 1.0f) });  // bottom-right
    vertices.push_back({ FVector(0.0f, -0.5f, -0.5f), Normal, Color, FVector2(0.0f, 1.0f) }); // bottom-left

    indices.assign({ 0, 1, 2, 0, 2, 3 });
}

void FMeshBufferManager::CreatePlane()
{
    FMeshData&       Data     = MeshDataMap[EPrimitiveMeshShape::Plane];
    TArray<FVertex>& vertices = Data.Vertices;
    TArray<uint32>&  indices  = Data.Indices;

    FVector4 color(1.0f, 1.0f, 1.0f, 1.0f);

    vertices = {
        { { -0.5f, -0.5f, 0.01f }, color },
        { { -0.5f, 0.5f, 0.01f }, color },
        { { 0.5f, 0.5f, 0.01f }, color },
        { { 0.5f, -0.5f, 0.01f }, color },
        { { -0.5f, -0.5f, -0.01f }, color },
        { { 0.5f, 0.5f, -0.01f }, color },
        { { -0.5f, 0.5f, -0.01f }, color },
        { { 0.5f, -0.5f, -0.01f }, color }
    };

    indices = {
        0, 2, 1, 0, 3, 2,
        4, 6, 5, 4, 5, 7
    };
}
