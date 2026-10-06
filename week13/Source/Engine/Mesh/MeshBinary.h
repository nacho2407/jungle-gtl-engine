#pragma once

namespace MeshBinary
{
	// StaticMesh와 SkeletalMesh는 저장 데이터가 다르다.
	// 확장자만 보고 타입을 알 수 있게 나누고, 파일 안에는 타입 구분값을 넣지 않는다.
	constexpr const wchar_t* AssetPackageExtension = L".uasset";
	constexpr const wchar_t* StaticMeshBinaryExtension = AssetPackageExtension;
	constexpr const wchar_t* SkeletalMeshBinaryExtension = AssetPackageExtension;
}
