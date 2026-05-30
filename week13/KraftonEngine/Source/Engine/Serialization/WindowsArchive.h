#pragma once

#include "Archive.h"
#include "Platform/Paths.h"
#include <fstream>
#include <string>
#include <iostream>

class FWindowsBinWriter : public FArchive
{
private:
	mutable std::ofstream FileStream;

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
	bool CanSeek() const override { return true; }
	int64 Tell() const override { return FileStream.is_open() ? static_cast<int64>(FileStream.tellp()) : -1; }
	void Seek(int64 Offset) override
	{
		if (FileStream.is_open())
		{
			FileStream.seekp(static_cast<std::streamoff>(Offset), std::ios::beg);
		}
	}

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
	mutable std::ifstream FileStream;

public:
	FWindowsBinReader(const std::string& FilePath)
	{
		bIsLoading = true; // 나는 '읽기' 전용이다!
		FileStream.open(FPaths::ToWide(FilePath), std::ios::binary);
	}

	~FWindowsBinReader() override
	{
		if (FileStream.is_open()) FileStream.close();
	}

	bool IsValid() const { return FileStream.is_open() && FileStream.good(); }
	bool CanSeek() const override { return true; }
	int64 Tell() const override
	{
		if (!FileStream.is_open())
		{
			return -1;
		}

		const std::streampos Current = FileStream.tellg();
		return Current == std::streampos(-1) ? -1 : static_cast<int64>(Current);
	}
	void Seek(int64 Offset) override
	{
		if (FileStream.is_open())
		{
			FileStream.clear();
			FileStream.seekg(static_cast<std::streamoff>(Offset), std::ios::beg);
		}
	}
	bool IsAtEnd() const override
	{
		if (!FileStream.is_open())
		{
			return true;
		}

		const std::streampos Current = FileStream.tellg();
		if (Current == std::streampos(-1))
		{
			return true;
		}

		FileStream.seekg(0, std::ios::end);
		const std::streampos End = FileStream.tellg();
		FileStream.seekg(Current);
		return Current >= End;
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
