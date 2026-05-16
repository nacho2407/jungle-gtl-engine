#pragma once

#include "Core/CoreTypes.h"

// Asset picker dropdown 에 표시되는 항목. {표시명, 자산 경로} 묶음.
struct FAssetListItem
{
	FString DisplayName;
	FString FullPath;
};

// Asset 종류 (UObject 자식 클래스 이름) 별 사용 가능한 자산 목록을 한 곳에서 조회.
// 내부적으로는 각 manager(FMeshManager 등) 가 들고 있는 정적 캐시로 라우팅한다.
// UClass* 기반이 아닌 type-name 기반 — D 의 Reflection 의존을 피하기 위한 의도.
//
// 새 자산 타입 추가 = AssetRegistry.cpp 의 라우팅 한 줄만 늘리면 됨.
namespace FAssetRegistry
{
	// 주어진 type-name 에 해당하는 자산 목록을 반환. 모르는 type-name 이면 빈 배열.
	// 반환 참조는 manager 의 정적 캐시를 그대로 가리키므로 caller 가 소유하지 않는다.
	const TArray<FAssetListItem>& ListByTypeName(const char* AssetTypeName);
}
