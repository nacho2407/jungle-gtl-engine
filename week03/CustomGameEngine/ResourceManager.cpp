#include "ResourceManager.h"
#include "VertexSimple.h"
#include "Sphere.h"
#include "Triangle.h"


void ResourceManager::AddCubeMesh(ID3D11Device* Device)
{
	FVertexSimple cubeVertices[] =
	{
		// Front (+Z) - Red
		{ -0.5f,-0.5f, 0.5f, 1,0,0,1 },
		{  0.5f,-0.5f, 0.5f, 1,0,0,1 },
		{  0.5f, 0.5f, 0.5f, 1,0,0,1 },

		{ -0.5f,-0.5f, 0.5f, 1,0,0,1 },
		{  0.5f, 0.5f, 0.5f, 1,0,0,1 },
		{ -0.5f, 0.5f, 0.5f, 1,0,0,1 },


		// Back (-Z) - Green
		{  0.5f,-0.5f,-0.5f, 0,1,0,1 },
		{ -0.5f,-0.5f,-0.5f, 0,1,0,1 },
		{ -0.5f, 0.5f,-0.5f, 0,1,0,1 },

		{  0.5f,-0.5f,-0.5f, 0,1,0,1 },
		{ -0.5f, 0.5f,-0.5f, 0,1,0,1 },
		{  0.5f, 0.5f,-0.5f, 0,1,0,1 },


		// Left (-X) - Blue
		{ -0.5f,-0.5f,-0.5f, 0,0,1,1 },
		{ -0.5f,-0.5f, 0.5f, 0,0,1,1 },
		{ -0.5f, 0.5f, 0.5f, 0,0,1,1 },

		{ -0.5f,-0.5f,-0.5f, 0,0,1,1 },
		{ -0.5f, 0.5f, 0.5f, 0,0,1,1 },
		{ -0.5f, 0.5f,-0.5f, 0,0,1,1 },


		// Right (+X) - Yellow
		{ 0.5f,-0.5f, 0.5f, 1,1,0,1 },
		{ 0.5f,-0.5f,-0.5f, 1,1,0,1 },
		{ 0.5f, 0.5f,-0.5f, 1,1,0,1 },

		{ 0.5f,-0.5f, 0.5f, 1,1,0,1 },
		{ 0.5f, 0.5f,-0.5f, 1,1,0,1 },
		{ 0.5f, 0.5f, 0.5f, 1,1,0,1 },


		// Top (+Y) - Cyan
		{ -0.5f,0.5f, 0.5f, 0,1,1,1 },
		{  0.5f,0.5f, 0.5f, 0,1,1,1 },
		{  0.5f,0.5f,-0.5f, 0,1,1,1 },

		{ -0.5f,0.5f, 0.5f, 0,1,1,1 },
		{  0.5f,0.5f,-0.5f, 0,1,1,1 },
		{ -0.5f,0.5f,-0.5f, 0,1,1,1 },


		// Bottom (-Y) - Magenta
		{ -0.5f,-0.5f,-0.5f, 1,0,1,1 },
		{  0.5f,-0.5f,-0.5f, 1,0,1,1 },
		{  0.5f,-0.5f, 0.5f, 1,0,1,1 },

		{ -0.5f,-0.5f,-0.5f, 1,0,1,1 },
		{  0.5f,-0.5f, 0.5f, 1,0,1,1 },
		{ -0.5f,-0.5f, 0.5f, 1,0,1,1 },
	};

	int Cnt = sizeof(cubeVertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(Cnt);
	for (int i = 0; i < Cnt; ++i)
	{
		Vertices[i] = FVector(cubeVertices[i].x, cubeVertices[i].y, cubeVertices[i].z);
	}
	CreateGeometry("Cube", cubeVertices, sizeof(cubeVertices), Device, 0, Vertices);
}

void ResourceManager::AddSphereMesh(ID3D11Device* Device)
{
	int Cnt = sizeof(sphere_vertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(Cnt);
	for (int i = 0; i < Cnt; ++i)
	{
		Vertices[i] = FVector(sphere_vertices[i].x, sphere_vertices[i].y, sphere_vertices[i].z);
	}
	CreateGeometry("Sphere", sphere_vertices, sizeof(sphere_vertices), Device, 0, Vertices);
}

void ResourceManager::AddTriangleMesh(ID3D11Device* Device)
{
	int Cnt = sizeof(triangle_vertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(Cnt);
	for (int i = 0; i < Cnt; ++i)
	{
		Vertices[i] = FVector(triangle_vertices[i].x, triangle_vertices[i].y, triangle_vertices[i].z);
	}
	CreateGeometry("Triangle", triangle_vertices, sizeof(triangle_vertices), Device, 0, Vertices);
}

void ResourceManager::AddPlaneMesh(ID3D11Device* Device)
{
	FVertexSimple planeVertices[] =
	{
		{ -5.0f,-5.0f, 0.0f, 81 / 255.0, 91 / 255.0, 212 / 255.0,1 },
		{  5.0f,-5.0f, 0.0f, 129 / 255.0, 52 / 255.0, 175 / 255.0,1 },
		{  5.0f, 5.0f, 0.0f, 221 / 255.0, 42 / 255.0, 123 / 255.0,1 },

		{ -5.0f,-5.0f, 0.0f, 81 / 255.0,91 / 255.0, 212 / 255.0,1 },
		{  5.0f, 5.0f, 0.0f, 221 / 255.0, 42 / 255.0, 123 / 255.0,1 },
		{ -5.0f, 5.0f, 0.0f, 254 / 255.0, 218 / 255.0, 119 / 255.0,1 }
	};
	int Cnt = sizeof(planeVertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(Cnt);
	for (int i = 0; i < Cnt; ++i)
	{
		Vertices[i] = FVector(planeVertices[i].x, planeVertices[i].y, planeVertices[i].z);
	}
	CreateGeometry("Plane", planeVertices, sizeof(planeVertices), Device, 0, Vertices);
}

void ResourceManager::AddAxisMesh(ID3D11Device* Device)
{
	FVertexSimple AxisVertices[] =
	{
		/*X축 (Red): (0,0,0) -> (1,0,0)
	   { 0.0f, 0.0f, 0.0f, 1,0,0,1 },
	   { 10000000.0f, 0.0f, 0.0f, 1,0,0,1 },*/

	   // Y축 (Green): (0,0,0) -> (0,1,0)
	   { 0.0f, 0.0f, -10000.0f, 0.2f,0.2f,1.0f,1 },
	   { 0.0f, 0.0f, 10000.0f, 0.2f,0.2f,1.0f,1 },

	   //// Z축 (Blue): (0,0,0) -> (0,0,1)
	   //{ 0.0f, 0.0f, 0.0f, 0,0,1,1 },
	   //{ 0.0f, 0.0f, 10000000.0f, 0,0,1,1 }
	};

	int Cnt = sizeof(AxisVertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(Cnt);
	for (int i = 0; i < Cnt; ++i)
	{
		Vertices[i] = FVector(AxisVertices[i].x, AxisVertices[i].y, AxisVertices[i].z);
	}
	CreateGeometry("Axis", AxisVertices, sizeof(AxisVertices), Device, 0, Vertices);
}