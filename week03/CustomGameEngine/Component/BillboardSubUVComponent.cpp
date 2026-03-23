#include "BillboardSubUVComponent.h"

#include <algorithm>

#include "Component/CameraComponent.h"
#include "EngineCore/ResourceManager.h"
#include "Scene.h"

UBillboardSubUVComponent::UBillboardSubUVComponent()
{
}

UBillboardSubUVComponent::~UBillboardSubUVComponent()
{
	if (RenderObj != nullptr)
	{
		if (Scene != nullptr)
		{
			Scene->Level.UnregisterRenderObject(RenderObj);
		}
		delete RenderObj;
	}
}

void UBillboardSubUVComponent::OnComponentAdded()
{
	RenderObj = CreateRenderObject();
	if (RenderObj != nullptr && Scene != nullptr)
	{
		Scene->Level.RegisterRenderObject(RenderObj);
	}
}

void UBillboardSubUVComponent::Update(float DeltaTime)
{
	CurrentTime += DeltaTime;
	UpdateRenderObject();
}

void UBillboardSubUVComponent::SetSize(float InWidth, float InHeight)
{
	Width = InWidth;
	Height = InHeight;
	UpdateRenderObject();
}

void UBillboardSubUVComponent::SetGrid(int32 InRows, int32 InColumns, int32 InFrameCount)
{
	Rows = (std::max)(1, InRows);
	Columns = (std::max)(1, InColumns);
	FrameCount = (std::max)(1, InFrameCount);
	UpdateRenderObject();
}

void UBillboardSubUVComponent::SetPlayback(float InFramesPerSecond, bool bInLoop)
{
	FramesPerSecond = (std::max)(0.0f, InFramesPerSecond);
	bLoop = bInLoop;
	UpdateRenderObject();
}

RenderObject* UBillboardSubUVComponent::CreateRenderObject()
{
	ResourceManager* resourceManager = ResourceManager::GetInstance();

	RenderObject* renderObject = new RenderObject();
	renderObject->Geometry = &resourceManager->GetGeometry("BillboardQuad");
	renderObject->Material = &resourceManager->GetMaterial(L"Shader/ShaderSubUV.hlsl");
	renderObject->ShowFlag = EEngineShowFlags::SF_BillboardText;
	renderObject->bDepthEnabled = false;

	RenderObj = renderObject;
	UpdateRenderObject();
	return renderObject;
}

void UBillboardSubUVComponent::UpdateRenderObject()
{
	if (RenderObj == nullptr)
	{
		return;
	}

	const int32 totalFrames = (std::max)(1, (std::min)(FrameCount, Rows * Columns));
	const int32 frameIndex = (std::min)(GetCurrentFrameIndex(), totalFrames - 1);
	const int32 column = frameIndex % Columns;
	const int32 row = frameIndex / Columns;

	const float uvScaleX = 1.0f / static_cast<float>(Columns);
	const float uvScaleY = 1.0f / static_cast<float>(Rows);
	RenderObj->UVTransform = FVector4(
		uvScaleX,
		uvScaleY,
		column * uvScaleX,
		row * uvScaleY);

	const UCameraComponent* camera = UCameraComponent::GetMainCamera();
	FVector forward = FVector::Forward();
	FVector right = FVector::Right();
	FVector up = FVector::Up();
	if (camera != nullptr)
	{
		forward = camera->Direction.Normalize();
		right = camera->SideDirection.Normalize();
		up = camera->UpDirection.Normalize();
	}

	const FVector location = GetRelativeLocation();
	FMatrix world = FMatrix::IdentityMatrix();
	world[0][0] = forward.x;
	world[0][1] = forward.y;
	world[0][2] = forward.z;
	world[1][0] = right.x * Width;
	world[1][1] = right.y * Width;
	world[1][2] = right.z * Width;
	world[2][0] = up.x * Height;
	world[2][1] = up.y * Height;
	world[2][2] = up.z * Height;
	world[3][0] = location.x;
	world[3][1] = location.y;
	world[3][2] = location.z;

	RenderObj->World = world;
	RenderObj->bIsSelected = bIsSelected;
}

int32 UBillboardSubUVComponent::GetCurrentFrameIndex() const
{
	const int32 totalFrames = (std::max)(1, (std::min)(FrameCount, Rows * Columns));
	if (FramesPerSecond <= 0.0f)
	{
		return 0;
	}

	const int32 frameIndex = static_cast<int32>(CurrentTime * FramesPerSecond);
	if (bLoop)
	{
		return frameIndex % totalFrames;
	}

	return (std::min)(frameIndex, totalFrames - 1);
}

REGISTER_CLASS(UBillboardSubUVComponent);
