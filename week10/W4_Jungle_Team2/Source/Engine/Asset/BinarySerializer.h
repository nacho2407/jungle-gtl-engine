#pragma once

#include "Core/CoreMinimal.h"

struct FStaticMesh;

struct FStaticMeshBinaryHeader
{
	uint32 MagicNumber = 0x4853454D;
	uint32 Version = 1;
	uint32 VertexCount = 0;
	uint32 IndexCount = 0;
	uint32 SectionCount = 0;
	uint32 SlotNameCount = 0;
	
	uint64 SourceFileWriteTime = 0;
};

class FBinarySerializer
{
public:
	bool SaveStaticMesh(const FString & BinaryPath, const FString & SourcePath, const FStaticMesh & Data);
	bool LoadStaticMesh(const FString & BinaryPath, FStaticMesh & OutData);
	
	//	Header Read + 검사 장치
	bool ReadStaticMeshHeader(const FString & BinaryPath, FStaticMeshBinaryHeader & OutHeader) const;
private:
	template <typename T>
	void WriteValue(std::ofstream & Out, const T & Value);
	
	template <typename T>
	void ReadValue(std::ifstream & In, T & OutValue) const;
	
	void WriteString(std::ofstream & Out, const FString & String);
	void ReadString(std::ifstream & In, FString & OutString) const;
	
	template <typename T>
	void WriteArray(std::ofstream & Out, const TArray<T> & Array);
	
	template <typename T>
	void ReadArray(std::ifstream & In, TArray<T> & OutArray) const;
};
