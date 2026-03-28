#include "Core/CoreMinimal.h"
#include "Asset/StreamableRenderAsset.h"

UStreamableRenderAsset::UStreamableRenderAsset() {}

UStreamableRenderAsset::~UStreamableRenderAsset() {}

void UStreamableRenderAsset::Serialize(class FArchive& Ar)
{
    // 공통 스트리밍 로직 직렬화 (나중에 구현)
}

// 기반 클래스이므로 REGISTER_CLASS 하지 않음
