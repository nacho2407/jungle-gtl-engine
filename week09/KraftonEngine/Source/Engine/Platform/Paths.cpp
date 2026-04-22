#include "Engine/Platform/Paths.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <system_error>

std::wstring FPaths::RootDir()
{
	static std::wstring Cached;
	if (Cached.empty())
	{
		WCHAR Buffer[MAX_PATH];
		GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
		const std::filesystem::path ExeDir = std::filesystem::path(Buffer).parent_path();

		if (std::filesystem::exists(ExeDir / L"Shaders"))
		{
			Cached = (ExeDir.lexically_normal().wstring() + L"\\");
		}
		else
		{
			Cached = (std::filesystem::current_path().lexically_normal().wstring() + L"\\");
		}
	}
	return Cached;
}

std::wstring FPaths::AssetDir() { return RootDir() + L"Asset\\"; }
std::wstring FPaths::ContentDir() { return AssetDir() + L"Content\\"; }
std::wstring FPaths::EditorDir() { return AssetDir() + L"Editor\\"; }
std::wstring FPaths::ShaderDir() { return RootDir() + L"Shaders\\"; }
std::wstring FPaths::SceneDir() { return ContentDir() + L"Scene\\"; }
std::wstring FPaths::DumpDir() { return RootDir() + L"Saves\\Dump\\"; }
std::wstring FPaths::SettingsDir() { return RootDir() + L"Settings\\"; }

std::wstring FPaths::SettingsFilePath() { return SettingsDir() + L"Editor.ini"; }
std::wstring FPaths::ResourceFilePath() { return SettingsDir() + L"Resource.ini"; }

std::wstring FPaths::Combine(const std::wstring& Base, const std::wstring& Child)
{
	std::filesystem::path Result(Base);
	Result /= Child;
	return Result.lexically_normal().wstring();
}

void FPaths::CreateDir(const std::wstring& Path)
{
	std::filesystem::create_directories(std::filesystem::path(Path));
}

std::wstring FPaths::ToWide(const std::string& Utf8Str)
{
	if (Utf8Str.empty())
	{
		return {};
	}

	const int Size = MultiByteToWideChar(CP_UTF8, 0, Utf8Str.c_str(), -1, nullptr, 0);
	if (Size <= 1)
	{
		return {};
	}

	std::wstring Result(static_cast<size_t>(Size - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, Utf8Str.c_str(), -1, Result.data(), Size);
	return Result;
}

std::string FPaths::ToUtf8(const std::wstring& WideStr)
{
	if (WideStr.empty())
	{
		return {};
	}

	const int Size = WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (Size <= 1)
	{
		return {};
	}

	std::string Result(static_cast<size_t>(Size - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), -1, Result.data(), Size, nullptr, nullptr);
	return Result;
}

std::filesystem::path FPaths::ToPath(const std::string& Utf8Path)
{
	return std::filesystem::path(ToWide(Utf8Path));
}

std::filesystem::path FPaths::ToPath(const std::wstring& WidePath)
{
	return std::filesystem::path(WidePath);
}

std::string FPaths::FromPath(const std::filesystem::path& Path)
{
	return ToUtf8(Path.generic_wstring());
}

std::string FPaths::FromWide(const std::wstring& WideStr)
{
	return ToUtf8(WideStr);
}

std::string FPaths::ResolveAssetPath(const std::string& BaseFilePath, const std::string& TargetPath)
{
	const std::filesystem::path FileDir = ToPath(BaseFilePath).parent_path();
	const std::filesystem::path Target = ToPath(TargetPath);
	const std::filesystem::path FullPath = (FileDir / Target).lexically_normal();
	const std::filesystem::path ProjectRoot = std::filesystem::path(RootDir()).lexically_normal();

	std::filesystem::path RelativePath;
	if (FullPath.is_absolute())
	{
		RelativePath = FullPath.lexically_relative(ProjectRoot);
		if (RelativePath.empty())
		{
			RelativePath = FullPath.filename();
		}
	}
	else
	{
		RelativePath = FullPath;
	}

	return ToUtf8(RelativePath.generic_wstring());
}

std::string FPaths::MakeRelativeToRoot(const std::filesystem::path& Path)
{
	const std::filesystem::path RootPath = std::filesystem::path(RootDir()).lexically_normal();
	const std::filesystem::path Normalized = Path.lexically_normal();

	std::error_code Ec;
	std::filesystem::path Relative = std::filesystem::relative(Normalized, RootPath, Ec);
	if (Ec || Relative.empty())
	{
		Relative = Normalized.lexically_relative(RootPath);
	}
	if (Relative.empty())
	{
		Relative = Normalized;
	}

	return ToUtf8(Relative.generic_wstring());
}

bool FPaths::PathContainsDirectory(const std::filesystem::path& Path, const std::wstring& DirectoryName)
{
	std::wstring Needle = DirectoryName;
	std::transform(Needle.begin(), Needle.end(), Needle.begin(), towlower);

	for (const auto& Part : Path)
	{
		std::wstring Component = Part.wstring();
		std::transform(Component.begin(), Component.end(), Component.begin(), towlower);
		if (Component == Needle)
		{
			return true;
		}
	}
	return false;
}

std::string FPaths::AssetRelativePath(const std::string& RelativePath)
{
	return MakeRelativeToRoot(std::filesystem::path(AssetDir()) / ToWide(RelativePath));
}

std::string FPaths::ContentRelativePath(const std::string& RelativePath)
{
	return MakeRelativeToRoot(std::filesystem::path(ContentDir()) / ToWide(RelativePath));
}

std::string FPaths::EditorRelativePath(const std::string& RelativePath)
{
	return MakeRelativeToRoot(std::filesystem::path(EditorDir()) / ToWide(RelativePath));
}
