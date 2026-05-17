#pragma once

#include "Core/CoreTypes.h"
#include "Animation/SkeletonTypes.h"

class USkeleton;
class USkeletalMesh;
class UAnimSequence;

// Asset picker dropdown 에 표시되는 항목. {표시명, 자산 경로} 묶음.
struct FAssetListItem
{
	FString DisplayName;
	FString FullPath;
};

// Asset 종류 (UObject 자식 클래스 이름) 별 사용 가능한 자산 목록을 한 곳에서 조회.
// 내부적으로는 각 manager(FMeshManager 등) 가 들고 있는 정적 캐시로 라우팅한다.
namespace FAssetRegistry
{
	// 주어진 type-name 에 해당하는 자산 목록을 반환. 모르는 type-name 이면 빈 배열.
	const TArray<FAssetListItem>& ListByTypeName(const char* AssetTypeName);

    // Skeleton 관계 기반 조회. UI는 가능한 한 이 경로를 통해 Mesh/Anim 목록을 받아야 한다.
    TArray<FAssetListItem> ListSkeletons();
    TArray<FAssetListItem> ListAnimationsForSkeleton(const FSkeletonBinding& Skeleton, bool bAllowSameStructure = true);
    TArray<FAssetListItem> ListMeshesForSkeleton(const FSkeletonBinding& Skeleton, bool bAllowSameStructure = true);

    USkeleton* FindSkeletonByGuid(const FString& SkeletonAssetGuid);

    bool CheckAnimationForMesh(const UAnimSequence* Sequence, const USkeletalMesh* Mesh, FSkeletonCompatibilityReport* OutReport = nullptr);
    bool ValidateAssetBinding(const FSkeletonBinding& A, const FSkeletonBinding& B, bool bAllowSameStructure = true, FSkeletonCompatibilityReport* OutReport = nullptr);
}
