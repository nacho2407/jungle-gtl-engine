#pragma once

#include <string>
#include <Windows.h>

// 엔진 전역 경로를 관리합니다.
// 모든 경로는 실행 파일 기준 상대 경로이며, 한글 경로를 위해 wstring 기반입니다.
class FPaths
{
public:
	// 프로젝트 루트 (실행 파일이 있는 디렉터리)
	static std::wstring RootDir();

	// 주요 디렉터리
	static std::wstring ShaderDir();      // Shaders/
	static std::wstring SceneDir();       // Asset/Scene/
	static std::wstring DumpDir();        // Saves/Dump/
	static std::wstring SettingsDir();    // Settings/
	static std::wstring MaterialTextureDir(); //Model/Texture/

	// 주요 파일 경로
	static std::wstring ShaderFilePath(); // Shaders/ShaderW0.hlsl
	static std::wstring SettingsFilePath();  // Settings/Editor.ini
	static std::wstring ViewerSettingsFilePath(); // Settings/ObjViewer.ini
	static std::wstring ResourceFilePath();  // Settings/Resource.ini
	static std::wstring ResourceDefaultMaterialTexture(); // Asset/Mesh/Default.png
	static std::wstring ToRelative(const std::wstring& AbsolutePath);
	static std::wstring ToAbsolute(const std::wstring& RelativePath);
	static std::string ToRelativeString(const std::wstring& AbsolutePath);
	static std::string ToAbsoluteString(const std::wstring& RelativePath);

	// 경로 결합: FPaths::Combine(L"Asset/Scene", L"Default.Scene")
	static std::wstring Combine(const std::wstring& Base, const std::wstring& Child);

	// 디렉터리가 없으면 재귀적으로 생성
	static void CreateDir(const std::wstring& Path);

	// 변환 유틸리티 (한글 경로 지원)
	static std::wstring ToWide(const std::string& Utf8Str);
	static std::string ToUtf8(const std::wstring& WideStr);
};
