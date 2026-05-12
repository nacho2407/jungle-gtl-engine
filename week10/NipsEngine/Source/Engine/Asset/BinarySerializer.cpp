#include "BinarySerializer.h"

#include "Asset/StaticMeshTypes.h"
#include "Asset/SkeletalMeshTypes.h"
#include "Core/Paths.h"
#include "Math/Matrix.h"

#include <filesystem>
#include <chrono>
#include <cstring>

/*
 *	Raw Binary Serialization
 * [장점]
 *	- struct 그대로 write
 *	- 빠름
 *	- 구현 간단
 *	
 *	[단점]
 *	- ABI(Application Binary Interface) 의존성 - 컴파일러에 따라 다르게 해석 가능
 *	- padding 문제
 *	- 플랫폼 종속
 *	
 *	[언리얼과 비교]
 *	- 언리얼은 그냥 write가 아닌 Serialization Abstraction이 존재
 *	- 이를 통해 엔디안, padding 등에 대응 가능
 *	- 또한 Vertices, Indices 등을 Chunk 단위로 묶음 (일부만 로딩 혹은 streaming 가능)
 *	- 이는 Offset 기반으로도 실현 가능 (Jump)
 *
 *	[현재 수정 방향]
 *	- 파일 포맷은 Little-Endian으로 고정
 *	- Header / Body를 struct 통째로 write 하지 않고 멤버 단위 serialize
 *	- 즉, padding / endianness 문제를 줄이는 방향으로 수정
 */

/* Validation Check Constants */
constexpr uint32 STATIC_MESH_BINARY_MAGIC = 0x4853454D; // 'MESH'
constexpr uint32 STATIC_MESH_BINARY_VERSION = 1;

constexpr uint32 SKELETAL_MESH_BINARY_MAGIC   = 0x534D4B53; // 'SKMS'
constexpr uint32 SKELETAL_MESH_BINARY_VERSION = 1;

//	Vailidation Checkers
constexpr uint32 MAX_STATIC_MESH_VERTEX_COUNT   = 10'000'000;
constexpr uint32 MAX_STATIC_MESH_INDEX_COUNT    = 30'000'000;
constexpr uint32 MAX_STATIC_MESH_SECTION_COUNT  = 100'000;
constexpr uint32 MAX_STATIC_MESH_SLOTNAME_COUNT = 1024;
constexpr uint32 MAX_STRING_LENGTH              = 4096;

constexpr uint32 MAX_SKELETAL_MESH_VERTEX_COUNT   = 10'000'000;
constexpr uint32 MAX_SKELETAL_MESH_INDEX_COUNT    = 30'000'000;
constexpr uint32 MAX_SKELETAL_MESH_SECTION_COUNT  = 100'000;
constexpr uint32 MAX_SKELETAL_MESH_SLOTNAME_COUNT = 1024;
constexpr uint32 MAX_SKELETAL_MESH_BONE_COUNT     = 65'536;

static bool IsValidStaticMeshHeader(const FStaticMeshBinaryHeader& Header)
{
	if (Header.MagicNumber != STATIC_MESH_BINARY_MAGIC)
	{
		return false;
	}

	if (Header.Version != STATIC_MESH_BINARY_VERSION)
	{
		return false;
	}

	if (Header.VertexCount > MAX_STATIC_MESH_VERTEX_COUNT)
	{
		return false;
	}

	if (Header.IndexCount > MAX_STATIC_MESH_INDEX_COUNT)
	{
		return false;
	}

	if (Header.SectionCount > MAX_STATIC_MESH_SECTION_COUNT)
	{
		return false;
	}

	if (Header.SlotCount > MAX_STATIC_MESH_SLOTNAME_COUNT)
	{
		return false;
	}

	return true;
}

static bool IsValidSkeletalMeshHeader(const FSkeletalMeshBinaryHeader& Header)
{
	if (Header.MagicNumber != SKELETAL_MESH_BINARY_MAGIC)
	{
		return false;
	}

	if (Header.Version != SKELETAL_MESH_BINARY_VERSION)
	{
		return false;
	}

	if (Header.VertexCount > MAX_SKELETAL_MESH_VERTEX_COUNT)
	{
		return false;
	}

	if (Header.IndexCount > MAX_SKELETAL_MESH_INDEX_COUNT)
	{
		return false;
	}

	if (Header.SectionCount > MAX_SKELETAL_MESH_SECTION_COUNT)
	{
		return false;
	}

	if (Header.SlotCount > MAX_SKELETAL_MESH_SLOTNAME_COUNT)
	{
		return false;
	}

	if (Header.BoneCount > MAX_SKELETAL_MESH_BONE_COUNT)
	{
		return false;
	}

	return true;
}

/* Time Checker */
static uint64 GetFileWriteTimeTicks(const FString& Path)
{
	namespace fs = std::filesystem;

	fs::path FilePath(FPaths::ToWide(Path));
	if (!fs::exists(FilePath))
	{
		return 0;
	}

	auto WriteTime = fs::last_write_time(FilePath);
	auto Duration = WriteTime.time_since_epoch();
	return static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(Duration).count());
}

