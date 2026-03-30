#include "Engine/Core/Paths.h"

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
		ExeDir /= "Test";

 		if (std::filesystem::exists(ExeDir / L"Shaders"))
		{
			// 배포: exe와 리소스가 같은 디렉터리
			Cached = ExeDir.generic_wstring() + L"/";
		}
		else
		{
			// 개발: CWD(= $(ProjectDir))에 리소스가 있음
			Cached = std::filesystem::current_path().generic_wstring() + L"/";
		}
	}
	return Cached;
}

std::wstring FPaths::ShaderDir() { return RootDir() + L"Shaders/"; }
std::wstring FPaths::SceneDir() { return RootDir() + L"Asset/Scene/"; }
std::wstring FPaths::DumpDir() { return RootDir() + L"Saves/Dump/"; }
std::wstring FPaths::SettingsDir() { return RootDir() + L"Settings/"; }
std::wstring FPaths::ShaderFilePath() { return RootDir() + L"Shaders/ShaderW0.hlsl"; }
std::wstring FPaths::SettingsFilePath() { return RootDir() + L"Settings/Editor.ini"; }
std::wstring FPaths::ViewerSettingsFilePath() { return RootDir() + L"Settings/ObjViewer.ini"; }
std::wstring FPaths::ResourceFilePath() { return RootDir() + L"Settings/Resource.ini"; }
std::wstring FPaths::ResourceDefaultMaterialTexture() { return RootDir() + L"Asset/Mesh/Default.png"; }


std::wstring FPaths::Combine(const std::wstring& Base, const std::wstring& Child)
{
	std::filesystem::path Result(Base);
	Result /= Child;
	return Result.generic_wstring(); // backslash를 slash로 변환해서 반환한다.
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

// 절대경로를 입력받아서 RootDir 기준의 상대 경로로 변환한다.
std::wstring FPaths::ToRelative(const std::wstring& AbsolutePath)
{
	if (AbsolutePath.empty()) return L"";

	std::filesystem::path AbsPath(AbsolutePath);
	std::filesystem::path Root(RootDir());
	
	// RootDir 기준으로 상대 경로를 계산 (예: Asset/Scene/map.json)
	std::filesystem::path RelPath = std::filesystem::relative(AbsPath, Root);
	
	return RelPath.generic_wstring();
}

// 상대 경로를 입력받아서 RootDir 기준의 절대 경로로 변환한다.
std::wstring FPaths::ToAbsolute(const std::wstring& RelativePath)
{
	if (RelativePath.empty()) return L"";

	std::filesystem::path TargetPath(RelativePath);

	// 이미 C:/ 등 절대 경로라면 변환하지 않고 그대로 반환
	if (TargetPath.is_absolute())
	{
		return TargetPath.generic_wstring();
	}

	std::filesystem::path Root(RootDir());
	
	// 상대 경로라면 RootDir에 붙인 뒤, 불필요한 슬래시 등을 정리(lexically_normal)
	return (Root / TargetPath).lexically_normal().generic_wstring();
}

// 절대경로를 입력받아서 RootDir 기준의 상대 경로 string으로 변환한다.
std::string FPaths::ToRelativeString(const std::wstring &AbsolutePath) 
{
	return ToUtf8(ToRelative(AbsolutePath));
}

// 상대 경로를 입력받아서 RootDir 기준의 절대 경로 string으로 변환한다.
std::string FPaths::ToAbsoluteString(const std::wstring &RelativePath) 
{
	return ToUtf8(ToAbsolute(RelativePath));
}