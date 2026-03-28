#include "PrimitiveObj.h"
#include "Core/Paths.h"
#include <fstream>
#include <sstream>

FPrimitiveObj::FPrimitiveObj()
{
	MeshData = std::make_shared<FMeshData>();
}

FPrimitiveObj::FPrimitiveObj(const FString& FilePath)
{
	MeshData = std::make_shared<FMeshData>();
	LoadObj(FilePath);
}

void FPrimitiveObj::LoadObj(const FString& FilePath)
{
	SetPrimitiveFileName(FilePath);

	auto Cached = GetCached(FilePath);
	if (Cached)
	{
		MeshData = Cached;
		return;
	}

	std::ifstream File(FPaths::ToAbsolutePath(FilePath));
	if (!File.is_open())
	{
		printf("[OBJ] Failed to open: %s\n", FilePath.c_str());
		return;
	}

	std::vector<FVector> Positions;
	std::vector<FVector2> UVs;

	std::string Line;

	while (std::getline(File, Line))
	{
		std::stringstream SS(Line);
		std::string Type;
		SS >> Type;

		// =========================
		// Vertex Position
		// =========================
		if (Type == "v")
		{
			FVector Pos;
			SS >> Pos.X >> Pos.Y >> Pos.Z;
			Positions.push_back(Pos);
		}
		// =========================
		// UV
		// =========================
		else if (Type == "vt")
		{
			FVector2 UV;
			SS >> UV.X >> UV.Y;

			// OBJ는 V가 뒤집혀 있는 경우 많음 (선택)
			UV.Y = 1.0f - UV.Y;

			UVs.push_back(UV);
		}
		// =========================
		// Face
		// =========================
		else if (Type == "f")
		{
			std::string VStr;

			struct FIndex
			{
				uint32 PosIdx;
				uint32 UVIdx;
			};

			std::vector<FIndex> Face;

			while (SS >> VStr)
			{
				std::stringstream VSS(VStr);

				std::string PosStr, UVStr;

				std::getline(VSS, PosStr, '/');
				std::getline(VSS, UVStr, '/');

				FIndex Idx{};
				Idx.PosIdx = std::stoi(PosStr) - 1;

				if (!UVStr.empty())
					Idx.UVIdx = std::stoi(UVStr) - 1;
				else
					Idx.UVIdx = 0;

				Face.push_back(Idx);
			}

			// =========================
			// Fan triangulation
			// =========================
			for (size_t i = 1; i + 1 < Face.size(); ++i)
			{
				FIndex I0 = Face[0];
				FIndex I1 = Face[i];
				FIndex I2 = Face[i + 1];

				uint32 BaseIndex = (uint32)MeshData->Vertices.size();

				auto AddVertex = [&](const FIndex& Idx)
					{
						FPrimitiveVertex V{};

						V.Position = Positions[Idx.PosIdx];
						V.Color = FVector4(V.Position.X, V.Position.Y, V.Position.Z, 1);

						if (!UVs.empty())
							V.UV = UVs[Idx.UVIdx];
						else
							V.UV = FVector2(0, 0);

						MeshData->Vertices.push_back(V);
						MeshData->UVs.push_back(V.UV);
					};

				AddVertex(I0);
				AddVertex(I1);
				AddVertex(I2);

				MeshData->Indices.push_back(BaseIndex + 0);
				MeshData->Indices.push_back(BaseIndex + 1);
				MeshData->Indices.push_back(BaseIndex + 2);
			}
		}
	}

	MeshData->Topology = EMeshTopology::EMT_TriangleList;

	RegisterMeshData(FilePath, MeshData);
}