/* Primitive LE Writers */
void FBinarySerializer::WriteInt32LE(std::ofstream& Out, int32 Value)
{
	WriteUInt32LE(Out, static_cast<uint32>(Value));
}

void FBinarySerializer::WriteUInt32LE(std::ofstream& Out, uint32 Value)
{
	//	하위 바이트부터 저장하는 Little Endian [LSB -> MSB]
	unsigned char Bytes[4];
	Bytes[0] = static_cast<unsigned char>((Value >> 0) & 0xFF);
	Bytes[1] = static_cast<unsigned char>((Value >> 8) & 0xFF);
	Bytes[2] = static_cast<unsigned char>((Value >> 16) & 0xFF);
	Bytes[3] = static_cast<unsigned char>((Value >> 24) & 0xFF);

	//	reinterpret_cast : 주소를 그저 byte로 해석하라 (타입에 대하여 고려하지 않고, 비트 그대로 해석)
	//	이 메모리를 그냥 바이트 덩어리로 넘기고 싶을 때 사용 (unsigned char -> char *로 API 요구 타입만 변경)
	Out.write(reinterpret_cast<const char*>(Bytes), 4);
}

void FBinarySerializer::WriteUInt64LE(std::ofstream& Out, uint64 Value)
{
	unsigned char Bytes[8];
	Bytes[0] = static_cast<unsigned char>((Value >> 0) & 0xFF);
	Bytes[1] = static_cast<unsigned char>((Value >> 8) & 0xFF);
	Bytes[2] = static_cast<unsigned char>((Value >> 16) & 0xFF);
	Bytes[3] = static_cast<unsigned char>((Value >> 24) & 0xFF);
	Bytes[4] = static_cast<unsigned char>((Value >> 32) & 0xFF);
	Bytes[5] = static_cast<unsigned char>((Value >> 40) & 0xFF);
	Bytes[6] = static_cast<unsigned char>((Value >> 48) & 0xFF);
	Bytes[7] = static_cast<unsigned char>((Value >> 56) & 0xFF);
	
	Out.write(reinterpret_cast<const char*>(Bytes), 8);
}

void FBinarySerializer::WriteFloatLE(std::ofstream& Out, float Value)
{
	static_assert(sizeof(float) == sizeof(uint32), "float size must be 4 bytes");

	//	float -> uint32 비트 그대로 복사 (해석만 다르게)
	//	float 3.14f → 0x4048F5C3 (IEEE 754)
	//	reinterpret_cast를 사용하면 안됨 (strict aliasing violation, UB 가능, 최적화에서 깨질 수 있음)
		//	컴파일러는 서로 다른 타입의 포인터는 같은 메모리를 가리키지 않을 것이라고 간주해버림
		//	Release 컴파일러 최적화에서 깨질 수 있음 (컴파일러의 가정을 깨기 때문임)
	//	타입은 다르지만 비트 패턴을 그대로 복사하고 싶을 때 memcpy 사용
	uint32 Bits = 0;
	std::memcpy(&Bits, &Value, sizeof(float));
	WriteUInt32LE(Out, Bits);
}

/* Primitive LE Readers */
bool FBinarySerializer::ReadInt32LE(std::ifstream& In, int32& OutValue) const
{
	uint32 Bits = 0;
	if (!ReadUInt32LE(In, Bits))
	{
		return false;
	}

	OutValue = static_cast<int32>(Bits);
	return true;
}

bool FBinarySerializer::ReadUInt32LE(std::ifstream& In, uint32& OutValue) const
{
	unsigned char Bytes[4] = {};
	In.read(reinterpret_cast<char*>(Bytes), 4);

	if (!In.good())
	{
		return false;
	}

	OutValue =
		(static_cast<uint32>(Bytes[0]) << 0) |
		(static_cast<uint32>(Bytes[1]) << 8) |
		(static_cast<uint32>(Bytes[2]) << 16) |
		(static_cast<uint32>(Bytes[3]) << 24);

	return true;
}

bool FBinarySerializer::ReadUInt64LE(std::ifstream& In, uint64& OutValue) const
{
	unsigned char Bytes[8] = {};
	In.read(reinterpret_cast<char*>(Bytes), 8);

	if (!In.good())
	{
		return false;
	}

	OutValue =
		(static_cast<uint64>(Bytes[0]) << 0)  |
		(static_cast<uint64>(Bytes[1]) << 8)  |
		(static_cast<uint64>(Bytes[2]) << 16) |
		(static_cast<uint64>(Bytes[3]) << 24) |
		(static_cast<uint64>(Bytes[4]) << 32) |
		(static_cast<uint64>(Bytes[5]) << 40) |
		(static_cast<uint64>(Bytes[6]) << 48) |
		(static_cast<uint64>(Bytes[7]) << 56);

	return true;
}

