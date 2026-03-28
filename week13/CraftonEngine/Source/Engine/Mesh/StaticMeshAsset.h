#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Resource/Buffer.h"
#include "Serialization/Archive.h"

// Cooked Data 내부용 정점
struct FNormalVertex
{
	FVector pos;
	FVector normal;
	FVector4 color;
	FVector2 tex;
};

// Cooked Data — GPU용 정점/인덱스
struct FStaticMesh
{
	std::string PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;

	FMeshBuffer* RenderBuffer = nullptr;

	void Serialize(FArchive& Ar)
	{
		// 1. std::string 직렬화 (문자열 길이 -> 문자열 내용)
		uint32 StringLen = static_cast<uint32>(PathFileName.size());
		Ar << StringLen;

		if (Ar.IsLoading()) PathFileName.resize(StringLen);
		if (StringLen > 0) Ar.Serialize(PathFileName.data(), StringLen);

		// 2. TArray 직렬화 (Archive.h에 만들어둔 오버로딩 덕분에 단 한 줄로 끝납니다!)
		Ar << Vertices;
		Ar << Indices;
	}
};
