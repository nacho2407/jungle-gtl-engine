#pragma once
#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"
#include "Render/Resource/VertexTypes.h"

//	Render/Resource/VertexTypes.h로 이동했습니다.
//struct FVertex
//{
//	FVector Postion;
//	FVector4 Color;
//};

//struct FMeshData
//{
//	TArray<FVertex> Vertices;
//	TArray<uint32> Indices;
//};


class FEditorMeshLibrary : public TSingleton<FEditorMeshLibrary>
{
	friend class TSingleton<FEditorMeshLibrary>;

private:
	FEditorMeshLibrary() = default;

	static FMeshData TranslationGizmoMeshData;
	static FMeshData RotationGizmoMeshData;
	static FMeshData ScaleGizmoMeshData;
	
	static void CreateTranslationGizmo();
	static void CreateRotationGizmo();
	static void CreateScaleGizmo();

#if TEST

	//static void CreateStandfordBunny();
	//static void LoadObj(const char* path, FMeshData& outMeshData);

#endif

	static bool bIsInitialized;

public:
	static void Initialize();
	static const FMeshData& GetTranslationGizmo() { return Get().TranslationGizmoMeshData; }
	static const FMeshData& GetRotationGizmo() { return Get().RotationGizmoMeshData; }
	static const FMeshData& GetScaleGizmo() { return Get().ScaleGizmoMeshData; }

};

// Backward compatibility alias (to be removed after all callsites migrate).
using FMeshManager = FEditorMeshLibrary;