bool FBinarySerializer::ReadFloatLE(std::ifstream& In, float& OutValue) const
{
	uint32 Bits = 0;
	if (!ReadUInt32LE(In, Bits))
	{
		return false;
	}

	std::memcpy(&OutValue, &Bits, sizeof(float));
	return true;
}

/* Header Serialization */
void FBinarySerializer::WriteHeader(std::ofstream& Out, const FStaticMeshBinaryHeader& Header)
{
	WriteUInt32LE(Out, Header.MagicNumber);
	WriteUInt32LE(Out, Header.Version);
	WriteUInt32LE(Out, Header.VertexCount);
	WriteUInt32LE(Out, Header.IndexCount);
	WriteUInt32LE(Out, Header.SectionCount);
	WriteUInt32LE(Out, Header.SlotCount);
	WriteUInt64LE(Out, Header.SourceFileWriteTime);
}

bool FBinarySerializer::ReadHeader(std::ifstream& In, FStaticMeshBinaryHeader& OutHeader) const
{
	return ReadUInt32LE(In, OutHeader.MagicNumber)
		&& ReadUInt32LE(In, OutHeader.Version)
		&& ReadUInt32LE(In, OutHeader.VertexCount)
		&& ReadUInt32LE(In, OutHeader.IndexCount)
		&& ReadUInt32LE(In, OutHeader.SectionCount)
		&& ReadUInt32LE(In, OutHeader.SlotCount)
		&& ReadUInt64LE(In, OutHeader.SourceFileWriteTime);
}

void FBinarySerializer::WriteString(std::ofstream& Out, const FString& String)
{
	//	Length + Data Pattern
	uint32 Length = static_cast<uint32>(String.length());
	WriteUInt32LE(Out, Length);

	if (Length > 0)
	{
		/*
		 *	주의:
		 *	- FString::value_type 크기에 의존함
		 *	- 현재 프로젝트가 wchar_t 기반이라면 같은 프로젝트 내에서는 문제없음
		 *	- 완전한 플랫폼 독립 문자열 포맷이 필요하면 UTF-8 변환 후 byte array로 저장하는 편이 더 낫다.
		 */
		Out.write(reinterpret_cast<const char*>(String.c_str()), sizeof(FString::value_type) * Length);
	}
}

bool FBinarySerializer::ReadString(std::ifstream& In, FString& OutString) const
{
	uint32 Length = 0;
	if (!ReadUInt32LE(In, Length))
	{
		OutString.clear();
		return false;
	}

	if (Length > MAX_STRING_LENGTH)
	{
		In.setstate(std::ios::failbit);
		OutString.clear();
		return false;
	}

	OutString.resize(Length);

	if (Length > 0)
	{
		In.read(reinterpret_cast<char*>(OutString.data()), sizeof(FString::value_type) * Length);

		if (!In.good())
		{
			OutString.clear();
			return false;
		}
	}

	return true;
}

void FBinarySerializer::WriteIndexArray(std::ofstream& Out, const TArray<uint32>& Array)
{
	//	Length + Data Pattern
	uint32 Count = static_cast<uint32>(Array.size());
	WriteUInt32LE(Out, Count);

	for (uint32 Value : Array)
	{
		WriteUInt32LE(Out, Value);
	}
}

bool FBinarySerializer::ReadIndexArray(std::ifstream& In, TArray<uint32>& OutArray) const
{
	uint32 Count = 0;
	if (!ReadUInt32LE(In, Count))
	{
		return false;
	}

	if (Count > MAX_STATIC_MESH_INDEX_COUNT)
	{
		In.setstate(std::ios::failbit);
		return false;
	}

	OutArray.resize(Count);

	for (uint32 i = 0; i < Count; i++)
	{
		if (!ReadUInt32LE(In, OutArray[i]))
		{
			return false;
		}
	}

	return true;
}

void FBinarySerializer::WriteVertices(std::ofstream& Out, const FStaticMesh& Data)
{

	uint32 Count = static_cast<uint32>(Data.Vertices.size());
	WriteUInt32LE(Out, Count);

	for (const FNormalVertex& Vertex : Data.Vertices)
	{
		//	Position
		WriteFloatLE(Out, Vertex.Position.X);
		WriteFloatLE(Out, Vertex.Position.Y);
		WriteFloatLE(Out, Vertex.Position.Z);

		//	Color
		WriteFloatLE(Out, Vertex.Color.R);
		WriteFloatLE(Out, Vertex.Color.G);
		WriteFloatLE(Out, Vertex.Color.B);
		WriteFloatLE(Out, Vertex.Color.A);

		//	Normal
		WriteFloatLE(Out, Vertex.Normal.X);
		WriteFloatLE(Out, Vertex.Normal.Y);
		WriteFloatLE(Out, Vertex.Normal.Z);

		//	UVs
		WriteFloatLE(Out, Vertex.UVs.X);
		WriteFloatLE(Out, Vertex.UVs.Y);

		// Tangent
		WriteFloatLE(Out, Vertex.Tangent.X);
        WriteFloatLE(Out, Vertex.Tangent.Y);
        WriteFloatLE(Out, Vertex.Tangent.Z);
	}
}

