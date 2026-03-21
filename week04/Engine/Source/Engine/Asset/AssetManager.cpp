#include "Core/CoreMinimal.h"
#include "AssetManager.h"

#include <cwctype>
#include <filesystem>
#include <fstream>

namespace
{
	namespace fs = std::filesystem;

	uint64 ToTicks(const fs::file_time_type& Time)
	{
		return static_cast<uint64>(Time.time_since_epoch().count());
	}

	uint64 ComputeFnv1a64(const uint8* Data, size_t Size)
	{
		uint64 Hash = 14695981039346656037ull;
		for (size_t i = 0; i < Size; ++i)
		{
			Hash ^= static_cast<uint64>(Data[i]);
			Hash *= 1099511628211ull;
		}
		return Hash;
	}

	FString ToHexString(uint64 Value)
	{
		std::ostringstream Stream;
		Stream << std::hex << std::setw(16) << std::setfill('0') << Value;
		return Stream.str();
	}
}

const FSourceRecord* FSourceCache::GetOrLoad(const FWString& Path)
{
	namespace fs = std::filesystem;
	// 1. 입력 경로 정규화
	FWString NormalizedPath = NormalizePath(Path);
	if (NormalizedPath.empty())
	{
		return nullptr;
	}

	// 2. 파일 존재 확인
	fs::path FilePath(NormalizedPath);
	std::error_code ErrorCode;

	if (!fs::exists(FilePath, ErrorCode) || !fs::is_regular_file(FilePath, ErrorCode))
	{
		return nullptr;
	}

	// 3. 현재 file size / last write time 조회
	const uint64 CurrentFileSize = static_cast<uint64>(fs::file_size(FilePath, ErrorCode));
	if (ErrorCode)
	{
		return nullptr;
	}

	const auto CurrentWriteTime = fs::last_write_time(FilePath, ErrorCode);
	if (ErrorCode)
	{
		return nullptr;
	}

	// 4. 캐시에 기존 record가 있으면 변경 여부 확인
	const uint64 CurrentWriteTimeTicks = ToTicks(CurrentWriteTime);

	auto It = Records.find(NormalizedPath);
	if (It != Records.end() && !HasFileChanged(It->second, CurrentFileSize, CurrentWriteTimeTicks))
	{
		// 5. 안 바뀌었으면 그대로 반환
		return &It->second;
	}

	// 6. 바뀌었거나 없으면 파일 다시 읽기
	FSourceRecord NewRecord;
	if (!BuildSourceRecord(NormalizedPath, NewRecord))
	{
		return nullptr;
	}

	// 7. hash 계산 후 record 갱신
	auto [InsertedIt, _] = Records.insert_or_assign(NormalizedPath, std::move(NewRecord));

	// 8. record 반환
	return &InsertedIt->second;
}

void FSourceCache::Invalidate(const FWString& Path)
{
	const FWString NormalizedPath = NormalizePath(Path);
	if (!NormalizedPath.empty())
	{
		Records.erase(NormalizedPath);
	}
}

void FSourceCache::Clear()
{
	Records.clear();
}

bool FSourceCache::BuildSourceRecord(const FWString& NormalizedPath, FSourceRecord& OutRecord)
{
	namespace fs = std::filesystem;

	const fs::path FilePath(NormalizedPath);
	std::error_code ErrorCode;

	if (!fs::exists(FilePath, ErrorCode) || !fs::is_regular_file(FilePath, ErrorCode))
	{
		return false;
	}

	const uint64 FileSize = fs::file_size(FilePath, ErrorCode);
	if (ErrorCode)
	{
		return false;
	}

	const auto LastWriteTime = fs::last_write_time(FilePath, ErrorCode);
	if (ErrorCode)
	{
		return false;
	}

	std::ifstream File(FilePath, std::ios::binary);
	if (!File.is_open())
	{
		return false;
	}

	OutRecord = {};
	OutRecord.NormalizedPath = NormalizedPath;
	OutRecord.FileSize = FileSize;
	OutRecord.LastWriteTimeTicks = ToTicks(LastWriteTime);
	OutRecord.FileBytes.resize(FileSize);

	if (FileSize > 0)
	{
		File.read(reinterpret_cast<char*>(OutRecord.FileBytes.data()), static_cast<std::streamsize>(FileSize));
		if (!File)
		{
			return false;
		}
	}

	const uint64 HashValue = ComputeFnv1a64(OutRecord.FileBytes.data(), OutRecord.FileBytes.size());
	OutRecord.SourceHash = ToHexString(HashValue);
	OutRecord.bFileBytesLoaded = true;

	return true;
}

bool FSourceCache::HasFileChanged(const FSourceRecord& Record, uint64 CurrentFileSize,
	uint64 CurrentWriteTimeTicks) const
{
	return !Record.bFileBytesLoaded
		|| Record.FileSize != CurrentFileSize
		|| Record.LastWriteTimeTicks != CurrentWriteTimeTicks;
}

FWString FSourceCache::NormalizePath(const FWString& Path) const
{
	if (Path.empty())
	{
		return {};
	}

	fs::path FilePath(Path);
	std::error_code ErrorCode;

	fs::path Normalized = fs::weakly_canonical(FilePath, ErrorCode);
	if (ErrorCode)
	{
		ErrorCode.clear();
		Normalized = FilePath.lexically_normal();
	}
	else
	{
		Normalized = Normalized.lexically_normal();
	}

	FWString Result = Normalized.native();

	std::ranges::replace(Result, L'/', L'\\');
	std::ranges::transform(Result, Result.begin(),
	                       [](wchar_t Ch)
	                       {
		                       return static_cast<wchar_t>(std::towlower(Ch));
	                       });

	return Result;
}
