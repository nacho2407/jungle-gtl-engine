#include "Paths.h"
#include <filesystem>
#include <Windows.h>

namespace fs = std::filesystem;

namespace
{
	std::wstring Utf8ToWide(const FString& Utf8String)
	{
		if (Utf8String.empty())
		{
			return L"";
		}

		const int32 RequiredChars = ::MultiByteToWideChar(
			CP_UTF8,
			0,
			Utf8String.c_str(),
			-1,
			nullptr,
			0);
		if (RequiredChars <= 1)
		{
			return L"";
		}

		std::wstring WideString;
		WideString.resize(static_cast<size_t>(RequiredChars));
		::MultiByteToWideChar(
			CP_UTF8,
			0,
			Utf8String.c_str(),
			-1,
			WideString.data(),
			RequiredChars);
		WideString.pop_back();
		return WideString;
	}

	std::string WideToUtf8(const std::wstring& WideString)
	{
		if (WideString.empty())
		{
			return "";
		}

		const int32 RequiredBytes = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			WideString.c_str(),
			-1,
			nullptr,
			0,
			nullptr,
			nullptr);
		if (RequiredBytes <= 1)
		{
			return "";
		}

		std::string Utf8String;
		Utf8String.resize(static_cast<size_t>(RequiredBytes));
		::WideCharToMultiByte(
			CP_UTF8,
			0,
			WideString.c_str(),
			-1,
			Utf8String.data(),
			RequiredBytes,
			nullptr,
			nullptr);
		Utf8String.pop_back();
		return Utf8String;
	}

	std::string PathToUtf8(const fs::path& Path)
	{
		return WideToUtf8(Path.wstring());
	}
}

// FString FPaths::Root;
std::filesystem::path FPaths::Root;
bool FPaths::bInitialized = false;

void FPaths::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	// 실행 파일 경로 획득
	wchar_t ExePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, ExePath, MAX_PATH);

	fs::path ExeDir = fs::path(ExePath).parent_path();

	// 실행 파일은 항상 {Root}/{Project}/Bin/{Config}/ 에 위치
	// 3단계 상위 = 프로젝트 루트
	fs::path Candidate = ExeDir.parent_path().parent_path().parent_path();

	// 검증: Engine/ 과 Assets/ 디렉토리가 모두 존재하는지 확인
	if (fs::is_directory(Candidate / "Engine") && fs::is_directory(Candidate / "Assets"))
	{
		SetRoot(Candidate);
		return;
	}

	// 폴백: 상위 디렉토리를 순회하며 Engine/ + Assets/ 조합 탐색
	fs::path CurrentDir = ExeDir;
	for (int32 i = 0; i < 10; ++i)
	{
		if (fs::is_directory(CurrentDir / "Engine") && fs::is_directory(CurrentDir / "Assets"))
		{
			SetRoot(CurrentDir);
			return;
		}
		CurrentDir = CurrentDir.parent_path();
	}

	// 최종 폴백: 실행 파일 디렉토리 사용
	SetRoot(ExeDir);
}

std::wstring FPaths::ToWide(const FString& Path)
{
	return Utf8ToWide(Path);
}

FString FPaths::FromWide(const std::wstring& Path)
{
	return WideToUtf8(Path);
}

std::filesystem::path FPaths::ToPath(const FString& Path)
{
	return fs::path(ToWide(Path));
}

FString FPaths::FromPath(const std::filesystem::path& Path)
{
	return PathToUtf8(Path);
}

std::string FPaths::ToRelativePath(const FString& Path)
{
	const fs::path InputPath = ToPath(Path).lexically_normal();
	if (!InputPath.is_absolute())
	{
		return FromPath(InputPath);
	}

	const fs::path RootPath = ProjectRoot().lexically_normal();
	const fs::path RelativePath = InputPath.lexically_relative(RootPath);
	if (RelativePath.empty())
	{
		return FromPath(InputPath);
	}

	return FromPath(RelativePath);
}

std::string FPaths::ToAbsolutePath(const FString& Path)
{
	const fs::path InputPath = ToPath(Path).lexically_normal();
	if (InputPath.is_absolute())
	{
		return FromPath(InputPath);
	}

	return FromPath((ProjectRoot() / InputPath).lexically_normal());
}

void FPaths::SetRoot(const std::filesystem::path& InPath)
{
	Root = InPath;
	bInitialized = true;
}

const std::filesystem::path& FPaths::ProjectRoot()
{
	return Root;
}

std::filesystem::path FPaths::EngineDir()
{
	return Root / "Engine/";
}

std::filesystem::path FPaths::ShaderDir()
{
	return Root / "Engine/Shaders/";
}

std::filesystem::path FPaths::AssetDir()
{
	return Root / "Assets/";
}

std::filesystem::path FPaths::SceneDir()
{
	return Root / "Assets/Scenes/";
}

std::filesystem::path FPaths::MaterialDir()
{
	return Root / "Assets/Materials/";
}

std::filesystem::path FPaths::MeshDir()
{
	return Root / "Assets/Meshes/";
}

std::filesystem::path FPaths::TextureDir()
{
	return Root / "Assets/Textures/";
}

std::filesystem::path FPaths::ContentDir()
{
	return Root / "Content/";
}

std::filesystem::path FPaths::ShaderCacheDir()
{
	return Root / "Content/Shaders/";
}

std::filesystem::path FPaths::IconDir()
{
	return Root / "Editor/Icon/";
}


/*
FString FPaths::Combine(const FString& Base, const FString& Relative)
{
	if (Base.empty())
	{
		return Relative;
	}
	if (Base.back() == '/' || Base.back() == '\\')
	{
		return Base + Relative;
	}
	return Base + "/" + Relative;
}

std::wstring FPaths::ToWide(const FString& Path)
{
	return std::wstring(Path.begin(), Path.end());
}
*/