bool FBinarySerializer::ReadVertices(std::ifstream& In, FStaticMesh& OutData, uint32 VertexCount) const
{
	uint32 Count = 0;
	if (!ReadUInt32LE(In, Count))
	{
		return false;
	}

	if (Count != VertexCount || Count > MAX_STATIC_MESH_VERTEX_COUNT)
	{
		In.setstate(std::ios::failbit);
		return false;
	}

	OutData.Vertices.resize(Count);

	for (FNormalVertex& Vertex : OutData.Vertices)
	{
		//	Position
		if (!ReadFloatLE(In, Vertex.Position.X) ||
			!ReadFloatLE(In, Vertex.Position.Y) ||
			!ReadFloatLE(In, Vertex.Position.Z))
		{
			return false;
		}

		//	Color
		if (!ReadFloatLE(In, Vertex.Color.R) ||
			!ReadFloatLE(In, Vertex.Color.G) ||
			!ReadFloatLE(In, Vertex.Color.B) ||
			!ReadFloatLE(In, Vertex.Color.A))
		{
			return false;
		}

		//	Normal
		if (!ReadFloatLE(In, Vertex.Normal.X) ||
			!ReadFloatLE(In, Vertex.Normal.Y) ||
			!ReadFloatLE(In, Vertex.Normal.Z))
		{
			return false;
		}

		//	UVs
		if (!ReadFloatLE(In, Vertex.UVs.X) ||
			!ReadFloatLE(In, Vertex.UVs.Y))
		{
			return false;
		}

		// Tangent
		if (!ReadFloatLE(In, Vertex.Tangent.X) ||
			!ReadFloatLE(In, Vertex.Tangent.Y) ||
			!ReadFloatLE(In, Vertex.Tangent.Z))
		{
			return false;
		}
	}

	return In.good();
}

void FBinarySerializer::WriteSections(std::ofstream& Out, const FStaticMesh& Data)
{
	uint32 Count = static_cast<uint32>(Data.Sections.size());
	WriteUInt32LE(Out, Count);

	for (const FStaticMeshSection& Section : Data.Sections)
	{
		WriteUInt32LE(Out, Section.StartIndex);
		WriteUInt32LE(Out, Section.IndexCount);
		WriteInt32LE(Out, Section.MaterialSlotIndex);
	}
}

bool FBinarySerializer::ReadSections(std::ifstream& In, FStaticMesh& OutData, uint32 SectionCount) const
{
	uint32 Count = 0;
	if (!ReadUInt32LE(In, Count))
	{
		return false;
	}

	if (Count != SectionCount || Count > MAX_STATIC_MESH_SECTION_COUNT)
	{
		In.setstate(std::ios::failbit);
		return false;
	}

	OutData.Sections.resize(Count);

	for (FStaticMeshSection& Section : OutData.Sections)
	{
		if (!ReadUInt32LE(In, Section.StartIndex) ||
			!ReadUInt32LE(In, Section.IndexCount) ||
			!ReadInt32LE(In, Section.MaterialSlotIndex))
		{
			return false;
		}
	}

	return In.good();
}

void FBinarySerializer::WriteBounds(std::ofstream& Out, const FStaticMesh& Data)
{

	WriteFloatLE(Out, Data.LocalBounds.Min.X);
	WriteFloatLE(Out, Data.LocalBounds.Min.Y);
	WriteFloatLE(Out, Data.LocalBounds.Min.Z);

	WriteFloatLE(Out, Data.LocalBounds.Max.X);
	WriteFloatLE(Out, Data.LocalBounds.Max.Y);
	WriteFloatLE(Out, Data.LocalBounds.Max.Z);
}

bool FBinarySerializer::ReadBounds(std::ifstream& In, FStaticMesh& OutData) const
{

	return ReadFloatLE(In, OutData.LocalBounds.Min.X)
		&& ReadFloatLE(In, OutData.LocalBounds.Min.Y)
		&& ReadFloatLE(In, OutData.LocalBounds.Min.Z)
		&& ReadFloatLE(In, OutData.LocalBounds.Max.X)
		&& ReadFloatLE(In, OutData.LocalBounds.Max.Y)
		&& ReadFloatLE(In, OutData.LocalBounds.Max.Z);
}

