// 직렬화 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Archive.h"
#include "Platform/Paths.h"
#include <fstream>
#include <string>
#include <iostream>

// FWindowsBinWriter는 에디터 UI 표시와 입력 처리를 담당합니다.
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
        if (FileStream.is_open())
            FileStream.close();
    }

    // 파일이 정상적으로 열렸는지 확인
    bool IsValid() const { return FileStream.is_open(); }

    void Serialize(void* Data, size_t Num) override
    {
        if (FileStream.is_open() && Num > 0)
        {
            // 하드 디스크에 데이터를 씁니다.
            FileStream.write(static_cast<const char*>(Data), Num);
        }
    }
};

// FWindowsBinReader는 에디터 UI 표시와 입력 처리를 담당합니다.
class FWindowsBinReader : public FArchive
{
private:
    std::ifstream FileStream;

public:
    FWindowsBinReader(const std::string& FilePath)
    {
        bIsLoading = true; // 나는 '읽기' 전용이다!
        FileStream.open(FPaths::ToWide(FilePath), std::ios::binary);
    }

    ~FWindowsBinReader() override
    {
        if (FileStream.is_open())
            FileStream.close();
    }

    bool IsValid() const { return FileStream.is_open(); }

    void Serialize(void* Data, size_t Num) override
    {
        if (FileStream.is_open() && Num > 0)
        {
            // 하드 디스크에서 데이터를 읽어옵니다.
            FileStream.read(static_cast<char*>(Data), Num);
        }
    }
};
