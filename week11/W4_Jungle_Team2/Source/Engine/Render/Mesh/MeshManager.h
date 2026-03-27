#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"
#include "Render/Resource/VertexTypes.h"


class FMeshManager : public TSingleton<FMeshManager>
{
	friend class TSingleton<FMeshManager>;

private:
	FMeshManager() = default;

	static FMeshData CubeMeshData;
	static FMeshData PlaneMeshData;
	static FMeshData SphereMeshData;
	static FMeshData TranslationGizmoMeshData;
	static FMeshData RotationGizmoMeshData;
	static FMeshData ScaleGizmoMeshData;
	static FMeshData QuadMeshData;
	
	static void CreateCube();
	static void CreatePlane();
	static void CreateSphere(int slices = 20, int stacks = 20);
	static void CreateTranslationGizmo();
	static void CreateRotationGizmo();
	static void CreateScaleGizmo();
	static void CreateQuad();

	static bool bIsInitialized;

public:
	static void Initialize();
	static const FMeshData& GetCube(){return Get().CubeMeshData; }
	static const FMeshData& GetPlane(){ return Get().PlaneMeshData; }
	static const FMeshData& GetSphere(){ return Get().SphereMeshData; }
	static const FMeshData& GetTranslationGizmo() { return Get().TranslationGizmoMeshData; }
	static const FMeshData& GetRotationGizmo() { return Get().RotationGizmoMeshData; }
	static const FMeshData& GetScaleGizmo() { return Get().ScaleGizmoMeshData; }
	static const FMeshData& GetQuad() { return Get().QuadMeshData; }

};