//	보내는 순서와 읽는 순서는 동일 (Header + Body 순서를 고정 -> protocol의 정의)
bool FBinarySerializer::SaveStaticMesh(const FString& BinaryPath, const FString& SourcePath, const FStaticMesh& Data)
{
	std::ofstream Out(BinaryPath, std::ios::binary);
	if (!Out.is_open())
	{
		return false;
	}
	
	//	Packet Header와 유사한 개념 (쓰레기 데이터를 읽지 않기 위함)
	FStaticMeshBinaryHeader Header;
	Header.MagicNumber = STATIC_MESH_BINARY_MAGIC;	//	우리의 포맷인지 확인
	Header.Version = STATIC_MESH_BINARY_VERSION;	//	포맷이 변경되었을 시 Version을 통해 무력화 혹은 대응 가능
	//	Count류 -> Parsing 안정성
	Header.VertexCount = static_cast<uint32>(Data.Vertices.size());
	Header.IndexCount = static_cast<uint32>(Data.Indices.size());
	Header.SectionCount = static_cast<uint32>(Data.Sections.size());
	Header.SlotCount = static_cast<uint32>(Data.Slots.size());
	Header.SourceFileWriteTime = GetFileWriteTimeTicks(SourcePath);

	if (!IsValidStaticMeshHeader(Header))
	{
		return false;
	}

	WriteHeader(Out, Header);

	WriteString(Out, Data.PathFileName);
	WriteVertices(Out, Data);
	WriteIndexArray(Out, Data.Indices);
	WriteSections(Out, Data);

	uint32 Count = static_cast<uint32>(Data.Slots.size());
	WriteUInt32LE(Out, Count);
	for (const auto& Slot : Data.Slots)
	{
		WriteString(Out, Slot.SlotName);
	}

	WriteBounds(Out, Data);

	return Out.good();
}

bool FBinarySerializer::LoadStaticMesh(const FString& BinaryPath, FStaticMesh& OutData)
{
	std::ifstream In(std::filesystem::path(FPaths::ToWide(BinaryPath)), std::ios::binary);
	if (!In.is_open())
	{
		return false;
	}

	FStaticMeshBinaryHeader Header;
	if (!ReadHeader(In, Header))
	{
		return false;
	}

	if (!IsValidStaticMeshHeader(Header))
	{
		return false;
	}

	if (!ReadString(In, OutData.PathFileName))
	{
		return false;
	}

	if (!ReadVertices(In, OutData, Header.VertexCount))
	{
		return false;
	}

	if (!ReadIndexArray(In, OutData.Indices))
	{
		return false;
	}

	if (!ReadSections(In, OutData, Header.SectionCount))
	{
		return false;
	}

	uint32 Count = 0;
	if (!ReadUInt32LE(In, Count))
	{
		return false;
	}

	if (Count != Header.SlotCount || Count > MAX_STATIC_MESH_SLOTNAME_COUNT)
	{
		return false;
	}

	OutData.Slots.resize(Count);

	for (uint32 i = 0; i < Count; i++)
	{
		if (!ReadString(In, OutData.Slots[i].SlotName))
		{
			return false;
		}
	}

	if (!ReadBounds(In, OutData))
	{
		return false;
	}

	if (!In.good())
	{
		return false;
	}

	return OutData.Vertices.size() == Header.VertexCount
		&& OutData.Indices.size() == Header.IndexCount
		&& OutData.Sections.size() == Header.SectionCount
		&& OutData.Slots.size() == Header.SlotCount;
}

bool FBinarySerializer::ReadStaticMeshHeader(const FString& BinaryPath, FStaticMeshBinaryHeader& OutHeader) const
{
	std::ifstream In(std::filesystem::path(FPaths::ToWide(BinaryPath)), std::ios::binary);
	if (!In.is_open())
	{
		return false;
	}

	if (!ReadHeader(In, OutHeader))
	{
		return false;
	}

	if (!In.good())
	{
		return false;
	}

	if (!IsValidStaticMeshHeader(OutHeader))
	{
		return false;
	}

	return true;
}

/* ============================================================================
 *  Skeletal Mesh Serialization
 *  - 정책은 StaticMesh와 동일: Little-Endian, 멤버 단위, Header→Body,
 *    Length-Prefix, Magic/Version/Counts validation, Read마다 cross-check.
 *  - 추가 요소: FBoneInfo, FMatrix(4x4), Vertex의 BoneIndices/Weights.
 *  - flat 캐시(InverseBindPoseMatrices/ReferenceLocal/GlobalPose)는
 *    Bones에서 도출 가능하므로 디스크에 쓰지 않고 Load 직후 재구성.
 * ========================================================================== */

void FBinarySerializer::WriteSkeletalHeader(std::ofstream& Out, const FSkeletalMeshBinaryHeader& Header)
{
	WriteUInt32LE(Out, Header.MagicNumber);
	WriteUInt32LE(Out, Header.Version);
	WriteUInt32LE(Out, Header.VertexCount);
	WriteUInt32LE(Out, Header.IndexCount);
	WriteUInt32LE(Out, Header.SectionCount);
	WriteUInt32LE(Out, Header.SlotCount);
	WriteUInt32LE(Out, Header.BoneCount);
	WriteUInt64LE(Out, Header.SourceFileWriteTime);
}

