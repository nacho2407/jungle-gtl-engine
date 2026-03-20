#include "Core/CoreMinimal.h"
#include "AssetManager.h"

const FSourceRecord* FSourceCache::GetOrLoad(const WFString& Path)
{
	// 1. 입력 경로 정규화

	// 2. 파일 존재 확인

	// 3. 현재 file size / last write time 조회

	// 4. 캐시에 기존 record가 있으면 변경 여부 확인

	// 5. 안 바뀌었으면 그대로 반환

	// 6. 바뀌었거나 없으면 파일 다시 읽기

	// 7. hash 계산 후 record 갱신

	// 8. record 반환

	return nullptr;
}

void FSourceCache::Invalidate(const WFString& Path)
{
}

void FSourceCache::Clear()
{
}

bool FSourceCache::BuildSourceRecord(const WFString& NormalizedPath, FSourceRecord& OutRecord)
{
	// 1. 파일 바이트 읽기

	// 2. file size 채우기
	
	// 3. last write time 채우기

	// 4. hash 계산

	OutRecord.bFileBytesLoaded = true;
}

bool FSourceCache::HasFileChanged(const FSourceRecord& Record, uint64 CurrentFileSize,
	uint64 CurrentWriteTimeTicks) const
{
}

WFString FSourceCache::NormalizePath(const WFString& Path) const
{
	// 1. 절대 경로로 변환

	// 2. slash 통일

	// 3. 대소문자 정책 통일

	// 4. 가능하면 . .. 정리
}
