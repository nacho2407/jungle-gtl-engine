#pragma once
#include <fstream>
#include <filesystem>

struct FContentItem
{
	std::filesystem::path Path;
	std::wstring Name;
	bool bIsDirectory = false;
};