bool FBinarySerializer::ReadSkeletalHeader(std::ifstream& In, FSkeletalMeshBinaryHeader& OutHeader) const
{
	return ReadUInt32LE(In, OutHeader.MagicNumber)
		&& ReadUInt32LE(In, OutHeader.Version)
		&& ReadUInt32LE(In, OutHeader.VertexCount)
		&& ReadUInt32LE(In, OutHeader.IndexCount)
		&& ReadUInt32LE(In, OutHeader.SectionCount)
		&& ReadUInt32LE(In, OutHeader.SlotCount)
		&& ReadUInt32LE(In, OutHeader.BoneCount)
		&& ReadUInt64LE(In, OutHeader.SourceFileWriteTime);
}

void FBinarySerializer::WriteMatrix4x4(std::ofstream& Out, const FMatrix& M)
{
	// row-major 16 float
	for (int32 i = 0; i < 4; ++i)
	{
		for (int32 j = 0; j < 4; ++j)
		{
			WriteFloatLE(Out, M.M[i][j]);
		}
	}
}

bool FBinarySerializer::ReadMatrix4x4(std::ifstream& In, FMatrix& OutM) const
{
	for (int32 i = 0; i < 4; ++i)
	{
		for (int32 j = 0; j < 4; ++j)
		{
			if (!ReadFloatLE(In, OutM.M[i][j]))
			{
				return false;
			}
		}
	}

	return true;
}

void FBinarySerializer::WriteSkeletalVertices(std::ofstream& Out, const FSkeletalMesh& Data)
{
	uint32 Count = static_cast<uint32>(Data.Vertices.size());
	WriteUInt32LE(Out, Count);

	for (const FSkeletalMeshVertex& V : Data.Vertices)
	{
		//	Position
		WriteFloatLE(Out, V.Position.X);
		WriteFloatLE(Out, V.Position.Y);
		WriteFloatLE(Out, V.Position.Z);

		//	Color
		WriteFloatLE(Out, V.Color.R);
		WriteFloatLE(Out, V.Color.G);
		WriteFloatLE(Out, V.Color.B);
		WriteFloatLE(Out, V.Color.A);

		//	Normal
		WriteFloatLE(Out, V.Normal.X);
		WriteFloatLE(Out, V.Normal.Y);
		WriteFloatLE(Out, V.Normal.Z);

		//	UVs
		WriteFloatLE(Out, V.UVs.X);
		WriteFloatLE(Out, V.UVs.Y);

		//	Tangent (FVector4 — bitangent sign 보존을 위해 W까지)
		WriteFloatLE(Out, V.Tangent.X);
		WriteFloatLE(Out, V.Tangent.Y);
		WriteFloatLE(Out, V.Tangent.Z);
		WriteFloatLE(Out, V.Tangent.W);

		//	Bone influences: 4 × uint8 + 4 × float
		//	uint8은 endianness 영향 없음 → byte 그대로 write
		Out.write(reinterpret_cast<const char*>(V.BoneIndices), 4);

		WriteFloatLE(Out, V.BoneWeights[0]);
		WriteFloatLE(Out, V.BoneWeights[1]);
		WriteFloatLE(Out, V.BoneWeights[2]);
		WriteFloatLE(Out, V.BoneWeights[3]);
	}
}

bool FBinarySerializer::ReadSkeletalVertices(std::ifstream& In, FSkeletalMesh& OutData, uint32 VertexCount) const
{
	uint32 Count = 0;
	if (!ReadUInt32LE(In, Count))
	{
		return false;
	}

	if (Count != VertexCount || Count > MAX_SKELETAL_MESH_VERTEX_COUNT)
	{
		In.setstate(std::ios::failbit);
		return false;
	}

	OutData.Vertices.resize(Count);

	for (FSkeletalMeshVertex& V : OutData.Vertices)
	{
		//	Position
		if (!ReadFloatLE(In, V.Position.X) ||
			!ReadFloatLE(In, V.Position.Y) ||
			!ReadFloatLE(In, V.Position.Z))
		{
			return false;
		}

		//	Color
		if (!ReadFloatLE(In, V.Color.R) ||
			!ReadFloatLE(In, V.Color.G) ||
			!ReadFloatLE(In, V.Color.B) ||
			!ReadFloatLE(In, V.Color.A))
		{
			return false;
		}

		//	Normal
		if (!ReadFloatLE(In, V.Normal.X) ||
			!ReadFloatLE(In, V.Normal.Y) ||
			!ReadFloatLE(In, V.Normal.Z))
		{
			return false;
		}

		//	UVs
		if (!ReadFloatLE(In, V.UVs.X) ||
			!ReadFloatLE(In, V.UVs.Y))
		{
			return false;
		}

		//	Tangent.xyzw
		if (!ReadFloatLE(In, V.Tangent.X) ||
			!ReadFloatLE(In, V.Tangent.Y) ||
			!ReadFloatLE(In, V.Tangent.Z) ||
			!ReadFloatLE(In, V.Tangent.W))
		{
			return false;
		}

		//	Bone influences
		In.read(reinterpret_cast<char*>(V.BoneIndices), 4);
		if (!In.good())
		{
			return false;
		}

		if (!ReadFloatLE(In, V.BoneWeights[0]) ||
			!ReadFloatLE(In, V.BoneWeights[1]) ||
			!ReadFloatLE(In, V.BoneWeights[2]) ||
			!ReadFloatLE(In, V.BoneWeights[3]))
		{
			return false;
		}
	}

	return In.good();
}

