#pragma once
#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"
#include "Render/Resource/VertexTypes.h"


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

	static bool bIsInitialized;

public:
	static void Initialize();
	static const FMeshData& GetTranslationGizmo() { return Get().TranslationGizmoMeshData; }
	static const FMeshData& GetRotationGizmo() { return Get().RotationGizmoMeshData; }
	static const FMeshData& GetScaleGizmo() { return Get().ScaleGizmoMeshData; }

};

// Backward compatibility alias (to be removed after all callsites migrate).
using FMeshManager = FEditorMeshLibrary;


