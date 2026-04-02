#include "Engine/Core/Paths.h"
#include <filesystem>

std::wstring FPaths::RootDir()
{
	static std::wstring Cached;
	if (Cached.empty())
	{
		WCHAR Buffer[MAX_PATH];
		GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
		std::filesystem::path ExeDir = std::filesystem::path(Buffer).parent_path();

		// 1. 배포 환경: exe 파일과 같은 폴더에 바로 Shaders/ 가 있는 경우
		if (std::filesystem::exists(ExeDir / L"Shaders"))
		{
			Cached = ExeDir.generic_wstring() + L"/";
		}
		// 2. 솔루션(개발) 환경: exe가 Bin/ObjViewer/ 또는 Bin/Debug/ 등에 있는 경우
		else if (std::filesystem::exists(ExeDir / L".." / L".." / L"Shaders"))
		{
			// lexically_normal()을 사용해 "../../" 등 지저분한 경로를 깔끔하게 정리합니다.
			Cached = (ExeDir / L".." / L"..").lexically_normal().generic_wstring() + L"/";
		}
		// 3. VS에서 직접 실행하여 작업 디렉터리(CWD)가 프로젝트 루트로 정상 지정된 경우 (Fallback)
		else
		{
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
std::wstring FPaths::AssetDirectoryPath() { return RootDir() + L"Asset"; }
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
	int32_t Size = WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), (int)WideStr.length(), nullptr, 0, nullptr, nullptr);
	std::string Result(Size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), (int)WideStr.length(), Result.data(), Size, nullptr, nullptr);
	return Result;
}

FString FPaths::ToString(const std::wstring& wstring)
{
	return ToUtf8(wstring);
}

//	절대경로를 입력받아서 RootDir 기준의 상대 경로로 변환한다.
std::wstring FPaths::ToRelative(const std::wstring& AbsolutePath)
{
	if (AbsolutePath.empty()) return L"";

	std::filesystem::path AbsPath(AbsolutePath);
	std::filesystem::path Root(RootDir());
	
	//	RootDir 기준으로 상대 경로를 계산 (예: Asset/Scene/map.json)
	std::filesystem::path RelPath = std::filesystem::relative(AbsPath, Root);
	
	return RelPath.generic_wstring();
}

//	상대 경로를 입력받아서 RootDir 기준의 절대 경로로 변환한다.
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
