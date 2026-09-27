#pragma once

#include "Archive.h"
#include "Core/CoreTypes.h"
#include <cstring>

// 메모리 버퍼 백엔드 FArchive.
// - Save 모드: 내부 버퍼에 append.
// - Load 모드: 외부 버퍼 참조, 순차 read.
// Duplicate(직렬화 왕복)용. 파일 IO 없음.
class FMemoryArchive : public FArchive
{
public:
	// Save용 생성자
	explicit FMemoryArchive(bool bInIsSaving)
	{
		bIsSaving = bInIsSaving;
		bIsLoading = !bInIsSaving;
	}

	// Load용 생성자 (외부 버퍼 참조)
	FMemoryArchive(const TArray<uint8>& InBuffer, bool bInIsSaving)
	{
		bIsSaving = bInIsSaving;
		bIsLoading = !bInIsSaving;
		Buffer = InBuffer;
	}

	const TArray<uint8>& GetBuffer() const { return Buffer; }

	void Serialize(void* Data, size_t Num) override
	{
		if (Num == 0) return;

		if (bIsSaving)
		{
			const size_t OldSize = Buffer.size();
			Buffer.resize(OldSize + Num);
			std::memcpy(Buffer.data() + OldSize, Data, Num);
		}
		else
		{
			if (Offset + Num > Buffer.size())
			{
				// 읽기 오버런 — 안전하게 0으로 채움
				std::memset(Data, 0, Num);
				return;
			}
			std::memcpy(Data, Buffer.data() + Offset, Num);
			Offset += Num;
		}
	}

private:
	TArray<uint8> Buffer;
	size_t Offset = 0;
};
