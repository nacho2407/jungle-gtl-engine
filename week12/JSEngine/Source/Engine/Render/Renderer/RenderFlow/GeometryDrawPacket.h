#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/RenderCommand.h"

#include <d3d11.h>

// Vertex / index / instance 입력을 하나의 draw call 단위로 묶은 geometry packet
struct FGeometryDrawPacket
{
	ID3D11Buffer* VertexBuffers[2] = { nullptr, nullptr };
	uint32 Strides[2] = { 0, 0 };
	uint32 Offsets[2] = { 0, 0 };
	uint32 VertexBufferCount = 0;
	ID3D11Buffer* IndexBuffer = nullptr;
	uint32 IndexCount = 0;
	uint32 IndexStart = 0;
	uint32 VertexCount = 0;
	uint32 InstanceCount = 0;
	bool bInstanced = false;
};

inline bool BuildMeshGeometryDrawPacket(
	const FRenderCommand& Command,
	FGeometryDrawPacket& OutPacket,
	bool bInstancedDraw = false)
{
	if (Command.MeshBuffer == nullptr || !Command.MeshBuffer->IsValid())
	{
		return false;
	}

	ID3D11Buffer* VertexBuffer = Command.MeshBuffer->GetVertexBuffer().GetBuffer();
	const uint32 VertexCount = Command.MeshBuffer->GetVertexBuffer().GetVertexCount();
	const uint32 Stride = Command.MeshBuffer->GetVertexBuffer().GetStride();
	if (VertexBuffer == nullptr || VertexCount == 0 || Stride == 0)
	{
		return false;
	}

	ID3D11Buffer* IndexBuffer = Command.MeshBuffer->GetIndexBuffer().GetBuffer();
	if (bInstancedDraw && (IndexBuffer == nullptr || Command.SectionIndexCount == 0))
	{
		return false;
	}

	OutPacket.VertexBuffers[0] = VertexBuffer;
	OutPacket.Strides[0] = Stride;
	OutPacket.Offsets[0] = 0;
	OutPacket.VertexBufferCount = 1;
	OutPacket.IndexBuffer = IndexBuffer;
	OutPacket.IndexCount = Command.SectionIndexCount;
	OutPacket.IndexStart = Command.SectionIndexStart;
	OutPacket.VertexCount = VertexCount;

	if (bInstancedDraw)
	{
		OutPacket.VertexBuffers[1] = Command.InstanceBufferView.Buffer;
		OutPacket.Strides[1] = Command.InstanceBufferView.Stride;
		OutPacket.Offsets[1] = Command.InstanceBufferView.Offset;
		OutPacket.VertexBufferCount = 2;
		OutPacket.InstanceCount = Command.InstanceBufferView.InstanceCount;
		OutPacket.bInstanced = true;
	}
	return true;
}

inline bool ExecuteGeometryDrawPacket(ID3D11DeviceContext* DeviceContext, const FGeometryDrawPacket& Packet)
{
	if (DeviceContext == nullptr || Packet.VertexBufferCount == 0 || Packet.VertexBuffers[0] == nullptr)
	{
		return false;
	}

	DeviceContext->IASetVertexBuffers(0, Packet.VertexBufferCount, Packet.VertexBuffers, Packet.Strides, Packet.Offsets);

	if (Packet.IndexBuffer != nullptr)
	{
		DeviceContext->IASetIndexBuffer(Packet.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	}

	if (Packet.bInstanced)
	{
		if (Packet.IndexBuffer == nullptr || Packet.IndexCount == 0 || Packet.InstanceCount == 0)
		{
			return false;
		}
		DeviceContext->DrawIndexedInstanced(Packet.IndexCount, Packet.InstanceCount, Packet.IndexStart, 0, 0);
	}
	else if (Packet.IndexBuffer != nullptr)
	{
		DeviceContext->DrawIndexed(Packet.IndexCount, Packet.IndexStart, 0);
	}
	else
	{
		DeviceContext->Draw(Packet.VertexCount, 0);
	}

	if (Packet.VertexBufferCount > 1)
	{
		ID3D11Buffer* NullVertexBuffer = nullptr;
		uint32 NullStride = 0;
		uint32 NullOffset = 0;
		DeviceContext->IASetVertexBuffers(1, 1, &NullVertexBuffer, &NullStride, &NullOffset);
	}
	return true;
}