void FBinarySerializer::WriteSkeletalSections(std::ofstream& Out, const FSkeletalMesh& Data)
{
	uint32 Count = static_cast<uint32>(Data.Sections.size());
	WriteUInt32LE(Out, Count);

	for (const FStaticMeshSection& Section : Data.Sections)
	{
		WriteUInt32LE(Out, Section.StartIndex);
		WriteUInt32LE(Out, Section.IndexCount);
		WriteInt32LE(Out, Section.MaterialSlotIndex);
	}
}

bool FBinarySerializer::ReadSkeletalSections(std::ifstream& In, FSkeletalMesh& OutData, uint32 SectionCount) const
{
	uint32 Count = 0;
	if (!ReadUInt32LE(In, Count))
	{
		return false;
	}

	if (Count != SectionCount || Count > MAX_SKELETAL_MESH_SECTION_COUNT)
	{
		In.setstate(std::ios::failbit);
		return false;
	}

	OutData.Sections.resize(Count);

	for (FStaticMeshSection& Section : OutData.Sections)
	{
		if (!ReadUInt32LE(In, Section.StartIndex) ||
			!ReadUInt32LE(In, Section.IndexCount) ||
			!ReadInt32LE(In, Section.MaterialSlotIndex))
		{
			return false;
		}
	}

	return In.good();
}

void FBinarySerializer::WriteBones(std::ofstream& Out, const FSkeletalMesh& Data)
{
	uint32 Count = static_cast<uint32>(Data.Bones.size());
	WriteUInt32LE(Out, Count);

	for (const FBoneInfo& Bone : Data.Bones)
	{
		WriteString(Out, Bone.Name);
		WriteInt32LE(Out, Bone.ParentIndex);
		WriteMatrix4x4(Out, Bone.LocalBindTransform);
		WriteMatrix4x4(Out, Bone.GlobalBindTransform);
		WriteMatrix4x4(Out, Bone.InverseBindPose);
	}
}

bool FBinarySerializer::ReadBones(std::ifstream& In, FSkeletalMesh& OutData, uint32 BoneCount) const
{
	uint32 Count = 0;
	if (!ReadUInt32LE(In, Count))
	{
		return false;
	}

	if (Count != BoneCount || Count > MAX_SKELETAL_MESH_BONE_COUNT)
	{
		In.setstate(std::ios::failbit);
		return false;
	}

	OutData.Bones.resize(Count);

	for (FBoneInfo& Bone : OutData.Bones)
	{
		if (!ReadString(In, Bone.Name))
		{
			return false;
		}

		if (!ReadInt32LE(In, Bone.ParentIndex))
		{
			return false;
		}

		if (!ReadMatrix4x4(In, Bone.LocalBindTransform) ||
			!ReadMatrix4x4(In, Bone.GlobalBindTransform) ||
			!ReadMatrix4x4(In, Bone.InverseBindPose))
		{
			return false;
		}
	}

	return In.good();
}

void FBinarySerializer::WriteSkeletalBounds(std::ofstream& Out, const FSkeletalMesh& Data)
{
	WriteFloatLE(Out, Data.LocalBounds.Min.X);
	WriteFloatLE(Out, Data.LocalBounds.Min.Y);
	WriteFloatLE(Out, Data.LocalBounds.Min.Z);

	WriteFloatLE(Out, Data.LocalBounds.Max.X);
	WriteFloatLE(Out, Data.LocalBounds.Max.Y);
	WriteFloatLE(Out, Data.LocalBounds.Max.Z);
}

bool FBinarySerializer::ReadSkeletalBounds(std::ifstream& In, FSkeletalMesh& OutData) const
{
	return ReadFloatLE(In, OutData.LocalBounds.Min.X)
		&& ReadFloatLE(In, OutData.LocalBounds.Min.Y)
		&& ReadFloatLE(In, OutData.LocalBounds.Min.Z)
		&& ReadFloatLE(In, OutData.LocalBounds.Max.X)
		&& ReadFloatLE(In, OutData.LocalBounds.Max.Y)
		&& ReadFloatLE(In, OutData.LocalBounds.Max.Z);
}

