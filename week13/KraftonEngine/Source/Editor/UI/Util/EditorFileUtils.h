#pragma once

#include "Core/CoreTypes.h"

struct FEditorFileDialogOptions
{
	const wchar_t* Filter = L"All Files (*.*)\0*.*\0";
	const wchar_t* Title = L"Select File";
	const wchar_t* DefaultExtension = nullptr;
	const wchar_t* InitialDirectory = nullptr;
	const wchar_t* DefaultFileName = nullptr;
	void* OwnerWindowHandle = nullptr;
	bool bFileMustExist = true;
	bool bPathMustExist = true;
	bool bPromptOverwrite = false;
	bool bReturnRelativeToProjectRoot = false;
};

namespace FEditorFileUtils
{
	FString OpenFileDialog(const FEditorFileDialogOptions& InOptions);
	FString SaveFileDialog(const FEditorFileDialogOptions& InOptions);
}
