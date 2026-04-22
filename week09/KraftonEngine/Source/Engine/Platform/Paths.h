#pragma once

#include <string>
#include <filesystem>
#include <Windows.h>

// 엔진 전역 경로를 관리합니다.
// 모든 경로는 실행 파일 기준 상대 경로이며, 한글 경로를 위해 wstring 기반입니다.
class FPaths
{
public:
	// 프로젝트 루트 (실행 파일이 있는 디렉터리)
	static std::wstring RootDir();

	// 주요 디렉터리
	static std::wstring AssetDir();       // Asset/
	static std::wstring ContentDir();     // Asset/Content/
	static std::wstring EditorDir();      // Asset/Editor/
	static std::wstring ShaderDir();      // Shaders/
	static std::wstring SceneDir();       // Asset/Content/Scene/
	static std::wstring DumpDir();        // Saves/Dump/
	static std::wstring SettingsDir();    // Settings/

	// 주요 파일 경로
	static std::wstring SettingsFilePath();  // Settings/Editor.ini
	static std::wstring ResourceFilePath();  // Settings/Resource.ini

	// 경로 결합: FPaths::Combine(L"Asset/Scene", L"Default.Scene")
	static std::wstring Combine(const std::wstring& Base, const std::wstring& Child);

	// 디렉터리가 없으면 재귀적으로 생성
	static void CreateDir(const std::wstring& Path);

	// 변환 유틸리티 (한글 경로 지원)
	static std::wstring ToWide(const std::string& Utf8Str);
	static std::string ToUtf8(const std::wstring& WideStr);

	// 문자열 경로를 std::filesystem::path로 변환
	static std::filesystem::path ToPath(const std::string& Utf8Path);
	static std::filesystem::path ToPath(const std::wstring& WidePath);

	// std::filesystem::path 및 wide string을 UTF-8 문자열 경로로 변환
	static std::string FromPath(const std::filesystem::path& Path);
	static std::string FromWide(const std::wstring& WideStr);

	// BaseFilePath 기준으로 TargetPath를 실제 에셋 상대 경로로 해석합니다.
	static std::string ResolveAssetPath(const std::string& BaseFilePath, const std::string& TargetPath);

	// 프로젝트 루트 기준 상대 경로 생성 유틸
	static std::string MakeRelativeToRoot(const std::filesystem::path& Path);
	static bool PathContainsDirectory(const std::filesystem::path& Path, const std::wstring& DirectoryName);

	// Asset 루트 기반 경로 생성 유틸
	static std::string AssetRelativePath(const std::string& RelativePath);
	static std::string ContentRelativePath(const std::string& RelativePath);
	static std::string EditorRelativePath(const std::string& RelativePath);
};
