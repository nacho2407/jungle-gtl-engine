#pragma once

#include "Core/Types/CoreTypes.h"
#include "Render/Resource/Buffer.h"

class FShader;

struct FMaterialParameterInfo
{
	FString BufferName;
	uint32 SlotIndex = 0;
	uint32 Offset = 0;
	uint32 Size = 0;
	uint32 BufferSize = 0;
};

class FMaterialTemplate
{
private:
	uint32 MaterialTemplateID = 0;
	FShader* Shader = nullptr;
	TMap<FString, FMaterialParameterInfo*> ParameterLayout;

public:
	const TMap<FString, FMaterialParameterInfo*>& GetParameterInfo() const { return ParameterLayout; }
	void Create(FShader* InShader);

	FShader* GetShader() const { return Shader; }
	bool GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const;
};

struct FMaterialConstantBuffer
{
	uint8* CPUData = nullptr;
	FConstantBuffer GPUBuffer;
	uint32 Size = 0;
	UINT SlotIndex = 0;
	bool bDirty = false;

	FMaterialConstantBuffer() = default;
	~FMaterialConstantBuffer();

	FMaterialConstantBuffer(const FMaterialConstantBuffer&) = delete;
	FMaterialConstantBuffer& operator=(const FMaterialConstantBuffer&) = delete;

	void Init(ID3D11Device* InDevice, uint32 InSize, uint32 InSlot);
	void SetData(const void* Data, uint32 InSize, uint32 Offset = 0);
	void Upload(ID3D11DeviceContext* DeviceContext);
	void Release();

	FConstantBuffer* GetConstantBuffer() { return &GPUBuffer; }
};