bool FBinarySerializer::SaveSkeletalMesh(const FString& BinaryPath, const FString& SourcePath, const FSkeletalMesh& Data)
{
	std::ofstream Out(BinaryPath, std::ios::binary);
	if (!Out.is_open())
	{
		return false;
	}

	FSkeletalMeshBinaryHeader Header;
	Header.MagicNumber = SKELETAL_MESH_BINARY_MAGIC;
	Header.Version     = SKELETAL_MESH_BINARY_VERSION;
	Header.VertexCount  = static_cast<uint32>(Data.Vertices.size());
	Header.IndexCount   = static_cast<uint32>(Data.Indices.size());
	Header.SectionCount = static_cast<uint32>(Data.Sections.size());
	Header.SlotCount    = static_cast<uint32>(Data.MaterialSlots.size());
	Header.BoneCount    = static_cast<uint32>(Data.Bones.size());
	Header.SourceFileWriteTime = GetFileWriteTimeTicks(SourcePath);

	if (!IsValidSkeletalMeshHeader(Header))
	{
		return false;
	}

	WriteSkeletalHeader(Out, Header);

	WriteString(Out, Data.PathFileName);
	WriteSkeletalVertices(Out, Data);
	WriteIndexArray(Out, Data.Indices);
	WriteSkeletalSections(Out, Data);

	//	Material Slots — StaticMesh와 동일하게 SlotName만 저장. Material* 포인터는 로드 후 resolve.
	uint32 SlotCount = static_cast<uint32>(Data.MaterialSlots.size());
	WriteUInt32LE(Out, SlotCount);
	for (const FStaticMeshMaterialSlot& Slot : Data.MaterialSlots)
	{
		WriteString(Out, Slot.SlotName);
	}

	WriteBones(Out, Data);
	WriteSkeletalBounds(Out, Data);

	return Out.good();
}

bool FBinarySerializer::LoadSkeletalMesh(const FString& BinaryPath, FSkeletalMesh& OutData)
{
	std::ifstream In(std::filesystem::path(FPaths::ToWide(BinaryPath)), std::ios::binary);
	if (!In.is_open())
	{
		return false;
	}

	FSkeletalMeshBinaryHeader Header;
	if (!ReadSkeletalHeader(In, Header))
	{
		return false;
	}

	if (!IsValidSkeletalMeshHeader(Header))
	{
		return false;
	}

	if (!ReadString(In, OutData.PathFileName))
	{
		return false;
	}

	if (!ReadSkeletalVertices(In, OutData, Header.VertexCount))
	{
		return false;
	}

	if (!ReadIndexArray(In, OutData.Indices))
	{
		return false;
	}

	if (!ReadSkeletalSections(In, OutData, Header.SectionCount))
	{
		return false;
	}

	uint32 SlotCount = 0;
	if (!ReadUInt32LE(In, SlotCount))
	{
		return false;
	}

	if (SlotCount != Header.SlotCount || SlotCount > MAX_SKELETAL_MESH_SLOTNAME_COUNT)
	{
		return false;
	}

	OutData.MaterialSlots.resize(SlotCount);
	for (uint32 i = 0; i < SlotCount; ++i)
	{
		if (!ReadString(In, OutData.MaterialSlots[i].SlotName))
		{
			return false;
		}

		OutData.MaterialSlots[i].Material = nullptr; // load 후 resolve
	}

	if (!ReadBones(In, OutData, Header.BoneCount))
	{
		return false;
	}

	if (!ReadSkeletalBounds(In, OutData))
	{
		return false;
	}

	if (!In.good())
	{
		return false;
	}

	//	Header ↔ Body 카운트 cross-check
	if (!(OutData.Vertices.size()      == Header.VertexCount  &&
	      OutData.Indices.size()       == Header.IndexCount   &&
	      OutData.Sections.size()      == Header.SectionCount &&
	      OutData.MaterialSlots.size() == Header.SlotCount    &&
	      OutData.Bones.size()         == Header.BoneCount))
	{
		return false;
	}

	//	flat 캐시는 Bones에서 도출 — 디스크에는 굳이 굽지 않고 로드 후 한 번 채움.
	//	(Bones의 세 행렬이 곧 캐시의 원본 값이라 재계산 없이 그대로 복사하면 됨.)
	const uint32 BoneCount = Header.BoneCount;
	OutData.ReferenceLocalPose.resize(BoneCount);
	OutData.ReferenceGlobalPose.resize(BoneCount);
	OutData.InverseBindPoseMatrices.resize(BoneCount);
	for (uint32 i = 0; i < BoneCount; ++i)
	{
		OutData.ReferenceLocalPose[i]      = OutData.Bones[i].LocalBindTransform;
		OutData.ReferenceGlobalPose[i]     = OutData.Bones[i].GlobalBindTransform;
		OutData.InverseBindPoseMatrices[i] = OutData.Bones[i].InverseBindPose;
	}

	return true;
}

bool FBinarySerializer::ReadSkeletalMeshHeader(const FString& BinaryPath, FSkeletalMeshBinaryHeader& OutHeader) const
{
	std::ifstream In(std::filesystem::path(FPaths::ToWide(BinaryPath)), std::ios::binary);
	if (!In.is_open())
	{
		return false;
	}

	if (!ReadSkeletalHeader(In, OutHeader))
	{
		return false;
	}

	if (!In.good())
	{
		return false;
	}

	if (!IsValidSkeletalMeshHeader(OutHeader))
	{
		return false;
	}

	return true;
}
