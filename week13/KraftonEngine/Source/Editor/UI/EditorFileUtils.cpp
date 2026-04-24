#include "Editor/UI/EditorFileUtils.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shobjidl.h>

#include <filesystem>
#include <vector>

#include "Engine/Platform/Paths.h"

namespace
{
	std::vector<COMDLG_FILTERSPEC> BuildFilterSpecs(const wchar_t* InFilter)
	{
		std::vector<COMDLG_FILTERSPEC> Specs;
		if (!InFilter)
		{
			return Specs;
		}

		const wchar_t* Cursor = InFilter;
		while (*Cursor)
		{
			const wchar_t* DisplayName = Cursor;
			Cursor += wcslen(Cursor) + 1;
			if (!*Cursor)
			{
				break;
			}

			const wchar_t* Pattern = Cursor;
			Cursor += wcslen(Cursor) + 1;
			Specs.push_back({ DisplayName, Pattern });
		}

		return Specs;
	}

	FString RunFileDialog(const FEditorFileDialogOptions& InOptions, bool bOpenDialog)
	{
		HRESULT InitHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		const bool bShouldUninitialize = SUCCEEDED(InitHr);
		if (FAILED(InitHr) && InitHr != RPC_E_CHANGED_MODE)
		{
			return FString();
		}

		IFileDialog* Dialog = nullptr;
		const HRESULT CreateHr = bOpenDialog
			? CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Dialog))
			: CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Dialog));
		if (FAILED(CreateHr) || !Dialog)
		{
			if (bShouldUninitialize)
			{
				CoUninitialize();
			}
			return FString();
		}

		if (InOptions.Title)
		{
			Dialog->SetTitle(InOptions.Title);
		}
		if (InOptions.DefaultExtension)
		{
			Dialog->SetDefaultExtension(InOptions.DefaultExtension);
		}
		if (InOptions.DefaultFileName)
		{
			Dialog->SetFileName(InOptions.DefaultFileName);
		}

		const std::vector<COMDLG_FILTERSPEC> FilterSpecs = BuildFilterSpecs(InOptions.Filter);
		if (!FilterSpecs.empty())
		{
			Dialog->SetFileTypes(static_cast<UINT>(FilterSpecs.size()), FilterSpecs.data());
			Dialog->SetFileTypeIndex(1);
		}

		DWORD Flags = 0;
		Dialog->GetOptions(&Flags);
		Flags |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
		if (InOptions.bPathMustExist)
		{
			Flags |= FOS_PATHMUSTEXIST;
		}
		if (InOptions.bFileMustExist)
		{
			Flags |= FOS_FILEMUSTEXIST;
		}
		if (InOptions.bPromptOverwrite)
		{
			Flags |= FOS_OVERWRITEPROMPT;
		}
		Dialog->SetOptions(Flags);

		IShellItem* InitialFolder = nullptr;
		if (InOptions.InitialDirectory && *InOptions.InitialDirectory)
		{
			const HRESULT FolderHr = SHCreateItemFromParsingName(
				InOptions.InitialDirectory,
				nullptr,
				IID_PPV_ARGS(&InitialFolder));
			if (SUCCEEDED(FolderHr) && InitialFolder)
			{
				Dialog->SetFolder(InitialFolder);
				Dialog->SetDefaultFolder(InitialFolder);
			}
		}

		const HRESULT ShowHr = Dialog->Show(static_cast<HWND>(InOptions.OwnerWindowHandle));
		FString Result;
		if (SUCCEEDED(ShowHr))
		{
			IShellItem* ResultItem = nullptr;
			if (SUCCEEDED(Dialog->GetResult(&ResultItem)) && ResultItem)
			{
				PWSTR FilePath = nullptr;
				if (SUCCEEDED(ResultItem->GetDisplayName(SIGDN_FILESYSPATH, &FilePath)) && FilePath)
				{
					const std::filesystem::path AbsolutePath = std::filesystem::path(FilePath).lexically_normal();
					if (InOptions.bReturnRelativeToProjectRoot)
					{
						const std::filesystem::path ProjectRoot(FPaths::RootDir());
						Result = FPaths::ToUtf8(AbsolutePath.lexically_relative(ProjectRoot).generic_wstring());
					}
					else
					{
						Result = FPaths::ToUtf8(AbsolutePath.generic_wstring());
					}
					CoTaskMemFree(FilePath);
				}
				ResultItem->Release();
			}
		}

		if (InitialFolder)
		{
			InitialFolder->Release();
		}
		Dialog->Release();
		if (bShouldUninitialize)
		{
			CoUninitialize();
		}

		return Result;
	}
}

namespace FEditorFileUtils
{
	FString OpenFileDialog(const FEditorFileDialogOptions& InOptions)
	{
		return RunFileDialog(InOptions, true);
	}

	FString SaveFileDialog(const FEditorFileDialogOptions& InOptions)
	{
		return RunFileDialog(InOptions, false);
	}
}
