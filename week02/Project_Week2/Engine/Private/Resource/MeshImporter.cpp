#include "Resource/MeshImporter.h"

#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>

#include "Resource/UStaticMesh.h"
#include "Json/json.hpp"

using json = nlohmann::json;

namespace
{
    struct FVector3Raw
    {
        float X, Y, Z;
    };

    static std::vector<uint8_t> ReadBinaryFile(const std::string& FilePath)
    {
        std::ifstream File(FilePath, std::ios::binary);
        if (!File)
        {
            return {};
        }

        File.seekg(0, std::ios::end);
        const std::streamsize Size = File.tellg();
        File.seekg(0, std::ios::beg);

        std::vector<uint8_t> Data(static_cast<size_t>(Size));
        if (Size > 0)
        {
            File.read(reinterpret_cast<char*>(Data.data()), Size);
        }

        return Data;
    }

    static json ReadJsonFile(const std::string& FilePath)
    {
        std::ifstream File(FilePath);
        if (!File)
        {
            return {};
        }

        json J;
        File >> J;
        return J;
    }
}

namespace MeshImporter
{
    UStaticMesh* LoadStaticMeshFromGltf(const std::string& GltfPath)
    {
        const json Gltf = ReadJsonFile(GltfPath);
        if (Gltf.is_null())
        {
            return nullptr;
        }

        if (!Gltf.contains("buffers") || !Gltf.contains("bufferViews") ||
            !Gltf.contains("accessors") || !Gltf.contains("meshes"))
        {
            return nullptr;
        }

        const std::filesystem::path GltfFilePath(GltfPath);
        const std::filesystem::path BaseDir = GltfFilePath.parent_path();

        const std::string BinUri = Gltf["buffers"][0]["uri"].get<std::string>();
        const std::filesystem::path BinPath = BaseDir / BinUri;

        const std::vector<uint8_t> BinData = ReadBinaryFile(BinPath.string());
        if (BinData.empty())
        {
            return nullptr;
        }

        const json& Mesh = Gltf["meshes"][0];
        const json& Primitive = Mesh["primitives"][0];

        const int PositionAccessorIndex = Primitive["attributes"]["POSITION"].get<int>();
        const int IndicesAccessorIndex = Primitive["indices"].get<int>();

        const json& PositionAccessor = Gltf["accessors"][PositionAccessorIndex];
        const json& PositionBufferView = Gltf["bufferViews"][PositionAccessor["bufferView"].get<int>()];

        const json& IndexAccessor = Gltf["accessors"][IndicesAccessorIndex];
        const json& IndexBufferView = Gltf["bufferViews"][IndexAccessor["bufferView"].get<int>()];

        const size_t PositionCount = PositionAccessor["count"].get<size_t>();
        const size_t PositionByteOffset =
            PositionBufferView.value("byteOffset", 0) +
            PositionAccessor.value("byteOffset", 0);

        const size_t IndexCount = IndexAccessor["count"].get<size_t>();
        const size_t IndexByteOffset =
            IndexBufferView.value("byteOffset", 0) +
            IndexAccessor.value("byteOffset", 0);

        std::vector<FVertexSimple> Vertices;
        Vertices.reserve(PositionCount);

        const FVector3Raw* Positions =
            reinterpret_cast<const FVector3Raw*>(BinData.data() + PositionByteOffset);

        for (size_t i = 0; i < PositionCount; ++i)
        {
            const FVector3Raw& P = Positions[i];

            // 임시 흰색
            Vertices.emplace_back(P.X, P.Y, P.Z, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        std::vector<uint32_t> Indices;
        Indices.reserve(IndexCount);

        const int ComponentType = IndexAccessor["componentType"].get<int>();

        if (ComponentType == 5123) // UNSIGNED_SHORT
        {
            const uint16_t* Src =
                reinterpret_cast<const uint16_t*>(BinData.data() + IndexByteOffset);

            for (size_t i = 0; i < IndexCount; ++i)
            {
                Indices.push_back(static_cast<uint32_t>(Src[i]));
            }
        }
        else if (ComponentType == 5125) // UNSIGNED_INT
        {
            const uint32_t* Src =
                reinterpret_cast<const uint32_t*>(BinData.data() + IndexByteOffset);

            for (size_t i = 0; i < IndexCount; ++i)
            {
                Indices.push_back(Src[i]);
            }
        }
        else
        {
            return nullptr;
        }

        UStaticMesh* StaticMesh = new UStaticMesh();
        StaticMesh->SetVertices(Vertices);
        StaticMesh->SetIndices(Indices);
        StaticMesh->SetTopology(EMeshTopology::TriangleList);

        return StaticMesh;
    }
}