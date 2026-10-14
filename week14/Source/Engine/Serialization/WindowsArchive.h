#pragma once

#include "Archive.h"
#include "Platform/Paths.h"
#include <fstream>
#include <string>
#include <iostream>

class FWindowsBinWriter : public FArchive
{
private:
	std::ofstream FileStream;

public:
	FWindowsBinWriter(const std::string& FilePath)
	{
		bIsSaving = true; // 나는 '쓰기' 전용이다!
		FileStream.open(FPaths::ToWide(FilePath), std::ios::binary);
	}

	~FWindowsBinWriter() override
	{
		if (FileStream.is_open()) FileStream.close();
	}

	// 파일이 정상적으로 열렸는지 확인
	bool IsValid() const { return FileStream.is_open() && FileStream.good(); }

	void Serialize(void* Data, size_t Num) override
	{
		if (FileStream.is_open() && Num > 0)
		{
			// 하드 디스크에 데이터를 씁니다.
			FileStream.write(static_cast<const char*>(Data), Num);
		}
	}
};

class FWindowsBinReader : public FArchive
{
private:
	std::ifstream FileStream;
	std::streamoff FileSize = 0; // 트레일링 섹션 EOF 판정용 (peek 대신 위치 비교)

public:
	FWindowsBinReader(const std::string& FilePath)
	{
		bIsLoading = true; // 나는 '읽기' 전용이다!
		FileStream.open(FPaths::ToWide(FilePath), std::ios::binary);

		// 전체 파일 크기를 미리 구해둔다. 끝까지 읽었는지 tellg와 비교만 하면 되니
		// peek()로 eofbit를 건드려 IsValid()를 깨뜨리는 일이 없다.
		if (FileStream.is_open())
		{
			FileStream.seekg(0, std::ios::end);
			FileSize = static_cast<std::streamoff>(FileStream.tellg());
			FileStream.seekg(0, std::ios::beg);
		}
	}

	~FWindowsBinReader() override
	{
		if (FileStream.is_open()) FileStream.close();
	}

	bool IsValid() const { return FileStream.is_open() && FileStream.good(); }

	// 현재 읽기 위치가 파일 끝에 닿았는지. eofbit를 세우지 않으므로 이후 IsValid()에 영향 없음.
	bool AtEnd() override
	{
		if (!FileStream.is_open() || !FileStream.good())
		{
			return true;
		}
		return static_cast<std::streamoff>(FileStream.tellg()) >= FileSize;
	}

	void Serialize(void* Data, size_t Num) override
	{
		if (FileStream.is_open() && Num > 0)
		{
			// 하드 디스크에서 데이터를 읽어옵니다.
			FileStream.read(static_cast<char*>(Data), Num);
		}
	}
};
