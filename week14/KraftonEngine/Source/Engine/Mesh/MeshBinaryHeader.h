#pragma once
// ==============================================================
// 엔진 내부 Mesh cooked binary cache용 header
// StaticMesh면 MSTC, SkeletalMesh면 MSKL 헤더가 Binary 앞에 붙음
// ==============================================================

#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"

namespace MeshBinary
{
	constexpr uint32 MakeFourCC(char A, char B, char C, char D)
	{
		return (static_cast<uint32>(static_cast<uint8>(A)) << 0)
			| (static_cast<uint32>(static_cast<uint8>(B)) << 8)
			| (static_cast<uint32>(static_cast<uint8>(C)) << 16)
			| (static_cast<uint32>(static_cast<uint8>(D)) << 24);
	}

	constexpr uint32 StaticMeshMagic = MakeFourCC('M', 'S', 'T', 'C');
	constexpr uint32 SkeletalMeshMagic = MakeFourCC('M', 'S', 'K', 'L');

	inline bool WriteHeader(FArchive& Ar, uint32 Magic)
	{
		Ar << Magic;
		return true;
	}

	inline bool ReadHeader(FArchive& Ar, uint32 ExpectedMagic)
	{
		uint32 Magic = 0;
		Ar << Magic;

		return Magic == ExpectedMagic;
	}
}
