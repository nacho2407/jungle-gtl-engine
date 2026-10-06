#pragma once

class UObject;
class UStruct;

namespace FInlinePropertyRenderer
{
	bool RenderStructProperties(UStruct* StructType, void* StructPtr, UObject* Owner, const char* TableId);
}
