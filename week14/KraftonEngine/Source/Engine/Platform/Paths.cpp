#include "Engine/Platform/Paths.h"

#include <algorithm>
#include <filesystem>
#include <vector>

namespace
{
	std::filesystem::path GetExecutablePath()
	{
		std::wstring Buffer(MAX_PATH, L'\0');
		for (;;)
		{
			const DWORD Length = GetModuleFileNameW(nullptr, Buffer.data(), static_cast<DWORD>(Buffer.size()));
			if (Length == 0)
			{
				return {};
			}

			if (Length < Buffer.size())
			{
				Buffer.resize(Length);
				return std::filesystem::path(Buffer);
			}

			Buffer.resize(Buffer.size() * 2);
		}
	}

	bool LooksLikeProjectRoot(const std::filesystem::path& Path)
	{
		std::error_code Error;
		const bool HasSettings = std::filesystem::exists(Path / L"Settings" / L"ProjectSettings.ini", Error);
		Error.clear();
		const bool HasShaders = std::filesystem::exists(Path / L"Shaders", Error);
		Error.clear();
		const bool HasContent = std::filesystem::exists(Path / L"Content", Error);
		return HasSettings || (HasShaders && HasContent);
	}

	void AppendRootCandidates(std::vector<std::filesystem::path>& Candidates, std::filesystem::path Start)
	{
		std::error_code Error;
		Start = std::filesystem::absolute(Start, Error);
		if (Error)
		{
			Start.clear();
		}

		for (std::filesystem::path Current = Start; !Current.empty();)
		{
			Current = Current.lexically_normal();
			if (std::find(Candidates.begin(), Candidates.end(), Current) == Candidates.end())
			{
				Candidates.push_back(Current);
			}

			const std::filesystem::path Parent = Current.parent_path();
			if (Parent.empty() || Parent == Current)
			{
				break;
			}
			Current = Parent;
		}
	}

	std::filesystem::path ResolveProjectRoot(const std::filesystem::path& ExeDir)
	{
		std::vector<std::filesystem::path> Candidates;
		if (!ExeDir.empty())
		{
			AppendRootCandidates(Candidates, ExeDir);
		}

		std::error_code Error;
		const std::filesystem::path CurrentPath = std::filesystem::current_path(Error);
		if (!Error && !CurrentPath.empty())
		{
			AppendRootCandidates(Candidates, CurrentPath);
		}

		for (const std::filesystem::path& Candidate : Candidates)
		{
			if (!Candidate.empty() && LooksLikeProjectRoot(Candidate))
			{
				return Candidate.lexically_normal();
			}
		}

		return CurrentPath.empty() ? ExeDir.lexically_normal() : CurrentPath.lexically_normal();
	}

	std::wstring ConvertToWide(const std::string& Text, UINT CodePage, DWORD Flags)
	{
		const int32_t Size = MultiByteToWideChar(CodePage, Flags, Text.c_str(), -1, nullptr, 0);
		if (Size <= 0)
		{
			return {};
		}

		std::wstring Result(Size - 1, L'\0');
		MultiByteToWideChar(CodePage, Flags, Text.c_str(), -1, Result.data(), Size);
		return Result;
	}
}

std::wstring FPaths::RootDir()
{
	static std::wstring Cached;
	if (Cached.empty())
	{
		const std::filesystem::path ExeDir = GetExecutablePath().parent_path();
		const std::filesystem::path RootPath = ResolveProjectRoot(ExeDir);
		Cached = (RootPath / L"").generic_wstring();
	}
	return Cached;
}

std::wstring FPaths::ShaderDir()		{ return RootDir() + L"Shaders/"; }
std::wstring FPaths::AssetDir()			{ return RootDir() + L"Content/"; }
std::wstring FPaths::AudioDir()			{ return RootDir() + L"Content/Audio/"; }
std::wstring FPaths::SceneDir()			{ return RootDir() + L"Content/Scene/"; }
std::wstring FPaths::ScriptDir()		{ return RootDir() + L"Content/Script/"; }
std::wstring FPaths::DataDir()			{ return RootDir() + L"Content/Data/"; }
std::wstring FPaths::SaveDir()			{ return RootDir() + L"Saves/"; }
std::wstring FPaths::DumpDir()			{ return RootDir() + L"Saves/Dump/"; }
std::wstring FPaths::LogDir()			{ return RootDir() + L"Saves/Logs/"; }
std::wstring FPaths::ShaderCacheDir() 	{ return RootDir() + L"Saves/ShaderCache/"; }
std::wstring FPaths::SettingsDir()		{ return RootDir() + L"Settings/"; }

std::wstring FPaths::SettingsFilePath() { return RootDir() + L"Settings/Editor.ini"; }
std::wstring FPaths::ResourceFilePath() { return RootDir() + L"Settings/Resource.ini"; }
std::wstring FPaths::ProjectSettingsFilePath() { return RootDir() + L"Settings/ProjectSettings.ini"; }

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
	std::wstring Result = ConvertToWide(Utf8Str, CP_UTF8, MB_ERR_INVALID_CHARS);
	if (!Result.empty()) return Result;
	return ConvertToWide(Utf8Str, CP_ACP, 0);
}

std::string FPaths::ToUtf8(const std::wstring& WideStr)
{
	if (WideStr.empty()) return {};
	int32_t Size = WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (Size <= 0) return {};
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

std::string FPaths::MakeProjectRelative(const std::string& Path)
{
	if (Path.empty()) return Path;

	std::filesystem::path P(ToWide(Path));
	if (!P.is_absolute())
	{
		// 이미 상대 경로 — 백슬래시만 슬래시로 정리
		return ToUtf8(P.lexically_normal().generic_wstring());
	}

	std::filesystem::path ProjectRoot(RootDir());
	std::filesystem::path Rel = P.lexically_relative(ProjectRoot);

	// 드라이브가 다르거나 변환 불가한 경우 → 입력 유지
	if (Rel.empty() || Rel.native().rfind(L"..", 0) == 0)
	{
		return ToUtf8(P.generic_wstring());
	}

	return ToUtf8(Rel.generic_wstring());
}
