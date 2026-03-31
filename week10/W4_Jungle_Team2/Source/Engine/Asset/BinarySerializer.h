#pragma once

#include "Core/CoreMinimal.h"

#include <fstream>

struct FStaticMesh;

/*
 *	[주의사항]
 *	- Header나 Body 정보가 변경되면 반드시 Version을 바꿔야 합니다.
 */
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
	bool SaveStaticMesh(const FString& BinaryPath, const FString& SourcePath, const FStaticMesh& Data);
	bool LoadStaticMesh(const FString& BinaryPath, FStaticMesh& OutData);
	
	//	Header Read + 검사 장치
	bool ReadStaticMeshHeader(const FString& BinaryPath, FStaticMeshBinaryHeader& OutHeader) const;

private:
	/*
	 *	[Endian 대응]
	 *	- 파일 포맷은 Little-Endian으로 고정한다.
	 *	- 메모리 레이아웃을 그대로 write/read 하지 않고,
	 *	  기본 타입 단위로 바이트 순서를 명시적으로 기록한다.
	 */
	
	void WriteInt32LE(std::ofstream& Out, int32 Value);
	void WriteUInt32LE(std::ofstream& Out, uint32 Value);
	void WriteUInt64LE(std::ofstream& Out, uint64 Value);
	void WriteFloatLE(std::ofstream& Out, float Value);
	
	bool ReadInt32LE(std::ifstream& In, int32& OutValue) const;
	bool ReadUInt32LE(std::ifstream& In, uint32& OutValue) const;
	bool ReadUInt64LE(std::ifstream& In, uint64& OutValue) const;
	bool ReadFloatLE(std::ifstream& In, float& OutValue) const;

	/*
	 *	[Padding 대응]
	 *	- Header / Vertex / Section / Bounds 를 struct 통째로 쓰지 않는다.
	 *	- 반드시 멤버 단위로 serialize 한다.
	 */
	void WriteHeader(std::ofstream& Out, const FStaticMeshBinaryHeader& Header);
	bool ReadHeader(std::ifstream& In, FStaticMeshBinaryHeader& OutHeader) const;

	void WriteString(std::ofstream& Out, const FString& String);
	bool ReadString(std::ifstream& In, FString& OutString) const;

	void WriteIndexArray(std::ofstream& Out, const TArray<uint32>& Array);
	bool ReadIndexArray(std::ifstream& In, TArray<uint32>& OutArray) const;

	/*
	 *	[중요]
	 *	- 아래 3개는 실제 FStaticMeshTypes.h의 멤버 구성에 맞춰 채워야 한다.
	 *	- 즉, Position / Normal / UV / Bounds Min/Max 같은 실제 필드명을 맞춰서 작성.
	 */
	void WriteVertices(std::ofstream& Out, const FStaticMesh& Data);
	bool ReadVertices(std::ifstream& In, FStaticMesh& OutData, uint32 VertexCount) const;

	void WriteSections(std::ofstream& Out, const FStaticMesh& Data);
	bool ReadSections(std::ifstream& In, FStaticMesh& OutData, uint32 SectionCount) const;

	void WriteBounds(std::ofstream& Out, const FStaticMesh& Data);
	bool ReadBounds(std::ifstream& In, FStaticMesh& OutData) const;
};