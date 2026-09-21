#pragma once

#include <string>

class UStaticMesh;

namespace MeshImporter
{
    UStaticMesh* LoadStaticMeshFromGltf(const std::string& GltfPath);
}