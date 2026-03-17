#pragma once

#include "Serializable.h"
#include "EngineTypes.h"
#include "Logger.h"
#include <iostream>
#include <fstream>
#include <sstream>

class SerializeHelper
{
public:
	template <typename T>
	static void SaveAsJson(T* obj, const FString filePath)
	{
		FString jsonStr = obj->ToJson();
		std::ofstream outFile(filePath);

		if (outFile.is_open())
		{
			outFile << jsonStr;
			outFile.close();
			UE_LOG("Saved file: %s\n", filePath.c_str());
		}
		else
		{
			// TODO: 에러 출력 로그 레벨 변경
			UE_LOG("Cannot open file : %s\n", filePath.c_str());
		}
	}

	template <typename T>
	static T* LoadFromJson(const FString filePath)
	{
		std::ifstream inFile;
		inFile.open(filePath);
		if (inFile.is_open())
		{
			FString jsonStr((std::istreambuf_iterator<char>(inFile)),
				std::istreambuf_iterator<char>());
			inFile >> jsonStr;
			inFile.close();

			T* loaded = T::FromJson(jsonStr);

			UE_LOG("Loaded file: %s\n", filePath.c_str());
			return loaded;
		}
		else
		{
			// TODO: 에러 출력 로그 레벨 변경
			UE_LOG("Cannot open file : %s\n", filePath.c_str());
			return nullptr;
		}
	}
};