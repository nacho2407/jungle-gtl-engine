#include "Engine/Platform/Paths.h"

#include <filesystem>

std::wstring FPaths::RootDir()
{
	static std::wstring Cached;
	if (Cached.empty())
	{
		// exe 옆에 Shaders/ 가 있으면 배포 환경, 없으면 개발 환경 (CWD 사용)
		WCHAR Buffer[MAX_PATH];
		GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
		std::filesystem::path ExeDir = std::filesystem::path(Buffer).parent_path();

		if (std::filesystem::exists(ExeDir / L"Shaders"))
		{
			// 배포: exe와 리소스가 같은 디렉터리
			Cached = ExeDir.wstring() + L"\\";
		}
		else
		{
			// 개발: CWD(= $(ProjectDir))에 리소스가 있음
			Cached = std::filesystem::current_path().wstring() + L"\\";
		}
	}
	return Cached;
}

std::wstring FPaths::ShaderDir() { return RootDir() + L"Shaders\\"; }
std::wstring FPaths::SceneDir() { return RootDir() + L"Asset\\Scene\\"; }
std::wstring FPaths::DumpDir() { return RootDir() + L"Saves\\Dump\\"; }
std::wstring FPaths::SettingsDir() { return RootDir() + L"Settings\\"; }

std::wstring FPaths::ShaderFilePath() { return RootDir() + L"Shaders\\ShaderW0.hlsl"; }
std::wstring FPaths::SettingsFilePath() { return RootDir() + L"Settings\\Editor.ini"; }
std::wstring FPaths::ResourceFilePath() { return RootDir() + L"Settings\\Resource.ini"; }

std::wstring FPaths::Combine(const std::wstring& Base, const std::wstring& Child)
{
	std::filesystem::path Result(Base);
	Result /= Child;
	return Result.wstring();
}

void FPaths::CreateDir(const std::wstring& Path)
{
	std::filesystem::create_directories(Path);
}

std::wstring FPaths::ToWide(const std::string& Utf8Str)
{
	if (Utf8Str.empty()) return {};
	int32_t Size = MultiByteToWideChar(CP_UTF8, 0, Utf8Str.c_str(), -1, nullptr, 0);
	std::wstring Result(Size - 1, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, Utf8Str.c_str(), -1, &Result[0], Size);
	return Result;
}

std::string FPaths::ToUtf8(const std::wstring& WideStr)
{
	if (WideStr.empty()) return {};
	int32_t Size = WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string Result(Size - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), -1, &Result[0], Size, nullptr, nullptr);
	return Result;
}

std::string FPaths::ResolveAssetPath(const std::string& BaseFilePath, const std::string& TargetPath)
{
	// 1. 기준 파일(OBJ 또는 MTL)의 폴더 경로 추출
	std::filesystem::path FileDir(ToWide(BaseFilePath));
	FileDir = FileDir.parent_path();

	// 2. 타겟 파일 경로(텍스처나 MTL 이름)
	std::filesystem::path Target(ToWide(TargetPath));

	// 3. 두 경로 합치기 및 정규화
	std::filesystem::path FullPath = (FileDir / Target).lexically_normal();
	std::filesystem::path ProjectRoot(RootDir());

	std::filesystem::path RelativePath;

	// 4. 절대/상대 경로 분기 처리
	if (FullPath.is_absolute())
	{
		RelativePath = FullPath.lexically_relative(ProjectRoot);
		if (RelativePath.empty())
		{
			// 드라이브가 다르거나 계산이 불가능한 경우 최후의 수단으로 파일명만 추출
			RelativePath = FullPath.filename();
		}
	}
	else
	{
		RelativePath = FullPath;
	}

	// 5. 엔진에서 사용하는 UTF-8 포맷으로 반환 (Windows 백슬래시를 슬래시로 통일)
	return ToUtf8(RelativePath.generic_wstring());
}