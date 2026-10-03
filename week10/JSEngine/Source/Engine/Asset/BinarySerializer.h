#pragma once

#include "Core/CoreMinimal.h"

#include <fstream>

struct FStaticMesh;
struct FSkeletalMesh;
struct FMatrix;

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
	uint32 SlotCount = 0;

	uint64 SourceFileWriteTime = 0;
};

struct FSkeletalMeshBinaryHeader
{
	uint32 MagicNumber = 0x534D4B53;	// 'SKMS' (Skeletal MeSh)
	uint32 Version = 2;					// v2: Sockets 블록 추가
	uint32 VertexCount = 0;
	uint32 IndexCount = 0;
	uint32 SectionCount = 0;
	uint32 SlotCount = 0;
	uint32 BoneCount = 0;
	uint32 SocketCount = 0;				// v2 신규

	uint64 SourceFileWriteTime = 0;
};

class FBinarySerializer
{
public:
	bool SaveStaticMesh(const FString& BinaryPath, const FString& SourcePath, const FStaticMesh& Data);
	bool LoadStaticMesh(const FString& BinaryPath, FStaticMesh& OutData);

	bool SaveSkeletalMesh(const FString& BinaryPath, const FString& SourcePath, const FSkeletalMesh& Data);
	bool LoadSkeletalMesh(const FString& BinaryPath, FSkeletalMesh& OutData);

	//	Header Read + 검사 장치
	bool ReadStaticMeshHeader(const FString& BinaryPath, FStaticMeshBinaryHeader& OutHeader) const;
	bool ReadSkeletalMeshHeader(const FString& BinaryPath, FSkeletalMeshBinaryHeader& OutHeader) const;

private:
	
	void WriteInt32LE(std::ofstream& Out, int32 Value);
	void WriteUInt32LE(std::ofstream& Out, uint32 Value);
	void WriteUInt64LE(std::ofstream& Out, uint64 Value);
	void WriteFloatLE(std::ofstream& Out, float Value);
	
	bool ReadInt32LE(std::ifstream& In, int32& OutValue) const;
	bool ReadUInt32LE(std::ifstream& In, uint32& OutValue) const;
	bool ReadUInt64LE(std::ifstream& In, uint64& OutValue) const;
	bool ReadFloatLE(std::ifstream& In, float& OutValue) const;
	
	void WriteHeader(std::ofstream& Out, const FStaticMeshBinaryHeader& Header);
	bool ReadHeader(std::ifstream& In, FStaticMeshBinaryHeader& OutHeader) const;

	void WriteString(std::ofstream& Out, const FString& String);
	bool ReadString(std::ifstream& In, FString& OutString) const;

	void WriteIndexArray(std::ofstream& Out, const TArray<uint32>& Array);
	bool ReadIndexArray(std::ifstream& In, TArray<uint32>& OutArray) const;
	
	void WriteVertices(std::ofstream& Out, const FStaticMesh& Data);
	bool ReadVertices(std::ifstream& In, FStaticMesh& OutData, uint32 VertexCount) const;

	void WriteSections(std::ofstream& Out, const FStaticMesh& Data);
	bool ReadSections(std::ifstream& In, FStaticMesh& OutData, uint32 SectionCount) const;

	void WriteBounds(std::ofstream& Out, const FStaticMesh& Data);
	bool ReadBounds(std::ifstream& In, FStaticMesh& OutData) const;

	/* Skeletal Mesh */
	void WriteSkeletalHeader(std::ofstream& Out, const FSkeletalMeshBinaryHeader& Header);
	bool ReadSkeletalHeader(std::ifstream& In, FSkeletalMeshBinaryHeader& OutHeader) const;

	void WriteMatrix4x4(std::ofstream& Out, const FMatrix& M);
	bool ReadMatrix4x4(std::ifstream& In, FMatrix& OutM) const;

	void WriteSkeletalVertices(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadSkeletalVertices(std::ifstream& In, FSkeletalMesh& OutData, uint32 VertexCount) const;

	void WriteSkeletalSections(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadSkeletalSections(std::ifstream& In, FSkeletalMesh& OutData, uint32 SectionCount) const;

	void WriteBones(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadBones(std::ifstream& In, FSkeletalMesh& OutData, uint32 BoneCount) const;

	void WriteSockets(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadSockets(std::ifstream& In, FSkeletalMesh& OutData, uint32 SocketCount) const;

	void WriteSkeletalBounds(std::ofstream& Out, const FSkeletalMesh& Data);
	bool ReadSkeletalBounds(std::ifstream& In, FSkeletalMesh& OutData) const;
};