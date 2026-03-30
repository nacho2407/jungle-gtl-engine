#include "BinarySerializer.h"

#include "Asset/StaticMeshTypes.h"
#include "Core/Paths.h"

#include <fstream>
#include <filesystem>
#include <chrono>

/* Validation Check Constants */
constexpr uint32 STATIC_MESH_BINARY_MAGIC = 0x4853454D; // 'MESH'
constexpr uint32 STATIC_MESH_BINARY_VERSION = 1;

// sanity check
constexpr uint32 MAX_STATIC_MESH_VERTEX_COUNT   = 10'000'000;
constexpr uint32 MAX_STATIC_MESH_INDEX_COUNT    = 30'000'000;
constexpr uint32 MAX_STATIC_MESH_SECTION_COUNT  = 100'000;
constexpr uint32 MAX_STATIC_MESH_SLOTNAME_COUNT = 1024;
constexpr uint32 MAX_STRING_LENGTH              = 4096;

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

	if (Header.SlotNameCount > MAX_STATIC_MESH_SLOTNAME_COUNT)
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

bool FBinarySerializer::SaveStaticMesh(const FString& BinaryPath, const FString & SourcePath, const FStaticMesh& Data)
{
	std::ofstream Out(BinaryPath, std::ios::binary);
	if (!Out.is_open())
	{
		return false;
	}

	FStaticMeshBinaryHeader Header;
	Header.MagicNumber = STATIC_MESH_BINARY_MAGIC;
	Header.Version = STATIC_MESH_BINARY_VERSION;
	Header.VertexCount = static_cast<uint32>(Data.Vertices.size());
	Header.IndexCount = static_cast<uint32>(Data.Indices.size());
	Header.SectionCount = static_cast<uint32>(Data.Sections.size());
	Header.SlotNameCount = static_cast<uint32>(Data.SlotNames.size());
	Header.SourceFileWriteTime = GetFileWriteTimeTicks(SourcePath);

	if (!IsValidStaticMeshHeader(Header))
	{
		return false;
	}

	WriteValue(Out, Header);

	WriteString(Out, Data.PathFileName);
	WriteArray(Out, Data.Vertices);
	WriteArray(Out, Data.Indices);
	WriteArray(Out, Data.Sections);

	uint32 Count = static_cast<uint32>(Data.SlotNames.size());
	WriteValue(Out, Count);
	for (const auto& SlotName : Data.SlotNames)
	{
		WriteString(Out, SlotName);
	}

	WriteValue(Out, Data.LocalBounds);

	return Out.good();
}

bool FBinarySerializer::LoadStaticMesh(const FString& BinaryPath, FStaticMesh& OutData)
{
	std::ifstream In(BinaryPath, std::ios::binary);

	if (!In.is_open())
	{
		return false;
	}

	FStaticMeshBinaryHeader Header;
	ReadValue(In, Header);

	if (!IsValidStaticMeshHeader(Header))
	{
		return false;
	}

	ReadString(In, OutData.PathFileName);
	ReadArray(In, OutData.Vertices);
	ReadArray(In, OutData.Indices);
	ReadArray(In, OutData.Sections);

	uint32 Count = 0;
	ReadValue(In, Count);
	OutData.SlotNames.resize(Count);

	for (uint32 i = 0; i < Count; i++)
	{
		ReadString(In, OutData.SlotNames[i]);
	}

	ReadValue(In, OutData.LocalBounds);

	if (!In.good())
	{
		return false;
	}

	return OutData.Vertices.size() == Header.VertexCount
		&& OutData.Indices.size() == Header.IndexCount
		&& OutData.Sections.size() == Header.SectionCount
		&& OutData.SlotNames.size() == Header.SlotNameCount;
}

bool FBinarySerializer::ReadStaticMeshHeader(const FString& BinaryPath, FStaticMeshBinaryHeader& OutHeader) const
{
	std::ifstream In(BinaryPath, std::ios::binary);
	if (!In.is_open())
	{
		return false;
	}

	ReadValue(In, OutHeader);

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

template <typename T>
void FBinarySerializer::WriteValue(std::ofstream& Out, const T& Value)
{
	Out.write(reinterpret_cast<const char*>(&Value), sizeof(T));
}

template <typename T>
void FBinarySerializer::ReadValue(std::ifstream& In, T& OutValue) const
{
	In.read(reinterpret_cast<char*>(&OutValue), sizeof(T));
}

void FBinarySerializer::WriteString(std::ofstream& Out, const FString& String)
{
	uint32 Length = static_cast<uint32>(String.length());
	WriteValue(Out, Length);

	if (Length > 0)
	{
		Out.write(reinterpret_cast<const char*>(String.c_str()), sizeof(FString::value_type) * Length);
	}
}

void FBinarySerializer::ReadString(std::ifstream& In, FString& OutString) const
{
	uint32 Length = 0;
	ReadValue(In, Length);

	if (!In.good())
	{
		OutString.clear();
		return;
	}

	if (Length > MAX_STRING_LENGTH)
	{
		In.setstate(std::ios::failbit);
		OutString.clear();
		return;
	}

	OutString.resize(Length);

	if (Length > 0)
	{
		In.read(reinterpret_cast<char*>(OutString.data()), sizeof(FString::value_type) * Length);
	}
}

template <typename T>
void FBinarySerializer::WriteArray(std::ofstream& Out, const TArray<T, std::allocator<T>>& Array)
{
	uint32 Count = static_cast<uint32>(Array.size());
	WriteValue(Out, Count);

	if (Count > 0)
	{
		Out.write(reinterpret_cast<const char*>(Array.data()), sizeof(T) * Count);
	}
}

template <typename T>
void FBinarySerializer::ReadArray(std::ifstream& In, TArray<T, std::allocator<T>>& OutArray) const
{
	uint32 Count = 0;
	ReadValue(In, Count);

	OutArray.resize(Count);

	if (Count > 0)
	{
		In.read(reinterpret_cast<char*>(OutArray.data()), sizeof(T) * Count);
	}
}
