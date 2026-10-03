#pragma once

#include "Core/Containers/Array.h"
#include "Serialization/Archive.h"

struct FMemoryWriter : public FArchive
{
	TArray<unsigned char>& Bytes;
	uint32 Offset;

	FMemoryWriter(TArray<unsigned char>& InBytes)
		: Bytes(InBytes), Offset(0)
	{
	}

	virtual bool IsSaving() const override { return true; }

	virtual void Serialize(void* Value, uint32 Size) override
	{
		if (Offset + Size > (uint32)Bytes.size())
		{
			Bytes.resize(Offset + Size);
		}
		std::memcpy(&Bytes[Offset], Value, Size);
		Offset += Size;
	}
};