#include "AnimGraphAsset.h"

#include "Serialization/Archive.h"

void UAnimGraphAsset::Serialize(FArchive& Ar)
{
	(void)Ar;
	// 데이터 모델 정의 전까지 빈 본문 — 패키지 헤더 + 메타데이터만 라운드트립.
	// UObject::Serialize 호출 안 함: 다른 자산(UFloatCurveAsset 등)과 동일하게
	// ObjectName 직렬화 생략 (자산 패키지 컨텍스트에서 무의미).
}
